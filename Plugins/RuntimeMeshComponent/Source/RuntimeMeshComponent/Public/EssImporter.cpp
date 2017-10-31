#include "RuntimeMeshComponentPluginPrivatePCH.h"
#include "EssImporter.h"
#include "Classes/Engine/World.h"
#include "Public/Async/ParallelFor.h"
#include "Public/Interfaces/IImageWrapper.h"
#include "Public/Interfaces/IImageWrapperModule.h"

#define MULTI_THREADING_BUILD 1
#define INVERT_VERTEX_ORDER_FOR_NEGATIVE_SACLE 0
#undef UpdateResource

enum EShaderID
{
	SHADER_ID_BITMAP,
	SHADER_ID_BLEND,
	SHADER_ID_FALLOF,
	SHADER_ID_STDOUT,
	SHADER_ID_STDUV,
	SHADER_ID_STDXYZ,
	SHADER_ID_VRAY_MTL
};

static const TMap<FString, int32>& GetShaderTypeMap()
{
	static TMap<FString, int32> ShaderTypeIDMap;
	if (ShaderTypeIDMap.Num() == 0)
	{
		ShaderTypeIDMap.Add(TEXT("max_bitmap"), SHADER_ID_BITMAP);
		ShaderTypeIDMap.Add(TEXT("max_blend"), SHADER_ID_BLEND);
		ShaderTypeIDMap.Add(TEXT("max_falloff"), SHADER_ID_FALLOF);
		ShaderTypeIDMap.Add(TEXT("max_stdout"), SHADER_ID_STDOUT);
		ShaderTypeIDMap.Add(TEXT("max_stduv"), SHADER_ID_STDUV);
		ShaderTypeIDMap.Add(TEXT("max_stdxyz"), SHADER_ID_STDXYZ);
		ShaderTypeIDMap.Add(TEXT("max_vray_mtl"), SHADER_ID_VRAY_MTL);
	}

	return ShaderTypeIDMap;
}

extern int32 GetShaderTypeID(const FString& shaderType)
{
	const int32* shaderIndex = GetShaderTypeMap().Find(shaderType);
	if (NULL != shaderIndex)
	{
		return *shaderIndex;
	}

	return INDEX_NONE;
}

FEssImporter::FEssImporter() : m_pThread(NULL), mParseResult(false)
{ }

FEssImporter::~FEssImporter()
{
	if (m_pThread)
	{
		ei_end_context();
		delete m_pThread;
		m_pThread = nullptr;
	}
}

bool FEssImporter::Initialize(const FString& FullPath, const FTimerDelegate& timerDelegate)
{
	if (!FPaths::FileExists(FullPath))
	{
		return false;
	}

	ei_context();
	m_strFullPath = FullPath;
	m_pThread = FRunnableThread::Create(this, TEXT("FEssImporter"), 0, EThreadPriority::TPri_BelowNormal);
	GEngine->GameViewport->GetWorld()->GetTimerManager().SetTimer(mTimerHandle, timerDelegate, 1.0f, true);
	return true;
}

struct FVertexKey
{
	int32 indices[5];
	int num;
	uint32 hashKey;
	FVertexKey()
	{
		ZeroMemory((void*)indices, sizeof(int32) * 6);
		hashKey = 0xffffffff;
	}

	void BuildHashKey()
	{
		hashKey = FCrc::MemCrc32((void*)indices, num * sizeof(int32));
	}
};

inline bool operator==(const FVertexKey& KeyA, const FVertexKey& KeyB)
{
	return memcmp((void*)KeyA.indices, (void*)KeyB.indices, KeyA.num * sizeof(int32)) == 0;
}

inline uint32 GetTypeHash(const FVertexKey& Key)
{
	return Key.hashKey;
}

bool FEssImporter::CheckParseFinished()
{
	if (mParseFinished.AtomicSet(false))
	{
		GEngine->GameViewport->GetWorld()->GetTimerManager().ClearTimer(mTimerHandle);
		return true;
	}

	return false;
}

eiTag getArrayTag(const eiDataAccessor<eiNode>& node, const char* arrayName)
{
	eiIndex index = ei_node_find_param(node.get(), arrayName);
	if (index != EI_NULL_INDEX)
	{
		return ei_node_get_array(node.get(), index);
	}

	return EI_NULL_TAG;
}

