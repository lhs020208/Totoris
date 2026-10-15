#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DummyDragonFlightComponent.generated.h"

class UMaterialInstanceDynamic;

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

private:
	FVector EvaluateFlight(float Alpha) const;
	FVector EvaluateFlightVelocity(float Alpha) const;
	FVector KeepInCameraView(const FVector& Location) const;
	void ApplyOpacity(float Opacity);
	void PrepareDynamicMaterials();

	UPROPERTY(Transient)
	FTransform InitialTransform;
	FVector WindowTarget = FVector::ZeroVector;
	// A small authored spline: an approach lane through the board followed by
	// one broad turn into the window.  This avoids the old back-and-forth arc.
	TArray<FVector> FlightPathPoints;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FlightMaterials;
	float FlightElapsedSeconds = 0.f;
	bool bInitialized = false;
	bool bPlaying = false;

	// The approach runs at half speed (5.8 s).  The separate window dissolve
	// retains its authored 1.4 s timing.
	static constexpr float FlightDurationSeconds = 7.2f;
	static constexpr float FadeDurationSeconds = 1.4f;
};
