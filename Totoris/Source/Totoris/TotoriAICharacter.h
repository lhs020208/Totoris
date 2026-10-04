// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Navigation/PathFollowingComponent.h"
#include "TotoriAICharacter.generated.h"

class AAIController;
class UAnimationAsset;

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
	void PlayLoopingAnimation(UAnimationAsset* Animation);
	bool IsInsideActivityArea(const FVector& WorldLocation) const;
	bool FindWeightedWanderDestination(FNavLocation& OutLocation, bool bRecovery) const;
};
