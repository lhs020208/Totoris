#include "MinecraftBlockEventComponent.h"
#include "MinecraftLavaComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "UObject/ConstructorHelpers.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UMinecraftBlockEventComponent::UMinecraftBlockEventComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// Hard asset references ensure these runtime-sampled clips are cooked.
	for (const TCHAR* Name : {TEXT("MC_CrouchPose"), TEXT("MC_LookDown"), TEXT("MC_PlaceBlock"), TEXT("MC_Walk")})
	{
		const FString Path = FString::Printf(TEXT("/Game/Totoris/Objects/Minecraft/%s.%s"), Name, Name);
		ConstructorHelpers::FObjectFinder<UAnimSequence> Clip(*Path);
		Clips.Add(Clip.Object);
	}
}

void UMinecraftBlockEventComponent::PlayEvent()
{
	StopEvent();
	if (!IsValid(GetOwner()))
	{
		return;
	}

	SpawnedBlockCount = 0;
	EventOrigin = GetOwner()->GetActorTransform();
	UStaticMeshComponent* BlockMesh = GetOwner()->FindComponentByClass<UStaticMeshComponent>();
	if (!BlockMesh) return;
	const FBox BlockBounds = BlockMesh->CalcBounds(BlockMesh->GetComponentTransform()).GetBox();
	BlockTop = BlockBounds.Max.Z;
	FirstBlockEnd = BlockBounds.Max.Y;
	if (!StartAsito())
	{
		UE_LOG(LogTemp, Error, TEXT("Minecraft event: Asito mesh, foot geometry or animation is missing."));
		StopEvent();
		return;
	}
	bPlaying = true;
	SetComponentTickEnabled(true);
	StartLavaOnFloor();
}

void UMinecraftBlockEventComponent::StopEvent()
{
	if (IsValid(LavaComponent)) LavaComponent->StopLava();
	bPlaying = false;
	SetComponentTickEnabled(false);
	SpawnedBlockCount = 0;
	// Reparent before destroying the runtime mesh, retaining the hand offset/scale.
	if (HeldBlock.IsValid() && OriginalMesh.IsValid())
		HeldBlock->AttachToComponent(OriginalMesh.Get(), FAttachmentTransformRules::KeepRelativeTransform, TEXT("MinecraftRightHandTip"));
	HeldBlock.Reset();
	if (IsValid(EventMesh)) EventMesh->DestroyComponent();
	EventMesh = nullptr;
	if (OriginalMesh.IsValid()) OriginalMesh->SetVisibility(bOriginalVisible);
	if (AsitoActor.IsValid()) AsitoActor->SetActorTransform(AsitoOriginalTransform);
	OriginalMesh.Reset();
	AsitoActor.Reset();
	FootVertices.Reset();
	ComponentPose.Reset();

	for (TWeakObjectPtr<AActor>& SpawnedBlock : SpawnedBlocks)
	{
		if (SpawnedBlock.IsValid())
		{
			SpawnedBlock->Destroy();
		}
	}
	SpawnedBlocks.Reset();
}

void UMinecraftBlockEventComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopEvent();
	Super::EndPlay(Reason);
}

void UMinecraftBlockEventComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bPlaying)
	{
		return;
	}

	if (!AsitoActor.IsValid() || !OriginalMesh.IsValid() || !IsValid(EventMesh))
	{
		StopEvent();
		return;
	}
	// Small steps keep foot-edge crossings stable even at low frame rates.
	float Remaining = FMath::Max(0.f, DeltaTime);
	while (bPlaying && Remaining > SMALL_NUMBER)
	{
		const float Step = FMath::Min(Remaining, 1.f / 120.f);
		AdvanceAsito(Step);
		Remaining -= Step;
	}
}