bool FEssImporter::ParseMesh(const eiNodeAccessor& node, FMeshMapInfo& meshMapInfo)
{
	TMeshArray& meshArray = meshMapInfo.meshArray;
	eiTag positionsTag = getArrayTag(node, "pos_list");
	if (EI_NULL_TAG == positionsTag)
	{
		return false;
	}
	eiTag triListTag = getArrayTag(node, "triangle_list");
	if (EI_NULL_TAG == triListTag)
	{
		return false;
	}
	eiTag normalTag = getArrayTag(node, "N");
	if (EI_NULL_TAG == normalTag)
	{
		return  false;
	}
	eiTag normalIndicesTag = getArrayTag(node, "N_idx");
	if (EI_NULL_TAG == normalIndicesTag)
	{
		return false;
	}

	eiTag uv1Tag = getArrayTag(node, "uv1");
	eiTag uv1IndicesTag = getArrayTag(node, "uv1_idx"); 
	eiTag uv2Tag = getArrayTag(node, "uv2"); 
	eiTag uv2IndicesTag = getArrayTag(node, "uv2_idx"); 
	eiTag dPduTag = getArrayTag(node, "dPdu"); 
	eiTag dPduIndicesTag = getArrayTag(node, "dPdu_idx"); 

	eiDataTableAccessor<eiIndex> tri_list(triListTag);
	eiDataTableAccessor<eiVector> positions(positionsTag);
	eiDataTableAccessor<eiVector> normals(normalTag);
	eiDataTableAccessor<eiIndex> normalIndices(normalIndicesTag);
	eiDataTableAccessor<eiVector> uv1s(uv1Tag);
	eiDataTableAccessor<eiIndex> uv1Indices(uv1IndicesTag);
	eiDataTableAccessor<eiVector> uv2s(uv2Tag);
	eiDataTableAccessor<eiIndex> uv2Indices(uv2IndicesTag);
	eiDataTableAccessor<eiVector> dPdus(dPduTag);
	eiDataTableAccessor<eiIndex> dPduIndices(dPduIndicesTag);

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> Uv1s;
	TArray<FVector2D> Uv2s;
	TArray<FRuntimeMeshTangent> Tangents;
	int channelNum = 2 + (EI_NULL_TAG != uv1Tag) + (EI_NULL_TAG != uv2Tag) + (EI_NULL_TAG != dPduTag);

	for (int i = 0; i < positions.size(); ++i)
	{
		eiVector& position = positions.get(i);
		Vertices.Add(FVector(position.x, position.y, position.z));
	}
	for (int i = 0; i < normals.size(); ++i)
	{
		eiVector& normal = normals.get(i);
		Normals.Add(FVector(normal.x, normal.y, normal.z));
	}
	if (EI_NULL_TAG != uv1Tag)
	{
		for (int i = 0; i < uv1s.size(); ++i)
		{
			eiVector& uv1 = uv1s.get(i);
			Uv1s.Add(FVector2D(uv1.x, uv1.y));
		}
	}
	if (EI_NULL_TAG != uv2Tag)
	{
		for (int i = 0; i < uv2s.size(); ++i)
		{
			eiVector& uv2 = uv2s.get(i);
			Uv2s.Add(FVector2D(uv2.x, uv2.y));
		}
	}
	if (EI_NULL_TAG != dPduTag)
	{
		for (int i = 0; i < dPdus.size(); ++i)
		{
			eiVector& dPdu = dPdus.get(i);
			Tangents.Add(FVector(dPdu.x, dPdu.y, dPdu.z));
		}
	}

	auto BuildMesh = [&](FMeshInfo& meshInfo, bool bHasOriginalVertexOrder, bool bHasInvertVertexOrder)
	{
		TMap<FVertexKey, int32> VertexMap;
		TArray<int32> indices;
		for (int i = 0; i < meshInfo.Triangles.Num(); ++i)
		{
			FVertexKey vertexKey;
			vertexKey.num = channelNum;
			int32 index = meshInfo.Triangles[i];
			int j = 2;
			vertexKey.indices[0] = tri_list.get(index);
			vertexKey.indices[1] = normalIndices.get(index);
			if (EI_NULL_TAG != uv1Tag)
			{
				vertexKey.indices[j++] = uv1Indices.get(index);
			}
			if (EI_NULL_TAG != uv2Tag)
			{
				vertexKey.indices[j++] = uv2Indices.get(index);
			}
			if (EI_NULL_TAG != dPduTag)
			{
				vertexKey.indices[j] = dPduIndices.get(index);
			}
			vertexKey.BuildHashKey();
			int32* pMappedIndex = VertexMap.Find(vertexKey);
			int mappedIndex = 0;
			if (NULL == pMappedIndex)
			{
				mappedIndex = VertexMap.Num();
				VertexMap.Add(vertexKey, mappedIndex);
				meshInfo.Vertices.Add(Vertices[vertexKey.indices[0]]);
				meshInfo.Normals.Add(Normals[vertexKey.indices[1]]);
				j = 2;
				if (EI_NULL_TAG != uv1Tag)
				{
					meshInfo.Uv1s.Add(Uv1s[vertexKey.indices[j++]]);
				}
				if (EI_NULL_TAG != uv2Tag)
				{
					meshInfo.Uv2s.Add(Uv2s[vertexKey.indices[j++]]);
				}
				if (EI_NULL_TAG != dPduTag)
				{
					meshInfo.Tangents.Add(Tangents[vertexKey.indices[j++]]);
				}
			}
			else
			{
				mappedIndex = *pMappedIndex;
			}
			indices.Add(mappedIndex);
		}
		if (bHasOriginalVertexOrder)
		{
			meshInfo.Triangles = indices;
			if (bHasInvertVertexOrder)
			{
				int numFace = meshInfo.Triangles.Num() / 3;
				meshInfo.InvertTriangles.SetNum(numFace * 3);
				for (int i = 0; i < numFace; ++i)
				{
					meshInfo.InvertTriangles[i * 3] = indices[i * 3];
					meshInfo.InvertTriangles[i * 3 + 1] = indices[i * 3 + 2];
					meshInfo.InvertTriangles[i * 3 + 2] = indices[i * 3 + 1];
				}
			}
		}
		else if (bHasInvertVertexOrder)
		{
			meshInfo.Triangles.SetNum(0);
			meshInfo.InvertTriangles = indices;
		}
	};

	int numFace = tri_list.size() / 3;
	int secondVertIndex = meshMapInfo.bHasOriginalVertexOrder ? 2 : 1;
	int thirdVertIndex = meshMapInfo.bHasOriginalVertexOrder ? 1 : 2;
	eiTag mtlIndexTag = getArrayTag(node, "mtl_index");
	if (EI_NULL_TAG != mtlIndexTag)
	{
		eiDataTableAccessor<eiIndex> mtlIndexList(mtlIndexTag);
		TMap<eiIndex, int> mtlIndexToMeshIndex;
		for (int i = 0; i < numFace; ++i)
		{
			eiIndex mtlIndex = mtlIndexList.get(i);
			int meshIndex = 0;
			int* pMeshIndex = mtlIndexToMeshIndex.Find(mtlIndex);
			if (NULL == pMeshIndex)
			{
				meshIndex = meshArray.Num();
				meshArray.AddDefaulted();
				meshArray.Last().mtlIndex = mtlIndex;
				mtlIndexToMeshIndex.Add(mtlIndex, meshIndex);
			}
			else
			{
				meshIndex = *pMeshIndex;
			}
			
			TArray<int32>& triangles = meshArray[meshIndex].Triangles;
			triangles.Add(i * 3);
			triangles.Add(i * 3 + secondVertIndex);
			triangles.Add(i * 3 + thirdVertIndex);
		}

		for (auto& mesh : meshArray)
		{
			BuildMesh(mesh, meshMapInfo.bHasOriginalVertexOrder, meshMapInfo.bHasInvertVertexOrder);
		}
	}
	else
	{
		meshArray.AddDefaulted();
		meshArray.Last().mtlIndex = 0;
		TArray<int32>& triangles = meshArray.Last().Triangles;
		for (int i = 0; i < numFace; ++i)
		{
			triangles.Add(i * 3);
			triangles.Add(i * 3 + secondVertIndex);
			triangles.Add(i * 3 + thirdVertIndex);
		}
		BuildMesh(meshArray.Last(), meshMapInfo.bHasOriginalVertexOrder, meshMapInfo.bHasInvertVertexOrder);
	}
	
	return true;
}

