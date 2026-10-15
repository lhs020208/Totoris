#include "DummyDragonFlightComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Camera/PlayerCameraManager.h"
#include "Math/RotationMatrix.h"
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
	// The imported dragon's pivot is at its feet. Keep the authored actor point
	// as the visual centre by lowering the pivot by its current bounds offset.
	FVector BoundsOrigin;
	FVector BoundsExtent;
	Dragon->GetActorBounds(false, BoundsOrigin, BoundsExtent);
	InitialTransform.AddToTranslation(FVector::UpVector *
		(InitialTransform.GetLocation().Z - BoundsOrigin.Z - AdditionalStartLowering));
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
	// Rocket through Rocket10, transformed from their authored world positions
	// into the original Dragon_Model local coordinate system. The markers are
	// intentionally not needed at runtime: this data drives the contrails.
	RocketLocalOffsets =
	{
		FVector(-0.168425f, 1.243345f, 63.877947f),
		FVector(-16.704354f, 1.243345f, 55.431721f),
		FVector(16.022401f, 1.243345f, 55.431721f),
		FVector(28.716568f, 1.243345f, 47.264947f),
		FVector(-28.937136f, 1.243345f, 47.264947f),
		FVector(-32.990596f, 1.243345f, 26.600007f),
		FVector(32.632205f, 1.243345f, 26.600007f),
		FVector(22.148602f, 1.243345f, 14.732488f),
		FVector(-22.132740f, 1.243345f, 14.732488f),
		FVector(-0.168424f, 1.243345f, 4.192756f)
	};
	CreateRocketParticlePool();
	CreateDragonAuraPool();
	CreateWingEmbers();
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
	ResetRocketTrails();
	ResetDragonAura();
	ResetWingEmbers();
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
		UpdateWingEmbers(DeltaTime, false);
		return;
	}

	FlightElapsedSeconds += FMath::Max(0.f, DeltaTime);
	// Finish the entire route before starting the separate window dissolve.
	const float Alpha = GetApproachPathAlpha(FlightElapsedSeconds);
	const FVector Location = KeepInCameraView(EvaluateFlight(Alpha));
	const FVector Velocity = (KeepInCameraView(EvaluateFlight(FMath::Min(Alpha + .001f, 1.f)))
		- KeepInCameraView(EvaluateFlight(FMath::Max(Alpha - .001f, 0.f)))).GetSafeNormal();
	const float FadeAlpha = FMath::Clamp(
		(FlightElapsedSeconds - ApproachSeconds) / FadeDurationSeconds, 0.f, 1.f);
	// Keep the authored facing direction intact. Apply the route turn only on
	// the local Y axis (Pitch); no dynamic X-axis (Roll) rotation is added.
	FRotator Rotation = InitialTransform.Rotator();
	const FVector BaseDirection = (WindowTarget - InitialTransform.GetLocation()).GetSafeNormal2D();
	const FVector TravelDirection = Velocity.GetSafeNormal2D();
	if (!BaseDirection.IsNearlyZero() && !TravelDirection.IsNearlyZero())
	{
		const float SignedTurnDegrees = FMath::RadiansToDegrees(FMath::Atan2(
			FVector::CrossProduct(BaseDirection, TravelDirection).Z,
			FVector::DotProduct(BaseDirection, TravelDirection)));
		Rotation.Pitch += FMath::Clamp(SignedTurnDegrees * .65f, -35.f, 35.f);
	}
	// The window is a flat plane. During the dissolve, turn on the same local
	// Y axis so the exit direction remains consistent with the route motion.
	Rotation.Pitch -= FMath::InterpEaseInOut(0.f, 48.f, FadeAlpha, 2.f);
	const float ScaleFactor = FMath::Lerp(1.f, .04f, FadeAlpha);
	Dragon->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Dragon->SetActorScale3D(InitialTransform.GetScale3D() * ScaleFactor);
	ApplyOpacity(1.f - FadeAlpha);
	UpdateRocketTrails(DeltaTime);
	UpdateDragonAura(DeltaTime);
	UpdateWingEmbers(DeltaTime, FadeAlpha < .85f);

	if (FadeAlpha >= 1.f)
	{
		bPlaying = false;
		ApplyOpacity(0.f);
		Dragon->SetActorHiddenInGame(true);
		// Keep the authored transform ready for the next event invocation.
		Dragon->SetActorTransform(InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void UDummyDragonFlightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(WingEmberHost))
	{
		WingEmberHost->Destroy();
	}
	WingEmbers.Reset();
	Super::EndPlay(EndPlayReason);
}

