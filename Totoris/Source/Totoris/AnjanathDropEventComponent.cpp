#include "AnjanathDropEventComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"

UAnjanathDropEventComponent::UAnjanathDropEventComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
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

	// The actor is authored at its intended resting position. Trace upward to
	// use the actual ceiling if available, with a safe fallback for the level.
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

	CreateDustPool();
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
	SpawnCeilingDust();
}

void UAnjanathDropEventComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateDust(DeltaTime);

	AActor* Anjanath = GetOwner();
	if (!bPlaying || !IsValid(Anjanath))
	{
		return;
	}

	ElapsedSeconds += FMath::Max(DeltaTime, 0.f);
	const float Alpha = FMath::Clamp(ElapsedSeconds / FallDurationSeconds, 0.f, 1.f);
	// Ease-in gives the cube a convincing acceleration after breaking through
	// the ceiling, while stopping exactly at its authored floor transform.
	const float FallAlpha = Alpha * Alpha;
	const FVector StartLocation = CeilingBreakLocation + FVector::UpVector * CeilingClearance;
	Anjanath->SetActorLocation(FMath::Lerp(StartLocation, LandingTransform.GetLocation(), FallAlpha),
		false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.f)
	{
		Anjanath->SetActorTransform(LandingTransform, false, nullptr, ETeleportType::TeleportPhysics);
		bPlaying = false;
	}
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
		return;
	}

	// Allocate dust once for the event; the amount can be tuned per actor in Details.
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
		Particle->RegisterComponent();
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(DustMaterial, Particle);
		Particle->SetMaterial(0, Material);
		Particle->SetHiddenInGame(true);
		DustParticles.Add({ Particle, Material });
	}
}

void UAnjanathDropEventComponent::SpawnCeilingDust()
{
	for (FAnjanathDustParticle& Dust : DustParticles)
	{
		if (!IsValid(Dust.Component))
		{
			continue;
		}

		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Radius = FMath::Sqrt(FMath::FRand()) * 135.f;
		const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius,
			FMath::FRandRange(-95.f, -45.f));
		Dust.Age = FMath::FRandRange(-.18f, 0.f);
		Dust.Lifetime = FMath::FRandRange(2.0f, 2.8f);
		Dust.StartScale = FMath::FRandRange(.85f, 1.45f);
		Dust.Velocity = FVector(
			FMath::Cos(Angle) * 35.f,
			FMath::Sin(Angle) * 35.f,
			FMath::FRandRange(-42.f, -18.f));
		Dust.bActive = true;
		Dust.Component->SetWorldLocation(CeilingBreakLocation + Offset);
		Dust.Component->SetWorldRotation(FRotator(
			FMath::FRandRange(-45.f, 45.f),
			FMath::FRandRange(0.f, 360.f), 0.f));
		Dust.Component->SetWorldScale3D(FVector(Dust.StartScale, Dust.StartScale, 1.f));
		Dust.Component->SetHiddenInGame(true);
		if (IsValid(Dust.Material))
		{
			Dust.Material->SetVectorParameterValue(TEXT("DustColor"),
				FLinearColor(.30f, .19f, .085f, 1.f) * FMath::FRandRange(.85f, 1.15f));
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

		Dust.Age += FMath::Max(DeltaTime, 0.f);
		if (Dust.Age < 0.f) continue;
		Dust.Component->SetHiddenInGame(false);
		if (Dust.Age >= Dust.Lifetime)
		{
			Dust.bActive = false;
			Dust.Component->SetHiddenInGame(true);
			continue;
		}

		const float Progress = FMath::Max(0.f, Dust.Age / Dust.Lifetime);
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
			Dust.Material->SetScalarParameterValue(TEXT("DustOpacity"), .42f * FadeIn * FadeOut);
		}
	}
}
