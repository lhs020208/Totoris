#include "MinecraftBlockEventComponent.h"
#include "MinecraftLavaComponent.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UMinecraftBlockEventComponent::UMinecraftBlockEventComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMinecraftBlockEventComponent::PlayEvent()
{
	StopEvent();
	if (!IsValid(GetOwner()))
	{
		return;
	}

	SpawnElapsedSeconds = 0.f;
	SpawnedBlockCount = 0;
	EventOrigin = GetOwner()->GetActorTransform();
	bPlaying = true;
	SetComponentTickEnabled(true);
	StartLavaOnFloor();
}

void UMinecraftBlockEventComponent::StopEvent()
{
	if (IsValid(LavaComponent)) LavaComponent->StopLava();
	bPlaying = false;
	SetComponentTickEnabled(false);
	SpawnElapsedSeconds = 0.f;
	SpawnedBlockCount = 0;

	for (TWeakObjectPtr<AActor>& SpawnedBlock : SpawnedBlocks)
	{
		if (SpawnedBlock.IsValid())
		{
			SpawnedBlock->Destroy();
		}
	}
	SpawnedBlocks.Reset();
}

void UMinecraftBlockEventComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopEvent();
	Super::EndPlay(Reason);
}

void UMinecraftBlockEventComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bPlaying)
	{
		return;
	}

	SpawnElapsedSeconds += FMath::Max(0.f, DeltaTime);
	while (bPlaying && SpawnElapsedSeconds + KINDA_SMALL_NUMBER >= SpawnIntervalSeconds)
	{
		SpawnElapsedSeconds -= SpawnIntervalSeconds;
		SpawnNextBlock();
	}
}

void UMinecraftBlockEventComponent::SpawnNextBlock()
{
	AActor* SourceBlock = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(SourceBlock) || !IsValid(World))
	{
		StopEvent();
		return;
	}

	++SpawnedBlockCount;
	FTransform SpawnTransform = EventOrigin;
	SpawnTransform.AddToTranslation(FVector(0.f, BlockStepCentimetres * SpawnedBlockCount, 0.f));

	FActorSpawnParameters SpawnParameters;
	// Do not reuse a fixed object name here. Destroy() marks the prior runtime
	// copy pending-kill, but its UObject name can remain occupied until GC runs.
	// A second mission start before that GC would otherwise assert inside
	// SpawnActor when it tries to create MinecraftEventBlock_1 again.
	SpawnParameters.Name = NAME_None;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AActor* SpawnedBlock = World->SpawnActor<AActor>(SourceBlock->GetClass(), SpawnTransform, SpawnParameters))
	{
		SpawnedBlocks.Add(SpawnedBlock);
	}

	if (SpawnedBlockCount >= BlocksToSpawn)
	{
		bPlaying = false;
		SetComponentTickEnabled(false);
	}
}

void UMinecraftBlockEventComponent::StartLavaOnFloor()
{
	if (!GetWorld() || !GetOwner()) return;
	const FVector LastBlockPosition = EventOrigin.GetLocation() + FVector(0, BlocksToSpawn * BlockStepCentimetres, 0);
	// Only the room floor defines the source height. Rugs, furniture and event
	// blocks must not lift the source or obstruct this presentation effect.
	FVector SurfacePoint(LastBlockPosition.X, LastBlockPosition.Y, -5.f);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		bool bFloor = It->GetName().Contains(TEXT("Scene_RoundFloor")) || It->ActorHasTag(TEXT("Scene_RoundFloor"));
#if WITH_EDITOR
		bFloor |= It->GetActorLabel().Equals(TEXT("Scene_RoundFloor"));
#endif
		if (!bFloor) continue;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MinecraftLavaSource), true);
		if (It->ActorLineTraceSingle(Hit, LastBlockPosition + FVector(0,0,5000),
			LastBlockPosition - FVector(0,0,5000), ECC_Visibility, Params) && Hit.ImpactNormal.Z > .7f)
		{
			SurfacePoint.Z = Hit.ImpactPoint.Z;
			break;
		}
	}
	if (!IsValid(LavaComponent))
	{
		LavaComponent = NewObject<UMinecraftLavaComponent>(GetOwner());
		GetOwner()->AddInstanceComponent(LavaComponent);
		LavaComponent->RegisterComponent();
	}
	LavaComponent->StartLava(SurfacePoint);
}
