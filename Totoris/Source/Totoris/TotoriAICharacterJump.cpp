#include "TotoriAICharacter.h"

#include "AIController.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogTotoriJump, Log, All);

namespace
{
FVector ArcPoint(const FVector& Start, const FVector& End, float Height, float Alpha)
{
	return FMath::Lerp(Start, End, Alpha) + FVector(0, 0, 4.f * Height * Alpha * (1.f - Alpha));
}
}

bool ATotoriAICharacter::GetFurnitureLanding(const AActor* Target, FVector& OutFeet) const
{
	if (!IsValid(Target)) return false;
	if (Target->ActorHasTag(TEXT("TotoriJumpSofa"))) OutFeet = SofaLanding;
	else if (Target->ActorHasTag(TEXT("TotoriJumpCushion"))) OutFeet = CushionLanding;
	else if (Target->ActorHasTag(TEXT("TotoriJumpBed"))) OutFeet = BedLanding;
	else return false;
	return true;
}

bool ATotoriAICharacter::IsJumpSegmentClear(const FVector& From, const FVector& To) const
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TotoriJump), false, this);
	FHitResult Hit;
	// Use the same collision profile and full capsule as the actual movement.
	// Do not ignore the destination furniture: its backrest/arms are obstacles.
	return !GetWorld()->SweepSingleByProfile(Hit, From, To, FQuat::Identity,
		Capsule->GetCollisionProfileName(), FCollisionShape::MakeCapsule(
			Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Params);
}

bool ATotoriAICharacter::GetSupportedLanding(const AActor* Target, FVector& OutCenter) const
{
	FVector Marker;
	if (!GetFurnitureLanding(Target, Marker)) return false;
	// The bed marker was at its center. Land on the camera-facing long edge so
	// the capsule never has to pass through the mattress side to get on top.
	if (Target->ActorHasTag(TEXT("TotoriJumpBed")))
	{
		Marker += FVector(-55.f, 0.f, 0.f);
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	if (Target->ActorHasTag(TEXT("TotoriJumpSofa")))
	{
		// The sofa's collision extends up its backrest. A downward trace at the
		// central marker therefore reports the backrest top rather than the seat.
		// This baked seat point is deliberately authoritative for the sofa.
		OutCenter = Marker + FVector(0.f, 0.f, HalfHeight + LandingFootClearance + SoftFurnitureLandingSink);
		UE_LOG(LogTotoriJump, Log, TEXT("Using baked sofa-seat landing at %s"), *OutCenter.ToCompactString());
		return true;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TotoriLanding), false, this);
	const bool bSoftFurniture = Target->ActorHasTag(TEXT("TotoriJumpSofa"))
		|| Target->ActorHasTag(TEXT("TotoriJumpCushion"));
	TArray<FVector, TInlineAllocator<5>> Candidates;
	// The marker is the intended central landing point. Sofa approach direction
	// is constrained separately; it must not change this deep landing point.
	Candidates.Add(Marker);
	FVector SofaContactFallback = FVector::ZeroVector;

	for (const FVector& Candidate : Candidates)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByProfile(Hit, Candidate + FVector(0, 0, 400.f),
			Candidate - FVector(0, 0, 100.f), Capsule->GetCollisionProfileName(), Params)
			|| Hit.ImpactNormal.Z < 0.65f)
		{
			continue;
		}
		const FVector Center = Hit.ImpactPoint + FVector(0, 0, HalfHeight + LandingFootClearance);
		// Shrink only this placement probe by a few centimetres. The actual jump
		// still uses the full capsule sweep, while a foot-level contact with the
		// intended furniture is allowed.
		const FCollisionShape Probe = FCollisionShape::MakeCapsule(
			FMath::Max(1.f, Capsule->GetScaledCapsuleRadius() - 3.f),
			FMath::Max(2.f, HalfHeight - 3.f));
		if (!GetWorld()->OverlapBlockingTestByProfile(Center, FQuat::Identity,
			Capsule->GetCollisionProfileName(), Probe, Params))
		{
			OutCenter = Center + FVector(0.f, 0.f, bSoftFurniture ? SoftFurnitureLandingSink : 0.f);
			return true;
		}
		// The sofa itself is the deliberate landing surface. Keep a front-side
		// fallback only; this never permits the rear/backrest side.
		if (Target->ActorHasTag(TEXT("TotoriJumpSofa")) && Hit.GetActor() == Target
			&& SofaContactFallback.IsNearlyZero())
		{
			SofaContactFallback = Center;
		}
	}
	if (!SofaContactFallback.IsNearlyZero())
	{
		OutCenter = SofaContactFallback + FVector(0.f, 0.f, SoftFurnitureLandingSink);
		return true;
	}

	// Bed and sofa collision is authored differently from the cushion: on this
	// map their visible top can be absent from the capsule trace channel. The
	// marker planes were placed on the intended top surfaces, so retain a
	// constrained, camera-facing fallback instead of rejecting those jumps.
	if (Target->ActorHasTag(TEXT("TotoriJumpBed")))
	{
		OutCenter = Marker + FVector(0.f, 0.f, HalfHeight + LandingFootClearance);
		UE_LOG(LogTotoriJump, Log, TEXT("Using baked bed landing fallback at %s"), *OutCenter.ToCompactString());
		return true;
	}
	if (Target->ActorHasTag(TEXT("TotoriJumpSofa")))
	{
		OutCenter = Marker + FVector(0.f, 0.f, HalfHeight + LandingFootClearance + SoftFurnitureLandingSink);
		UE_LOG(LogTotoriJump, Log, TEXT("Using baked central-sofa landing fallback at %s"), *OutCenter.ToCompactString());
		return true;
	}

	UE_LOG(LogTotoriJump, Warning, TEXT("No supported jump landing near %s"), *Marker.ToCompactString());
	return false;
}

