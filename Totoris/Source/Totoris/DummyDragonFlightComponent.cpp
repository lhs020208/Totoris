#include "DummyDragonFlightComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "SceneView.h"

UDummyDragonFlightComponent::UDummyDragonFlightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UDummyDragonFlightComponent::InitializeFlight(AActor* InWindowPlane)
{
	AActor* Dragon = GetOwner();
	if (!IsValid(Dragon) || !IsValid(InWindowPlane) || bInitialized)
	{
		return;
	}

	if (USceneComponent* Root = Dragon->GetRootComponent())
	{
		Root->SetMobility(EComponentMobility::Movable);
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Root))
		{
			Primitive->SetSimulatePhysics(false);
			Primitive->SetEnableGravity(false);
		}
	}

	InitialTransform = Dragon->GetActorTransform();
	const FVector Start = InitialTransform.GetLocation();
	const FVector ToWindow = (InWindowPlane->GetActorLocation() - Start).GetSafeNormal();
	// The window is a textured plane, not an opening. Stop slightly in front of
	// it, then shrink/fade so the dragon appears to pass through rather than
	// intersecting its surface.
	WindowTarget = InWindowPlane->GetActorLocation() - ToWindow * 75.f;
	const FVector Side = FVector::CrossProduct(ToWindow, FVector::UpVector).GetSafeNormal();
	// The first three points form a stable lane across the gameplay board.  The
	// final control point then pulls the dragon around once before it reaches
	// the window, matching the intentional broad S bend rather than oscillating.
	FlightPathPoints =
	{
		Start,
		Start + ToWindow * 600.f,
		Start + ToWindow * 1100.f,
		WindowTarget - ToWindow * 550.f - Side * 180.f,
		WindowTarget - ToWindow * 240.f + Side * 140.f,
		WindowTarget
	};
	PrepareDynamicMaterials();
	bInitialized = true;
	Dragon->SetActorHiddenInGame(true);
}

void UDummyDragonFlightComponent::PlayFlight()
{
	AActor* Dragon = GetOwner();
	if (!bInitialized || !IsValid(Dragon))
	{
		return;
	}

	FlightElapsedSeconds = 0.f;
	bPlaying = true;
	Dragon->SetActorHiddenInGame(false);
	Dragon->SetActorTransform(InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);
	ApplyOpacity(1.f);
}

void UDummyDragonFlightComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Dragon = GetOwner();
	if (!bPlaying || !IsValid(Dragon))
	{
		return;
	}

	FlightElapsedSeconds += FMath::Max(0.f, DeltaTime);
	// Finish the entire route before starting the separate window dissolve.
	const float ApproachSeconds = FlightDurationSeconds - FadeDurationSeconds;
	const float Alpha = FMath::Clamp(FlightElapsedSeconds / ApproachSeconds, 0.f, 1.f);
	const FVector Location = KeepInCameraView(EvaluateFlight(Alpha));
	const FVector Velocity = (KeepInCameraView(EvaluateFlight(FMath::Min(Alpha + .001f, 1.f)))
		- KeepInCameraView(EvaluateFlight(FMath::Max(Alpha - .001f, 0.f)))).GetSafeNormal();
	const float FadeAlpha = FMath::Clamp(
		(FlightElapsedSeconds - ApproachSeconds) / FadeDurationSeconds, 0.f, 1.f);
	// Keep the authored facing direction intact.  Only bank around the local X
	// axis when the route actually turns; there is no continuous wobble and no
	// pitch/yaw steering while the dragon flies straight through the board.
	FRotator Rotation = InitialTransform.Rotator();
	const FVector BaseDirection = (WindowTarget - InitialTransform.GetLocation()).GetSafeNormal2D();
	const FVector TravelDirection = Velocity.GetSafeNormal2D();
	if (!BaseDirection.IsNearlyZero() && !TravelDirection.IsNearlyZero())
	{
		const float SignedTurnDegrees = FMath::RadiansToDegrees(FMath::Atan2(
			FVector::CrossProduct(BaseDirection, TravelDirection).Z,
			FVector::DotProduct(BaseDirection, TravelDirection)));
		Rotation.Roll += FMath::Clamp(SignedTurnDegrees * .65f, -35.f, 35.f);
	}
	// The window is a flat plane.  During the dissolve, bank into a negative-X
	// roll so the dragon reads as turning left and continuing beyond it.
	Rotation.Roll -= FMath::InterpEaseInOut(0.f, 48.f, FadeAlpha, 2.f);
	const float ScaleFactor = FMath::Lerp(1.f, .04f, FadeAlpha);
	Dragon->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Dragon->SetActorScale3D(InitialTransform.GetScale3D() * ScaleFactor);
	ApplyOpacity(1.f - FadeAlpha);

	if (FadeAlpha >= 1.f)
	{
		bPlaying = false;
		ApplyOpacity(0.f);
		Dragon->SetActorHiddenInGame(true);
		// Keep the authored transform ready for the next event invocation.
		Dragon->SetActorTransform(InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

FVector UDummyDragonFlightComponent::KeepInCameraView(const FVector& Location) const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
	FSceneViewProjectionData Projection;
	if (!Player || !Player->ViewportClient || !Player->ViewportClient->Viewport
		|| !Player->GetProjectionData(Player->ViewportClient->Viewport, Projection))
	{
		return Location;
	}
	const FMatrix ViewProjection = Projection.ComputeViewProjectionMatrix();
	FVector4 Clip = ViewProjection.TransformFVector4(FVector4(Location, 1.0));
	// The authored entrance begins behind the camera. Once in front of its
	// near plane, keep the center inside the actual projection (including FOV
	// and aspect-ratio changes), preserving its distance along the view axis.
	if (Clip.W <= 0.0 || Clip.Z > Clip.W)
	{
		return Location;
	}
	const double Limit = Clip.W * .94;
	Clip.X = FMath::Clamp(Clip.X, -Limit, Limit);
	Clip.Y = FMath::Clamp(Clip.Y, -Limit, Limit);
	const FVector4 World = ViewProjection.Inverse().TransformFVector4(Clip);
	return FMath::Abs(World.W) > UE_SMALL_NUMBER
		? FVector(World.X, World.Y, World.Z) / World.W : Location;
}

FVector UDummyDragonFlightComponent::EvaluateFlight(float Alpha) const
{
	if (FlightPathPoints.Num() < 2)
	{
		return InitialTransform.GetLocation();
	}

	const float PathAlpha = FMath::Clamp(Alpha, 0.f, 1.f) * (FlightPathPoints.Num() - 1);
	const int32 Segment = FMath::Min(FMath::FloorToInt(PathAlpha), FlightPathPoints.Num() - 2);
	const float T = PathAlpha - Segment;
	const FVector& P0 = FlightPathPoints[FMath::Max(Segment - 1, 0)];
	const FVector& P1 = FlightPathPoints[Segment];
	const FVector& P2 = FlightPathPoints[Segment + 1];
	const FVector& P3 = FlightPathPoints[FMath::Min(Segment + 2, FlightPathPoints.Num() - 1)];
	const float T2 = T * T;
	const float T3 = T2 * T;
	return .5f * ((2.f * P1) + (-P0 + P2) * T
		+ (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T2
		+ (-P0 + 3.f * P1 - 3.f * P2 + P3) * T3);
}

FVector UDummyDragonFlightComponent::EvaluateFlightVelocity(float Alpha) const
{
	if (FlightPathPoints.Num() < 2)
	{
		return FVector::ForwardVector;
	}

	const float PathAlpha = FMath::Clamp(Alpha, 0.f, 1.f) * (FlightPathPoints.Num() - 1);
	const int32 Segment = FMath::Min(FMath::FloorToInt(PathAlpha), FlightPathPoints.Num() - 2);
	const float T = PathAlpha - Segment;
	const FVector& P0 = FlightPathPoints[FMath::Max(Segment - 1, 0)];
	const FVector& P1 = FlightPathPoints[Segment];
	const FVector& P2 = FlightPathPoints[Segment + 1];
	const FVector& P3 = FlightPathPoints[FMath::Min(Segment + 2, FlightPathPoints.Num() - 1)];
	const float T2 = T * T;
	return .5f * ((-P0 + P2)
		+ 2.f * (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T
		+ 3.f * (-P0 + 3.f * P1 - 3.f * P2 + P3) * T2);
}

void UDummyDragonFlightComponent::PrepareDynamicMaterials()
{
	AActor* Dragon = GetOwner();
	if (!IsValid(Dragon))
	{
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> Components(Dragon);
	for (UPrimitiveComponent* Primitive : Components)
	{
		for (int32 MaterialIndex = 0; MaterialIndex < Primitive->GetNumMaterials(); ++MaterialIndex)
		{
			if (UMaterialInstanceDynamic* DynamicMaterial = Primitive->CreateDynamicMaterialInstance(MaterialIndex))
			{
				FlightMaterials.Add(DynamicMaterial);
			}
		}
	}
}

void UDummyDragonFlightComponent::ApplyOpacity(float Opacity)
{
	for (UMaterialInstanceDynamic* Material : FlightMaterials)
	{
		if (IsValid(Material))
		{
			// The final dragon material should expose EventOpacity. Opacity is also
			// written for material variants that use the conventional name.
			Material->SetScalarParameterValue(TEXT("EventOpacity"), Opacity);
			Material->SetScalarParameterValue(TEXT("Opacity"), Opacity);
		}
	}
}