void UDummyDragonFlightComponent::CreateWingEmbers()
{
	if (!WingEmbers.IsEmpty() || !IsValid(GetOwner()))
	{
		return;
	}
	WingMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	if (!IsValid(WingMesh))
	{
		return;
	}
	// Read the completed animation pose, including when the wings are offscreen.
	AddTickPrerequisiteComponent(WingMesh);
	WingMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Totoris/Materials/Events/M_DragonWingEmber.M_DragonWingEmber"));
	if (!Plane || !Material)
	{
		return;
	}
	// Independent owner: hiding/resetting the dragon must not hide or move
	// already emitted world-space embers.
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags |= RF_Transient;
	WingEmberHost = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Spawn);
	if (!WingEmberHost)
	{
		return;
	}
	for (int32 Index = 0; Index < 128; ++Index)
	{
		UStaticMeshComponent* Quad = NewObject<UStaticMeshComponent>(WingEmberHost);
		Quad->SetMobility(EComponentMobility::Movable);
		Quad->SetStaticMesh(Plane);
		Quad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Quad->SetCastShadow(false);
		Quad->SetReceivesDecals(false);
		Quad->RegisterComponent();
		UMaterialInstanceDynamic* MID = Quad->CreateDynamicMaterialInstance(0, Material);
		Quad->SetHiddenInGame(true);
		WingEmbers.Add({Quad, MID});
	}
}

void UDummyDragonFlightComponent::ResetWingEmbers()
{
	WingEmberAccumulator = 0.f;
	for (FDragonRocketTrailParticle& Ember : WingEmbers)
	{
		Ember.bActive = false;
		if (IsValid(Ember.Component))
		{
			Ember.Component->SetHiddenInGame(true);
		}
	}
}