void FEssImporter::InsertNodeInfo(const eiNodeAccessor& node)
{
	eiTag elementTag = ei_node_get_node(node.get(), ei_node_find_param(node.get(), "element"));
	if (EI_NULL_TAG == elementTag)
	{
		return;
	}

	eiNodeAccessor element(elementTag);
	eiDataAccessor<eiNodeDesc> desc(element->desc);
	if (strcmp(ei_node_desc_name(desc.get()), "poly") != 0)
	{
		return;
	}

	mNodeArray.AddDefaulted();
	FMaxNodeInfo& nodeInfo = mNodeArray.Last();
	nodeInfo.name = UTF8_TO_TCHAR(node->unique_name);
	nodeInfo.bInvertVertexOrder = false;
		
	nodeInfo.meshName = UTF8_TO_TCHAR(element->unique_name);
	FMeshMapInfo* pMeshMapInfo = mMeshMap.Find(nodeInfo.meshName);
	if (NULL == pMeshMapInfo)
	{
		pMeshMapInfo = &mMeshMap.Add(nodeInfo.meshName);
	}
	
	eiMatrix* matrix = ei_node_get_matrix(node.get(), ei_node_find_param(node.get(), "transform"));
	if (NULL != matrix)
	{
		memcpy((void*)(&nodeInfo.matrix.M[0][0]), (void*)(&matrix->m[0][0]), sizeof(float) * 16);
#if INVERT_VERTEX_ORDER_FOR_NEGATIVE_SACLE
		if (nodeInfo.matrix.Determinant() < 0)
		{
			pMeshMapInfo->bHasInvertVertexOrder = true;
			nodeInfo.bInvertVertexOrder = true;
		}
		else
		{
			pMeshMapInfo->bHasOriginalVertexOrder = true;
		}
#else
		pMeshMapInfo->bHasOriginalVertexOrder = true;
#endif

		nodeInfo.matrix.M[0][1] = -nodeInfo.matrix.M[0][1];
		nodeInfo.matrix.M[1][1] = -nodeInfo.matrix.M[1][1];
		nodeInfo.matrix.M[2][1] = -nodeInfo.matrix.M[2][1];
		nodeInfo.matrix.M[3][1] = -nodeInfo.matrix.M[3][1];
	}
}

