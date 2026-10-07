// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Navigation/PathFollowingComponent.h"
#include "TotoriAICharacter.generated.h"

class AAIController;
class UAnimationAsset;
class UInputComponent;
class APlayerController;

/**
 * Autonomous room-roaming character used by BP_TotoriAI.
 *
 * Navigation remains on the NavMesh; the class only supplies the action
 * selection, approach, observation, and cooldown policy.
 */
UCLASS()
class TOTORIS_API ATotoriAICharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATotoriAICharacter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "Totori AI|Movement", meta = (ClampMin = "1.0"))
	float WalkSpeed = 27.5f;

	UPROPERTY(EditAnywhere, Category = "Totori AI|Movement", meta = (ClampMin = "1.0"))
	float TurnRateDegreesPerSecond = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Totori AI|Movement", meta = (ClampMin = "1.0"))
	float WanderRadius = 550.0f;

	UPROPERTY(EditAnywhere, Category = "Totori AI|Observation", meta = (ClampMin = "1.0"))
	float ApproachRadius = 125.0f;

	UPROPERTY(EditAnywhere, Category = "Totori AI|Observation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ForwardDotThreshold = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Totori AI|Observation", meta = (ClampMin = "0.1"))
	float ObserveDuration = 4.5f;

	UPROPERTY(EditAnywhere, Category = "Totori AI|Observation", meta = (ClampMin = "0.0"))
	float ObserveCooldown = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Totori AI|Timing", meta = (ClampMin = "0.1"))
	float WanderRetargetInterval = 4.5f;

	// The imported mesh sits slightly above the capsule's visual floor contact
	// point. This moves only the mesh, leaving collision and NavMesh unchanged.
	UPROPERTY(EditAnywhere, Category = "Totori AI|Movement")
	float MeshGroundingOffset = -2.0f;

	UPROPERTY(EditAnywhere, Category = "Totori AI|Animation", meta = (ClampMin = "0.0"))
	float AnimationBlendTime = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Totori AI|Animation")
	TObjectPtr<UAnimationAsset> WalkAnimation;

	UPROPERTY(EditAnywhere, Category = "Totori AI|Animation")
	TObjectPtr<UAnimationAsset> IdleAnimation;

	UPROPERTY(EditAnywhere, Category = "Totori AI|Jump")
	TObjectPtr<UAnimationAsset> JumpAnimation;

	UPROPERTY(EditAnywhere, Category = "Totori AI|Jump", meta = (ClampMin = "0.1"))
	float PerchWaitSeconds = 5.0f;

	// The source jump clip has a run-up. Keep the actor grounded until the
	// take-off frame, then complete all travel by its landing frame.
	UPROPERTY(EditAnywhere, Category = "Totori AI|Jump|Animation Timing", meta = (ClampMin = "1.0"))
	float JumpAnimationFrameCount = 121.0f;
	UPROPERTY(EditAnywhere, Category = "Totori AI|Jump|Animation Timing", meta = (ClampMin = "0.0"))
	float JumpTakeoffFrame = 48.0f;
	UPROPERTY(EditAnywhere, Category = "Totori AI|Jump|Animation Timing", meta = (ClampMin = "0.0"))
	float JumpLandingFrame = 63.0f;

	// Small clearance avoids a capsule penetration while making the feet sit on
	// soft furniture instead of visibly hovering above it.
	UPROPERTY(EditAnywhere, Category = "Totori AI|Jump", meta = (ClampMin = "-10.0", ClampMax = "5.0"))
	float LandingFootClearance = 0.5f;

	// Applied only while waiting on furniture. It is visual-only, so the
	// capsule stays valid while the feet settle naturally into soft cushions.
	UPROPERTY(EditAnywhere, Category = "Totori AI|Jump", meta = (ClampMin = "-10.0", ClampMax = "0.0"))
	float PerchedMeshVisualOffset = -3.0f;
	UPROPERTY(EditAnywhere, Category = "Totori AI|Jump", meta = (ClampMin = "-10.0", ClampMax = "0.0"))
	float SoftFurnitureAdditionalVisualOffset = -4.0f;

	// Soft furniture is intentionally allowed to compress around the feet.
	// This affects the actual perch destination (not just the mesh offset).
	UPROPERTY(EditAnywhere, Category = "Totori AI|Jump", meta = (ClampMin = "-30.0", ClampMax = "0.0"))
	float SoftFurnitureLandingSink = -8.0f;

	// Baked from the three JumpP marker planes. These are foot positions.
	UPROPERTY(EditAnywhere, Category = "Totori AI|Jump")
	// The Sofa actor's bounds include its tall backrest. Its marker was placed
	// at that bounds height, so use the actual seat surface instead.
	FVector SofaLanding = FVector(-47830.949851, -14118.789782, 110.000000);
	UPROPERTY(EditAnywhere, Category = "Totori AI|Jump")
	FVector CushionLanding = FVector(-48037.110439, -13909.247700, 25.130408);
	UPROPERTY(EditAnywhere, Category = "Totori AI|Jump")
	FVector BedLanding = FVector(-47430.536960, -14063.750408, 111.456589);


	enum class EJumpPhase : uint8 { None, Outbound, Turning, Waiting, Returning };
	EJumpPhase JumpPhase = EJumpPhase::None;
	FVector JumpOrigin = FVector::ZeroVector;
	FVector JumpStart = FVector::ZeroVector;
	FVector JumpEnd = FVector::ZeroVector;
	float JumpArcHeight = 0.f;
	float JumpDuration = 1.f;
	float JumpElapsed = 0.f;
	float JumpLastUpdateTime = 0.f;
	float PerchYaw = 0.f;
	FTimerHandle JumpUpdateTimer;
	FTimerHandle PerchTimer;
	bool bPerchedMeshOffsetApplied = false;
	float AppliedPerchedMeshOffset = 0.f;
	bool GetFurnitureLanding(const AActor* Target, FVector& OutFeet) const;
	bool FindJumpApproach(AActor* Target, FVector& OutGoal) const;
	bool GetSupportedLanding(const AActor* Target, FVector& OutCenter) const;
	bool FindClearJumpArc(const FVector& From, const FVector& To, float& OutHeight) const;
	bool IsJumpSegmentClear(const FVector& From, const FVector& To) const;
	bool TryStartFurnitureJump();
	void BeginJumpArc(const FVector& Destination, float Height, bool bReturning);
	void UpdateFurnitureJump();
	void ReturnFromFurniture();
	void ResumeAfterFurniture();
	void SetPerchedVisualOffset(bool bPerched);
	bool DebugJumpToFurniture(FName FurnitureTag);

	// This is the source clip, rather than its transient dynamic montage.
	// Keeping it lets repeated walk requests continue at their current time.
	TObjectPtr<UAnimationAsset> CurrentAnimation;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ObservationTarget;

	bool bCanObserve = true;
	bool bApproachingObservation = false;
	bool bWandering = false;
	// MoveTo may synchronously abort the previous request. While a replacement
	// request is being issued, its old completion callback must not change the
	// state of the new action.
	bool bIssuingMoveRequest = false;
	float StationaryTime = 0.0f;
	FVector LastProgressLocation = FVector::ZeroVector;

	FTimerHandle NextActionTimer;
	FTimerHandle ObservationTimer;
	FTimerHandle CooldownTimer;
	FTimerHandle MovementWatchdogTimer;

	void ChooseNextAction();
	void Wander();
	void BeginApproach(AActor* Target);
	void StartObserving();
	void FinishObserving();
	void EndObservationCooldown();
	void MonitorWanderProgress();
	void RecoverFromStall();
	UFUNCTION()
	void OnMoveCompleted(FAIRequestID RequestID, const EPathFollowingResult::Type Result);

	AActor* ChooseForwardObservationTarget() const;
	AAIController* GetTotoriAIController() const;
	void PlayAnimationBlended(UAnimationAsset* Animation, bool bLooping);
	void PlayLoopingAnimation(UAnimationAsset* Animation);
	bool IsInsideActivityArea(const FVector& WorldLocation) const;
	bool FindWeightedWanderDestination(FNavLocation& OutLocation, bool bRecovery) const;
};
