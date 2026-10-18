#include "AnjanathDropEventComponent.h"
#include "TotorisAudioMix.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimationAsset.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "Sound/SoundBase.h"

UAnjanathDropEventComponent::UAnjanathDropEventComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UAnjanathDropEventComponent::ConfigureFireBreath(
	float InDurationSeconds, float InConeAngleDegrees,
	int32 InFireParticlesPerSecond, int32 InSmokeParticlesPerSecond,
	FVector InMouthLocalOffset)
{
	FireDurationSeconds = FMath::Clamp(InDurationSeconds, .1f, 30.f);
	FireConeAngleDegrees = FMath::Clamp(InConeAngleDegrees, 5.f, 110.f);
	FireParticlesPerSecond = FMath::Clamp(InFireParticlesPerSecond, 1, 1200);
	SmokeParticlesPerSecond = FMath::Clamp(InSmokeParticlesPerSecond, 0, 120);
	MouthLocalOffset = InMouthLocalOffset;
	// Pool sizes are set in InitializeDrop. Changing emission rates afterward
	// requires a new component/PIE run for guaranteed capacity.
}

void UAnjanathDropEventComponent::InitializeDrop()
{
	AActor* Anjanath = GetOwner();
	if (!IsValid(Anjanath) || bInitialized)
	{
		return;
	}

	if (USceneComponent* Root = Anjanath->GetRootComponent())
	{
		Root->SetMobility(EComponentMobility::Movable);
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Root))
		{
			Primitive->SetSimulatePhysics(false);
			Primitive->SetEnableGravity(false);
		}
	}

	LandingTransform = Anjanath->GetActorTransform();
	FVector BoundsOrigin;
	FVector BoundsExtent;
	Anjanath->GetActorBounds(false, BoundsOrigin, BoundsExtent);
	ModelBottomOffsetZ = BoundsOrigin.Z - BoundsExtent.Z - LandingTransform.GetLocation().Z;

	FHitResult CeilingHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AnjanathCeilingTrace), false, Anjanath);
	const FVector TraceStart = LandingTransform.GetLocation() + FVector::UpVector * BoundsExtent.Z;
	const FVector TraceEnd = TraceStart + FVector::UpVector * 5000.f;
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(
		CeilingHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		CeilingBreakLocation = CeilingHit.ImpactPoint;
	}
	else
	{
		CeilingBreakLocation = TraceStart + FVector::UpVector * 750.f;
	}

	// A Blueprint actor can contain a scene/arrow component named
	// "AnjanathFireOrigin". Its transform takes precedence over the cube offset.
	TArray<USceneComponent*> Scenes;
	Anjanath->GetComponents<USceneComponent>(Scenes);
	for (USceneComponent* Scene : Scenes)
	{
		if (IsValid(Scene) && Scene->GetFName() == MouthMarkerName)
		{
			MouthMarker = Scene;
			break;
		}
	}

	CreateDustPool();
	CreateBreathPools();
	AnjanathMesh = Anjanath->FindComponentByClass<USkeletalMeshComponent>();
	LandingAnimation = LoadObject<UAnimationAsset>(nullptr,
		TEXT("/Game/Totoris/Objects/Characters/Anjanath/Anjanath_Landing.Anjanath_Landing"));
	RoarFirePrepAnimation = LoadObject<UAnimationAsset>(nullptr,
		TEXT("/Game/Totoris/Objects/Characters/Anjanath/Anjanath_Roar_FirePrep.Anjanath_Roar_FirePrep"));
	BreakWallSound = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/Totoris/Audio/Gameplay/MHBreakWallSound.MHBreakWallSound"));
	LandingSound = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/Totoris/Audio/Gameplay/MHLandingSound.MHLandingSound"));
	FireSound = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/Totoris/Audio/Gameplay/MHFire.MHFire"));
	HoldLandingAnimationAtFirstFrame();
	bInitialized = true;
	Anjanath->SetActorHiddenInGame(true);
}

