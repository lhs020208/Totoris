// Copyright Epic Games, Inc. All Rights Reserved.

#include "TotoriAICharacter.h"

#include "AIController.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTotoriAI, Log, All);

ATotoriAICharacter::ATotoriAICharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, TurnRateDegreesPerSecond, 0.0f);

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> WalkAsset(
		TEXT("/Game/Totoris/Anim/Totori_walk_forward.Totori_walk_forward"));
	if (WalkAsset.Succeeded())
	{
		WalkAnimation = WalkAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> IdleAsset(
		TEXT("/Game/Totoris/Anim/Totori_idle_neutral_Anim.Totori_idle_neutral_Anim"));
	if (IdleAsset.Succeeded())
	{
		IdleAnimation = IdleAsset.Object;
	}
}

void ATotoriAICharacter::BeginPlay()
{
	Super::BeginPlay();

	// The previous Blueprint behavior was Tick-driven. Disable it so this
	// state machine is the only source of movement and animation decisions.
	SetActorTickEnabled(false);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->bOrientRotationToMovement = true;
	Movement->bUseControllerDesiredRotation = false;
	Movement->RotationRate = FRotator(0.0f, TurnRateDegreesPerSecond, 0.0f);

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->AddLocalOffset(FVector(0.0f, 0.0f, MeshGroundingOffset));
	}

	if (AAIController* AIController = GetTotoriAIController())
	{
		AIController->ReceiveMoveCompleted.AddDynamic(this, &ATotoriAICharacter::OnMoveCompleted);
		UE_LOG(LogTotoriAI, Log, TEXT("BeginPlay: controller=%s, walk speed=%.1f, location=%s"),
			*GetNameSafe(AIController), Movement->MaxWalkSpeed, *GetActorLocation().ToCompactString());
	}
	else
	{
		UE_LOG(LogTotoriAI, Error, TEXT("BeginPlay: no AAIController is possessing %s; movement requests cannot run."), *GetName());
	}

	PlayLoopingAnimation(WalkAnimation);
	GetWorldTimerManager().SetTimer(MovementWatchdogTimer, this, &ATotoriAICharacter::MonitorWanderProgress, 0.4f, true);
	GetWorldTimerManager().SetTimer(NextActionTimer, this, &ATotoriAICharacter::ChooseNextAction, 1.0f, false);
}

void ATotoriAICharacter::ChooseNextAction()
{
	// A completed/aborted request can schedule a retry. Consume that timer
	// before starting the next action so it cannot replace this action midway.
	GetWorldTimerManager().ClearTimer(NextActionTimer);

	if (bCanObserve)
	{
		if (AActor* Target = ChooseForwardObservationTarget())
		{
			BeginApproach(Target);
			return;
		}
	}

	Wander();
}

void ATotoriAICharacter::Wander()
{
	ObservationTarget = nullptr;
	bApproachingObservation = false;
	bWandering = true;
	StationaryTime = 0.0f;
	LastProgressLocation = GetActorLocation();
	PlayLoopingAnimation(WalkAnimation);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->bUseControllerDesiredRotation = false;

	if (AAIController* AIController = GetTotoriAIController())
	{
		FNavLocation Destination;
		if (FindWeightedWanderDestination(Destination, false))
		{
			bIssuingMoveRequest = true;
			const EPathFollowingRequestResult::Type MoveRequest = AIController->MoveToLocation(Destination.Location, 25.0f, true, true, false, true);
			bIssuingMoveRequest = false;
			UE_LOG(LogTotoriAI, Log, TEXT("Wander MoveTo: from=%s to=%s request=%d"),
				*GetActorLocation().ToCompactString(), *Destination.Location.ToCompactString(), static_cast<int32>(MoveRequest));

			if (MoveRequest == EPathFollowingRequestResult::Failed)
			{
				bWandering = false;
				PlayLoopingAnimation(IdleAnimation);
				UE_LOG(LogTotoriAI, Warning, TEXT("Wander MoveTo was rejected; idling briefly before retry."));
				GetWorldTimerManager().SetTimer(NextActionTimer, this, &ATotoriAICharacter::ChooseNextAction, 0.8f, false);
				return;
			}
		}
		else
		{
			bWandering = false;
			PlayLoopingAnimation(IdleAnimation);
			UE_LOG(LogTotoriAI, Warning, TEXT("Wander found no valid NavMesh destination from %s; idling briefly before retry."), *GetActorLocation().ToCompactString());
			GetWorldTimerManager().SetTimer(NextActionTimer, this, &ATotoriAICharacter::ChooseNextAction, 0.8f, false);
			return;
		}
	}
	else
	{
		bWandering = false;
		PlayLoopingAnimation(IdleAnimation);
		UE_LOG(LogTotoriAI, Error, TEXT("Wander aborted: no AIController."));
		GetWorldTimerManager().SetTimer(NextActionTimer, this, &ATotoriAICharacter::ChooseNextAction, 0.8f, false);
		return;
	}

	GetWorldTimerManager().SetTimer(NextActionTimer, this, &ATotoriAICharacter::ChooseNextAction, WanderRetargetInterval, false);
}

