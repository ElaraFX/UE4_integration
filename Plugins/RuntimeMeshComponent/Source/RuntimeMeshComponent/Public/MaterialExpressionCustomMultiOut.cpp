// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialExpressions.cpp - Material expressions implementation.
=============================================================================*/

#include "RuntimeMeshComponentPluginPrivatePCH.h"
#include "MaterialExpressionCustomMultiOut.h"
#include "MaterialCompiler.h"
#include "Private/Materials/MaterialUniformExpressions.h"
#include "Private/Materials/HLSLMaterialTranslator.h"
#if WITH_EDITOR
#include "Editor/MaterialEditor/Public/MaterialEditorUtilities.h"
#include "Editor/MaterialEditor/Public/IMaterialEditor.h"
#endif
#include "Classes/Materials/MaterialExpressionScalarParameter.h"
#include "Classes/Materials/MaterialExpressionVectorParameter.h"
#include "Classes/Materials/MaterialExpressionTextureObjectParameter.h"
#include "FileHelper.h"

#define LOCTEXT_NAMESPACE "MaterialExpression"

#define UI_DELTA_Y 50
#define UI_DELTA_X 125
#define ESS_NODES_PREFIX TEXT("ess_nodeID")

extern int32 GetShaderTypeID(const FString& shaderType);