void UMinecraftBlockEventComponent::SpawnNextBlock()
{
	AActor* SourceBlock = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(SourceBlock) || !IsValid(World))
	{
		StopEvent();
		return;
	}

	FTransform SpawnTransform = EventOrigin;
	SpawnTransform.AddToTranslation(FVector(0.f, BlockStepCentimetres * (SpawnedBlockCount + 1), 0.f));

	FActorSpawnParameters SpawnParameters;
	// Do not reuse a fixed object name here. Destroy() marks the prior runtime
	// copy pending-kill, but its UObject name can remain occupied until GC runs.
	// A second mission start before that GC would otherwise assert inside
	// SpawnActor when it tries to create MinecraftEventBlock_1 again.
	SpawnParameters.Name = NAME_None;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AActor* SpawnedBlock = World->SpawnActor<AActor>(SourceBlock->GetClass(), SpawnTransform, SpawnParameters))
	{
		SpawnedBlocks.Add(SpawnedBlock);
		++SpawnedBlockCount;
		PlaceTime = 0.f;
		UE_LOG(LogTemp, Log, TEXT("Minecraft event: placed block %d at foot-edge crossing."), SpawnedBlockCount);
	}
	else
	{
		StopEvent();
	}
}

bool UMinecraftBlockEventComponent::StartAsito()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		USkeletalMeshComponent* Mesh = It->FindComponentByClass<USkeletalMeshComponent>();
		if (!Mesh || !Mesh->GetSkeletalMeshAsset() ||
			Mesh->GetSkeletalMeshAsset()->GetName() != TEXT("MC_CustomSkin_Model")) continue;
		AsitoActor = *It;
		OriginalMesh = Mesh;
		break;
	}
	if (!AsitoActor.IsValid()) return false;
	AsitoOriginalTransform = AsitoActor->GetActorTransform();
	bOriginalVisible = OriginalMesh->IsVisible();
	if (Clips.Num() != 4) return false;
	for (UAnimSequence* Clip : Clips)
	{
		if (!Clip || Clip->GetSkeleton() != OriginalMesh->GetSkeletalMeshAsset()->GetSkeleton()) return false;
	}
	EventMesh = NewObject<UPoseableMeshComponent>(AsitoActor.Get());
	AsitoActor->AddInstanceComponent(EventMesh);
	EventMesh->SetupAttachment(AsitoActor->GetRootComponent());
	EventMesh->SetSkinnedAssetAndUpdate(OriginalMesh->GetSkeletalMeshAsset());
	EventMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EventMesh->RegisterComponent();
	EventMesh->SetWorldTransform(OriginalMesh->GetComponentTransform());
	for (int32 I = 0; I < OriginalMesh->GetNumMaterials(); ++I)
		EventMesh->SetMaterial(I, OriginalMesh->GetMaterial(I));
	if (!CacheFootVertices()) return false;
	WalkTime = 0.f;
	WalkStopTime = 0.f;
	PlaceTime = -1.f;
	FallTime = 0.f;
	bFalling = false;
	bHasLandingFloor = false;
	bLanded = false;
	OriginalMesh->SetVisibility(false);
	UpdateAsitoPose();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("MinecraftHeldBlock"))) continue;
		if (EventMesh->DoesSocketExist(TEXT("MinecraftRightHandTip")))
		{
			HeldBlock = *It;
			It->AttachToComponent(EventMesh, FAttachmentTransformRules::KeepRelativeTransform, TEXT("MinecraftRightHandTip"));
		}
		break;
	}
	FVector Location = AsitoActor->GetActorLocation();
	Location.Z += BlockTop - GetFootBounds().Min.Z;
	AsitoActor->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	UE_LOG(LogTemp, Log, TEXT("Minecraft event: Asito aligned at %s, block top %.3f."), *Location.ToString(), BlockTop);
	return true;
}

