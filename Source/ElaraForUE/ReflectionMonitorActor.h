// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Classes/Engine/SphereReflectionCapture.h"
#include "ReflectionMonitorActor.generated.h"

UENUM()
enum EObjectTrackMode
{
	MODE_DRAGANDDROP,
	MODE_AGRESSIVE,
	MODE_AGRESSIVE_ESS
};

UCLASS()
class AReflectionMonitorActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AReflectionMonitorActor();

	UFUNCTION(BlueprintCallable, Category = "Components")
	static void ChangeMobility(USceneComponent* pComponent, EComponentMobility::Type mobility);

	UPROPERTY(EditAnywhere, Category = ObjectTracking)
	TEnumAsByte<enum EObjectTrackMode> ObjectTrackMode;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	bool mbMouseDown;
	TSet<TWeakObjectPtr<ASphereReflectionCapture>> mSphereReflectionCaptures;
	TSet<TWeakObjectPtr<ASphereReflectionCapture>> mPendingUpdatingCaptures;
	TSet<TWeakObjectPtr<AActor>> mTempStaticActors;
	TMap<uint32, FTransform> mMovableActorTransforms;
	int32 mMovableActorCount;
};