#if WITH_EDITOR
class FHLSLMaterialTranslatorEx : public FHLSLMaterialTranslator
{
public:
	int32 CustomExpressionEx(UMaterialExpressionCustomMultiOut* Custom, TArray<int32>& CompiledInputs, TArray<int32>& OutputResults, const FString& frameStamp,
		const TArray<FInputShaderInfo>& inputShaders)
	{
		FString externalDepedency;
		FString innerFunctionBody;
		const FString START_FUNCTION_BODY("START FUNCTION BODY");
		for (auto& shaderInfo : inputShaders)
		{
			FString fileString;
			if (!FFileHelper::LoadFileToString(fileString, *shaderInfo.ShaderFilePath.FilePath))
			{
				continue;
			}
			
			int functionBodyStart = fileString.Find(START_FUNCTION_BODY);
			if (INDEX_NONE == functionBodyStart)
			{
				continue;
			}

			FString functionBody = fileString.RightChop(functionBodyStart + START_FUNCTION_BODY.Len());
			fileString.ReplaceInline(*functionBody, TEXT("\n"));
			if (shaderInfo.TextureParameters.Num() > 0)
			{
				TArray<FString> lines;
				int shaderID = GetShaderTypeID(shaderInfo.ShaderType);
				functionBody.ParseIntoArray(lines, TEXT("\r\n"));
				FString replacedCodes;
				for (auto& line : lines)
				{
					if (line.Find("Texture2D ") != INDEX_NONE || line.Find("SamplerState ") != INDEX_NONE)
					{
						continue;
					}

					for (auto& textureParameter : shaderInfo.TextureParameters)
					{
						if (line.Find(textureParameter) != INDEX_NONE)
						{
							FString originalLine = line;
							line.Trim();
							int32 indexTrimmedLine = originalLine.Find(line);
							FString spaces = originalLine.Left(indexTrimmedLine);

							// replacedCodes += spaces + TEXT("switch (shaderIndex){\r\n");
							for (int i = 0; i < shaderInfo.MaxShaderCount; ++i)
							{
								replacedCodes += spaces + FString::Printf(TEXT("if (shaderIndex == %d) "), i);
								FString actualTexParameter = FString::Printf(TEXT("e%d%s%d"), shaderID, *textureParameter, i);
								replacedCodes += line.Replace(*textureParameter, *actualTexParameter);
								replacedCodes += TEXT("\r\n");
								// replacedCodes += TEXT("break;\r\n");
							}
							// replacedCodes += spaces + TEXT("}");
							line = replacedCodes;
							break;
						}
					}
				}
				functionBody = FString::Join(lines, TEXT("\r\n"));
			}
			innerFunctionBody += functionBody + TEXT("\r\n");
			externalDepedency += fileString;
		}
		OutputResults.Reset();
		OutputResults.AddZeroed(Custom->OutputSlots.Num());
		int32 ResultIdx = INDEX_NONE;

		FString OutputTypeString;
		EMaterialValueType OutputType;
		switch (Custom->OutputSlots[0].OutputType)
		{
		case CMOT_Float2:
			OutputType = MCT_Float2;
			OutputTypeString = TEXT("MaterialFloat2");
			break;
		case CMOT_Float3:
			OutputType = MCT_Float3;
			OutputTypeString = TEXT("MaterialFloat3");
			break;
		case CMOT_Float4:
			OutputType = MCT_Float4;
			OutputTypeString = TEXT("MaterialFloat4");
			break;
		default:
			OutputType = MCT_Float;
			OutputTypeString = TEXT("MaterialFloat");
			break;
		}

		// Declare implementation function
		FString InputParamDecl;
		check(Custom->Inputs.Num() == CompiledInputs.Num());
		for (int32 i = 0; i < Custom->Inputs.Num(); i++)
		{
			// skip over unnamed inputs
			if (Custom->Inputs[i].InputName.Len() == 0)
			{
				continue;
			}
			InputParamDecl += TEXT(",");
			switch (GetParameterType(CompiledInputs[i]))
			{
			case MCT_Float:
			case MCT_Float1:
				InputParamDecl += TEXT("MaterialFloat ");
				InputParamDecl += Custom->Inputs[i].InputName;
				break;
			case MCT_Float2:
				InputParamDecl += TEXT("MaterialFloat2 ");
				InputParamDecl += Custom->Inputs[i].InputName;
				break;
			case MCT_Float3:
				InputParamDecl += TEXT("MaterialFloat3 ");
				InputParamDecl += Custom->Inputs[i].InputName;
				break;
			case MCT_Float4:
				InputParamDecl += TEXT("MaterialFloat4 ");
				InputParamDecl += Custom->Inputs[i].InputName;
				break;
			case MCT_Texture2D:
				InputParamDecl += TEXT("Texture2D ");
				InputParamDecl += Custom->Inputs[i].InputName;
				InputParamDecl += TEXT(", SamplerState ");
				InputParamDecl += Custom->Inputs[i].InputName;
				InputParamDecl += TEXT("Sampler ");
				break;
			case MCT_TextureCube:
				InputParamDecl += TEXT("TextureCube ");
				InputParamDecl += Custom->Inputs[i].InputName;
				InputParamDecl += TEXT(", SamplerState ");
				InputParamDecl += Custom->Inputs[i].InputName;
				InputParamDecl += TEXT("Sampler ");
				break;
			default:
				return Errorf(TEXT("Bad type %s for %s input %s"), DescribeType(GetParameterType(CompiledInputs[i])), *Custom->Description, *Custom->Inputs[i].InputName);
				break;
			}
		}
		for (int32 i = 1; i < Custom->OutputSlots.Num(); i++)
		{
			// skip over unnamed inputs
			if (Custom->OutputSlots[i].OutputName.Len() == 0)
			{
				continue;
			}
			switch (Custom->OutputSlots[i].OutputType)
			{
			case CMOT_Float1:
				InputParamDecl += TEXT(" ,out MaterialFloat ");
				break;
			case CMOT_Float2:
				InputParamDecl += TEXT(" ,out MaterialFloat2 ");
				break;
			case CMOT_Float3:
				InputParamDecl += TEXT(" ,out MaterialFloat3 ");
				break;
			case CMOT_Float4:
				InputParamDecl += TEXT(" ,out MaterialFloat4 ");
				break;
			default:
				continue;
				break;
			}
			InputParamDecl += Custom->OutputSlots[i].OutputName;
		}

		int32 CustomExpressionIndex = CustomExpressionImplementations.Num();
		FString Code = Custom->Code;
		if (!Code.Contains(TEXT("return")))
		{
			Code = FString(TEXT("return ")) + Code + TEXT(";");
		}
		Code.ReplaceInline(TEXT("\n"), TEXT("\r\n"), ESearchCase::CaseSensitive);

		FString ParametersType = ShaderFrequency == SF_Vertex ? TEXT("Vertex") : (ShaderFrequency == SF_Domain ? TEXT("Tessellation") : TEXT("Pixel"));

		FString ImplementationCode = FString::Printf(TEXT("%s\r\n /* FunctionFrameStamp %s*/\r\n%s CustomExpression%d(FMaterial%sParameters Parameters%s)\r\n{\r\n uint shaderIndex = 0;\r\nstruct Functions {\r\n%s};\r\n%s\r\n}\r\n"), *externalDepedency, *frameStamp, *OutputTypeString, CustomExpressionIndex, *ParametersType, *InputParamDecl, *innerFunctionBody, *Code);
		CustomExpressionImplementations.Add(ImplementationCode);

		// Add call to implementation function
		FString CodeChunk = FString::Printf(TEXT("CustomExpression%d(Parameters"), CustomExpressionIndex);
		for (int32 i = 0; i < CompiledInputs.Num(); i++)
		{
			// skip over unnamed inputs
			if (Custom->Inputs[i].InputName.Len() == 0)
			{
				continue;
			}

			FString ParamCode = GetParameterCode(CompiledInputs[i]);
			EMaterialValueType ParamType = GetParameterType(CompiledInputs[i]);

			CodeChunk += TEXT(",");
			CodeChunk += *ParamCode;
			if (ParamType == MCT_Texture2D || ParamType == MCT_TextureCube)
			{
				CodeChunk += TEXT(",");
				CodeChunk += *ParamCode;
				CodeChunk += TEXT("Sampler");
			}
		}
		for (int32 i = 1; i < Custom->OutputSlots.Num(); i++)
		{
			// skip over unnamed inputs
			if (Custom->OutputSlots[i].OutputName.Len() == 0)
			{
				continue;
			}

			int32 outputResult = -1;
			if (Custom->OutputSlots[i].OutputType > CMOT_Float1)
			{
				outputResult = VectorParameter(*Custom->OutputSlots[i].OutputName, FLinearColor(0, 0, 0, 0));
			}
			else
			{
				outputResult = ScalarParameter(*Custom->OutputSlots[i].OutputName, 0);
			}
			OutputResults[i] = outputResult;
			
			FString ParamCode = GetParameterCode(outputResult);
			CodeChunk += TEXT(",");
			CodeChunk += *ParamCode;
		}
		CodeChunk += TEXT(")");

		ResultIdx = AddCodeChunk(
			OutputType,
			*CodeChunk
		);
		OutputResults[0] = ResultIdx;
		return ResultIdx;
	}

