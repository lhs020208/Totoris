#include "MinecraftBlockEventComponent.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"

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
	bPlaying = true;
	SetComponentTickEnabled(true);
}

void UMinecraftBlockEventComponent::StopEvent()
{
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
	FTransform SpawnTransform = SourceBlock->GetActorTransform();
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
