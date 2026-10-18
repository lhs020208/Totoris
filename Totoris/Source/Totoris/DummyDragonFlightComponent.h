#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DummyDragonFlightComponent.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class USoundBase;
class UAudioComponent;

struct FDragonRocketTrailParticle
{
	TObjectPtr<UStaticMeshComponent> Component;
	TObjectPtr<UMaterialInstanceDynamic> Material;
	FVector Velocity = FVector::ZeroVector;
	float Age = 0.f;
	float Lifetime = 0.f;
	float StartScale = 0.f;
	// Path streaks preserve their authored segment length, while the small
	// sparks below them continue to expand as they fade.
	FVector BaseScale = FVector::OneVector;
	FVector LocalOffset = FVector::ZeroVector;
	FVector AuraWorldOffset = FVector::ZeroVector;
	float Phase = 0.f;
	bool bPathStreak = false;
	bool bActive = false;
};

/**
 * Drives the authored DummyDragon along the room's camera-to-window flight
 * path.  The component is model-agnostic so the final dragon can replace the
 * placeholder actor without changing the event wiring.
 */
UCLASS(ClassGroup=(Totoris), meta=(BlueprintSpawnableComponent))
class TOTORIS_API UDummyDragonFlightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void StopEvent();
	UDummyDragonFlightComponent();

	/** Captures the dragon start and the authored Plane2 window target once. */
	void InitializeFlight(AActor* InWindowPlane);

	/** Restarts the complete camera-behind to window flight. */
	UFUNCTION(BlueprintCallable, Category="Totoris|Dragon")
	void PlayFlight();

	UFUNCTION(BlueprintPure, Category="Totoris|Dragon")
	bool IsFlightPlaying() const { return bPlaying; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FVector EvaluateFlight(float Alpha) const;
	FVector EvaluateFlightVelocity(float Alpha) const;
	float GetApproachPathAlpha(float ElapsedSeconds) const;
	FVector KeepInCameraView(const FVector& Location) const;
	void ResetRocketTrails();
	void UpdateRocketTrails(float DeltaTime);
	void CreateRocketParticlePool();
	void UpdateRocketParticles(float DeltaTime);
	void SpawnRocketParticle(const FVector& Location, const FVector& TrailDirection, int32 RocketIndex);
	void SpawnRocketPathStreak(const FVector& Start, const FVector& End);
	void ResetDragonAura();
	void CreateDragonAuraPool();
	void UpdateDragonAura(float DeltaTime);
	void SpawnDragonAuraParticle(int32 ParticleIndex);
	void PlayFlySoundAtDragon();
	void CreateWingEmbers();
	void UpdateWingEmbers(float DeltaTime, bool bEmit);
	void ResetWingEmbers();
	void ApplyOpacity(float Opacity);
	void PrepareDynamicMaterials();

	UPROPERTY(Transient)
	FTransform InitialTransform;
	FVector WindowTarget = FVector::ZeroVector;
	// A small authored spline: an approach lane through the board followed by
	// one broad turn into the window.  This avoids the old back-and-forth arc.
	TArray<FVector> FlightPathPoints;
	// Authored from Rocket through Rocket10. These are Dragon_Model local-space
	// offsets, captured before the temporary placement actors were removed.
	TArray<FVector> RocketLocalOffsets;
	TArray<FVector> PreviousRocketLocations;
	TArray<FDragonRocketTrailParticle> RocketTrailParticles;
	TArray<FDragonRocketTrailParticle> DragonAuraParticles;
	TArray<FDragonRocketTrailParticle> WingEmbers;
	UPROPERTY(Transient)
	TObjectPtr<AActor> WingEmberHost;
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> WingMesh;
	float WingEmberAccumulator = 0.f;
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> DragonFlySound;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> FlyAudioComponent;
	int32 NextFlySoundCue = 0;
	float RocketParticleSpawnAccumulator = 0.f;
	float AuraParticleSpawnAccumulator = 0.f;
	bool bHasRocketTrailHistory = false;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FlightMaterials;
	float FlightElapsedSeconds = 0.f;
	bool bInitialized = false;
	bool bPlaying = false;

	// Keep the final window dissolve at its authored timing. The first 40% of
	// the spline is the entrance/board lane and deliberately runs at one quarter
	// of its original speed; the rest of the approach keeps its prior pacing.
	static constexpr float NormalApproachSeconds = 5.8f;
	static constexpr float BoardPassPathAlpha = .4f;
	static constexpr float BoardPassSpeedDivisor = 2.f;
	static constexpr float BoardPassSeconds = NormalApproachSeconds * BoardPassPathAlpha * BoardPassSpeedDivisor;
	static constexpr float ApproachSeconds = BoardPassSeconds + NormalApproachSeconds * (1.f - BoardPassPathAlpha);
	static constexpr float FadeDurationSeconds = 1.4f;
	static constexpr float AdditionalStartLowering = 125.f;
};