bool UMinecraftBlockEventComponent::PrepareHeldBlock()
{
#if WITH_EDITOR
	USkeletalMeshComponent* Model = nullptr;
	AActor* Block = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetActorLabel() == TEXT("MinecraftBlock2")) Block = *It;
		auto* Candidate = It->FindComponentByClass<USkeletalMeshComponent>();
		if (Candidate && Candidate->GetSkeletalMeshAsset() && Candidate->GetSkeletalMeshAsset()->GetName() == TEXT("MC_CustomSkin_Model")) Model = Candidate;
	}
	if (!Model || !Block) return false;
	USkeletalMesh* Mesh = Model->GetSkeletalMeshAsset();
	const auto* Data = Mesh->GetResourceForRendering();
	if (!Data || Data->LODRenderData.IsEmpty()) return false;
	const auto& LOD = Data->LODRenderData[0];
	const auto& Positions = LOD.StaticVertexBuffers.PositionVertexBuffer;
	const auto* Weights = LOD.GetSkinWeightVertexBuffer();
	if (!Weights || !Positions.GetAllowCPUAccess() || !Weights->GetNeedsCPUAccess()) return false;
	const auto& Ref = Mesh->GetRefSkeleton();
	const int32 Arm = Ref.FindBoneIndex(TEXT("Bone_010"));
	if (Arm == INDEX_NONE) return false;
	FBox Bounds(ForceInit);
	for (const auto& Section : LOD.RenderSections)
		for (uint32 V = Section.BaseVertexIndex; V < Section.BaseVertexIndex + Section.NumVertices; ++V)
		{
			const uint32 Bone = Weights->GetBoneIndex(V, 0);
			if (Section.BoneMap.IsValidIndex(Bone) && Section.BoneMap[Bone] == Arm)
				Bounds += FVector(Positions.VertexPosition(V));
		}
	if (!Bounds.IsValid) return false;
	FTransform ArmCS = Ref.GetRefBonePose()[Arm];
	for (int32 Parent = Ref.GetParentIndex(Arm); Parent != INDEX_NONE; Parent = Ref.GetParentIndex(Parent))
		ArmCS *= Ref.GetRefBonePose()[Parent];
	FVector Tip = Bounds.GetCenter();
	Tip.Z = Bounds.Min.Z;
	Mesh->Modify();
	auto* Socket = Mesh->FindSocket(TEXT("MinecraftRightHandTip"));
	if (!Socket)
	{
		Socket = NewObject<USkeletalMeshSocket>(Mesh, NAME_None, RF_Transactional);
		Mesh->GetMeshOnlySocketList().Add(Socket);
	}
	Socket->Modify();
	Socket->SocketName = TEXT("MinecraftRightHandTip");
	Socket->BoneName = TEXT("Bone_010");
	Socket->RelativeLocation = ArmCS.InverseTransformPosition(Tip);
	Socket->RelativeRotation = FRotator::ZeroRotator;
	Socket->RelativeScale = FVector::OneVector;
	Mesh->MarkPackageDirty();
	Block->Modify();
	Block->Tags.AddUnique(TEXT("MinecraftHeldBlock"));
	Block->Tags.Remove(TEXT("MinecraftBlock"));
	Block->GetRootComponent()->Modify();
	Block->GetRootComponent()->SetMobility(EComponentMobility::Movable);
	if (auto* Primitive = Cast<UPrimitiveComponent>(Block->GetRootComponent()))
	{
		Primitive->SetSimulatePhysics(false);
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	const bool bAttached = Block->AttachToComponent(Model, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket->SocketName);
	Block->MarkPackageDirty();
	return bAttached;
#else
	return false;
#endif
}

bool UMinecraftBlockEventComponent::CacheFootVertices()
{
	USkeletalMesh* Mesh = OriginalMesh->GetSkeletalMeshAsset();
	const FSkeletalMeshRenderData* Data = Mesh->GetResourceForRendering();
	if (!Data || Data->LODRenderData.IsEmpty()) return false;
	const FSkeletalMeshLODRenderData& LOD = Data->LODRenderData[0];
	const auto& Positions = LOD.StaticVertexBuffers.PositionVertexBuffer;
	const FSkinWeightVertexBuffer* Weights = LOD.GetSkinWeightVertexBuffer();
	if (!Weights || !Positions.GetAllowCPUAccess() || !Weights->GetNeedsCPUAccess()) return false;
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	const int32 Left = Ref.FindBoneIndex(TEXT("Bone_006"));
	const int32 Right = Ref.FindBoneIndex(TEXT("Bone_008"));
	if (Left == INDEX_NONE || Right == INDEX_NONE) return false;
	TArray<FTransform> RefCS = Ref.GetRefBonePose();
	for (int32 I = 0; I < RefCS.Num(); ++I)
		if (Ref.GetParentIndex(I) != INDEX_NONE) RefCS[I] *= RefCS[Ref.GetParentIndex(I)];
	for (const int32 Leg : {Left, Right})
	{
		TArray<FVector> Points;
		float Lowest = TNumericLimits<float>::Max();
		for (const FSkelMeshRenderSection& Section : LOD.RenderSections)
		{
			for (uint32 V = Section.BaseVertexIndex; V < Section.BaseVertexIndex + Section.NumVertices; ++V)
			{
				const uint32 LocalBone = Weights->GetBoneIndex(V, 0);
				if (!Section.BoneMap.IsValidIndex(LocalBone) || Section.BoneMap[LocalBone] != Leg) continue;
				const FVector Point(Positions.VertexPosition(V));
				Points.Add(Point);
				Lowest = FMath::Min(Lowest, static_cast<float>(Point.Z));
			}
		}
		// Only sole corners, not the knee: the latter can lead during a stride.
		for (const FVector& Point : Points)
			if (Point.Z <= Lowest + 0.1f) FootVertices.Emplace(Leg, RefCS[Leg].InverseTransformPosition(Point));
	}
	return !FootVertices.IsEmpty();
}

