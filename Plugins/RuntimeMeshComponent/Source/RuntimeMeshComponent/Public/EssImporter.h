#pragma once
#include "Engine.h"
#include "RuntimeMeshCore.h"
#include <ei.h>
#include <ei_data_table.h>
#include <Public/HAL/ThreadSafeBool.h>

struct FMaxNodeInfo
{
	FString name;
	FString meshName;
	FMatrix matrix;
	bool bInvertVertexOrder;
};

struct FMeshInfo
{
	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FRuntimeMeshTangent> Tangents;
	TArray<FVector2D> Uv1s;
	TArray<FVector2D> Uv2s;
	TArray<int32> Triangles;
	TArray<int32> InvertTriangles;
	
	int mtlIndex;
};

struct FParseMaterialContext;

class FEssImporter : public FRunnable
{
public:
	FEssImporter();
	~FEssImporter();

	typedef TArray<FMeshInfo> TMeshArray;
	bool Initialize(const FString& FullPath, const FTimerDelegate& timerDelegate, bool inEditor);

	int GetNodeCount() const;
	const FMaxNodeInfo* GetNodeInfo(int index) const;
	const TMeshArray* GetMeshInfo(const FString& meshName) const;
	bool CheckParseFinished();
	inline bool GetParseResult() const { return mParseResult; };
	UMaterialInterface* GetNodeMaterial(int nodeIndex, int subMeshIndex, int mtlIndex, UPrimitiveComponent* pMeshComponent);

private:
	struct FMeshMapInfo
	{
		FMeshMapInfo()
		{
			bHasOriginalVertexOrder = false;
			bHasInvertVertexOrder = false;
		}

		bool bHasOriginalVertexOrder;
		bool bHasInvertVertexOrder;
		TMeshArray meshArray;
	};
	typedef eiDataAccessor<eiNode> eiNodeAccessor;
	typedef eiDeferDataAccessor<eiNode> eiDeferNodeAccessor;
	typedef TMap<FString, FMeshMapInfo> TMeshMap;

	bool DoParseEssFile();
	void InsertNodeInfo(const eiNodeAccessor& node);
	bool ParseMesh(const eiNodeAccessor& node, FMeshMapInfo& meshMapInfo);
	bool ParseMaterial(const eiNodeAccessor& shaderNode, FParseMaterialContext& context);
	FRunnableThread* m_pThread;
	FString m_strFullPath;

	virtual uint32 Run() override;

	TArray<FMaxNodeInfo> mNodeArray;
	TMeshMap mMeshMap;
	FTimerHandle mTimerHandle;
	FThreadSafeBool mParseFinished;
	bool mParseResult;
	TMap<FString, UMaterialInstanceDynamic*> mMaterailMap;
	TMap<FString, int32> mVectorParamMap;
	TMap<FString, int32> mScalarParamMap;
	bool mbInEditor;
};