void ATotoriAICharacter::BeginApproach(AActor* Target)
{
	AAIController* AIController = GetTotoriAIController();
	if (!IsValid(Target) || !AIController || !IsInsideActivityArea(Target->GetActorLocation()))
	{
		Wander();
		return;
	}

	ObservationTarget = Target;
	bApproachingObservation = true;
	bWandering = false;
	PlayLoopingAnimation(WalkAnimation);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->bUseControllerDesiredRotation = false;
	bIssuingMoveRequest = true;
	const EPathFollowingRequestResult::Type MoveRequest = AIController->MoveToActor(Target, ApproachRadius, true, true, true, nullptr, true);
	bIssuingMoveRequest = false;
	UE_LOG(LogTotoriAI, Log, TEXT("Approach MoveTo: target=%s from=%s request=%d"),
		*GetNameSafe(Target), *GetActorLocation().ToCompactString(), static_cast<int32>(MoveRequest));
	if (MoveRequest == EPathFollowingRequestResult::Failed)
	{
		// The completion callback can arrive synchronously while the request is
		// issued. It is intentionally ignored above, so handle a rejected new
		// request here once the call has returned.
		bApproachingObservation = false;
		UE_LOG(LogTotoriAI, Warning, TEXT("Approach MoveTo was rejected; switching back to wander."));
		Wander();
	}
}

void ATotoriAICharacter::OnMoveCompleted(FAIRequestID RequestID, const EPathFollowingResult::Type Result)
{
	UE_LOG(LogTotoriAI, Log, TEXT("Move completed: request=%u result=%d approach=%d wandering=%d location=%s velocity=%.2f"),
		RequestID.GetID(), static_cast<int32>(Result), bApproachingObservation, bWandering,
		*GetActorLocation().ToCompactString(), GetVelocity().Size2D());
	if (bIssuingMoveRequest)
	{
		// Replacing an active path causes the old request to report Aborted before
		// MoveTo returns. It belongs to the old state, not the action being issued.
		UE_LOG(LogTotoriAI, Log, TEXT("Ignoring completion of superseded request %u while issuing a replacement."), RequestID.GetID());
		return;
	}

	if (!bApproachingObservation)
	{
		bWandering = false;
		// A regular wander request can finish immediately (the point is already
		// within acceptance radius), be aborted, or fail after NavMesh changes.
		// In all of those cases there is no active movement, so never leave the
		// walk clip playing until the normal retarget timer expires.
		PlayLoopingAnimation(IdleAnimation);
		const float RetryDelay = Result == EPathFollowingResult::Success ? 0.35f : 0.8f;
		GetWorldTimerManager().SetTimer(NextActionTimer, this, &ATotoriAICharacter::ChooseNextAction, RetryDelay, false);
		return;
	}

	bApproachingObservation = false;
	if (Result == EPathFollowingResult::Success && IsValid(ObservationTarget))
	{
		StartObserving();
	}
	else
	{
		Wander();
	}
}

void ATotoriAICharacter::StartObserving()
{
	AAIController* AIController = GetTotoriAIController();
	if (!AIController || !IsValid(ObservationTarget))
	{
		Wander();
		return;
	}

	// Focus uses the CharacterMovement rotation rate, so the final look is
	// eased instead of a SetActorRotation snap.
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	AIController->SetFocus(ObservationTarget);
	AIController->StopMovement();
	PlayLoopingAnimation(IdleAnimation);
	GetWorldTimerManager().SetTimer(ObservationTimer, this, &ATotoriAICharacter::FinishObserving, ObserveDuration, false);
}