bool FEssImporter::DoParseEssFile()
{
	char* filename = TCHAR_TO_UTF8(*m_strFullPath);
	FString pluginPath = FPaths::GamePluginsDir() + TEXT("RuntimeMeshComponent/ThirdParty/ERSDK/shaders");
	ei_add_shader_searchpath(TCHAR_TO_UTF8(*pluginPath));
	if (!ei_parse2(filename)) {
		ei_error("Failed to parse file: %s\n", filename);
		return false;
	}

	eiTag tagGroup = ei_find_node("mtoer_instgroup_00");
	if (EI_NULL_TAG == tagGroup)
	{
		return false;
	}

	eiNodeAccessor instGroup(tagGroup);
	eiTag tagNodeTable = getArrayTag(instGroup, "instance_list");
	if (EI_NULL_TAG == tagNodeTable)
	{
		return false;
	}

	eiDataTableAccessor<eiTag> nodeTable(tagNodeTable);
	for (eiInt i = 0; i < nodeTable.size(); ++i)
	{
		eiTag nodeTag = nodeTable.get(i);
		if (EI_NULL_TAG != nodeTag)
		{
			eiDataAccessor<eiNode> node(nodeTag);
			InsertNodeInfo(node);
		}
	}

#if MULTI_THREADING_BUILD
	struct Pair
	{
		FString meshkey;
		FMeshMapInfo* meshMapInfo;
	};
	TArray<Pair> pairArray;
	for (auto& iter : mMeshMap)
	{
		Pair pair;
		pair.meshkey = iter.Key;
		pair.meshMapInfo = &iter.Value;
		pairArray.Add(pair);
	}

	DWORD threadID = GetCurrentThreadId();

	ParallelFor(pairArray.Num(), [&](int32 index)
	{
		DWORD subThreadID = GetCurrentThreadId();
		if (subThreadID != threadID)
		{
			ei_job_register_thread();
		}
		Pair& pair = pairArray[index];
		eiTag meshTag = ei_find_node(TCHAR_TO_UTF8(*pair.meshkey));
		if (EI_NULL_TAG != meshTag)
		{
			eiNodeAccessor mesh(meshTag);
			ParseMesh(mesh, *pair.meshMapInfo);
		}
		if (subThreadID != threadID)
		{
			ei_job_unregister_thread();
		}
	});
#else
	for (auto& iter : mMeshMap)
	{
		char* meshKey = TCHAR_TO_UTF8(*iter.Key);
		eiTag meshTag = ei_find_node(meshKey);
		if (EI_NULL_TAG != meshTag)
		{
			eiNodeAccessor mesh(meshTag);
			ParseMesh(mesh, iter.Value);
		}
	}
#endif

	return true;
}