void UDummyDragonFlightComponent::UpdateWingEmbers(float DeltaTime, bool bEmit)
{
	if (WingEmbers.IsEmpty())
	{
		return;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const FVector Camera = PC && PC->PlayerCameraManager
		? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	for (FDragonRocketTrailParticle& Ember : WingEmbers)
	{
		if (!Ember.bActive || !IsValid(Ember.Component))
		{
			continue;
		}
		Ember.Age += FMath::Max(DeltaTime, 0.f);
		if (Ember.Age >= Ember.Lifetime)
		{
			Ember.bActive = false;
			Ember.Component->SetHiddenInGame(true);
			continue;
		}
		// Velocity has strictly zero X/Y; no dragon translation or rotation is
		// applied after emission. Use an absolute origin to avoid XY drift.
		const FVector Position = Ember.LocalOffset + Ember.Velocity * Ember.Age;
		Ember.Component->SetWorldLocation(Position);
		const FQuat Facing = FRotationMatrix::MakeFromZ((Camera - Position).GetSafeNormal()).ToQuat();
		Ember.Component->SetWorldRotation(Facing * FQuat(FVector::UpVector, Ember.Phase));
		// Hold the solid colour, then quickly alpha-fade over the final 0.22s.
		const float Fade = FMath::Clamp((Ember.Lifetime - Ember.Age) / .22f, 0.f, 1.f);
		if (IsValid(Ember.Material))
		{
			Ember.Material->SetScalarParameterValue(TEXT("ParticleOpacity"), Fade);
		}
	}
	if (!bEmit || !IsValid(WingMesh))
	{
		WingEmberAccumulator = 0.f;
		return;
	}
	// Wing membrane fans identified from the authored skeleton. Interpolate
	// between their animated bones instead of guessing a fixed world box.
	static const FName Bones[] = {TEXT("bone_14"), TEXT("bone_15"), TEXT("bone_17"),
		TEXT("bone_19"), TEXT("bone_21"), TEXT("bone_30"), TEXT("bone_31"),
		TEXT("bone_33"), TEXT("bone_35"), TEXT("bone_37")};
	FVector Points[10];
	for (int32 I = 0; I < 10; ++I)
	{
		if (WingMesh->GetBoneIndex(Bones[I]) == INDEX_NONE)
		{
			return;
		}
		Points[I] = WingMesh->GetBoneLocation(Bones[I]);
	}
	WingEmberAccumulator = FMath::Min(WingEmberAccumulator + FMath::Max(DeltaTime, 0.f), .1f);
	while (WingEmberAccumulator >= .04f)
	{
		WingEmberAccumulator -= .04f;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			for (int32 Sample = 0; Sample < 2; ++Sample)
			{
				FDragonRocketTrailParticle* Free = WingEmbers.FindByPredicate(
					[](const FDragonRocketTrailParticle& P) { return !P.bActive; });
				if (!Free)
				{
					return;
				}
				const int32 Base = Side * 5;
				const int32 Tip = FMath::RandRange(2, 3);
				const float U = FMath::Sqrt(FMath::FRand());
				const float V = FMath::FRand();
				Free->LocalOffset = Points[Base + 1] * (1.f - U)
					+ Points[Base + Tip] * (U * (1.f - V)) + Points[Base + Tip + 1] * (U * V);
				Free->Velocity = FVector(0.f, 0.f, -FMath::FRandRange(8.f, 18.f));
				Free->Age = 0.f;
				Free->Lifetime = FMath::FRandRange(.55f, .95f);
				Free->Phase = FMath::FRandRange(0.f, 2.f * PI);
				Free->bActive = true;
				Free->Component->SetWorldLocation(Free->LocalOffset);
				Free->Component->SetWorldRotation(FRotationMatrix::MakeFromZ(
					(Camera - Free->LocalOffset).GetSafeNormal()).Rotator());
				Free->Component->SetWorldScale3D(FVector(FMath::FRandRange(.045f, .085f)));
				Free->Material->SetScalarParameterValue(TEXT("ParticleOpacity"), 1.f);
				Free->Component->SetHiddenInGame(false);
			}
		}
	}
}

void UDummyDragonFlightComponent::ResetRocketTrails()
{
	PreviousRocketLocations.Reset();
	RocketParticleSpawnAccumulator = 0.f;
	bHasRocketTrailHistory = false;
	for (FDragonRocketTrailParticle& Particle : RocketTrailParticles)
	{
		Particle.bActive = false;
		if (IsValid(Particle.Component))
		{
			Particle.Component->SetHiddenInGame(true);
		}
	}
}

void UDummyDragonFlightComponent::ResetDragonAura()
{
	AuraParticleSpawnAccumulator = 0.f;
	for (FDragonRocketTrailParticle& Particle : DragonAuraParticles)
	{
		Particle.bActive = false;
		if (IsValid(Particle.Component))
		{
			Particle.Component->SetHiddenInGame(true);
		}
	}
}