void UMinecraftBlockEventComponent::UpdateAsitoPose()
{
	const FReferenceSkeleton& Ref = OriginalMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
	EventMesh->BoneSpaceTransforms = Ref.GetRefBonePose();
	auto Layer = [&](int32 ClipIndex, const TCHAR* BoneName, float Time, float Weight)
	{
		const FName Bone(BoneName);
		const int32 MeshIndex = Ref.FindBoneIndex(Bone);
		UAnimSequence* Clip = Clips[ClipIndex];
		const int32 SkeletonIndex = Clip->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(Bone);
		if (MeshIndex == INDEX_NONE || SkeletonIndex == INDEX_NONE) return;
		FTransform Sample;
		Clip->GetBoneTransform(Sample, FSkeletonPoseBoneIndex(SkeletonIndex),
			FAnimExtractContext(static_cast<double>(FMath::Clamp(Time, 0.f, Clip->GetPlayLength()))), false);
		// Mask individual local bones; each exported clip's untouched bones must
		// not overwrite the other layers (or the root's imported transform).
		EventMesh->BoneSpaceTransforms[MeshIndex].Blend(Ref.GetRefBonePose()[MeshIndex], Sample, Weight);
	};
	Layer(0, TEXT("Bone_001"), Clips[0]->GetPlayLength(), bFalling ? FMath::Clamp(1.f - FallTime / .18f, 0.f, 1.f) : 1.f);
	Layer(1, TEXT("Bone_002"), Clips[1]->GetPlayLength(), 1.f);
	if (PlaceTime >= 0.f && PlaceTime < Clips[2]->GetPlayLength()) Layer(2, TEXT("Bone_010"), PlaceTime, 1.f);
	const float WalkLength = FMath::Max(Clips[3]->GetPlayLength(), .01f);
	if (!bFalling || WalkTime < WalkStopTime)
	{
		const float Weight = bFalling ? FMath::Clamp((WalkStopTime - WalkTime) / .1f, 0.f, 1.f) : 1.f;
		Layer(3, TEXT("Bone_006"), FMath::Fmod(WalkTime, WalkLength), Weight);
		Layer(3, TEXT("Bone_008"), FMath::Fmod(WalkTime, WalkLength), Weight);
	}
	ComponentPose = EventMesh->BoneSpaceTransforms;
	for (int32 I = 0; I < ComponentPose.Num(); ++I)
		if (Ref.GetParentIndex(I) != INDEX_NONE) ComponentPose[I] *= ComponentPose[Ref.GetParentIndex(I)];
	EventMesh->MarkRefreshTransformDirty();
	EventMesh->RefreshBoneTransforms();
}

FBox UMinecraftBlockEventComponent::GetFootBounds() const
{
	FBox Box(ForceInit);
	for (const auto& Vertex : FootVertices)
		Box += EventMesh->GetComponentTransform().TransformPosition(ComponentPose[Vertex.Key].TransformPosition(Vertex.Value));
	return Box;
}

void UMinecraftBlockEventComponent::BeginFall()
{
	bFalling = true;
	FallTime = 0.f;
	FallOrigin = AsitoActor->GetActorLocation();
	bHasLandingFloor = false;
	bLanded = false;
	// Trace only the authored floor, never the rug, lava or bridge blocks.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		bool bFloor = It->GetName().Contains(TEXT("Scene_RoundFloor")) || It->ActorHasTag(TEXT("Scene_RoundFloor"));
#if WITH_EDITOR
		bFloor |= It->GetActorLabel().Equals(TEXT("Scene_RoundFloor"));
#endif
		if (!bFloor) continue;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MinecraftAsitoLanding), true);
		if (It->ActorLineTraceSingle(Hit, FallOrigin + FVector(0, 0, 5000),
			FallOrigin - FVector(0, 0, 5000), ECC_Visibility, Params) && Hit.ImpactNormal.Z > .7f)
		{
			LandingFloorZ = Hit.ImpactPoint.Z;
			bHasLandingFloor = true;
			break;
		}
	}
	if (!bHasLandingFloor)
		UE_LOG(LogTemp, Warning, TEXT("Minecraft event: Scene_RoundFloor was not hit below Asito."));
	PlaceTime = -1.f;
	const float Length = FMath::Max(Clips[3]->GetPlayLength(), .01f);
	WalkStopTime = FMath::CeilToFloat(WalkTime / Length) * Length;
	UE_LOG(LogTemp, Log, TEXT("Minecraft event: falling after third block; remaining stride %.3fs."), WalkStopTime - WalkTime);
}