bool ATotoriAICharacter::FindClearJumpArc(const FVector& From, const FVector& To, float& OutHeight) const
{
	if (FVector::Dist2D(From, To) > 500.f) return false;
	for (float Height : {80.f, 120.f, 180.f, 240.f, 320.f})
	{
		bool bClear = true;
		for (int32 Step = 1; Step <= 80; ++Step)
		{
			const FVector Next = ArcPoint(From, To, Height, Step / 80.f);
			// Jumping deliberately ignores actor collision. The only hard spatial
			// restriction during the arc is the fixed activity/frustum boundary.
			if (!IsInsideActivityArea(Next))
			{
				bClear = false;
				break;
			}
		}
		if (bClear) { OutHeight = Height; return true; }
	}
	return false;
}

bool ATotoriAICharacter::FindJumpApproach(AActor* Target, FVector& OutGoal) const
{
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!Nav || !IsValid(Target)) return false;
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector Landing;
	if (!GetSupportedLanding(Target, Landing)) return false;
	// All furniture uses one near, camera-facing take-off location. Projecting
	// that location and validating the complete path prevents any off-NavMesh
	// shortcut while preserving the sofa's "never from the back" rule.
	FVector NearCandidate = Landing + FVector(-175.f, 0.f, 0.f);
	NearCandidate.Z = GetActorLocation().Z - HalfHeight;
	FNavLocation NearNav;
	if (!Nav->ProjectPointToNavigation(NearCandidate, NearNav, FVector(120.f, 120.f, 100.f))
		|| !IsInsideActivityArea(NearNav.Location)) return false;
	UNavigationPath* Path = Nav->FindPathToLocationSynchronously(GetWorld(), GetActorLocation(), NearNav.Location, const_cast<ATotoriAICharacter*>(this));
	if (!Path || !Path->IsValid() || Path->IsPartial()) return false;
	for (const FVector& Point : Path->PathPoints)
	{
		if (!IsInsideActivityArea(Point)) return false;
	}
	OutGoal = NearNav.Location;
	return true;
}

bool ATotoriAICharacter::TryStartFurnitureJump()
{
	FVector Feet;
	if (!JumpAnimation || !GetFurnitureLanding(ObservationTarget, Feet)) return false;
	if (ObservationTarget->ActorHasTag(TEXT("TotoriJumpSofa")) && GetActorLocation().X > Feet.X - 80.f) return false;
	FVector Destination;
	if (!GetSupportedLanding(ObservationTarget, Destination)) return false;
	float Height, ReturnHeight;
	if (!FindClearJumpArc(GetActorLocation(), Destination, Height)
		|| !FindClearJumpArc(Destination, GetActorLocation(), ReturnHeight))
	{
		UE_LOG(LogTotoriJump, Warning, TEXT("Jump skipped: blocked outbound/return arc for %s"), *GetNameSafe(ObservationTarget));
		return false;
	}
	JumpOrigin = GetActorLocation();
	BeginJumpArc(Destination, Height, false);
	return true;
}

