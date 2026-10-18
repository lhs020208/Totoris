#include "SmilemoSitoJumpComponent.h"
#include "TotorisEventAudio.h"

#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TotorisAudioMix.h"

USmilemoSitoJumpComponent::USmilemoSitoJumpComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SitoSpinLineClearSound = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/Totoris/Audio/Gameplay/SitoSpinLineClearSound.SitoSpinLineClearSound"));
}

void USmilemoSitoJumpComponent::InitializeRoute(const TArray<FVector>& InLandingPoints)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || bRouteInitialized || InLandingPoints.Num() == 0)
	{
		return;
	}

	// The actor is authored as a StaticMeshActor.  Do not leave its transform
	// under physics control: a running body can overwrite SetActorLocation on
	// the next physics update while the route (and its landing sounds) still
	// advances normally.
	if (USceneComponent* OwnerRoot = Owner->GetRootComponent())
	{
		OwnerRoot->SetMobility(EComponentMobility::Movable);
		if (UPrimitiveComponent* PrimitiveRoot = Cast<UPrimitiveComponent>(OwnerRoot))
		{
			PrimitiveRoot->SetSimulatePhysics(false);
			PrimitiveRoot->SetEnableGravity(false);
		}
	}

	InitialTransform = Owner->GetActorTransform();
	LandingPoints.Reset(InLandingPoints.Num() + 1);
	LandingPoints.Add(InitialTransform.GetLocation());
	LandingPoints.Append(InLandingPoints);
	BuildSegments();
	bRouteInitialized = Segments.Num() > 0;
}

void USmilemoSitoJumpComponent::StopEvent()
{
	TotorisEventAudio::Stop(this);
	bPlaying = false;
	bBossFinalAdvance = false;
	SetComponentTickEnabled(false);
	if (GetOwner()) { GetOwner()->SetActorTransform(InitialTransform); GetOwner()->SetActorHiddenInGame(true); }
	if (BossActor.IsValid()) { BossActor->SetActorTransform(BossInitialTransform); BossActor->SetActorHiddenInGame(true); }
}

void USmilemoSitoJumpComponent::PlayJumpRoute()
{
	SetComponentTickEnabled(true);
	if (GetOwner()) GetOwner()->SetActorHiddenInGame(false);
	if (BossActor.IsValid()) BossActor->SetActorHiddenInGame(false);
	AActor* Owner = GetOwner();
	if (!bRouteInitialized || !IsValid(Owner))
	{
		return;
	}

	CurrentSegmentIndex = 0;
	CurrentSegmentElapsed = 0.f;
	bPlaying = true;
	Owner->SetActorTransform(InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);
	ApplyFacing(InitialTransform.GetLocation());
	ResetBossFollower();
}

void USmilemoSitoJumpComponent::InitializeBossFollower(AActor* InBossActor)
{
	if (!bRouteInitialized || !IsValid(InBossActor))
	{
		return;
	}

	BossActor = InBossActor;
	BossInitialTransform = InBossActor->GetActorTransform();
	bBossInitialized = true;
	if (USceneComponent* BossRoot = InBossActor->GetRootComponent())
	{
		BossRoot->SetMobility(EComponentMobility::Movable);
	}
}

void USmilemoSitoJumpComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!bPlaying || !IsValid(Owner) || !Segments.IsValidIndex(CurrentSegmentIndex))
	{
		if (bBossFinalAdvance)
		{
			UpdateBossFollower(DeltaTime);
		}
		return;
	}

	float RemainingDelta = FMath::Max(0.f, DeltaTime);
	while (bPlaying && Segments.IsValidIndex(CurrentSegmentIndex))
	{
		const FJumpSegment& Segment = Segments[CurrentSegmentIndex];
		const float TimeRemaining = FMath::Max(0.f, Segment.Duration - CurrentSegmentElapsed);
		const float Advance = FMath::Min(RemainingDelta, TimeRemaining);
		CurrentSegmentElapsed += Advance;
		RemainingDelta -= Advance;

		const float Distance = Segment.TotalArcLength * FMath::Clamp(CurrentSegmentElapsed / Segment.Duration, 0.f, 1.f);
		const float CurveAlpha = FindCurveAlphaAtDistance(Segment, Distance);
		const FVector NewLocation = EvaluateSegment(Segment, CurveAlpha);
		// Apply on the root itself.  This keeps the presentation actor responsive
		// even if an actor-level movement update is queued during PIE startup.
		if (USceneComponent* OwnerRoot = Owner->GetRootComponent())
		{
			OwnerRoot->SetWorldLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
		}
		else
		{
			Owner->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
		}
		ApplyFacing(NewLocation);

		if (CurrentSegmentElapsed + KINDA_SMALL_NUMBER < Segment.Duration)
		{
			break;
		}

		const bool bPlayLandingSound = IsDownwardToUpwardTransition(CurrentSegmentIndex);
		const FVector LandingLocation = Segment.End;
		++CurrentSegmentIndex;
		if (bPlayLandingSound && SitoSpinLineClearSound)
		{
			TotorisEventAudio::PlaySoundAtLocation(this, SitoSpinLineClearSound, LandingLocation,
				TotorisAudioMix::SitoJumpLanding);
		}
		CurrentSegmentElapsed = 0.f;
		if (!Segments.IsValidIndex(CurrentSegmentIndex))
		{
			bPlaying = false;
			if (USceneComponent* OwnerRoot = Owner->GetRootComponent())
			{
				OwnerRoot->SetWorldLocation(Segment.End, false, nullptr, ETeleportType::TeleportPhysics);
			}
			else
			{
				Owner->SetActorLocation(Segment.End, false, nullptr, ETeleportType::TeleportPhysics);
			}
			ApplyFacing(Segment.End);
		}

		if (RemainingDelta <= KINDA_SMALL_NUMBER)
		{
			break;
		}
	}

	UpdateBossFollower(DeltaTime);
}

void USmilemoSitoJumpComponent::BuildSegments()
{
	Segments.Reset();
	for (int32 Index = 0; Index + 1 < LandingPoints.Num(); ++Index)
	{
		FJumpSegment& Segment = Segments.AddDefaulted_GetRef();
		Segment.Start = LandingPoints[Index];
		Segment.End = LandingPoints[Index + 1];
		Segment.ArcLengths.SetNumZeroed(ArcLengthSamples + 1);

		FVector Previous = EvaluateSegment(Segment, 0.f);
		for (int32 Sample = 1; Sample <= ArcLengthSamples; ++Sample)
		{
			const FVector Current = EvaluateSegment(Segment, static_cast<float>(Sample) / ArcLengthSamples);
			const FVector2D PreviousXY(Previous.X, Previous.Y);
			const FVector2D CurrentXY(Current.X, Current.Y);
			Segment.TotalArcLength += FVector2D::Distance(PreviousXY, CurrentXY);
			Segment.ArcLengths[Sample] = Segment.TotalArcLength;
			Previous = Current;
		}

		Segment.Duration = FMath::Max(MinimumSegmentDuration, Segment.TotalArcLength / XYUnitsPerSecond);
	}
}

FVector USmilemoSitoJumpComponent::EvaluateSegment(const FJumpSegment& Segment, float CurveAlpha) const
{
	const float T = FMath::Clamp(CurveAlpha, 0.f, 1.f);
	const FVector BaseLocation = FMath::Lerp(Segment.Start, Segment.End, T);
	const float XYDistance = FVector2D::Distance(FVector2D(Segment.Start.X, Segment.Start.Y), FVector2D(Segment.End.X, Segment.End.Y));
	const float ZTravel = FMath::Abs(Segment.End.Z - Segment.Start.Z);
	// Unreal's vertical axis is Z.  Scale the arch from the authored planar
	// travel so the compact 1->2 and 2->3 hops remain visibly airborne.
	const float HopHeight = FMath::Clamp(XYDistance * .28f + ZTravel * .45f, 140.f, 360.f);
	return BaseLocation + FVector(0.f, 0.f, 4.f * HopHeight * T * (1.f - T));
}

