// Fill out your copyright notice in the Description page of Project Settings.

#include "ReflectionMonitorActor.h"
#include "UObjectIterator.h"
#include "Engine.h"
#include "Windows.h"

// Sets default values
AReflectionMonitorActor::AReflectionMonitorActor() : 
	mbMouseDown(false),
	mMovableActorCount(0),
	ObjectTrackMode(MODE_DRAGANDDROP)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

bool IsMovableActor(AActor* pActor)
{
	if (NULL == pActor)
	{
		return false;
	}

	if (Cast<ACharacter>(pActor) != NULL)
	{
		return false;
	}

	TArray<UPrimitiveComponent*> primitiveComponents;
	pActor->GetComponents(primitiveComponents);
	if (primitiveComponents.Num() == 0)
	{
		return false;
	}

	USceneComponent* pRootComponent = pActor->GetRootComponent();
	return NULL != pRootComponent && pRootComponent->Mobility == EComponentMobility::Movable;
}

int GetMovableActorCount()
{
	int count = 0;
	for (TObjectIterator<AActor> It; It; ++It)
	{
		AActor* pActor = *It;
		if (IsMovableActor(pActor))
		{
			count++;
		}
	}

	return count;
}

// Called when the game starts or when spawned
void AReflectionMonitorActor::BeginPlay()
{
	Super::BeginPlay();
	if (mSphereReflectionCaptures.Num() == 0)
	{
		for (TObjectIterator<ASphereReflectionCapture> It; It; ++It)
		{
			ASphereReflectionCapture* pCaptureObj = *It;
#ifdef WITH_EDITOR
			if (GEngine->GameViewport->GetWorld() == pCaptureObj->GetWorld())
#endif
			mSphereReflectionCaptures.Add(pCaptureObj);
		}
		mMovableActorCount = GetMovableActorCount();
	}
}

void AReflectionMonitorActor::ChangeMobility(USceneComponent* pComponent, EComponentMobility::Type mobility)
{
	pComponent->SetMobility(mobility);
}

bool GetActiveCameraSetting(UWorld* world, FVector& cameraOrigin, FVector& cameraDirection)
{
	bool bFindCameraSettings = false;
	APlayerCameraManager* pManager = UGameplayStatics::GetPlayerCameraManager(world, 0);
	if (NULL != pManager)
	{
		cameraOrigin = pManager->GetCameraLocation();
		// With x-axis for forward direction
		cameraDirection = pManager->GetCameraRotation().RotateVector(FVector(1.f, 0.f, 0.f));
		bFindCameraSettings = true;
	}
	else
	{
		USceneComponent* pActiveCameraRootComponent = NULL;
		for (TObjectIterator<UCameraComponent> It; It; ++It)
		{
			UCameraComponent* pCameraComponent = *It;
			if (NULL != pCameraComponent && pCameraComponent->bIsActive)
			{
				TArray<USceneComponent*> parents;
				pCameraComponent->GetParentComponents(parents);
				if (parents.Num() > 0 && parents.Last() != NULL)
				{
					pActiveCameraRootComponent = parents.Last();
					if (NULL != pActiveCameraRootComponent)
					{
						const FTransform& cameraTransform = pActiveCameraRootComponent->GetComponentTransform();
						cameraOrigin = cameraTransform.GetLocation();
						cameraDirection = cameraTransform.GetUnitAxis(EAxis::X);
						bFindCameraSettings = true;
						break;
					}
				}
			}
		}
	}

	return bFindCameraSettings;
}