void UMinecraftBlockEventComponent::AdvanceAsito(float Seconds)
{
	WalkTime += Seconds;
	if (PlaceTime >= 0.f) PlaceTime += Seconds;
	if (bFalling)
	{
		FallTime += Seconds;
		UpdateAsitoPose();
		// Horizontal movement ends immediately; only the current leg cycle finishes.
		if (!bLanded)
			AsitoActor->SetActorLocation(FallOrigin + FVector(0, 0, -.5f * 980.f * FallTime * FallTime), false);
		const float FootZ = GetFootBounds().Min.Z;
		if (bHasLandingFloor && (bLanded || FootZ <= LandingFloorZ))
		{
			// Keep the sole grounded while the remaining stride blends to idle.
			AsitoActor->AddActorWorldOffset(FVector(0, 0, LandingFloorZ - FootZ), false);
			if (!bLanded)
				UE_LOG(LogTemp, Log, TEXT("Minecraft event: Asito landed on Scene_RoundFloor at Z=%.3f."), LandingFloorZ);
			bLanded = true;
			if (WalkTime >= WalkStopTime && FallTime >= .18f)
			{
				bPlaying = false;
				SetComponentTickEnabled(false);
			}
		}
		else if (!bHasLandingFloor && FallTime >= 3.f)
		{
			EventMesh->SetVisibility(false);
			bPlaying = false;
			SetComponentTickEnabled(false);
		}
		return;
	}
	AsitoActor->AddActorWorldOffset(FVector(0, MoveSpeedCentimetres * Seconds, 0), false);
	UpdateAsitoPose();
	const FBox Feet = GetFootBounds();
	AsitoActor->AddActorWorldOffset(FVector(0, 0, BlockTop - Feet.Min.Z), false);
	// Facing world Y-: the forward-most sole point is the last point to
	// leave the supporting block while backing up along world Y+.
	if (Feet.Min.Y >= FirstBlockEnd + SpawnedBlockCount * BlockStepCentimetres)
	{
		if (SpawnedBlockCount < BlocksToSpawn) SpawnNextBlock();
		else BeginFall();
	}
}

void UMinecraftBlockEventComponent::StartLavaOnFloor()
{
	if (!GetWorld() || !GetOwner()) return;
	const FVector LastBlockPosition = EventOrigin.GetLocation() + FVector(0, BlocksToSpawn * BlockStepCentimetres, 0);
	// Only the room floor defines the source height. Rugs, furniture and event
	// blocks must not lift the source or obstruct this presentation effect.
	FVector SurfacePoint(LastBlockPosition.X, LastBlockPosition.Y, -5.f);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		bool bFloor = It->GetName().Contains(TEXT("Scene_RoundFloor")) || It->ActorHasTag(TEXT("Scene_RoundFloor"));
#if WITH_EDITOR
		bFloor |= It->GetActorLabel().Equals(TEXT("Scene_RoundFloor"));
#endif
		if (!bFloor) continue;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MinecraftLavaSource), true);
		if (It->ActorLineTraceSingle(Hit, LastBlockPosition + FVector(0,0,5000),
			LastBlockPosition - FVector(0,0,5000), ECC_Visibility, Params) && Hit.ImpactNormal.Z > .7f)
		{
			SurfacePoint.Z = Hit.ImpactPoint.Z;
			break;
		}
	}
	if (!IsValid(LavaComponent))
	{
		LavaComponent = NewObject<UMinecraftLavaComponent>(GetOwner());
		GetOwner()->AddInstanceComponent(LavaComponent);
		LavaComponent->RegisterComponent();
	}
	LavaComponent->StartLava(SurfacePoint);
}