int32 GetShaderID(const eiDataAccessor<eiNode>& node)
{
	eiDataAccessor<eiNodeDesc> desc(node->desc);
	FString shaderName = UTF8_TO_TCHAR(ei_node_desc_name(desc.get()));
	return GetShaderTypeID(shaderName);
}

bool IsVectorType(eiShort type)
{
	switch (type)
	{
		case EI_TYPE_COLOR:
		case EI_TYPE_VECTOR:
		case EI_TYPE_POINT:
		case EI_TYPE_VECTOR2:
		case EI_TYPE_VECTOR4:
		case EI_TYPE_CVECTOR:
		case EI_TYPE_HVECTOR:
		case EI_TYPE_HVECTOR2:
		case EI_TYPE_CLOSURE_COLOR:
			return true;
			break;
	}

	return false;
}

bool IsScalarType(eiShort type)
{
	
	switch (type)
	{
	case EI_TYPE_SCALAR:
	case EI_TYPE_BYTE:
	case EI_TYPE_INT:
	case EI_TYPE_BOOL:
	case EI_TYPE_INDEX:
		return true;
			break;
	}

	return false;
}

struct FParseMaterialContext
{
	FParseMaterialContext(UMaterialInstanceDynamic* pDynamicMaterialInstance) :
		pMaterial(pDynamicMaterialInstance)
	{
		int32 numshaders = GetShaderTypeMap().Num();
		shaderNodesCountPerType.AddZeroed(numshaders);
	}

	UMaterialInstanceDynamic* pMaterial;
	TMap<FString, int32> existingShaderNodesMap;
	TArray<int32> shaderNodesCountPerType;
	TArray<int32> shaderNodeIDs;
};

UTexture2D* CreateTexture2D(const FString& filename)
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TArray<uint8> RawFileData;
	if (FFileHelper::LoadFileToArray(RawFileData, *filename))
	{
		FString extension = FPaths::GetExtension(filename).Trim().ToLower();
		EImageFormat::Type format = EImageFormat::Invalid;
		if (extension == TEXT("png"))
		{
			format = EImageFormat::PNG;
		}
		else if (extension == TEXT("jpg") || extension == TEXT("jpeg"))
		{
			format = EImageFormat::JPEG;
		}
		else if (extension == TEXT("bmp"))
		{
			format = EImageFormat::BMP;
		}

		if (format != EImageFormat::Invalid)
		{
			while (true)
			{
				IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(format);
				if (!ImageWrapper.IsValid())
				{
					break;
				}

				if (ImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num()))
				{
					const TArray<uint8>* UncompressedBGRA = NULL;
					bool isGrayScale = EImageFormat::GrayscaleJPEG == format;
					if (ImageWrapper->GetRaw(isGrayScale ? ERGBFormat::Gray : ERGBFormat::BGRA, 8, UncompressedBGRA))
					{
						UTexture2D* textureParam = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), isGrayScale ? PF_G8 : PF_B8G8R8A8);
						// textureParam->MipGenSettings = TMGS_NoMipmaps;
						//textureParam->AddressX = TA_Clamp;
						//textureParam->AddressY = TA_Clamp;

						void* TextureData = textureParam->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
						if (TextureData)
						{
							FMemory::Memcpy(TextureData, UncompressedBGRA->GetData(), UncompressedBGRA->Num());
							textureParam->PlatformData->Mips[0].BulkData.Unlock();

							// Update the rendering resource from data.
							(*textureParam).UpdateResource();
						}
						else
						{
							textureParam->PlatformData->Mips[0].BulkData.Unlock();
							textureParam->ConditionalBeginDestroy();
							textureParam = NULL;
						}

						return textureParam;
					}
				}
				if (EImageFormat::JPEG == format)
				{
					// so failed for jpeg format, try use gray scale jpeg format
					format = EImageFormat::GrayscaleJPEG;
					continue;
				}
				break;
			}
		}
	}

	return NULL;
}