// Called every frame
void AReflectionMonitorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	bool movableActorChanged = false;
	UWorld* world = GEngine->GameViewport->GetWorld();
	bool bMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	switch (ObjectTrackMode)
	{
	case MODE_DRAGANDDROP:
		if (!mbMouseDown)
		{
			if (bMouseDown)
			{
				mMovableActorTransforms.Reset();
				for (TObjectIterator<AActor> It; It; ++It)
				{
					AActor* pActor = *It;
					if (IsMovableActor(pActor))
					{
						USceneComponent* pRootComponent = pActor->GetRootComponent();
						mMovableActorTransforms.Add(pActor->GetUniqueID(), pRootComponent->GetComponentTransform());
					}
				}
			}
			else if (mTempStaticActors.Num() == 0)
			{
				int curentMovableActorCount = GetMovableActorCount();
				movableActorChanged = curentMovableActorCount != mMovableActorCount;
				mMovableActorCount = curentMovableActorCount;
			}
		}
		else if (!bMouseDown)
		{
			int movableActorCount = 0;
			for (TObjectIterator<AActor> It; It; ++It)
			{
				AActor* pActor = *It;
				movableActorCount++;
				if (movableActorCount > mMovableActorCount)
				{
					movableActorChanged = true;
					break;
				}

				if (IsMovableActor(pActor))
				{
					USceneComponent* pRootComponent = pActor->GetRootComponent();
					FTransform* pTransform = mMovableActorTransforms.Find(pActor->GetUniqueID());
					if (NULL == pTransform || !pTransform->Equals(pRootComponent->GetComponentTransform()))
					{
						movableActorChanged = true;
						break;
					}
				}
			}
		}
		mbMouseDown = bMouseDown;
		break;
	case MODE_AGRESSIVE:
		if (!bMouseDown)
		{
			static int sCheckTime = 0;
			const int CHECK_FREQUENCY = 5;
			TMap<uint32, FTransform> currentMovableActorTransforms;
			if (sCheckTime++ == CHECK_FREQUENCY)
			{
				sCheckTime = sCheckTime % CHECK_FREQUENCY;
				for (TObjectIterator<AActor> It; It; ++It)
				{
					AActor* pActor = *It;
					if (IsMovableActor(pActor))
					{
						USceneComponent* pRootComponent = pActor->GetRootComponent();
						FTransform* pTransform = mMovableActorTransforms.Find(pActor->GetUniqueID());
						if (NULL == pTransform || !pTransform->Equals(pRootComponent->GetComponentTransform()))
						{
							movableActorChanged = true;
						}
						currentMovableActorTransforms.Add(pActor->GetUniqueID(), pRootComponent->GetComponentTransform());
					}
				}
				movableActorChanged |= currentMovableActorTransforms.Num() != mMovableActorTransforms.Num();
				if (movableActorChanged)
				{
					mMovableActorTransforms = currentMovableActorTransforms;
				}
			}
		}
		break;
	case MODE_AGRESSIVE_ESS:
		if (!bMouseDown)
		{
			static int sCheckTime2 = 0;
			const int CHECK_FREQUENCY = 5;
			TMap<uint32, FTransform> currentMovableActorTransforms;
			if (sCheckTime2++ == CHECK_FREQUENCY)
			{
				sCheckTime2 = sCheckTime2 % CHECK_FREQUENCY;
				for (TObjectIterator<UMeshComponent> It; It; ++It)
				{
					UMeshComponent* pPrimitive = *It;
					if (NULL == pPrimitive || pPrimitive->GetClass()->GetName() != TEXT("RuntimeMeshComponent"))
					{
						continue;
					}

					FTransform* pTransform = mMovableActorTransforms.Find(pPrimitive->GetUniqueID());
					if (NULL == pTransform || !pTransform->Equals(pPrimitive->GetComponentTransform()))
					{
						movableActorChanged = true;
					}
					currentMovableActorTransforms.Add(pPrimitive->GetUniqueID(), pPrimitive->GetComponentTransform());
				}
				
				movableActorChanged |= currentMovableActorTransforms.Num() != mMovableActorTransforms.Num();
				if (movableActorChanged)
				{
					mMovableActorTransforms = currentMovableActorTransforms;
				}
			}
		}
		else
		{
			for (TObjectIterator<UMeshComponent> It; It; ++It)
			{
				UMeshComponent* pMeshComponent = *It;
				if (NULL != pMeshComponent && pMeshComponent->GetClass()->GetName() == TEXT("RuntimeMeshComponent") && pMeshComponent->IsSelected())
				{
					pMeshComponent->SetMobility(EComponentMobility::Movable);
				}
			}
		}
		break;
	default:
		break;
	}

	if (movableActorChanged)
	{
		bool anyInvalid = false;
		for (auto& capture : mSphereReflectionCaptures)
		{
			if (capture.IsValid())
			{
				mPendingUpdatingCaptures.Add(capture);
			}
			else
			{
				anyInvalid = true;
			}
		}
		if (MODE_AGRESSIVE_ESS != ObjectTrackMode)
		{
			for (TObjectIterator<AActor> It; It; ++It)
			{
				AActor* pActor = *It;
				if (IsMovableActor(pActor))
				{
					mTempStaticActors.Add(pActor);
					pActor->GetRootComponent()->SetMobility(EComponentMobility::Static);
				}
			}
		}
		else
		{
			for (TObjectIterator<UMeshComponent> It; It; ++It)
			{
				UMeshComponent* pMeshComponent = *It;
				if (NULL != pMeshComponent && pMeshComponent->GetClass()->GetName() == TEXT("RuntimeMeshComponent"))
				{
					pMeshComponent->SetMobility(EComponentMobility::Static);
				}
			}
		}
		
		if (anyInvalid)
		{
			mSphereReflectionCaptures = mPendingUpdatingCaptures;
		}
	}

	if (mPendingUpdatingCaptures.Num() > 0)
	{
		if (bMouseDown)
		{
			return;
		}

		FVector cameraOrigin;
		FVector cameraDirection;
		ASphereReflectionCapture* pCandidateCaptureActor = NULL;
		if (GetActiveCameraSetting(world, cameraOrigin, cameraDirection))
		{
			float candidateDepth = FLT_MAX;
			for (auto& capture : mPendingUpdatingCaptures)
			{
				if (!capture.IsValid())
				{
					continue;
				}

				UReflectionCaptureComponent* pReflectionCapture = capture->GetCaptureComponent();
				if (pReflectionCapture)
				{
					const FTransform& captureTransform = capture->GetRootComponent()->GetComponentTransform();
					FVector cameraToCapture = captureTransform.GetLocation() - cameraOrigin;
					float depth = FVector::DotProduct(cameraToCapture, cameraDirection);
					auto compareDepth = [&]()
					{
						if (depth > 0)
						{
							if (candidateDepth <= 0)
							{
								return true;
							}
							else if (depth < candidateDepth)
							{
								return true;
							}
						}
						else // depth <= 0
						{
							if (candidateDepth == FLT_MAX)
							{
								return true;
							}
							else if (depth > candidateDepth)
							{
								return true;
							}
						}
						return false;
					};
					if (compareDepth())
					{
						candidateDepth = depth;
						pCandidateCaptureActor = capture.Get();
					}
				}
			}
			if (FLT_MAX == candidateDepth)
			{
				mPendingUpdatingCaptures.Reset();
			}
		}
		else
		{
			pCandidateCaptureActor = (*mPendingUpdatingCaptures.CreateIterator()).Get();
		}

		if (NULL != pCandidateCaptureActor)
		{
			UReflectionCaptureComponent* pReflectionCapture = pCandidateCaptureActor->GetCaptureComponent();
			if (NULL != pReflectionCapture)
			{
				pReflectionCapture->SetCaptureIsDirty();
			}
			mPendingUpdatingCaptures.Remove(pCandidateCaptureActor);
		}
	}
	else
	{
		for (auto& staticActor : mTempStaticActors)
		{
			if (staticActor.IsValid())
			{
				USceneComponent* pRootComponent = staticActor->GetRootComponent();
				pRootComponent->SetMobility(EComponentMobility::Movable);
			}
		}
		mTempStaticActors.Reset();
	}
}