	bool CheckFrameStamp(const FString& frameStamp)
	{
		for (auto& customCode : CustomExpressionImplementations)
		{
			if (customCode.Find(frameStamp) != INDEX_NONE)
			{
				return true;
			}
		}

		return false;
	}
};

UMaterialExpressionScalarParameter* CreateScalarParameter(IMaterialEditor* pMaterialEditor, const FString& parameterName, 
	float defaultValue, const FVector2D& uiPos, TArray<FCustomInput>& Inputs)
{
	UMaterialExpressionScalarParameter* pScalarExpression = Cast<UMaterialExpressionScalarParameter>(pMaterialEditor->CreateNewMaterialExpression(
		UMaterialExpressionScalarParameter::StaticClass(), uiPos, false, true));
	if (NULL != pScalarExpression)
	{
		Inputs.AddDefaulted();
		Inputs.Last().InputName = parameterName;
		pScalarExpression->DefaultValue = defaultValue;
		pScalarExpression->ParameterName = *parameterName;
	}

	return pScalarExpression;
}

#endif

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionCustomMultiOut
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionCustomMultiOut::UMaterialExpressionCustomMultiOut(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Custom;
		FConstructorStatics()
			: NAME_Custom(LOCTEXT( "CustomMultiOut", "CustomMultiOut" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Description = TEXT("CustomMultiOut");
	Code = TEXT("1");

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Custom);
#endif

	TCHAR* outputNames[] = { TEXT("output_diffuse"), TEXT("output_specular"), TEXT("output_emissive"), TEXT("output_roughness") };
	ECustomMaterialOutputType types[] = { CMOT_Float3, CMOT_Float1, CMOT_Float4, CMOT_Float1 };

	for (int i = 0; i < sizeof(outputNames) / sizeof(TCHAR*); ++i)
	{
		OutputSlots.AddDefaulted();
		OutputSlots.Last().OutputType = types[i];
		OutputSlots.Last().OutputName = outputNames[i];
	}

	Inputs.Add(FCustomInput());
	Inputs[0].InputName = TEXT("ess_shaderCount");

	MaxShaderNodes = 0;

	bCollapsed = false;
}

#if WITH_EDITOR
int32 UMaterialExpressionCustomMultiOut::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	FHLSLMaterialTranslatorEx* pHLSLCompilerEx = static_cast<FHLSLMaterialTranslatorEx*>(Compiler);
	if (!mFrameStamp.Len() == 0 && pHLSLCompilerEx->CheckFrameStamp(mFrameStamp))
	{
		return mOutputResults[OutputIndex];
	}
	
	uint32 tickCount = FPlatformTime::Cycles();
	mFrameStamp = FString::FromInt(tickCount);
	mOutputResults.Reset();
	TArray<int32> CompiledInputs;

	for( int32 i=0;i<Inputs.Num();i++ )
	{
		// skip over unnamed inputs
		if( Inputs[i].InputName.Len()==0 )
		{
			CompiledInputs.Add(INDEX_NONE);
		}
		else
		{
			if(!Inputs[i].Input.GetTracedInput().Expression)
			{
				return Compiler->Errorf(TEXT("Custom material %s missing input %d (%s)"), *Description, i+1, *Inputs[i].InputName);
			}
			int32 InputCode = Inputs[i].Input.Compile(Compiler);
			if( InputCode < 0 )
			{
				return InputCode;
			}
			CompiledInputs.Add( InputCode );
		}
	}

	pHLSLCompilerEx->CustomExpressionEx(this, CompiledInputs, mOutputResults, mFrameStamp, InputShaders);
	return mOutputResults[OutputIndex];
}

void UMaterialExpressionCustomMultiOut::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(Description);
}
#endif // WITH_EDITOR

TArray<FExpressionOutput>& UMaterialExpressionCustomMultiOut::GetOutputs()
{
	Outputs.Reset();
	for (int32 i = 0; i < OutputSlots.Num(); i++)
	{
		Outputs.Add(OutputSlots[i].Output);
	}
	return Outputs;
}

const TArray<FExpressionInput*> UMaterialExpressionCustomMultiOut::GetInputs()
{
	TArray<FExpressionInput*> Result;
	for (int32 i = 0; i < Inputs.Num(); i++)
	{
		Result.Add(&Inputs[i].Input);
	}
	return Result;
}

FExpressionInput* UMaterialExpressionCustomMultiOut::GetInput(int32 InputIndex)
{
	if( InputIndex < Inputs.Num() )
	{
		return &Inputs[InputIndex].Input;
	}
	return NULL;
}

FString UMaterialExpressionCustomMultiOut::GetInputName(int32 InputIndex) const
{
	if( InputIndex < Inputs.Num() )
	{
		return Inputs[InputIndex].InputName;
	}
	return TEXT("");
}