void UDummyDragonFlightComponent::CreateRocketParticlePool()
{
	AActor* Dragon = GetOwner();
	if (!IsValid(Dragon) || !RocketTrailParticles.IsEmpty())
	{
		return;
	}

	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* TrailMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Totoris/Materials/Events/M_DragonTrailSoft.M_DragonTrailSoft"));
	if (!IsValid(PlaneMesh) || !IsValid(TrailMaterial))
	{
		return;
	}

	// Ten rockets emit both a tight moving glow and a briefly persistent path
	// segment.  Keep enough pooled quads that neither effect steals the other.
	RocketTrailParticles.Reserve(256);
	for (int32 Index = 0; Index < 256; ++Index)
	{
		UStaticMeshComponent* ParticleComponent = NewObject<UStaticMeshComponent>(Dragon,
			*FString::Printf(TEXT("DragonRocketTrailParticle_%02d"), Index));
		ParticleComponent->SetMobility(EComponentMobility::Movable);
		ParticleComponent->SetStaticMesh(PlaneMesh);
		ParticleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ParticleComponent->SetCastShadow(false);
		ParticleComponent->SetReceivesDecals(false);
		ParticleComponent->RegisterComponent();
		UMaterialInstanceDynamic* DynamicMaterial = ParticleComponent->CreateDynamicMaterialInstance(0, TrailMaterial);
		ParticleComponent->SetHiddenInGame(true);
		RocketTrailParticles.Add({ ParticleComponent, DynamicMaterial });
	}
}

void UDummyDragonFlightComponent::CreateDragonAuraPool()
{
	AActor* Dragon = GetOwner();
	if (!IsValid(Dragon) || !DragonAuraParticles.IsEmpty())
	{
		return;
	}

	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* AuraMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Totoris/Materials/Events/M_DragonTrailSoft.M_DragonTrailSoft"));
	if (!IsValid(PlaneMesh) || !IsValid(AuraMaterial))
	{
		return;
	}

	// A separate pool keeps the gentle body aura independent of the denser
	// rocket trails, so it remains visible even when all rockets are emitting.
	DragonAuraParticles.Reserve(192);
	for (int32 Index = 0; Index < 192; ++Index)
	{
		UStaticMeshComponent* ParticleComponent = NewObject<UStaticMeshComponent>(Dragon,
			*FString::Printf(TEXT("DragonAuraParticle_%02d"), Index));
		ParticleComponent->SetMobility(EComponentMobility::Movable);
		ParticleComponent->SetStaticMesh(PlaneMesh);
		ParticleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ParticleComponent->SetCastShadow(false);
		ParticleComponent->SetReceivesDecals(false);
		ParticleComponent->RegisterComponent();
		UMaterialInstanceDynamic* DynamicMaterial = ParticleComponent->CreateDynamicMaterialInstance(0, AuraMaterial);
		ParticleComponent->SetHiddenInGame(true);
		DragonAuraParticles.Add({ ParticleComponent, DynamicMaterial });
	}
}

void UDummyDragonFlightComponent::SpawnDragonAuraParticle(int32 ParticleIndex)
{
	if (DragonAuraParticles.IsEmpty())
	{
		return;
	}

	FDragonRocketTrailParticle* Candidate = nullptr;
	for (FDragonRocketTrailParticle& Particle : DragonAuraParticles)
	{
		if (!Particle.bActive)
		{
			Candidate = &Particle;
			break;
		}
	}
	if (!Candidate)
	{
		Candidate = &DragonAuraParticles[0];
		for (FDragonRocketTrailParticle& Particle : DragonAuraParticles)
		{
			if (Particle.Age / Particle.Lifetime > Candidate->Age / Candidate->Lifetime)
			{
				Candidate = &Particle;
			}
		}
	}

	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const float Radius = FMath::FRandRange(2.f, 11.f);
	Candidate->Age = 0.f;
	Candidate->Lifetime = FMath::FRandRange(.72f, 1.18f);
	Candidate->StartScale = 0.f;
	Candidate->BaseScale = FVector(FMath::FRandRange(.11f, .22f), FMath::FRandRange(.018f, .045f), 1.f);
	Candidate->LocalOffset = FVector(
		FMath::Cos(Angle) * Radius,
		FMath::Sin(Angle) * Radius * .42f,
		FMath::FRandRange(27.f, 34.f));
	// Fixed per-particle world-space placement: broaden the spawn distribution
	// upward without adding upward velocity or rotating the bias with the dragon.
	Candidate->AuraWorldOffset = FVector(20.f, 0.f, FMath::FRandRange(0.f, 35.f));
	Candidate->Phase = FMath::FRandRange(0.f, 2.f * PI);
	Candidate->Velocity = FVector::ZeroVector;
	Candidate->bPathStreak = false;
	Candidate->bActive = true;
	if (AActor* Dragon = GetOwner())
	{
		Candidate->Component->SetWorldLocation(Dragon->GetActorTransform().TransformPosition(Candidate->LocalOffset)
			+ Candidate->AuraWorldOffset);
	}
	Candidate->Component->SetHiddenInGame(false);
	if (IsValid(Candidate->Material))
	{
		const bool bSkyBlue = (ParticleIndex % 3) != 0;
		Candidate->Material->SetVectorParameterValue(TEXT("ParticleColor"), bSkyBlue
			? FLinearColor(2.2f, 7.5f, 11.f, 1.f)
			: FLinearColor(10.f, 7.5f, 1.2f, 1.f));
		Candidate->Material->SetScalarParameterValue(TEXT("ParticleOpacity"), .36f);
	}
}