bool ATotoriAICharacter::DebugJumpToFurniture(FName FurnitureTag)
{
	if (JumpPhase != EJumpPhase::None)
	{
		UE_LOG(LogTotoriJump, Warning, TEXT("Debug jump ignored: another jump is already active."));
		return false;
	}

	TArray<AActor*> Targets;
	UGameplayStatics::GetAllActorsWithTag(this, FurnitureTag, Targets);
	AActor* Target = Targets.IsEmpty() ? nullptr : Targets[0];
	FVector ApproachGoal;
	if (!IsValid(Target) || !FindJumpApproach(Target, ApproachGoal))
	{
		UE_LOG(LogTotoriJump, Warning, TEXT("Debug jump F-key failed: no safe approach for tag %s."), *FurnitureTag.ToString());
		return false;
	}

	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector ApproachCenter = ApproachGoal + FVector(0.f, 0.f, HalfHeight + LandingFootClearance);
	FHitResult Hit;
	// This is a test-only relocation to an already NavMesh-projected approach
	// point. Do not sweep it: the sofa arm can overlap the full capsule while
	// the following validated jump arc is still clear.
	SetActorLocation(ApproachCenter, false, &Hit);

	ObservationTarget = Target;
	if (!TryStartFurnitureJump())
	{
		UE_LOG(LogTotoriJump, Warning, TEXT("Debug jump F-key failed: arc to %s is blocked."), *GetNameSafe(Target));
		return false;
	}
	UE_LOG(LogTotoriJump, Log, TEXT("Debug jump started for %s."), *GetNameSafe(Target));
	return true;
}

void ATotoriAICharacter::SetPerchedVisualOffset(bool bPerched)
{
	if (bPerched == bPerchedMeshOffsetApplied || !GetMesh()) return;
	if (bPerched)
	{
		AppliedPerchedMeshOffset = PerchedMeshVisualOffset;
		if (ObservationTarget && (ObservationTarget->ActorHasTag(TEXT("TotoriJumpSofa"))
			|| ObservationTarget->ActorHasTag(TEXT("TotoriJumpCushion"))))
		{
			AppliedPerchedMeshOffset += SoftFurnitureAdditionalVisualOffset;
		}
		GetMesh()->AddLocalOffset(FVector(0.f, 0.f, AppliedPerchedMeshOffset));
	}
	else
	{
		GetMesh()->AddLocalOffset(FVector(0.f, 0.f, -AppliedPerchedMeshOffset));
		AppliedPerchedMeshOffset = 0.f;
	}
	bPerchedMeshOffsetApplied = bPerched;
}

void ATotoriAICharacter::BeginJumpArc(const FVector& Destination, float Height, bool bReturning)
{
	SetPerchedVisualOffset(false);
	JumpPhase = bReturning ? EJumpPhase::Returning : EJumpPhase::Outbound;
	bWandering = false;
	bApproachingObservation = false;
	bCanObserve = false;
	GetWorldTimerManager().ClearTimer(NextActionTimer);
	GetWorldTimerManager().ClearTimer(ObservationTimer);
	GetWorldTimerManager().ClearTimer(CooldownTimer);
	if (AAIController* AI = GetTotoriAIController())
	{
		AI->ClearFocus(EAIFocusPriority::Gameplay);
		AI->StopMovement();
	}
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	JumpStart = GetActorLocation();
	JumpEnd = Destination;
	JumpArcHeight = Height;
	JumpElapsed = 0.f;
	JumpLastUpdateTime = GetWorld()->GetTimeSeconds();
	const UAnimSequenceBase* Clip = Cast<UAnimSequenceBase>(JumpAnimation);
	JumpDuration = Clip ? FMath::Max(0.5f, Clip->GetPlayLength()) : 1.f;
	SetActorRotation(FRotator(0, (JumpEnd - JumpStart).Rotation().Yaw, 0));
	// Jump is a non-looping, clip-timed animation. Play it directly so its
	// takeoff/landing frames remain exactly aligned with the physical arc.
	GetMesh()->PlayAnimation(JumpAnimation, false);
	CurrentAnimation = nullptr;
	GetWorldTimerManager().SetTimer(JumpUpdateTimer, this, &ATotoriAICharacter::UpdateFurnitureJump, 1.f / 60.f, true);
	UE_LOG(LogTotoriJump, Log, TEXT("%s jump: %s -> %s height=%.1f duration=%.2f"),
		bReturning ? TEXT("Return") : TEXT("Outbound"), *JumpStart.ToCompactString(), *JumpEnd.ToCompactString(), Height, JumpDuration);
}