void UAnjanathDropEventComponent::PlayDrop()
{
	AActor* Anjanath = GetOwner();
	if (!bInitialized || !IsValid(Anjanath))
	{
		return;
	}

	ElapsedSeconds = 0.f;
	bPlaying = true;
	bLandingAnimationPlaying = false;
	bRoarFirePrepPlaying = false;
	bFireStartedDuringRoarFirePrep = false;
	bBreathingFire = false;
	bFireAudioFadeStarted = false;
	bModelHiddenAfterFireStart = false;
	bCeilingDustTriggered = false;
	if (IsValid(FireAudioComponent))
	{
		FireAudioComponent->Stop();
		FireAudioComponent = nullptr;
	}
	ResetBreathParticles();
	for (FAnjanathDustParticle& Dust : DustParticles)
	{
		Dust.bActive = false;
		if (IsValid(Dust.Component))
		{
			Dust.Component->SetHiddenInGame(true);
		}
	}

	const FVector StartLocation = CeilingBreakLocation + FVector::UpVector * CeilingClearance;
	Anjanath->SetActorTransform(LandingTransform, false, nullptr, ETeleportType::TeleportPhysics);
	Anjanath->SetActorLocation(StartLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Anjanath->SetActorHiddenInGame(false);
	if (IsValid(AnjanathMesh))
	{
		AnjanathMesh->SetHiddenInGame(false);
	}
	HoldLandingAnimationAtFirstFrame();
}

void UAnjanathDropEventComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const float SafeDeltaTime = FMath::Max(DeltaTime, 0.f);
	UpdateDust(SafeDeltaTime);
	UpdateFireBreath(SafeDeltaTime);
	if (bLandingAnimationPlaying)
	{
		LandingAnimationElapsedSeconds += SafeDeltaTime;
		if (!IsValid(LandingAnimation) ||
			LandingAnimationElapsedSeconds >= LandingAnimation->GetPlayLength())
		{
			bLandingAnimationPlaying = false;
			PlayRoarFirePrepAnimation();
		}
	}
	if (bRoarFirePrepPlaying)
	{
		RoarFirePrepElapsedSeconds += SafeDeltaTime;
		const float FirePrepDuration = IsValid(RoarFirePrepAnimation)
			? RoarFirePrepAnimation->GetPlayLength() : 0.f;
		// The authored sequence has 60 frames. Start the breath exactly as its
		// 30th frame is crossed, using duration so a reimported frame rate stays correct.
		if (!bFireStartedDuringRoarFirePrep &&
			RoarFirePrepElapsedSeconds >= FirePrepDuration * .5f)
		{
			bFireStartedDuringRoarFirePrep = true;
			BeginFireBreath();
		}
		if (RoarFirePrepElapsedSeconds >= FirePrepDuration)
		{
			bRoarFirePrepPlaying = false;
		}
	}

	AActor* Anjanath = GetOwner();
	if (!bPlaying || !IsValid(Anjanath))
	{
		return;
	}

	ElapsedSeconds += SafeDeltaTime;
	const float Alpha = FMath::Clamp(ElapsedSeconds / FallDurationSeconds, 0.f, 1.f);
	const float FallAlpha = Alpha * Alpha;
	const FVector StartLocation = CeilingBreakLocation + FVector::UpVector * CeilingClearance;
	const FVector CurrentLocation = FMath::Lerp(StartLocation, LandingTransform.GetLocation(), FallAlpha);
	Anjanath->SetActorLocation(CurrentLocation,
		false, nullptr, ETeleportType::TeleportPhysics);
	// The pivot is not at the feet, so wait until the model's actual lower bound
	// crosses the ceiling before releasing the ceiling-break dust.
	if (!bCeilingDustTriggered && CurrentLocation.Z + ModelBottomOffsetZ <= CeilingBreakLocation.Z)
	{
		bCeilingDustTriggered = true;
		SpawnCeilingDust();
	}

	if (Alpha >= 1.f)
	{
		Anjanath->SetActorTransform(LandingTransform, false, nullptr, ETeleportType::TeleportPhysics);
		bPlaying = false;
		PlayLandingAnimation();
	}
}