void ATotoriAICharacter::FinishObserving()
{
	if (AAIController* AIController = GetTotoriAIController())
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}

	bCanObserve = false;
	ObservationTarget = nullptr;
	GetWorldTimerManager().SetTimer(CooldownTimer, this, &ATotoriAICharacter::EndObservationCooldown, ObserveCooldown, false);
	Wander();
}

void ATotoriAICharacter::EndObservationCooldown()
{
	bCanObserve = true;
}

void ATotoriAICharacter::MonitorWanderProgress()
{
	// This is a safety net for a path that was already in flight when a
	// NavMesh rebuild or obstacle change occurred. It replaces that path with
	// an inward-biased one before the character can continue off-screen.
	if (!IsInsideActivityArea(GetActorLocation()))
	{
		RecoverFromStall();
		return;
	}

	if (!bWandering)
	{
		return;
	}

	const float ProgressSinceLastCheck = FVector::DistSquared2D(GetActorLocation(), LastProgressLocation);
	const float CurrentSpeed = GetVelocity().Size2D();
	if (CurrentSpeed < 3.0f && ProgressSinceLastCheck < FMath::Square(3.0f))
	{
		StationaryTime += 0.4f;
		if (StationaryTime >= 1.2f)
		{
			UE_LOG(LogTotoriAI, Warning, TEXT("Wander stall: location=%s velocity=%.2f progress=%.2f; replacing path."),
				*GetActorLocation().ToCompactString(), CurrentSpeed, FMath::Sqrt(ProgressSinceLastCheck));
			RecoverFromStall();
		}
	}
	else
	{
		StationaryTime = 0.0f;
		LastProgressLocation = GetActorLocation();
	}
}

void ATotoriAICharacter::RecoverFromStall()
{
	StationaryTime = 0.0f;
	if (AAIController* AIController = GetTotoriAIController())
	{
		FNavLocation RecoveryDestination;
		if (FindWeightedWanderDestination(RecoveryDestination, true))
		{
			bApproachingObservation = false;
			bWandering = true;
			ObservationTarget = nullptr;
			AIController->ClearFocus(EAIFocusPriority::Gameplay);
			GetCharacterMovement()->bOrientRotationToMovement = true;
			GetCharacterMovement()->bUseControllerDesiredRotation = false;
			PlayLoopingAnimation(WalkAnimation);
			// Replacing the path keeps the walk state active. CharacterMovement's
			// rotation rate eases into the new direction instead of snapping.
			bIssuingMoveRequest = true;
			const EPathFollowingRequestResult::Type MoveRequest = AIController->MoveToLocation(RecoveryDestination.Location, 25.0f, true, true, false, true);
			bIssuingMoveRequest = false;
			UE_LOG(LogTotoriAI, Log, TEXT("Recovery MoveTo: from=%s to=%s request=%d"),
				*GetActorLocation().ToCompactString(), *RecoveryDestination.Location.ToCompactString(), static_cast<int32>(MoveRequest));
			if (MoveRequest == EPathFollowingRequestResult::Failed)
			{
				bWandering = false;
				PlayLoopingAnimation(IdleAnimation);
				GetWorldTimerManager().SetTimer(NextActionTimer, this, &ATotoriAICharacter::ChooseNextAction, 0.5f, false);
				return;
			}
			LastProgressLocation = GetActorLocation();
			return;
		}
	}

	bWandering = false;
	PlayLoopingAnimation(IdleAnimation);
	UE_LOG(LogTotoriAI, Warning, TEXT("Recovery found no valid destination; idling briefly before retry."));
	GetWorldTimerManager().SetTimer(NextActionTimer, this, &ATotoriAICharacter::ChooseNextAction, 0.5f, false);
}

AActor* ATotoriAICharacter::ChooseForwardObservationTarget() const
{
	TArray<AActor*> Candidates;
	UGameplayStatics::GetAllActorsWithTag(this, TEXT("TotoriLookTarget"), Candidates);

	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	TArray<AActor*> ForwardCandidates;
	for (AActor* Candidate : Candidates)
	{
		if (!IsValid(Candidate) || !IsInsideActivityArea(Candidate->GetActorLocation()))
		{
			continue;
		}

		FVector ToCandidate = Candidate->GetActorLocation() - GetActorLocation();
		ToCandidate.Z = 0.0f;
		if (ToCandidate.Normalize() && FVector::DotProduct(Forward, ToCandidate) >= ForwardDotThreshold)
		{
			ForwardCandidates.Add(Candidate);
		}
	}

	return ForwardCandidates.IsEmpty() ? nullptr : ForwardCandidates[FMath::RandHelper(ForwardCandidates.Num())];
}