void UDummyDragonFlightComponent::UpdateDragonAura(float DeltaTime)
{
	AActor* Dragon = GetOwner();
	if (!IsValid(Dragon))
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const FVector CameraLocation = PC && PC->PlayerCameraManager
		? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	const FTransform DragonTransform = Dragon->GetActorTransform();
	for (FDragonRocketTrailParticle& Particle : DragonAuraParticles)
	{
		if (!Particle.bActive || !IsValid(Particle.Component))
		{
			continue;
		}

		Particle.Age += DeltaTime;
		if (Particle.Age >= Particle.Lifetime)
		{
			Particle.bActive = false;
			Particle.Component->SetHiddenInGame(true);
			continue;
		}

		const float Progress = Particle.Age / Particle.Lifetime;
		const FVector FlameOffset(
			FMath::Sin(Particle.Phase + Progress * 8.f) * 1.5f,
			FMath::Cos(Particle.Phase * 1.7f + Progress * 7.f) * 1.f,
			Progress * 16.f);
		const FVector Location = DragonTransform.TransformPosition(Particle.LocalOffset + FlameOffset)
			+ Particle.AuraWorldOffset;
		Particle.Component->SetWorldLocation(Location);
		if (!CameraLocation.IsNearlyZero())
		{
			const FVector ToCamera = (CameraLocation - Location).GetSafeNormal();
			Particle.Component->SetWorldRotation(FRotationMatrix::MakeFromXZ(FVector::UpVector, ToCamera).Rotator());
		}
		Particle.Component->SetWorldScale3D(Particle.BaseScale * (1.f + Progress * .32f));
		if (IsValid(Particle.Material))
		{
			const float FlameFade = FMath::Sin(Progress * PI);
			Particle.Material->SetScalarParameterValue(TEXT("ParticleOpacity"), FlameFade * .38f);
		}
	}

	AuraParticleSpawnAccumulator += DeltaTime;
	while (AuraParticleSpawnAccumulator >= .008f)
	{
		AuraParticleSpawnAccumulator -= .008f;
		SpawnDragonAuraParticle(FMath::Rand());
	}
}