void UAnjanathDropEventComponent::HoldLandingAnimationAtFirstFrame()
{
	if (!IsValid(AnjanathMesh) || !IsValid(LandingAnimation))
	{
		return;
	}

	AnjanathMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	AnjanathMesh->SetAnimation(LandingAnimation);
	AnjanathMesh->SetPosition(0.f, false);
}

void UAnjanathDropEventComponent::PlayLandingAnimation()
{
	if (!IsValid(AnjanathMesh) || !IsValid(LandingAnimation))
	{
		return;
	}

	AnjanathMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	AnjanathMesh->SetAnimation(LandingAnimation);
	AnjanathMesh->SetPosition(0.f, false);
	AnjanathMesh->Play(false);
	if (IsValid(LandingSound) && IsValid(GetOwner()))
	{
		UGameplayStatics::SpawnSoundAtLocation(this, LandingSound, GetOwner()->GetActorLocation(),
			FRotator::ZeroRotator, TotorisAudioMix::MHLanding * TotorisAudioMix::MasterVolume);
	}
	LandingAnimationElapsedSeconds = 0.f;
	bLandingAnimationPlaying = true;
}

void UAnjanathDropEventComponent::PlayRoarFirePrepAnimation()
{
	if (!IsValid(AnjanathMesh) || !IsValid(RoarFirePrepAnimation))
	{
		return;
	}

	AnjanathMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	AnjanathMesh->SetAnimation(RoarFirePrepAnimation);
	AnjanathMesh->SetPosition(0.f, false);
	AnjanathMesh->Play(false);
	RoarFirePrepElapsedSeconds = 0.f;
	bRoarFirePrepPlaying = true;
}

void UAnjanathDropEventComponent::CreateDustPool()
{
	AActor* Anjanath = GetOwner();
	if (!IsValid(Anjanath) || !DustParticles.IsEmpty())
	{
		return;
	}

	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* DustMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Totoris/Materials/Events/M_AnjanathDust.M_AnjanathDust"));
	if (!IsValid(PlaneMesh) || !IsValid(DustMaterial))
	{
		UE_LOG(LogTemp, Warning, TEXT("Anjanath: M_AnjanathDust missing; ceiling dust disabled."));
		return;
	}

	const int32 ParticleCount = FMath::Clamp(DustParticleCount, 1, 512);
	DustParticles.Reserve(ParticleCount);
	for (int32 Index = 0; Index < ParticleCount; ++Index)
	{
		UStaticMeshComponent* Particle = NewObject<UStaticMeshComponent>(Anjanath,
			*FString::Printf(TEXT("AnjanathCeilingDust_%03d"), Index));
		Particle->SetMobility(EComponentMobility::Movable);
		Particle->SetStaticMesh(PlaneMesh);
		Particle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Particle->SetCastShadow(false);
		Particle->SetReceivesDecals(false);
		Anjanath->AddInstanceComponent(Particle);
		Particle->RegisterComponent();
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(DustMaterial, Particle);
		Particle->SetMaterial(0, Material);
		Particle->SetHiddenInGame(true);
		DustParticles.Add({ Particle, Material });
	}
}

