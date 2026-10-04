// Copyright Epic Games, Inc. All Rights Reserved.

#include "TotoriAICharacter.h"

#include "AIController.h"
#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"

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

	if (AAIController* AIController = GetTotoriAIController())
	{
		AIController->ReceiveMoveCompleted.AddDynamic(this, &ATotoriAICharacter::OnMoveCompleted);
	}

	PlayLoopingAnimation(WalkAnimation);
	GetWorldTimerManager().SetTimer(NextActionTimer, this, &ATotoriAICharacter::ChooseNextAction, 1.0f, false);
}

void ATotoriAICharacter::ChooseNextAction()
{
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
	PlayLoopingAnimation(WalkAnimation);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->bUseControllerDesiredRotation = false;

	if (AAIController* AIController = GetTotoriAIController())
	{
		FNavLocation Destination;
		if (UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(GetWorld()))
		{
			if (Navigation->GetRandomReachablePointInRadius(GetActorLocation(), WanderRadius, Destination))
			{
				AIController->MoveToLocation(Destination.Location, 25.0f, true, true, false, true);
			}
		}
	}

	GetWorldTimerManager().SetTimer(NextActionTimer, this, &ATotoriAICharacter::ChooseNextAction, WanderRetargetInterval, false);
}

void ATotoriAICharacter::BeginApproach(AActor* Target)
{
	AAIController* AIController = GetTotoriAIController();
	if (!IsValid(Target) || !AIController)
	{
		Wander();
		return;
	}

	ObservationTarget = Target;
	bApproachingObservation = true;
	PlayLoopingAnimation(WalkAnimation);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->bUseControllerDesiredRotation = false;
	AIController->MoveToActor(Target, ApproachRadius, true, true, true, nullptr, true);
}

void ATotoriAICharacter::OnMoveCompleted(FAIRequestID RequestID, const EPathFollowingResult::Type Result)
{
	if (!bApproachingObservation)
	{
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

AActor* ATotoriAICharacter::ChooseForwardObservationTarget() const
{
	TArray<AActor*> Candidates;
	UGameplayStatics::GetAllActorsWithTag(this, TEXT("TotoriLookTarget"), Candidates);

	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	TArray<AActor*> ForwardCandidates;
	for (AActor* Candidate : Candidates)
	{
		if (!IsValid(Candidate))
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
	if (IsValid(Animation) && GetMesh())
	{
		GetMesh()->PlayAnimation(Animation, true);
	}
}
