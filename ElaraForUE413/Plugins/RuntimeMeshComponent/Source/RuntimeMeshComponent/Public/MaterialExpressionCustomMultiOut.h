// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "MaterialExpressionCustomMultiOut.generated.h"

USTRUCT()
struct FCustomOutput
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = FCustomOutput)
	FString OutputName;

	UPROPERTY(EditAnywhere, Category = FCustomOutput)
	TEnumAsByte<enum ECustomMaterialOutputType> OutputType;

	UPROPERTY()
	FExpressionOutput Output;

};

USTRUCT()
struct FInputShaderInfo
{
	GENERATED_USTRUCT_BODY()
	FInputShaderInfo() : MaxShaderCount(3) {}

	UPROPERTY(EditAnywhere, Category = FInputShaderInfo, meta = (FilePathFilter = "hlsl"))
	FFilePath ShaderFilePath;

	UPROPERTY(EditAnywhere, Category = FInputShaderInfo)
	int32 MaxShaderCount;

	UPROPERTY(VisibleAnywhere, Category = FInputShaderInfo)
	FString ShaderType;

	UPROPERTY()
	FString ParameterDeclartion;

	UPROPERTY()
	FString ShaderFunctionBody;

	UPROPERTY()
	TArray<FString> TextureParameters;

	inline bool Equals(const FInputShaderInfo& other)
	{
		return ShaderFilePath.FilePath == other.ShaderFilePath.FilePath && MaxShaderCount == other.MaxShaderCount;
	}
};

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionCustomMultiOut : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionCustomMultiOut, meta=(MultiLine=true))
	FString Code;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionCustomMultiOut)
	TArray<FCustomOutput> OutputSlots;

	UPROPERTY(EditAnywhere, Category = MaterialExpressionCustomMultiOut)
	TArray<FInputShaderInfo> InputShaders;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionCustomMultiOut)
	FString Description;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionCustomMultiOut)
	TArray<struct FCustomInput> Inputs;

	UPROPERTY(EditAnywhere, Category = MaterialExpressionCustomMultiOut)
	int32 MaxShaderNodes;

	UPROPERTY()
	FString EssNodeDeclaration;

	//~ Begin UObject Interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface.
	
	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual const TArray<FExpressionInput*> GetInputs() override;
	virtual TArray<FExpressionOutput>& GetOutputs() override;
	virtual FExpressionInput* GetInput(int32 InputIndex) override;
	virtual FString GetInputName(int32 InputIndex) const override;
#if WITH_EDITOR
	virtual uint32 GetInputType(int32 InputIndex) override {return MCT_Unknown;}
	virtual uint32 GetOutputType(int32 OutputIndex) override;
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface
private:
	bool DoImportShader(int32 currentShaderIndex);
	void DoModifyShaderNodeInputs();
	void DoCreateShaderBody();
	FVector2D PreDeleteInputs(const FString& prefix, int nodeCountsPerRow);
	void PostAddInputs(const TArray<FString> parameterNames, TArray<UEdGraphPin*> outGraphPins);
	FString mFrameStamp;
	TArray<int32> mOutputResults;
	TArray<FInputShaderInfo> mOldInputShaders;
};