void UAnjanathDropEventComponent::SpawnCeilingDust()
{
	if (IsValid(BreakWallSound))
	{
		UGameplayStatics::SpawnSoundAtLocation(this, BreakWallSound, CeilingBreakLocation,
			FRotator::ZeroRotator, TotorisAudioMix::MHBreakWall * TotorisAudioMix::MasterVolume);
	}

	const int32 ParticleCount = DustParticles.Num();
	for (int32 Index = 0; Index < ParticleCount; ++Index)
	{
		FAnjanathDustParticle& Dust = DustParticles[Index];
		if (!IsValid(Dust.Component))
		{
			continue;
		}

		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Radius = FMath::Sqrt(FMath::FRand()) * 165.f;
		const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius,
			FMath::FRandRange(-110.f, -35.f));
		// Activate cards evenly over a short cascade. A random-only delay caused
		// most of the pool to become visible in the same few frames.
		constexpr float DustBurstDuration = .60f;
		const float SequenceDelay = DustBurstDuration *
			(static_cast<float>(Index) / FMath::Max(1, ParticleCount - 1));
		Dust.Age = -SequenceDelay + FMath::FRandRange(-.018f, .018f);
		Dust.Lifetime = FMath::FRandRange(2.2f, 3.0f);
		Dust.StartScale = FMath::FRandRange(1.45f, 2.25f);
		Dust.Velocity = FVector(FMath::Cos(Angle) * 35.f, FMath::Sin(Angle) * 35.f,
			FMath::FRandRange(-52.f, -26.f));
		Dust.bActive = true;
		Dust.Component->SetWorldLocation(CeilingBreakLocation + Offset);
		Dust.Component->SetWorldRotation(FRotator(
			FMath::FRandRange(-45.f, 45.f), FMath::FRandRange(0.f, 360.f), 0.f));
		Dust.Component->SetWorldScale3D(FVector(Dust.StartScale, Dust.StartScale, 1.f));
		Dust.Component->SetHiddenInGame(true);
		if (IsValid(Dust.Material))
		{
			Dust.Material->SetVectorParameterValue(TEXT("DustColor"),
				FLinearColor(.72f, .39f, .13f, 1.f) * FMath::FRandRange(.88f, 1.12f));
			Dust.Material->SetScalarParameterValue(TEXT("DustOpacity"), 0.f);
		}
	}
}

void UAnjanathDropEventComponent::UpdateDust(float DeltaTime)
{
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	for (FAnjanathDustParticle& Dust : DustParticles)
	{
		if (!Dust.bActive || !IsValid(Dust.Component))
		{
			continue;
		}

		Dust.Age += DeltaTime;
		if (Dust.Age < 0.f) continue;
		if (Dust.Age >= Dust.Lifetime)
		{
			Dust.bActive = false;
			Dust.Component->SetHiddenInGame(true);
			continue;
		}
		Dust.Component->SetHiddenInGame(false);
		const float Progress = FMath::Clamp(Dust.Age / Dust.Lifetime, 0.f, 1.f);
		// Gentle gravity makes the dust settle instead of drifting weightlessly.
		Dust.Velocity.Z -= 58.f * DeltaTime;
		Dust.Component->SetWorldLocation(Dust.Component->GetComponentLocation() + Dust.Velocity * DeltaTime);
		if (Camera)
		{
			Dust.Component->SetWorldRotation(FRotationMatrix::MakeFromZ(
				Camera->GetCameraLocation() - Dust.Component->GetComponentLocation()).Rotator());
		}
		const float Size = Dust.StartScale * (1.f + Progress * .85f);
		Dust.Component->SetWorldScale3D(FVector(Size, Size, 1.f));
		if (IsValid(Dust.Material))
		{
			const float FadeIn = FMath::Clamp(Dust.Age / .12f, 0.f, 1.f);
			const float FadeOut = 1.f - FMath::SmoothStep(.3f, 1.f, Progress);
			Dust.Material->SetScalarParameterValue(TEXT("DustOpacity"), .48f * FadeIn * FadeOut);
		}
	}
}