FVector2D UMaterialExpressionCustomMultiOut::PreDeleteInputs(const FString& prefix, int nodeCountsPerRow)
{
#if WITH_EDITOR
	TArray<UEdGraphNode*> nodesToRemove;
	int32 startIndex = INDEX_NONE, endIndex = Inputs.Num() - 1;
	for (int32 i = 0; i < Inputs.Num(); ++i)
	{
		if (Inputs[i].InputName.StartsWith(prefix))
		{
			if (startIndex == INDEX_NONE)
			{
				startIndex = i;
			}
			nodesToRemove.Add(Inputs[i].Input.Expression->GraphNode);
		}
		else if (INDEX_NONE != startIndex)
		{
			endIndex = i - 1;
			break;
		}
	}

	FMaterialEditorUtilities::DeleteNodes(GraphNode->GetGraph(), nodesToRemove);
	if (startIndex != INDEX_NONE)
	{
		for (int32 i = endIndex; i >= startIndex; --i)
		{
			Inputs.RemoveAt(i);
		}
	}

	FVector2D uiPos(0, 0);
	if (Inputs.Num() > 0)
	{
		for (int32 i = Inputs.Num() - 1; i >= 0; --i)
		{
			if (NULL != Inputs[i].Input.Expression)
			{
				UEdGraphNode* pGraphNode = Inputs[i].Input.Expression->GraphNode;
				uiPos.X = GraphNode->NodePosX - UI_DELTA_X * (nodeCountsPerRow + 1);
				uiPos.Y = pGraphNode->NodePosY + UI_DELTA_Y;
				break;
			}
		}
	}

	if (uiPos == FVector2D::ZeroVector)
	{
		uiPos.X = GraphNode->NodePosX - UI_DELTA_X * (nodeCountsPerRow + 1);
		uiPos.Y = GraphNode->NodePosY;
	}

	return uiPos;
#else
	return FVector2D::ZeroVector;
#endif
}

void UMaterialExpressionCustomMultiOut::PostAddInputs(const TArray<FString> parameterNames, TArray<UEdGraphPin*> outGraphPins)
{
#if WITH_EDITOR
	if (GraphNode)
	{
		GraphNode->ReconstructNode();
	}

	for (int32 i = 0; i < outGraphPins.Num(); ++i)
	{
		UEdGraphPin* pinIn = GraphNode->FindPin(parameterNames[i], EGPD_Input);
		UEdGraphPin* pinOut = outGraphPins[i];
		GraphNode->GetGraph()->GetSchema()->TryCreateConnection(pinOut, pinIn);
	}
#endif
}

FString GetInputPrefix(const FInputShaderInfo& shaderInfo)
{
	int32 typeID = GetShaderTypeID(shaderInfo.ShaderType);
	return FString::Printf(TEXT("e%d"), typeID);
}

struct FInParameterDescription
{
	FString Name;
	FString AlphaVariableName;
};

struct FNonArrayParameters
{
	FString inputParameter;
	TArray<FString> parameters;
};

bool LoadFileString(FInputShaderInfo& shaderInfo, FString& fileString)
{
	if (FFileHelper::LoadFileToString(fileString, *shaderInfo.ShaderFilePath.FilePath))
	{
		return true;
	}

	FString fileName = FPaths::GetCleanFilename(shaderInfo.ShaderFilePath.FilePath);
	fileName = FPaths::GetPath(FPaths::GetProjectFilePath()) + "/Plugins/RuntimeMeshComponent/ThirdParty/elara_shaders/" + fileName;
	if (!FFileHelper::LoadFileToString(fileString, *fileName))
	{
		return false;
	}

	shaderInfo.ShaderFilePath.FilePath = fileName;
	return true;
}

