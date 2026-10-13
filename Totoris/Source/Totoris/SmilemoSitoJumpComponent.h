#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SmilemoSitoJumpComponent.generated.h"

class USoundBase;

/**
 * Replays the authored SmilemoSito hop route.  The route is deliberately kept
 * in code after its editor-only SMpos marker actors have been removed.
 */
UCLASS(ClassGroup=(Totoris), meta=(BlueprintSpawnableComponent))
class TOTORIS_API USmilemoSitoJumpComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USmilemoSitoJumpComponent();

	/** Captures the actor's authored transform exactly once and installs the landing points. */
	void InitializeRoute(const TArray<FVector>& InLandingPoints);

	/** Resets to the captured initial transform and plays the complete hop route. */
	UFUNCTION(BlueprintCallable, Category="Totoris|SmilemoSito")
	void PlayJumpRoute();

	/** Adds a boss that follows this route three metres behind in the XY plane. */
	void InitializeBossFollower(AActor* InBossActor);

	UFUNCTION(BlueprintPure, Category="Totoris|SmilemoSito")
	bool IsJumpRoutePlaying() const { return bPlaying; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FJumpSegment
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		float Duration = 0.f;
		TArray<float> ArcLengths;
		float TotalArcLength = 0.f;
	};

	void BuildSegments();
	FVector EvaluateSegment(const FJumpSegment& Segment, float CurveAlpha) const;
	float FindCurveAlphaAtDistance(const FJumpSegment& Segment, float Distance) const;
	float GetRouteDistance() const;
	float GetTotalRouteDistance() const;
	FVector GetRouteLocationAtDistance(float Distance) const;
	FVector GetBossOffsetLocation(float RouteDistance) const;
	float GetZVelocityAt(const FJumpSegment& Segment, float CurveAlpha) const;
	bool IsDownwardToUpwardTransition(int32 CompletedSegmentIndex) const;
	void ResetBossFollower();
	void UpdateBossFollower(float DeltaTime);
	FRotator MakeFacingRotation(const FVector& Location) const;
	void ApplyFacing(const FVector& Location);

	UPROPERTY(Transient)
	FTransform InitialTransform;

	TArray<FVector> LandingPoints;
	TArray<FJumpSegment> Segments;
	int32 CurrentSegmentIndex = 0;
	float CurrentSegmentElapsed = 0.f;
	bool bRouteInitialized = false;
	bool bPlaying = false;

	TWeakObjectPtr<AActor> BossActor;
	FTransform BossInitialTransform;
	bool bBossInitialized = false;
	bool bBossFinalAdvance = false;
	float BossRouteDistance = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> SitoSpinLineClearSound;

	// The horizontal XY arc speed is fixed at this value. Z is the vertical
	// jump axis in Unreal's world coordinate system.
	static constexpr float XYUnitsPerSecond = 420.f;
	static constexpr float BossXYUnitsPerSecond = 340.f;
	static constexpr float BossFollowDistance = 300.f;
	// The authored 1->2 and 2->3 hops have a short XY distance. Do not let
	// them collapse into an imperceptibly fast transition.
	static constexpr float MinimumSegmentDuration = .55f;
	static constexpr int32 ArcLengthSamples = 32;
};