void UAnjanathDropEventComponent::CreateBreathPools()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !FlameParticles.IsEmpty() || !SmokeParticles.IsEmpty())
	{
		return;
	}

	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!IsValid(Plane))
	{
		return;
	}

	const auto MakePool = [Owner, Plane](TArray<FAnjanathBreathParticle>& Pool,
		UMaterialInterface* BaseMaterial, int32 Count, const TCHAR* Prefix, int32 SortPriority)
	{
		if (!IsValid(BaseMaterial) || Count <= 0)
		{
			return;
		}
		Pool.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			UStaticMeshComponent* Card = NewObject<UStaticMeshComponent>(Owner,
				*FString::Printf(TEXT("%s_%04d"), Prefix, Index));
			Card->SetMobility(EComponentMobility::Movable);
			Card->SetStaticMesh(Plane);
			Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Card->SetCastShadow(false);
			Card->SetReceivesDecals(false);
			Card->SetCanEverAffectNavigation(false);
			// Fire needs to draw over translucent smoke; smoke stays behind the flames.
			Card->SetTranslucentSortPriority(SortPriority);
			Owner->AddInstanceComponent(Card);
			Card->RegisterComponent();
			UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, Card);
			Card->SetMaterial(0, DynamicMaterial);
			Card->SetHiddenInGame(true);
			Pool.Add({ Card, DynamicMaterial });
		}
	};

	UMaterialInterface* FlameMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Totoris/Materials/Events/M_AnjanathFire.M_AnjanathFire"));
	UMaterialInterface* SmokeMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Totoris/Materials/Events/M_AnjanathSmoke.M_AnjanathSmoke"));
	if (!IsValid(FlameMaterial))
	{
		UE_LOG(LogTemp, Warning, TEXT("Anjanath: Fire material missing! Run create_anjanath_fire_materials.py in Unreal Editor."));
	}
	if (!IsValid(SmokeMaterial))
	{
		UE_LOG(LogTemp, Warning, TEXT("Anjanath: Smoke material missing! Run create_anjanath_fire_materials.py in Unreal Editor."));
	}

	// Pool size covers the particles concurrently alive (not total over 4 seconds).
	// Fire lifetime is <= .9 s, smoke lifetime is <= 1.8 s.
	const int32 FlameCount = FMath::Clamp(FMath::CeilToInt(FireParticlesPerSecond * 1.0f) + 16, 24, 640);
	const int32 SmokeCount = (SmokeParticlesPerSecond > 0)
		? FMath::Clamp(FMath::CeilToInt(SmokeParticlesPerSecond * 1.9f) + 8, 16, 240) : 0;
	MakePool(FlameParticles, FlameMaterial, FlameCount, TEXT("AnjanathFlame"), 10);
	MakePool(SmokeParticles, SmokeMaterial, SmokeCount, TEXT("AnjanathSmoke"), -5);
}

FVector UAnjanathDropEventComponent::GetMouthWorldLocation(const FVector& TowardCamera) const
{
	(void)TowardCamera;
	return FireOriginWorldLocation;
}

void UAnjanathDropEventComponent::ResetBreathParticles()
{
	FireElapsedSeconds = 0.f;
	FlameSpawnCarry = 0.f;
	SmokeSpawnCarry = 0.f;
	FlamePoolCursor = 0;
	SmokePoolCursor = 0;
	for (FAnjanathBreathParticle& P : FlameParticles)
	{
		P.bActive = false;
		if (IsValid(P.Component)) P.Component->SetHiddenInGame(true);
	}
	for (FAnjanathBreathParticle& P : SmokeParticles)
	{
		P.bActive = false;
		if (IsValid(P.Component)) P.Component->SetHiddenInGame(true);
	}
}

void UAnjanathDropEventComponent::BeginFireBreath()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;

	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector PreliminaryOrigin = FireOriginWorldLocation;
	FireDirection = Camera
		? (Camera->GetCameraLocation() - PreliminaryOrigin).GetSafeNormal()
		: Owner->GetActorForwardVector();
	if (FireDirection.IsNearlyZero()) FireDirection = Owner->GetActorForwardVector();

	if (Camera)
	{
		FireDirection = (Camera->GetCameraLocation() - FireOriginWorldLocation).GetSafeNormal();
	}
	bBreathingFire = true;
	FireElapsedSeconds = 0.f;
	FlameSpawnCarry = 0.f;
	SmokeSpawnCarry = 0.f;
	bFireAudioFadeStarted = false;
	if (IsValid(FireSound))
	{
		FireAudioComponent = UGameplayStatics::SpawnSoundAtLocation(this, FireSound,
			FireOriginWorldLocation, FRotator::ZeroRotator,
			TotorisAudioMix::MHFire * TotorisAudioMix::MasterVolume);
	}

	// Start emitting instantly in the same frame as landing.
	for (int32 Index = 0; Index < FMath::Min(8, FireParticlesPerSecond); ++Index)
	{
		SpawnBreathParticle(false);
	}
}