bool UMaterialExpressionCustomMultiOut::DoImportShader(int32 currentShaderIndex)
{
#if WITH_EDITOR
	if (INDEX_NONE == currentShaderIndex)
	{
		return false;
	}

	FString fileString;
	FInputShaderInfo& shaderInfo = InputShaders[currentShaderIndex];
	if (!LoadFileString(shaderInfo, fileString))
	{
		return false;
	}

	static FRegexPattern pattern(TEXT("(\\w+)\\s*(\\w+)\\s*\\(\\s*((out|in)\\s+)?(\\w+)\\s*(\\w+)\\s*/\\*\\s*((\\d+(.\\d+)?\\s*)(,\\s*(\\d+(.\\d+)?)\\s*)*)\\s*\\*/\\s*(,\\s*((out|in)\\s+)?(\\w+)\\s*(\\w+)\\s*/\\*\\s*((\\d+(.\\d+)?\\s*)(,\\s*(\\d+(.\\d+)?)\\s*)*)\\s*\\*/\\s*)*\\)"));
	FRegexMatcher matcher(pattern, fileString);
	if (!matcher.FindNext())
	{
		return false;
	}

	FString shaderType = matcher.GetCaptureGroup(2);
	int32 typeID = GetShaderTypeID(shaderType);
	if (INDEX_NONE == typeID)
	{
		return false;
	}

	for (int32 i = 0; i < InputShaders.Num(); ++i)
	{
		if (i != currentShaderIndex && shaderType == InputShaders[i].ShaderType)
		{
			return false;
		}
	}

	shaderInfo.ShaderType = shaderType;
	shaderInfo.TextureParameters.Reset();
	FString oldInputPrefix = GetInputPrefix(shaderInfo);
	FVector2D uiPos = PreDeleteInputs(oldInputPrefix, shaderInfo.MaxShaderCount);

	TSharedPtr<class IMaterialEditor> pMaterialEditor = FMaterialEditorUtilities::GetIMaterialEditorForObject(this);
	FString inputPrefix = FString::Printf(TEXT("e%d"), typeID);
	FString declaration = matcher.GetCaptureGroup(0);
	static FRegexPattern parameterPattern(TEXT("((out|in)\\s+)?(\\w+)\\s*(\\w+)\\s*/\\*\\s*((\\d+(.\\d+)?\\s*)(,\\s*(\\d+(.\\d+)?)\\s*)*)\\s*\\*/\\s*"));
	FRegexMatcher parameterMatcher(parameterPattern, declaration);

	shaderInfo.MaxShaderCount = FMath::Max(1, shaderInfo.MaxShaderCount);
	TArray<UEdGraphPin*> outGraphPins;
	TArray<FString> parameterNames;
	TArray<FString> functioncallParameters;
	TArray<FInParameterDescription> inParameters;
	TArray<FNonArrayParameters> nonArrayParameters;
	FString variableDeclaration;
	FString parameterName;
	int32 outputCount = 0;
	const FString TEX_ALPHA_PREFIX("tex_alpha_");
	while (parameterMatcher.FindNext())
	{
		FString modifier = parameterMatcher.GetCaptureGroup(1);
		FString type = parameterMatcher.GetCaptureGroup(3);
		if (modifier.StartsWith("out"))
		{
			FString postfix;
			if (type == TEXT("float3"))
			{
				postfix = TEXT(".xyz");
			}
			else if (type == TEXT("float2"))
			{
				postfix = TEXT(".xy");
			}
			else if (type == TEXT("float"))
			{
				postfix = TEXT(".x");
			}
			functioncallParameters.Add(FString::Printf(TEXT("outputs[i][%d]%s"), outputCount, *postfix));
			outputCount++;
			continue;
		}
		
		FString originalParameterName = parameterMatcher.GetCaptureGroup(4);
		FString inputParameter = inputPrefix + originalParameterName;
		if (modifier.StartsWith("in"))
		{
			inParameters.AddDefaulted();
			inParameters.Last().Name = inputParameter;
		}
		else if (originalParameterName.StartsWith(TEX_ALPHA_PREFIX))
		{
			for (auto& inParameter : inParameters)
			{
				if (inParameter.Name == (inputPrefix + originalParameterName.RightChop(TEX_ALPHA_PREFIX.Len())))
				{
					inParameter.AlphaVariableName = inputParameter;
				}
			}
		}
		FVector2D localPos = uiPos;
		TArray<FString> parameterArray;
		if (type == "float" || type == "int")
		{
			for (int32 i = 0; i < shaderInfo.MaxShaderCount; ++i)
			{
				localPos.X += UI_DELTA_X;
				parameterName = FString::Printf(TEXT("%s%d"), *inputParameter, i);
				FString floatValue = parameterMatcher.GetCaptureGroup(5).Trim();
				UMaterialExpressionScalarParameter* pScalarExpression = CreateScalarParameter(pMaterialEditor.Get(), 
					parameterName, FCString::Atof(*floatValue), localPos, Inputs);
				if (NULL != pScalarExpression)
				{
					outGraphPins.Add(pScalarExpression->GraphNode->Pins[0]);
					parameterNames.Add(parameterName);
					parameterArray.Add(parameterName);
				}
			}
		}
		else if (type.StartsWith("float"))
		{
			for (int32 i = 0; i < shaderInfo.MaxShaderCount; ++i)
			{
				localPos.X += UI_DELTA_X;
				parameterName = FString::Printf(TEXT("%s%d"), *inputParameter, i);
				UMaterialExpressionVectorParameter* pVectorExpression = Cast<UMaterialExpressionVectorParameter>(pMaterialEditor->CreateNewMaterialExpression(
					UMaterialExpressionVectorParameter::StaticClass(), localPos, false, true));
				if (NULL != pVectorExpression)
				{
					Inputs.AddDefaulted();
					Inputs.Last().InputName = parameterName;
					FString vectorValue = parameterMatcher.GetCaptureGroup(5).Trim();
					FLinearColor color(0, 0, 0, 0);
					if (inParameters.Num() > 0 && inParameters.Last().Name == inputParameter)
					{
						color.A = -1;
						type = TEXT("float4");
					}
					if (type == TEXT("float4"))
					{
						pVectorExpression->Outputs[0].Mask = 0;
					}
					else if (type == TEXT("float3"))
					{
						pVectorExpression->Outputs[0].SetMask(1, 1, 1, 1, 0);
					}
					else if (type == TEXT("float2"))
					{
						pVectorExpression->Outputs[0].SetMask(1, 1, 1, 0, 0);
					}
					TArray<FString> splittedValues;
					vectorValue.ParseIntoArray(splittedValues, TEXT(","));
					for (int32 j = 0; j < FMath::Min(splittedValues.Num(), 4); ++j)
					{
						color.Component(j) = FCString::Atof(*splittedValues[j].Trim());
					}
					pVectorExpression->DefaultValue = color;
					pVectorExpression->ParameterName = *parameterName;
					outGraphPins.Add(pVectorExpression->GraphNode->Pins[0]);
					parameterNames.Add(parameterName);
					parameterArray.Add(parameterName);
				}
			}
		}
		else if (type.StartsWith("Texture2D"))
		{
			for (int32 i = 0; i < shaderInfo.MaxShaderCount; ++i)
			{
				localPos.X += UI_DELTA_X;
				parameterName = FString::Printf(TEXT("%s%d"), *inputParameter, i);
				UMaterialExpressionTextureObjectParameter* pTextureExpression = Cast<UMaterialExpressionTextureObjectParameter>(pMaterialEditor->CreateNewMaterialExpression(
					UMaterialExpressionTextureObjectParameter::StaticClass(), localPos, false, true));
				if (NULL != pTextureExpression)
				{
					Inputs.AddDefaulted();
					Inputs.Last().InputName = parameterName;
					pTextureExpression->ParameterName = *parameterName;
					outGraphPins.Add(pTextureExpression->GraphNode->Pins[0]);
					parameterNames.Add(parameterName);
				}
			}
			shaderInfo.TextureParameters.Add(originalParameterName);
			inputParameter = parameterName;
		}
		else if (type.StartsWith("SamplerState"))
		{
			const FString SAMPLER_POST_FIX("Sampler");
			inputParameter = FString::Printf(TEXT("%s%d%s"), *inputParameter.LeftChop(SAMPLER_POST_FIX.Len()), 0, *SAMPLER_POST_FIX);
		}
		else
		{
			continue;
		}

		if (type == TEXT("Texture2D") || type == TEXT("SamplerState"))
		{
			functioncallParameters.Add(inputParameter);
		}
		else
		{
			FString arrays = FString::Join(parameterArray, TEXT(","));
			variableDeclaration += FString::Printf(TEXT("%s %s[] = {%s};\n"), *type, *inputParameter, *arrays);
			functioncallParameters.Add(FString::Printf(TEXT("%s[shaderIndex]"), *inputParameter));
		}
		
		uiPos.Y += UI_DELTA_Y;
	}

	PostAddInputs(parameterNames, outGraphPins);
	pMaterialEditor->UpdateMaterialAfterGraphChange();

	shaderInfo.ShaderFunctionBody = FString::Printf(TEXT("	case %d:\n"), typeID);
	for (auto& inParameter : inParameters)
	{
		FString inParamaeterInitCode = inParameter.AlphaVariableName.Len() > 0 ? FString::Printf(TEXT(
			"	if (%s[shaderIndex].w >= 0)\n"
			"	{\n"
			"		int output_node_index = %s[shaderIndex].w / 10;\n"
			"		int output_index = %s[shaderIndex].w %% 10;\n"
			"		%s[shaderIndex].xyz = outputs[output_node_index][output_index].xyz;\n"
			"		%s[shaderIndex] = outputs[output_node_index][output_index].w;\n"
			"	}\n"), 
			*inParameter.Name, *inParameter.Name, *inParameter.Name, *inParameter.Name, *inParameter.AlphaVariableName) :
			FString::Printf(TEXT(
				"	if (%s[shaderIndex].w >= 0)\n"
				"	{\n"
				"		int output_node_index = %s[shaderIndex].w / 10;\n"
				"		int output_index = %s[shaderIndex].w %% 10;\n"
				"		%s[shaderIndex].xyz = outputs[output_node_index][output_index].xyz;\n"
				"	}\n"),
				*inParameter.Name, *inParameter.Name, *inParameter.Name, *inParameter.Name);
		shaderInfo.ShaderFunctionBody += inParamaeterInitCode;
	}
	FString functionCallParameter = FString::Join(functioncallParameters, TEXT(","));
	shaderInfo.ShaderFunctionBody += FString::Printf(TEXT("	f.%s(%s);\n"), *shaderType, *functionCallParameter);
	shaderInfo.ShaderFunctionBody += TEXT("	break;\n");
	shaderInfo.ParameterDeclartion = variableDeclaration;
#endif

	return true;
}