float USmilemoSitoJumpComponent::GetZVelocityAt(const FJumpSegment& Segment, float CurveAlpha) const
{
	const float T = FMath::Clamp(CurveAlpha, 0.f, 1.f);
	const float XYDistance = FVector2D::Distance(FVector2D(Segment.Start.X, Segment.Start.Y), FVector2D(Segment.End.X, Segment.End.Y));
	const float ZTravel = FMath::Abs(Segment.End.Z - Segment.Start.Z);
	const float HopHeight = FMath::Clamp(XYDistance * .28f + ZTravel * .45f, 140.f, 360.f);
	return (Segment.End.Z - Segment.Start.Z) + 4.f * HopHeight * (1.f - 2.f * T);
}

bool USmilemoSitoJumpComponent::IsDownwardToUpwardTransition(int32 CompletedSegmentIndex) const
{
	if (!Segments.IsValidIndex(CompletedSegmentIndex) || !Segments.IsValidIndex(CompletedSegmentIndex + 1))
	{
		return false;
	}

	return GetZVelocityAt(Segments[CompletedSegmentIndex], 1.f) < 0.f
		&& GetZVelocityAt(Segments[CompletedSegmentIndex + 1], 0.f) > 0.f;
}

float USmilemoSitoJumpComponent::FindCurveAlphaAtDistance(const FJumpSegment& Segment, float Distance) const
{
	if (Segment.TotalArcLength <= KINDA_SMALL_NUMBER || Segment.ArcLengths.Num() < 2)
	{
		return 1.f;
	}

	const float ClampedDistance = FMath::Clamp(Distance, 0.f, Segment.TotalArcLength);
	for (int32 Sample = 1; Sample < Segment.ArcLengths.Num(); ++Sample)
	{
		if (Segment.ArcLengths[Sample] >= ClampedDistance)
		{
			const float PreviousDistance = Segment.ArcLengths[Sample - 1];
			const float Span = FMath::Max(KINDA_SMALL_NUMBER, Segment.ArcLengths[Sample] - PreviousDistance);
			const float WithinSample = (ClampedDistance - PreviousDistance) / Span;
			return ((Sample - 1) + WithinSample) / ArcLengthSamples;
		}
	}

	return 1.f;
}

float USmilemoSitoJumpComponent::GetRouteDistance() const
{
	float Distance = 0.f;
	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const FJumpSegment& Segment = Segments[Index];
		if (Index == CurrentSegmentIndex && bPlaying)
		{
			return Distance + Segment.TotalArcLength * FMath::Clamp(CurrentSegmentElapsed / Segment.Duration, 0.f, 1.f);
		}
		Distance += Segment.TotalArcLength;
	}
	return Distance;
}

float USmilemoSitoJumpComponent::GetTotalRouteDistance() const
{
	float TotalDistance = 0.f;
	for (const FJumpSegment& Segment : Segments)
	{
		TotalDistance += Segment.TotalArcLength;
	}
	return TotalDistance;
}

FVector USmilemoSitoJumpComponent::GetRouteLocationAtDistance(float Distance) const
{
	if (Segments.Num() == 0)
	{
		return InitialTransform.GetLocation();
	}

	if (Distance < 0.f)
	{
		const FVector& Start = Segments[0].Start;
		const FVector2D FirstDirection = FVector2D(Segments[0].End.X - Start.X, Segments[0].End.Y - Start.Y).GetSafeNormal();
		return FVector(Start.X + FirstDirection.X * Distance, Start.Y + FirstDirection.Y * Distance, Start.Z);
	}

	float RemainingDistance = Distance;
	for (const FJumpSegment& Segment : Segments)
	{
		if (RemainingDistance <= Segment.TotalArcLength)
		{
			return EvaluateSegment(Segment, FindCurveAlphaAtDistance(Segment, RemainingDistance));
		}
		RemainingDistance -= Segment.TotalArcLength;
	}

	const FJumpSegment& FinalSegment = Segments.Last();
	const FVector2D FinalDirection = FVector2D(FinalSegment.End.X - FinalSegment.Start.X, FinalSegment.End.Y - FinalSegment.Start.Y).GetSafeNormal();
	return FVector(FinalSegment.End.X + FinalDirection.X * RemainingDistance, FinalSegment.End.Y + FinalDirection.Y * RemainingDistance, FinalSegment.End.Z);
}