void ATotoriAICharacter::UpdateFurnitureJump()
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float Delta = Now - JumpLastUpdateTime;
	JumpLastUpdateTime = Now;
	if (JumpPhase == EJumpPhase::Turning)
	{
		const FRotator Desired(0, PerchYaw, 0);
		SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Desired, Delta, TurnRateDegreesPerSecond));
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, PerchYaw)) < 1.f)
		{
			JumpPhase = EJumpPhase::Waiting;
			// The walk cycle is only used while turning in place. Once the turn is
			// complete, settle into the normal perch idle pose.
			PlayLoopingAnimation(IdleAnimation);
			GetWorldTimerManager().ClearTimer(JumpUpdateTimer);
			GetWorldTimerManager().SetTimer(PerchTimer, this, &ATotoriAICharacter::ReturnFromFurniture, PerchWaitSeconds, false);
		}
		return;
	}
	// Small swept substeps also handle hitches and objects entering the arc.
	const float FlightStartAlpha = FMath::Clamp(JumpTakeoffFrame / FMath::Max(1.f, JumpAnimationFrameCount), 0.f, 1.f);
	const float FlightEndAlpha = FMath::Clamp(JumpLandingFrame / FMath::Max(JumpTakeoffFrame + 1.f, JumpAnimationFrameCount), FlightStartAlpha + KINDA_SMALL_NUMBER, 1.f);
	const auto ToFlightAlpha = [FlightStartAlpha, FlightEndAlpha](float ClipAlpha)
	{
		return FMath::Clamp((ClipAlpha - FlightStartAlpha) / (FlightEndAlpha - FlightStartAlpha), 0.f, 1.f);
	};

	const float EndTime = FMath::Min(JumpElapsed + Delta, JumpDuration);
	while (JumpElapsed < EndTime)
	{
		const float PreviousFlightAlpha = ToFlightAlpha(JumpElapsed / JumpDuration);
		JumpElapsed = FMath::Min(JumpElapsed + 1.f / 120.f, EndTime);
		const float NewFlightAlpha = ToFlightAlpha(JumpElapsed / JumpDuration);
		// The clip's run-up and recovery frames intentionally keep the actor in
		// place. Only frames 48--63 carry the physical jump trajectory.
		if (NewFlightAlpha <= PreviousFlightAlpha + KINDA_SMALL_NUMBER) continue;
		const FVector Next = ArcPoint(JumpStart, JumpEnd, JumpArcHeight, NewFlightAlpha);
		SetActorLocation(Next, false);
	}
	if (JumpElapsed < JumpDuration) return;
	if (JumpPhase == EJumpPhase::Returning) { ResumeAfterFurniture(); return; }
	JumpPhase = EJumpPhase::Turning;
	PerchYaw = FRotator::NormalizeAxis(GetActorRotation().Yaw + 180.f);
	SetPerchedVisualOffset(true);
	PlayLoopingAnimation(WalkAnimation);
	UE_LOG(LogTotoriJump, Log, TEXT("Landed; turning 180 degrees, then waiting %.1fs"), PerchWaitSeconds);
}

void ATotoriAICharacter::ReturnFromFurniture()
{
	SetPerchedVisualOffset(false);
	float Height;
	if (!FindClearJumpArc(GetActorLocation(), JumpOrigin, Height))
	{
		UE_LOG(LogTotoriJump, Warning, TEXT("Return blocked; remain perched and retry in 1s"));
		GetWorldTimerManager().SetTimer(PerchTimer, this, &ATotoriAICharacter::ReturnFromFurniture, 1.f, false);
		return;
	}
	BeginJumpArc(JumpOrigin, Height, true);
}

void ATotoriAICharacter::ResumeAfterFurniture()
{
	SetPerchedVisualOffset(false);
	if (GetCharacterMovement()->IsFalling())
	{
		GetWorldTimerManager().SetTimer(PerchTimer, this, &ATotoriAICharacter::ResumeAfterFurniture, 0.2f, false);
		return;
	}
	GetWorldTimerManager().ClearTimer(JumpUpdateTimer);
	JumpPhase = EJumpPhase::None;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	ObservationTarget = nullptr;
	GetWorldTimerManager().SetTimer(CooldownTimer, this, &ATotoriAICharacter::EndObservationCooldown, ObserveCooldown, false);
	UE_LOG(LogTotoriJump, Log, TEXT("Jump complete: returned to %s"), *GetActorLocation().ToCompactString());
	Wander();
}