void UDummyDragonFlightComponent::UpdateRocketParticles(float DeltaTime)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const FVector CameraLocation = PC && PC->PlayerCameraManager
		? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	for (FDragonRocketTrailParticle& Particle : RocketTrailParticles)
	{
		if (!Particle.bActive || !IsValid(Particle.Component))
		{
			continue;
		}

		Particle.Age += DeltaTime;
		if (Particle.Age >= Particle.Lifetime)
		{
			Particle.bActive = false;
			Particle.Component->SetHiddenInGame(true);
			continue;
		}

		const float Fade = 1.f - Particle.Age / Particle.Lifetime;
		const FVector Location = Particle.Component->GetComponentLocation() + Particle.Velocity * DeltaTime;
		Particle.Component->SetWorldLocation(Location);
		if (!CameraLocation.IsNearlyZero())
		{
			const FVector ToCamera = (CameraLocation - Location).GetSafeNormal();
			const FVector BeamDirection = Particle.Velocity.GetSafeNormal();
			Particle.Component->SetWorldRotation(!BeamDirection.IsNearlyZero()
				? FRotationMatrix::MakeFromXZ(BeamDirection, ToCamera).Rotator()
				: FRotationMatrix::MakeFromZ(ToCamera).Rotator());
		}
		if (Particle.bPathStreak)
		{
			// Leave an explicit, narrow white segment between the rocket's two
			// sampled positions. This reads as a continuous flight path in addition
			// to the scattered blue-white glow particles.
			Particle.Component->SetWorldScale3D(Particle.BaseScale);
		}
		else
		{
			const float Scale = Particle.StartScale * (1.f + .65f * Particle.Age / Particle.Lifetime);
			// Stretch the soft quad along the flight direction into a thin blue-white ray.
			Particle.Component->SetWorldScale3D(FVector(Scale * 4.5f, Scale * .48f, 1.f));
		}
		if (IsValid(Particle.Material))
		{
			Particle.Material->SetScalarParameterValue(TEXT("ParticleOpacity"),
				Fade * (Particle.bPathStreak ? .82f : .92f));
		}
	}
}

void UDummyDragonFlightComponent::SpawnRocketParticle(const FVector& Location,
	const FVector& TrailDirection, int32 RocketIndex)
{
	if (TrailDirection.IsNearlyZero() || RocketTrailParticles.IsEmpty())
	{
		return;
	}

	FDragonRocketTrailParticle* Candidate = nullptr;
	for (FDragonRocketTrailParticle& Particle : RocketTrailParticles)
	{
		if (!Particle.bActive)
		{
			Candidate = &Particle;
			break;
		}
	}
	if (!Candidate)
	{
		Candidate = &RocketTrailParticles[0];
		for (FDragonRocketTrailParticle& Particle : RocketTrailParticles)
		{
			if (Particle.Age / Particle.Lifetime > Candidate->Age / Candidate->Lifetime)
			{
				Candidate = &Particle;
			}
		}
	}

	const FVector Side = FVector::CrossProduct(TrailDirection, FVector::UpVector).GetSafeNormal();
	Candidate->Age = 0.f;
	Candidate->Lifetime = .24f + .12f * FMath::FRand();
	Candidate->StartScale = .10f + .06f * FMath::FRand();
	Candidate->BaseScale = FVector::OneVector;
	Candidate->bPathStreak = false;
	Candidate->Velocity = -TrailDirection * (85.f + 65.f * FMath::FRand())
		+ Side * FMath::FRandRange(-12.f, 12.f) + FVector::UpVector * FMath::FRandRange(-10.f, 10.f);
	Candidate->bActive = true;
	Candidate->Component->SetWorldLocation(Location);
	Candidate->Component->SetWorldScale3D(FVector(Candidate->StartScale * 4.5f, Candidate->StartScale * .48f, 1.f));
	Candidate->Component->SetHiddenInGame(false);
	if (IsValid(Candidate->Material))
	{
		const FLinearColor Color = (RocketIndex % 3 == 0)
			? FLinearColor(16.f, 20.f, 24.f, 1.f) : FLinearColor(12.f, 17.f, 22.f, 1.f);
		Candidate->Material->SetVectorParameterValue(TEXT("ParticleColor"), Color);
		Candidate->Material->SetScalarParameterValue(TEXT("ParticleOpacity"), .92f);
	}
}