AAIController* ATotoriAICharacter::GetTotoriAIController() const
{
	return Cast<AAIController>(GetController());
}

void ATotoriAICharacter::PlayLoopingAnimation(UAnimationAsset* Animation)
{
	if (!IsValid(Animation) || !GetMesh())
	{
		return;
	}

	// Re-requesting walk while it is already playing must not reset its time
	// to zero. This removes the visible pop on walk -> walk retargets.
	if (CurrentAnimation == Animation)
	{
		return;
	}

	if (UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Animation))
	{
		// A transient montage gives the single-node animation instance a real
		// blend-out for the old clip and blend-in for this clip. The DefaultSlot
		// is registered automatically by UAnimSingleNodeInstance for montages.
		if (UAnimMontage* BlendedMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
			Sequence, TEXT("DefaultSlot"), AnimationBlendTime, AnimationBlendTime))
		{
			GetMesh()->PlayAnimation(BlendedMontage, true);
			CurrentAnimation = Animation;
			return;
		}
	}

	// Retain a safe fallback for any future non-sequence animation asset.
	GetMesh()->PlayAnimation(Animation, true);
	CurrentAnimation = Animation;
}

bool ATotoriAICharacter::IsInsideActivityArea(const FVector& WorldLocation) const
{
	// Fixed-camera ground-frustum footprint, expressed as a conservative
	// trapezoid in world space. It is intentionally slightly inset from the
	// visible frame, so the character never walks along the screen edge.
	constexpr float NearX = -48850.0f;
	constexpr float FarX = -47350.0f;
	constexpr float NearMinY = -14250.0f;
	constexpr float NearMaxY = -14050.0f;
	constexpr float FarMinY = -15000.0f;
	constexpr float FarMaxY = -13300.0f;

	if (WorldLocation.X < NearX || WorldLocation.X > FarX)
	{
		return false;
	}

	const float Alpha = (WorldLocation.X - NearX) / (FarX - NearX);
	const float MinY = FMath::Lerp(NearMinY, FarMinY, Alpha);
	const float MaxY = FMath::Lerp(NearMaxY, FarMaxY, Alpha);
	return WorldLocation.Y >= MinY && WorldLocation.Y <= MaxY;
}

bool ATotoriAICharacter::FindWeightedWanderDestination(FNavLocation& OutLocation, bool bRecovery) const
{
	UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!Navigation)
	{
		return false;
	}

	const FVector ActivityCenter(-48100.0f, -14150.0f, GetActorLocation().Z);
	const FVector CurrentForward = GetVelocity().Size2D() > 5.0f
		? GetVelocity().GetSafeNormal2D()
		: GetActorForwardVector().GetSafeNormal2D();

	float BestScore = -TNumericLimits<float>::Max();
	bool bFoundCandidate = false;
	const int32 SampleCount = bRecovery ? 32 : 20;
	for (int32 Attempt = 0; Attempt < SampleCount; ++Attempt)
	{
		FNavLocation Candidate;
		if (!Navigation->GetRandomReachablePointInRadius(GetActorLocation(), WanderRadius, Candidate)
			|| !IsInsideActivityArea(Candidate.Location))
		{
			continue;
		}

		FVector ToCandidate = Candidate.Location - GetActorLocation();
		ToCandidate.Z = 0.0f;
		const float Distance = ToCandidate.Size();
		if (Distance < 200.0f || !ToCandidate.Normalize())
		{
			continue;
		}

		FVector TowardCenter = ActivityCenter - Candidate.Location;
		TowardCenter.Z = 0.0f;
		TowardCenter.Normalize();

		// Soft scores only: a corridor remains valid even if its only exit is
		// sideways or briefly away from the center.
		const float InwardScore = FVector::DotProduct(ToCandidate, TowardCenter);
		const float ContinuityScore = FVector::DotProduct(CurrentForward, ToCandidate);
		const float DistanceScore = 1.0f - FMath::Abs(Distance - 450.0f) / 450.0f;
		const float Score = InwardScore * (bRecovery ? 1.2f : 0.55f)
			+ ContinuityScore * 0.35f + DistanceScore * 0.15f + FMath::FRandRange(0.0f, 0.05f);

		if (Score > BestScore)
		{
			BestScore = Score;
			OutLocation = Candidate;
			bFoundCandidate = true;
		}
	}

	return bFoundCandidate;
}