FVector USmilemoSitoJumpComponent::GetBossOffsetLocation(float RouteDistance) const
{
	FVector Location = GetRouteLocationAtDistance(RouteDistance);
	// Pull every boss-only route point 100cm inward toward the look target. This
	// leaves clearance from the room walls without changing Sito's authored path.
	const FVector2D ToLookTarget(-48000.f - Location.X, -14149.f - Location.Y);
	const FVector2D InwardOffset = ToLookTarget.GetSafeNormal() * 100.f;
	Location.X += InwardOffset.X;
	Location.Y += InwardOffset.Y;
	return Location;
}

void USmilemoSitoJumpComponent::ResetBossFollower()
{
	if (!bBossInitialized || !BossActor.IsValid())
	{
		return;
	}

	bBossFinalAdvance = false;
	BossRouteDistance = -BossFollowDistance;
	const FVector StartLocation = GetBossOffsetLocation(BossRouteDistance);
	BossActor->SetActorLocation(FVector(StartLocation.X, StartLocation.Y, BossInitialTransform.GetLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
	BossActor->SetActorRotation(MakeFacingRotation(StartLocation), ETeleportType::TeleportPhysics);
}

void USmilemoSitoJumpComponent::UpdateBossFollower(float DeltaTime)
{
	if (!bBossInitialized || !BossActor.IsValid())
	{
		return;
	}

	const float TotalRouteDistance = GetTotalRouteDistance();
	if (!bPlaying)
	{
		bBossFinalAdvance = true;
		// Continue until two metres beyond SMpos4 after Sito has finished.
		BossRouteDistance = FMath::Min(TotalRouteDistance + 200.f, BossRouteDistance + BossXYUnitsPerSecond * FMath::Max(0.f, DeltaTime));
	}
	else
	{
		const float DesiredFollowDistance = GetRouteDistance() - BossFollowDistance;
		// The boss may trail farther when Sito moves quickly, but never catches up
		// past the requested three-metre spacing.
		BossRouteDistance = FMath::Min(DesiredFollowDistance, BossRouteDistance + BossXYUnitsPerSecond * FMath::Max(0.f, DeltaTime));
	}

	const FVector RouteLocation = GetBossOffsetLocation(BossRouteDistance);
	BossActor->SetActorLocation(FVector(RouteLocation.X, RouteLocation.Y, BossInitialTransform.GetLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
	BossActor->SetActorRotation(MakeFacingRotation(RouteLocation), ETeleportType::TeleportPhysics);
}

FRotator USmilemoSitoJumpComponent::MakeFacingRotation(const FVector& Location) const
{
	// Actor details expose these axes as X=Roll, Y=Pitch, Z=Yaw. Keep X/Y
	// locked as requested and only turn Z toward the authored XY look target.
	const FVector2D ToLookTarget(-48000.f - Location.X, -14149.f - Location.Y);
	const float ZRotation = FMath::RadiansToDegrees(FMath::Atan2(ToLookTarget.Y, ToLookTarget.X));
	// The plane's authored forward direction is reversed and side-on from the
	// mathematical XY direction. Compensate both offsets while retaining X=90,Y=0.
	return FRotator(0.f, ZRotation + 270.f, 90.f);
}

void USmilemoSitoJumpComponent::ApplyFacing(const FVector& Location)
{
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorRotation(MakeFacingRotation(Location), ETeleportType::TeleportPhysics);
	}
}