void UDummyDragonFlightComponent::SpawnRocketPathStreak(const FVector& Start, const FVector& End)
{
	const FVector Segment = End - Start;
	const float SegmentLength = Segment.Size();
	if (SegmentLength < KINDA_SMALL_NUMBER || RocketTrailParticles.IsEmpty())
	{
		return;
	}

	FDragonRocketTrailParticle* Candidate = nullptr;
	for (FDragonRocketTrailParticle& Particle : RocketTrailParticles)
	{
		if (!Particle.bActive)
		{
			Candidate = &Particle;
			break;
		}
	}
	if (!Candidate)
	{
		Candidate = &RocketTrailParticles[0];
		for (FDragonRocketTrailParticle& Particle : RocketTrailParticles)
		{
			if (Particle.Age / Particle.Lifetime > Candidate->Age / Candidate->Lifetime)
			{
				Candidate = &Particle;
			}
		}
	}

	const FVector Direction = Segment / SegmentLength;
	Candidate->Age = 0.f;
	Candidate->Lifetime = .42f;
	Candidate->StartScale = 0.f;
	Candidate->BaseScale = FVector(SegmentLength / 100.f * 1.12f, .024f, 1.f);
	Candidate->Velocity = Direction * .001f;
	Candidate->bPathStreak = true;
	Candidate->bActive = true;
	Candidate->Component->SetWorldLocation((Start + End) * .5f);
	Candidate->Component->SetWorldScale3D(Candidate->BaseScale);
	Candidate->Component->SetHiddenInGame(false);
	if (IsValid(Candidate->Material))
	{
		Candidate->Material->SetVectorParameterValue(TEXT("ParticleColor"), FLinearColor(7.f, 8.5f, 10.f, 1.f));
		Candidate->Material->SetScalarParameterValue(TEXT("ParticleOpacity"), .82f);
	}
}

void UDummyDragonFlightComponent::UpdateRocketTrails(float DeltaTime)
{
	AActor* Dragon = GetOwner();
	if (!IsValid(Dragon) || RocketLocalOffsets.IsEmpty())
	{
		return;
	}

	UpdateRocketParticles(DeltaTime);
	TArray<FVector> CurrentRocketLocations;
	CurrentRocketLocations.Reserve(RocketLocalOffsets.Num());
	const FTransform DragonTransform = Dragon->GetActorTransform();
	for (const FVector& LocalOffset : RocketLocalOffsets)
	{
		CurrentRocketLocations.Add(DragonTransform.TransformPosition(LocalOffset));
	}

	RocketParticleSpawnAccumulator += DeltaTime;
	if (bHasRocketTrailHistory && PreviousRocketLocations.Num() == CurrentRocketLocations.Num()
		&& RocketParticleSpawnAccumulator >= .03f)
	{
		RocketParticleSpawnAccumulator = 0.f;
		for (int32 Index = 0; Index < CurrentRocketLocations.Num(); ++Index)
		{
			SpawnRocketPathStreak(PreviousRocketLocations[Index], CurrentRocketLocations[Index]);
			SpawnRocketParticle(CurrentRocketLocations[Index],
				(CurrentRocketLocations[Index] - PreviousRocketLocations[Index]).GetSafeNormal(), Index);
		}
	}

	PreviousRocketLocations = MoveTemp(CurrentRocketLocations);
	bHasRocketTrailHistory = true;
}

float UDummyDragonFlightComponent::GetApproachPathAlpha(float ElapsedSeconds) const
{
	if (ElapsedSeconds <= BoardPassSeconds)
	{
		return FMath::Clamp(ElapsedSeconds /
			(NormalApproachSeconds * BoardPassSpeedDivisor), 0.f, BoardPassPathAlpha);
	}

	return FMath::Clamp(BoardPassPathAlpha +
		(ElapsedSeconds - BoardPassSeconds) / NormalApproachSeconds,
		BoardPassPathAlpha, 1.f);
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