void UMaterialExpressionCustomMultiOut::DoModifyShaderNodeInputs()
{
#if WITH_EDITOR
	FVector2D uiPos = PreDeleteInputs(ESS_NODES_PREFIX, 1);
	
	TArray<UEdGraphPin*> outGraphPins;
	TArray<FString> parameterNames;
	TSharedPtr<class IMaterialEditor> pMaterialEditor = FMaterialEditorUtilities::GetIMaterialEditorForObject(this);
	for (int i = 0; i < MaxShaderNodes; ++i)
	{
		FString parameterName = FString::Printf(TEXT("%s%d"), ESS_NODES_PREFIX, i);
		UMaterialExpressionScalarParameter* pScalarExpression = CreateScalarParameter(pMaterialEditor.Get(),
			parameterName, 0, uiPos, Inputs);
		if (NULL != pScalarExpression)
		{
			outGraphPins.Add(pScalarExpression->GraphNode->Pins[0]);
			parameterNames.Add(parameterName);
		}

		uiPos.Y += UI_DELTA_Y;
	}

	PostAddInputs(parameterNames, outGraphPins);
	pMaterialEditor->UpdateMaterialAfterGraphChange();

	FString nodeParameters = FString::Join(parameterNames, TEXT(","));
	EssNodeDeclaration = FString::Printf(TEXT("uint nodeId[] = { %s };\n"), *nodeParameters);
#endif
}