void UAnjanathDropEventComponent::UpdateFireBreath(float DeltaTime)
{
	// Existing particles must finish fading even after active emission ends.
	UpdateBreathParticles(FlameParticles, DeltaTime, false);
	UpdateBreathParticles(SmokeParticles, DeltaTime, true);

	if (!bBreathingFire) return;

	const float Duration = FMath::Clamp(FireDurationSeconds, .1f, 30.f);
	const float EmissionTime = FMath::Min(DeltaTime, FMath::Max(0.f, Duration - FireElapsedSeconds));
	FireElapsedSeconds += DeltaTime;
	if (!bModelHiddenAfterFireStart && FireElapsedSeconds >= HideModelAfterFireSeconds)
	{
		// Hide only the model mesh: the world-space flame cards must remain
		// visible while their final particles finish fading.
		bModelHiddenAfterFireStart = true;
		if (IsValid(AnjanathMesh))
		{
			AnjanathMesh->SetHiddenInGame(true);
		}
	}

	// Avoid huge catch-up bursts after pauses/hitches while keeping rate frame independent.
	FlameSpawnCarry += FMath::Min(EmissionTime, .1f) * FMath::Clamp(FireParticlesPerSecond, 1, 600);
	const int32 FlameCount = FMath::FloorToInt(FlameSpawnCarry);
	FlameSpawnCarry -= FlameCount;
	for (int32 Index = 0; Index < FlameCount; ++Index) SpawnBreathParticle(false);

	if (SmokeParticlesPerSecond > 0 && FireElapsedSeconds >= .12f)
	{
		SmokeSpawnCarry += FMath::Min(EmissionTime, .1f) * FMath::Clamp(SmokeParticlesPerSecond, 0, 120);
		const int32 SmokeCount = FMath::FloorToInt(SmokeSpawnCarry);
		SmokeSpawnCarry -= SmokeCount;
		for (int32 Index = 0; Index < SmokeCount; ++Index) SpawnBreathParticle(true);
	}

	if (FireElapsedSeconds >= Duration)
	{
		bBreathingFire = false;
		if (!bFireAudioFadeStarted && IsValid(FireAudioComponent))
		{
			bFireAudioFadeStarted = true;
			FireAudioComponent->FadeOut(.35f, 0.f);
		}
	}
}

