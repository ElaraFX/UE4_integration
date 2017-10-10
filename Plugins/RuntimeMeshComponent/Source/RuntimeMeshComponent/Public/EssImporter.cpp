#include "RuntimeMeshComponentPluginPrivatePCH.h"
#include "EssImporter.h"
#include "Classes/Engine/World.h"
#include "Public/Async/ParallelFor.h"

#define MULTI_THREADING_BUILD 1
#define INVERT_VERTEX_ORDER_FOR_NEGATIVE_SACLE 0

FEssImporter::FEssImporter(const FString& FullPath, const FTimerDelegate& timerDelegate) :
	m_strFullPath(FullPath)
{
	m_pThread = FRunnableThread::Create(this, TEXT("FEssImporter"), 0, EThreadPriority::TPri_BelowNormal);
	GEngine->GameViewport->GetWorld()->GetTimerManager().SetTimer(mTimerHandle, timerDelegate, 1.0f, true);
}

FEssImporter::~FEssImporter()
{
	if (m_pThread)
	{
		delete m_pThread;
		m_pThread = nullptr;
	}
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
			Uv1s.Add(FVector2D(uv1.x, 1 - uv1.y));
		}
	}
	if (EI_NULL_TAG != uv2Tag)
	{
		for (int i = 0; i < uv2s.size(); ++i)
		{
			eiVector& uv2 = uv2s.get(i);
			Uv2s.Add(FVector2D(uv2.x, 1 - uv2.y));
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
		meshArray.Last().mtlIndex = -1;
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
	nodeInfo.name = ANSI_TO_TCHAR(node->unique_name);
	nodeInfo.bInvertVertexOrder = false;
		
	nodeInfo.meshName = ANSI_TO_TCHAR(element->unique_name);
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
	char* filename = TCHAR_TO_ANSI(*m_strFullPath);
	ei_context();
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
	eiTag tagNodeTable = ei_node_get_array(instGroup.get(), ei_node_find_param(instGroup.get(), "instance_list"));
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
		eiTag meshTag = ei_find_node(TCHAR_TO_ANSI(*pair.meshkey));
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
		char* meshKey = TCHAR_TO_ANSI(*iter.Key);
		eiTag meshTag = ei_find_node(meshKey);
		if (EI_NULL_TAG != meshTag)
		{
			eiNodeAccessor mesh(meshTag);
			ParseMesh(mesh, iter.Value);
		}
	}
#endif
	ei_end_context();

	return true;
}

uint32 FEssImporter::Run()
{
	DoParseEssFile();
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