bool FEssImporter::ParseMaterial(const eiNodeAccessor& shaderNode, FParseMaterialContext& context)
{
	eiInt paramCount = ei_node_param_count(shaderNode.get());
	int32 shaderID = GetShaderID(shaderNode);
	if (INDEX_NONE == shaderID)
	{
		return false;
	}

	for (eiInt i = 0; i < paramCount; ++i)
	{
		eiNodeParam* pNodeParam = ei_node_read_param(shaderNode.get(), i);
		if (NULL == pNodeParam || EI_NULL_TAG == pNodeParam->inst)
		{
			continue;
		}

		eiNodeAccessor inputNode(pNodeParam->inst);
		FString inputName = FString::Printf(TEXT("e%d%s0"), shaderID, UTF8_TO_TCHAR(pNodeParam->unique_name));
		int32* pInputParamIndex = mVectorParamMap.Find(inputName);
		if (pInputParamIndex)
		{
			int* pOutputShaderNodexIndex = context.existingShaderNodesMap.Find(UTF8_TO_TCHAR(inputNode->unique_name));
			if (NULL == pOutputShaderNodexIndex)
			{
				ParseMaterial(inputNode, context);
			}
		}
	}

	int32& countOfNodesOfShaderID = context.shaderNodesCountPerType[shaderID];
	for (eiInt i = 0; i < paramCount; ++i)
	{
		eiNodeParam* pNodeParam = ei_node_read_param(shaderNode.get(), i);
		if (NULL == pNodeParam)
		{
			continue;
		}

		FString paramName = FString::Printf(TEXT("e%d%s%d"), shaderID, UTF8_TO_TCHAR(pNodeParam->unique_name), countOfNodesOfShaderID);
		if (EI_NULL_TAG != pNodeParam->inst)
		{
			int32* pInputParamIndex = mVectorParamMap.Find(paramName);
			if (pInputParamIndex)
			{
				eiNodeAccessor inputNode(pNodeParam->inst);
				int* pOutputShaderNodexIndex = context.existingShaderNodesMap.Find(UTF8_TO_TCHAR(inputNode->unique_name));
				if (NULL == pOutputShaderNodexIndex)
				{
					continue;
				}

				eiNodeParam* pOutputParam = ei_node_read_param(inputNode.get(), pNodeParam->param);
				if (pOutputParam)
				{
					static TMap<FString, int32> sOutputIndexMap;
					if (sOutputIndexMap.Num() == 0)
					{
						sOutputIndexMap.Add(TEXT("result"), 0);
						sOutputIndexMap.Add(TEXT("result_bump"), 1);
						sOutputIndexMap.Add(TEXT("result_mono"), 2);
					}

					int* pOutputIndexInShader = sOutputIndexMap.Find(UTF8_TO_TCHAR(pOutputParam->unique_name));
					if (pOutputIndexInShader)
					{
						FLinearColor vector = context.pMaterial->VectorParameterValues[*pInputParamIndex].ParameterValue;
						vector.A = (*pOutputShaderNodexIndex) * 10 + *pOutputIndexInShader;
						context.pMaterial->SetVectorParameterByIndex(*pInputParamIndex, vector);
					}
				}
			}
		}
		else if (IsVectorType(pNodeParam->type))
		{
			int32* pInputParamIndex = mVectorParamMap.Find(paramName);
			if (pInputParamIndex)
			{
				FLinearColor vector = context.pMaterial->VectorParameterValues[*pInputParamIndex].ParameterValue;
				vector.R = pNodeParam->value.as_vector.x;
				vector.G = pNodeParam->value.as_vector.y;
				if (pNodeParam->type != EI_TYPE_VECTOR2 && pNodeParam->type != EI_TYPE_HVECTOR2)
				{
					vector.B = pNodeParam->value.as_vector.z;
				}
				context.pMaterial->SetVectorParameterByIndex(*pInputParamIndex, vector);
			}
		}
		else if (IsScalarType(pNodeParam->type))
		{
			int32* pInputParamIndex = mScalarParamMap.Find(paramName);
			if (pInputParamIndex)
			{
				float scalar = 0;
				switch (pNodeParam->type)
				{
				case EI_TYPE_INDEX:
					scalar = (float)pNodeParam->value.as_index;
					break;
				case EI_TYPE_INT:
					scalar = (float)pNodeParam->value.as_int;
					break;
				case EI_TYPE_BOOL:
					scalar = (float)pNodeParam->value.as_bool;
					break;
				case EI_TYPE_SCALAR:
					scalar = pNodeParam->value.as_scalar;
					break;
				}
				context.pMaterial->SetScalarParameterByIndex(*pInputParamIndex, scalar);
			}
		}
		else if (EI_TYPE_TOKEN == pNodeParam->type)
		{
			eiToken& token = pNodeParam->value.as_token;
			if (SHADER_ID_STDUV == shaderID && strcmp(pNodeParam->unique_name, "mapChannel") == 0)
			{
				int uv = token.str[2] - '1';
				uv = uv >= 2 ? 0 : uv;
				int32* pInputParamIndex = mScalarParamMap.Find(paramName);
				if (pInputParamIndex)
				{
					context.pMaterial->SetScalarParameterByIndex(*pInputParamIndex, uv);
				}
			}
			else if (SHADER_ID_BITMAP == shaderID && strcmp(pNodeParam->unique_name, "tex_fileName") == 0)
			{
				FString filename = UTF8_TO_TCHAR(token.str);
				UTexture2D* textureParam = CreateTexture2D(filename);
				if (NULL != textureParam)
				{
					context.pMaterial->SetTextureParameterValue(*paramName, textureParam);
				}
			}
		}
	}

	context.existingShaderNodesMap.Add(UTF8_TO_TCHAR(shaderNode->unique_name), context.shaderNodeIDs.Num());
	int nodeID = shaderID * 10 + countOfNodesOfShaderID;
	context.shaderNodeIDs.Add(nodeID);
	countOfNodesOfShaderID++;
	return true;
}