void UAnjanathDropEventComponent::SpawnBreathParticle(bool bSmoke)
{
	TArray<FAnjanathBreathParticle>& Pool = bSmoke ? SmokeParticles : FlameParticles;
	int32& Cursor = bSmoke ? SmokePoolCursor : FlamePoolCursor;
	if (Pool.IsEmpty()) return;

	// Rotate around the ring looking for a free particle. No mid-flight recycling.
	FAnjanathBreathParticle* P = nullptr;
	for (int32 Step = 0; Step < Pool.Num(); ++Step)
	{
		FAnjanathBreathParticle& Candidate = Pool[(Cursor + Step) % Pool.Num()];
		if (!Candidate.bActive && IsValid(Candidate.Component))
		{
			P = &Candidate;
			Cursor = (Cursor + Step + 1) % Pool.Num();
			break;
		}
	}
	if (!P) return;

	const float HalfAngleRad = FMath::DegreesToRadians(
		FMath::Clamp(FireConeAngleDegrees, 5.f, 110.f) * (bSmoke ? .43f : .5f));
	const FVector Direction = FMath::VRandCone(FireDirection, HalfAngleRad);
	const FVector Origin = GetMouthWorldLocation(FireDirection);
	P->Velocity = Direction * FMath::FRandRange(bSmoke ? 175.f : 430.f,
		bSmoke ? 280.f : 625.f);
	P->Age = 0.f;
	P->Lifetime = FMath::FRandRange(bSmoke ? 1.0f : .65f,
		bSmoke ? 1.5f : 1.0f);
	P->StartScale = bSmoke
		? FVector2D(FMath::FRandRange(.26f, .41f), FMath::FRandRange(.31f, .49f))
		: FVector2D(FMath::FRandRange(.44f, .70f), FMath::FRandRange(.60f, .94f));
	P->Phase = FMath::FRandRange(0.f, 100.f);
	P->bActive = true;
	// Previously smoke was born 125 cm CLOSER to the camera than the flames,
	// producing an opaque black foreground cloud. Keep it above/behind the jet.
	const FVector SpawnOffset = bSmoke
		? FVector::UpVector * FMath::FRandRange(15.f, 42.f) - FireDirection * 25.f
		: FireDirection * FMath::FRandRange(0.f, 16.f);
	P->Component->SetWorldLocation(Origin + SpawnOffset
		+ FMath::VRand() * FMath::FRandRange(0.f, bSmoke ? 10.f : 6.f));
	P->Component->SetHiddenInGame(false);
	if (IsValid(P->Material))
	{
		P->Material->SetScalarParameterValue(TEXT("EffectPhase"), P->Phase);
		P->Material->SetScalarParameterValue(TEXT("EffectAge"), 0.f);
		P->Material->SetScalarParameterValue(TEXT("EffectOpacity"), 0.f);
	}
}

void UAnjanathDropEventComponent::UpdateBreathParticles(
	TArray<FAnjanathBreathParticle>& Pool, float DeltaTime, bool bSmoke)
{
	if (Pool.IsEmpty()) return;
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	for (FAnjanathBreathParticle& P : Pool)
	{
		if (!P.bActive || !IsValid(P.Component)) continue;
		P.Age += DeltaTime;
		if (P.Age >= P.Lifetime)
		{
			P.bActive = false;
			P.Component->SetHiddenInGame(true);
			continue;
		}

		const float T = FMath::Clamp(P.Age / P.Lifetime, 0.f, 1.f);
		// Flames shoot forward with gentle lift; smoke rises more and slows down.
		P.Velocity += FVector::UpVector * (bSmoke ? 180.f : 65.f) * DeltaTime;
		if (bSmoke) P.Velocity *= FMath::Max(0.f, 1.f - .35f * DeltaTime);
		P.Component->SetWorldLocation(P.Component->GetComponentLocation() + P.Velocity * DeltaTime);

		if (Camera)
		{
			const FVector ToCamera = (Camera->GetCameraLocation() - P.Component->GetComponentLocation()).GetSafeNormal();
			if (!ToCamera.IsNearlyZero())
			{
				// Quad normal = camera, local Y = world up (vertical flame UVs).
				P.Component->SetWorldRotation(FRotationMatrix::MakeFromZY(ToCamera, FVector::UpVector).Rotator());
			}
		}

		const float Expansion = bSmoke ? (1.f + T * 1.25f) : (1.f + T * 1.35f);
		P.Component->SetWorldScale3D(FVector(P.StartScale.X * Expansion,
			P.StartScale.Y * Expansion, 1.f));
		if (IsValid(P.Material))
		{
			const float FadeIn = FMath::Clamp(P.Age / (bSmoke ? .20f : .09f), 0.f, 1.f);
			const float FadeOut = 1.f - FMath::SmoothStep(.38f, 1.f, T);
			const float Opacity = FadeIn * FadeOut * (bSmoke ? .32f : 1.f);
			P.Material->SetScalarParameterValue(TEXT("EffectAge"), P.Age);
			P.Material->SetScalarParameterValue(TEXT("EffectOpacity"), Opacity);
		}
	}
}
