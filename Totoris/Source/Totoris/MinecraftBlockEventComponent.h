#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MinecraftBlockEventComponent.generated.h"

/**
 * The first Minecraft presentation beat.  It retains the editor-authored
 * source block and temporarily grows three runtime copies along world Y+.
 */
UCLASS(ClassGroup=(Totoris), meta=(BlueprintSpawnableComponent))
class TOTORIS_API UMinecraftBlockEventComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMinecraftBlockEventComponent();

	/** Starts a clean three-block Y+ build, one 100cm cube per second. */
	void PlayEvent();

	/** Removes only runtime copies; the authored source block is untouched. */
	void StopEvent();

	bool IsEventPlaying() const { return bPlaying; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void SpawnNextBlock();

	TArray<TWeakObjectPtr<AActor>> SpawnedBlocks;
	float SpawnElapsedSeconds = 0.f;
	int32 SpawnedBlockCount = 0;
	bool bPlaying = false;

	static constexpr int32 BlocksToSpawn = 3;
	static constexpr float SpawnIntervalSeconds = 1.f;
	static constexpr float BlockStepCentimetres = 100.f;
};