void UMaterialExpressionCustomMultiOut::DoCreateShaderBody()
{
	Code = FString::Printf(TEXT(
		"float4 outputs[%d][4];\n"
		"%s"
		"for (int i = 0; i < ess_shaderCount; ++i)\n"
		"{\n"
		"	for (int j = 0; j < 4; ++j)\n"
		"	{\n"
		"		outputs[i][j] = 0;\n"
		"	}\n"
		"}\n"), MaxShaderNodes, *EssNodeDeclaration);

	for (auto& shaderInfo : InputShaders)
	{
		Code += shaderInfo.ParameterDeclartion;
	}

	Code += TEXT(
		"Functions f;\n"
		"for (i = 0; i < ess_shaderCount; ++i)\n"
		"{\n"
		"	uint nodeType = nodeId[i] / 10;\n"
		"	shaderIndex = nodeId[i] % 10;\n"
		"	switch (nodeType)\n"
		"	{\n");
	for (auto& shaderInfo : InputShaders)
	{
		Code += shaderInfo.ShaderFunctionBody;
	}

	Code += TEXT(
		"	default:\n"
		"	break;\n"
		"	}\n"
		"}\n"
		"float lastIndex = ess_shaderCount - 1;\n"
		"float3 output_diffuse = outputs[lastIndex][0].xyz;\n"
		"output_specular = outputs[lastIndex][0].w;\n"
		"output_emissive = outputs[lastIndex][1];\n"
		"output_roughness = outputs[lastIndex][1].w;\n"
		"output_normal = outputs[lastIndex][2];\n"
		"return output_diffuse;\n");
}