template <typename ObjClass>
static FORCEINLINE ObjClass* LoadObjFromPath(const FName& Path)
{
	if (Path == NAME_None) return NULL;
	return Cast<ObjClass>(StaticLoadObject(ObjClass::StaticClass(), NULL, *Path.ToString()));
}

static FORCEINLINE UMaterial* LoadMatFromPath(const FName& Path)
{
	if (Path == NAME_None) return NULL;
	return LoadObjFromPath<UMaterial>(Path);
}

UMaterialInterface* FEssImporter::GetNodeMaterial(int nodeIndex, int subMeshIndex, int mtlIndex, UPrimitiveComponent* pMeshComponent)
{
	char* nodeName = TCHAR_TO_UTF8(*mNodeArray[nodeIndex].name);
	eiTag nodeTag = ei_find_node(nodeName);
	if (EI_NULL_TAG == nodeTag)
	{
		return NULL;
	}

	eiNodeAccessor node(nodeTag);
	eiTag mtlListTag = getArrayTag(node, "mtl_list");
	if (EI_NULL_TAG == mtlListTag)
	{
		return false;
	}

	eiDataTableAccessor<eiTag> mtlList(mtlListTag);
	if (mtlIndex >= mtlList.size())
	{
		return NULL;
	}

	eiTag mtlTag = mtlList.get(mtlIndex);
	if (EI_NULL_TAG == mtlTag)
	{
		return NULL;
	}

	eiNodeAccessor mtl(mtlTag);
	FString materialName = UTF8_TO_TCHAR(mtl->unique_name);
	UMaterialInstanceDynamic** ppMaterial = mMaterailMap.Find(materialName);
	if (NULL != ppMaterial && NULL != *ppMaterial)
	{
		return *ppMaterial;
	}

	eiTag surfaceShaderTag = ei_node_get_node(mtl.get(), ei_node_find_param(mtl.get(), "surface_shader"));
	if (EI_NULL_TAG == surfaceShaderTag)
	{
		return NULL;
	}

	eiNodeAccessor surfaceShader(surfaceShaderTag);
	eiTag shaderNodesListTag = getArrayTag(surfaceShader, "nodes");
	if (EI_NULL_TAG == shaderNodesListTag)
	{
		return NULL;
	}

	eiDataTableAccessor<eiTag> shaderNodes(shaderNodesListTag);
	eiTag shaderNodeTag = shaderNodes.get(0);
	if (EI_NULL_TAG == shaderNodeTag)
	{
		return NULL;
	}

	eiNodeAccessor shaderNode(shaderNodeTag);
	eiInt inputIndex = ei_node_find_param(shaderNode.get(), "input");
	if (EI_NULL_INDEX == inputIndex)
	{
		return NULL;
	}

	eiNodeParam* pNodeParam = ei_node_read_param(shaderNode.get(), inputIndex);
	if (NULL == pNodeParam || EI_NULL_TAG == pNodeParam->inst)
	{
		return NULL;
	}

	eiNodeAccessor shaderRoot(pNodeParam->inst);
	if (GetShaderID(shaderRoot) == INDEX_NONE)
	{
		return NULL;
	}

	UMaterial* pEssMaterial = LoadMatFromPath(FName(TEXT("Material'/RuntimeMeshComponent/EssMaterial.EssMaterial'")));
	if (NULL == pEssMaterial)
	{
		return NULL;
	}

	UMaterialInstanceDynamic* pDynamicMaterialInstance = pMeshComponent->CreateDynamicMaterialInstance(subMeshIndex, pEssMaterial);
	if (NULL == pDynamicMaterialInstance)
	{
		return NULL;
	}

	pDynamicMaterialInstance->CopyScalarAndVectorParameters(*pEssMaterial, ERHIFeatureLevel::SM5);
	mMaterailMap.Add(materialName, pDynamicMaterialInstance);
	if (mVectorParamMap.Num() == 0)
	{
		for (int i = 0; i < pDynamicMaterialInstance->VectorParameterValues.Num(); ++i)
		{
			mVectorParamMap.Add(pDynamicMaterialInstance->VectorParameterValues[i].ParameterName.ToString(), i);
		}
	}
	
	if (mScalarParamMap.Num() == 0)
	{
		for (int i = 0; i < pDynamicMaterialInstance->ScalarParameterValues.Num(); ++i)
		{
			mScalarParamMap.Add(pDynamicMaterialInstance->ScalarParameterValues[i].ParameterName.ToString(), i);
		}
	}
	
	FParseMaterialContext context(pDynamicMaterialInstance);
	ParseMaterial(shaderRoot, context);
	for (int i = 0; i < context.shaderNodeIDs.Num(); ++i)
	{
		FString paramName = FString::Printf(TEXT("ess_nodeID%d"), i);
		int32* pNodeIdIndex = mScalarParamMap.Find(paramName);
		if (pNodeIdIndex)
		{
			context.pMaterial->SetScalarParameterByIndex(*pNodeIdIndex, context.shaderNodeIDs[i]);
		}
	}
	int32* pScalarIndex = mScalarParamMap.Find(TEXT("ess_shaderCount"));
	if (pScalarIndex)
	{
		context.pMaterial->SetScalarParameterByIndex(*pScalarIndex, context.shaderNodeIDs.Num());
	}
	return pDynamicMaterialInstance;
}

uint32 FEssImporter::Run()
{
	ei_job_register_thread();
	ei_sub_context();
	mParseResult = DoParseEssFile();
	ei_end_sub_context();
	ei_job_unregister_thread();
	mParseFinished.AtomicSet(true);
	return 0;
}

int FEssImporter::GetNodeCount() const
{
	return mNodeArray.Num();
}

const FMaxNodeInfo* FEssImporter::GetNodeInfo(int index) const
{
	if (index < GetNodeCount())
	{
		return &mNodeArray[index];
	}

	return NULL;
}

const FEssImporter::TMeshArray* FEssImporter::GetMeshInfo(const FString& meshName) const
{
	const FMeshMapInfo* pMeshMapInfo = mMeshMap.Find(meshName);
	if (NULL != pMeshMapInfo)
	{
		return &pMeshMapInfo->meshArray;
	}

	return NULL;
}