#if WITH_EDITOR
void UMaterialExpressionCustomMultiOut::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// strip any spaces from input name
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("InputName")) )
	{
		for( int32 i=0;i<Inputs.Num();i++ )
		{
			Inputs[i].InputName.ReplaceInline(TEXT(" "),TEXT(""));
		}
	}
	if (PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("OutputName")))
	{
		for (int32 i = 0; i < OutputSlots.Num(); i++)
		{
			OutputSlots[i].OutputName.ReplaceInline(TEXT(" "), TEXT(""));
		}
	}

	if (PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("FilePath")))
	{
		for (int32 i = 0; i < InputShaders.Num(); ++i)
		{
			if (!mOldInputShaders[i].Equals(InputShaders[i]))
			{
				if (DoImportShader(i))
				{
					DoCreateShaderBody();
				}
				mOldInputShaders[i] = InputShaders[i];
				break;
			}
		}
	}

	if (PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("MaxShaderNodes")))
	{
		DoModifyShaderNodeInputs();
		DoCreateShaderBody();
	}

	if (PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("InputShaders")))
	{
		mOldInputShaders = InputShaders;
	}

	if (PropertyChangedEvent.MemberProperty)
	{
		const FName PropertyName = PropertyChangedEvent.MemberProperty->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustomMultiOut, Inputs) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustomMultiOut, OutputSlots))
		{
			if (GraphNode)
			{
				GraphNode->ReconstructNode();
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

uint32 UMaterialExpressionCustomMultiOut::GetOutputType(int32 OutputIndex)
{
	switch (OutputSlots[OutputIndex].OutputType)
	{
	case CMOT_Float1:
		return MCT_Float;
	case CMOT_Float2:
		return MCT_Float2;
	case CMOT_Float3:
		return MCT_Float3;
	case CMOT_Float4:
		return MCT_Float4;
	default:
		return MCT_Unknown;
	}
}
#endif // WITH_EDITOR

void UMaterialExpressionCustomMultiOut::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

	// Make a copy of the current code before we change it
	const FString PreFixUp = Code;

	bool bDidUpdate = false;

	if (Ar.UE4Ver() < VER_UE4_INSTANCED_STEREO_UNIFORM_UPDATE)
	{
		// Look for WorldPosition rename
		if (Code.ReplaceInline(TEXT("Parameters.WorldPosition"), TEXT("Parameters.AbsoluteWorldPosition"), ESearchCase::CaseSensitive) > 0)
		{
			bDidUpdate = true;
		}
	}
	// Fix up uniform references that were moved from View to Frame as part of the instanced stereo implementation
	else if (Ar.UE4Ver() < VER_UE4_INSTANCED_STEREO_UNIFORM_REFACTOR)
	{
		// Uniform members that were moved from View to Frame
		static const FString UniformMembers[] = {
			FString(TEXT("FieldOfViewWideAngles")),
			FString(TEXT("PrevFieldOfViewWideAngles")),
			FString(TEXT("ViewRectMin")),
			FString(TEXT("ViewSizeAndInvSize")),
			FString(TEXT("BufferSizeAndInvSize")),
			FString(TEXT("ExposureScale")),
			FString(TEXT("DiffuseOverrideParameter")),
			FString(TEXT("SpecularOverrideParameter")),
			FString(TEXT("NormalOverrideParameter")),
			FString(TEXT("RoughnessOverrideParameter")),
			FString(TEXT("PrevFrameGameTime")),
			FString(TEXT("PrevFrameRealTime")),
			FString(TEXT("OutOfBoundsMask")),
			FString(TEXT("WorldCameraMovementSinceLastFrame")),
			FString(TEXT("CullingSign")),
			FString(TEXT("NearPlane")),
			FString(TEXT("AdaptiveTessellationFactor")),
			FString(TEXT("GameTime")),
			FString(TEXT("RealTime")),
			FString(TEXT("Random")),
			FString(TEXT("FrameNumber")),
			FString(TEXT("CameraCut")),
			FString(TEXT("UseLightmaps")),
			FString(TEXT("UnlitViewmodeMask")),
			FString(TEXT("DirectionalLightColor")),
			FString(TEXT("DirectionalLightDirection")),
			FString(TEXT("DirectionalLightShadowTransition")),
			FString(TEXT("DirectionalLightShadowSize")),
			FString(TEXT("DirectionalLightScreenToShadow")),
			FString(TEXT("DirectionalLightShadowDistances")),
			FString(TEXT("UpperSkyColor")),
			FString(TEXT("LowerSkyColor")),
			FString(TEXT("TranslucencyLightingVolumeMin")),
			FString(TEXT("TranslucencyLightingVolumeInvSize")),
			FString(TEXT("TemporalAAParams")),
			FString(TEXT("CircleDOFParams")),
			FString(TEXT("DepthOfFieldFocalDistance")),
			FString(TEXT("DepthOfFieldScale")),
			FString(TEXT("DepthOfFieldFocalLength")),
			FString(TEXT("DepthOfFieldFocalRegion")),
			FString(TEXT("DepthOfFieldNearTransitionRegion")),
			FString(TEXT("DepthOfFieldFarTransitionRegion")),
			FString(TEXT("MotionBlurNormalizedToPixel")),
			FString(TEXT("GeneralPurposeTweak")),
			FString(TEXT("DemosaicVposOffset")),
			FString(TEXT("IndirectLightingColorScale")),
			FString(TEXT("HDR32bppEncodingMode")),
			FString(TEXT("AtmosphericFogSunDirection")),
			FString(TEXT("AtmosphericFogSunPower")),
			FString(TEXT("AtmosphericFogPower")),
			FString(TEXT("AtmosphericFogDensityScale")),
			FString(TEXT("AtmosphericFogDensityOffset")),
			FString(TEXT("AtmosphericFogGroundOffset")),
			FString(TEXT("AtmosphericFogDistanceScale")),
			FString(TEXT("AtmosphericFogAltitudeScale")),
			FString(TEXT("AtmosphericFogHeightScaleRayleigh")),
			FString(TEXT("AtmosphericFogStartDistance")),
			FString(TEXT("AtmosphericFogDistanceOffset")),
			FString(TEXT("AtmosphericFogSunDiscScale")),
			FString(TEXT("AtmosphericFogRenderMask")),
			FString(TEXT("AtmosphericFogInscatterAltitudeSampleNum")),
			FString(TEXT("AtmosphericFogSunColor")),
			FString(TEXT("AmbientCubemapTint")),
			FString(TEXT("AmbientCubemapIntensity")),
			FString(TEXT("RenderTargetSize")),
			FString(TEXT("SkyLightParameters")),
			FString(TEXT("SceneFString(TEXTureMinMax")),
			FString(TEXT("SkyLightColor")),
			FString(TEXT("SkyIrradianceEnvironmentMap")),
			FString(TEXT("MobilePreviewMode")),
			FString(TEXT("HMDEyePaddingOffset")),
			FString(TEXT("DirectionalLightShadowFString(TEXTure")),
			FString(TEXT("SamplerState")),
		};

		const FString ViewUniformName(TEXT("View."));
		const FString FrameUniformName(TEXT("Frame."));
		for (const FString& Member : UniformMembers)
		{
			const FString SearchString = FrameUniformName + Member;
			const FString ReplaceString = ViewUniformName + Member;
			if (Code.ReplaceInline(*SearchString, *ReplaceString, ESearchCase::CaseSensitive) > 0)
			{
				bDidUpdate = true;
			}
		}
	}

	if (Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::RemovedRenderTargetSize)
	{
		if (Code.ReplaceInline(TEXT("View.RenderTargetSize"), TEXT("View.BufferSizeAndInvSize.xy"), ESearchCase::CaseSensitive) > 0)
		{
			bDidUpdate = true;
		}
	}

	// If we made changes, copy the original into the description just in case
	if (bDidUpdate)
	{
		Desc += TEXT("\n*** Original source before expression upgrade ***\n");
		Desc += PreFixUp;
		// UE_LOG(LogMaterial, Log, TEXT("Uniform references updated for custom material expression %s."), *Description);
	}

	if (Ar.IsLoading())
	{
		mOldInputShaders = InputShaders;
	}
}