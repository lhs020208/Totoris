
#include "TotorisBlockGeneratorComponent.h"

#include "Components/InputComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

UTotorisBlockGeneratorComponent::UTotorisBlockGeneratorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/Totoris/Materials/M_Mino.M_Mino"));
	CubeMesh = Cube.Object;
	BlockMaterial = Material.Object;
	InitializeDefaultKeyBindings();
}

void UTotorisBlockGeneratorComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!ensure(CubeMesh && BlockMaterial && GetOwner()->GetRootComponent())) return;
	BuildRenderComponents();
	Sequence.Initialize(bUseFixedSeed ? FixedSeed : FMath::Rand());
	DebugRestartCount = 0;
	bGameplayActive = false;
	SetGameplayVisible(false);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		InputController = PC;
		RebuildInputBindings();
	}
}

void UTotorisBlockGeneratorComponent::ConfigureClassicGame(
	const FTotorisClassicSettings& Settings, bool bInStartGravity, bool bInGravityIncrease, bool bInCheeseGarbage)
{
	ClassicSettings = Settings;
	ClassicSettings.TargetLines = FMath::Clamp(Settings.TargetLines, 1, 1000);
	ClassicSettings.LimitTimeSeconds = FMath::Clamp(Settings.LimitTimeSeconds, 1, 359);
	ClassicSettings.CheeseCount = FMath::Clamp(Settings.CheeseCount, 1, 100);
	bConfiguredStartGravity = bInStartGravity;
	bConfiguredGravityIncrease = bInGravityIncrease;
	bIncomingCheeseGarbage = bInCheeseGarbage;
}

void UTotorisBlockGeneratorComponent::CompleteRun(ETotorisRunResult Result)
{
	if (RunResult != ETotorisRunResult::None) return;
	RunResult = Result;
	bGameOver = true;
	UE_LOG(LogTemp, Display, TEXT("Totoris classic run ended: mode=%d result=%d elapsed_seconds=%.3f pieces=%d lines=%d"),
		static_cast<int32>(ClassicSettings.Mode), static_cast<int32>(Result),
		ElapsedSeconds, PlacedPieceCount, TotalClearedLines);
}

void UTotorisBlockGeneratorComponent::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);
	if (bGameplayActive && !bGameOver)
	{
		const double RemainingTime = ClassicSettings.Mode == ETotorisClassicMode::Blitz
			? FMath::Max(0.0, static_cast<double>(ClassicSettings.LimitTimeSeconds) - ElapsedSeconds)
			: static_cast<double>(DeltaSeconds);
		ElapsedSeconds += FMath::Min(static_cast<double>(DeltaSeconds), RemainingTime);
		if (ClassicSettings.Mode == ETotorisClassicMode::Blitz &&
			ElapsedSeconds >= static_cast<double>(ClassicSettings.LimitTimeSeconds))
		{
			CompleteRun(ETotorisRunResult::TimeExpired);
			RebuildRender();
			return;
		}
		TickHorizontalHandling(DeltaSeconds);
		TickGravity(DeltaSeconds);
	}
}

void UTotorisBlockGeneratorComponent::StartGame()
{
	if (!HasBegunPlay() || Bodies.Num() != 7 || Faces.Num() != 7)
	{
		UE_LOG(LogTemp, Warning, TEXT("Totoris StartGame ignored: render components are not ready"));
		return;
	}

	// Start every menu-launched game from a clean board and a fresh seven-bag.
	Sequence.Initialize(bUseFixedSeed ? FixedSeed : FMath::Rand());
	DebugRestartCount = 0;
	bGameplayActive = true;
	SetGameplayVisible(true);
	SpawnFirstAndPreview();

	UE_LOG(LogTemp, Display, TEXT("Totoris gameplay started"));
}

void UTotorisBlockGeneratorComponent::StopGame()
{
	bGameplayActive = false;

	bSoftDropHeld = false;
	bLeftHeld = false;
	bRightHeld = false;
	ActiveHorizontalDirection = 0;
	HorizontalHeldSeconds = 0.f;
	HorizontalARRAccumulator = 0.f;
	DCDRemainingSeconds = 0.f;
	GravityAccumulator = 0.f;
	LockTimer = 0.f;
	LockResets = 0;

	LockedCells.Reset();
	LockedTypes.Reset();
	CheeseCells.Reset();
	PendingGarbageSegments.Reset();
	CheeseRowsOnBoard = 0;
	NextPieceNames.Reset();
	ActiveSpawnCells.Reset();
	ActivePieceName.Empty();

	// RebuildRender clears all dynamic mino instances and returns immediately
	// while gameplay is inactive.
	RebuildRender();
	SetGameplayVisible(false);
}

void UTotorisBlockGeneratorComponent::SetGameplayVisible(bool bVisible)
{
	auto SetMeshArrayVisible = [bVisible](const TArray<TObjectPtr<UInstancedStaticMeshComponent>>& Meshes)
		{
			for (const TObjectPtr<UInstancedStaticMeshComponent>& Mesh : Meshes)
			{
				if (IsValid(Mesh))
				{
					Mesh->SetVisibility(bVisible, true);
					Mesh->SetHiddenInGame(!bVisible, true);
				}
			}
		};

	SetMeshArrayVisible(Bodies);
	SetMeshArrayVisible(Faces);
	SetMeshArrayVisible(GhostBodies);
	SetMeshArrayVisible(GhostFaces);
}

void UTotorisBlockGeneratorComponent::ApplyHandlingSettings(
	int32 InARRMilliseconds,
	int32 InDASMilliseconds,
	int32 InDCDMilliseconds,
	int32 InSDFMultiplier,
	bool bInSDFInfinite)
{
	HorizontalARRMilliseconds = FMath::Clamp(InARRMilliseconds, 0, 83);
	HorizontalDASMilliseconds = FMath::Clamp(InDASMilliseconds, 17, 333);
	HorizontalDCDMilliseconds = FMath::Clamp(InDCDMilliseconds, 0, 333);
	SoftDropMultiplier = FMath::Clamp(InSDFMultiplier, 5, 40);
	bSoftDropInfinite = bInSDFInfinite;

	// Clear accumulated timing so a large old accumulator cannot leak across
	// a live settings change.
	HorizontalARRAccumulator = 0.f;
	DCDRemainingSeconds = FMath::Min(
		DCDRemainingSeconds,
		HorizontalDCDMilliseconds * 0.001f);
	GravityAccumulator = 0.f;

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Totoris handling applied: ARR=%dms DAS=%dms DCD=%dms SDF=%s"),
		HorizontalARRMilliseconds,
		HorizontalDASMilliseconds,
		HorizontalDCDMilliseconds,
		bSoftDropInfinite
		? TEXT("inf")
		: *FString::Printf(TEXT("%dX"), SoftDropMultiplier));
}

void UTotorisBlockGeneratorComponent::ApplyKeyBindings(
	const TArray<FKey>& InMoveLeftKeys,
	const TArray<FKey>& InMoveRightKeys,
	const TArray<FKey>& InSoftDropKeys,
	const TArray<FKey>& InHardDropKeys,
	const TArray<FKey>& InRotateCWKeys,
	const TArray<FKey>& InRotateCCWKeys,
	const TArray<FKey>& InRotate180Keys,
	const TArray<FKey>& InHoldKeys)
{
	MoveLeftKeys = InMoveLeftKeys;
	MoveRightKeys = InMoveRightKeys;
	SoftDropKeys = InSoftDropKeys;
	HardDropKeys = InHardDropKeys;
	RotateCWKeys = InRotateCWKeys;
	RotateCCWKeys = InRotateCCWKeys;
	Rotate180Keys = InRotate180Keys;
	HoldKeys = InHoldKeys;

	RebuildInputBindings();
}

void UTotorisBlockGeneratorComponent::InitializeDefaultKeyBindings()
{
	MoveLeftKeys = { EKeys::Left };
	MoveRightKeys = { EKeys::Right };
	SoftDropKeys = { EKeys::Down };
	HardDropKeys = { EKeys::SpaceBar };
	RotateCWKeys = { EKeys::X, EKeys::Up };
	RotateCCWKeys = { EKeys::Z, EKeys::LeftControl };
	Rotate180Keys = { EKeys::A };
	HoldKeys = { EKeys::C, EKeys::LeftShift };
}

void UTotorisBlockGeneratorComponent::RebuildInputBindings()
{
	if (!InputController.IsValid() || !IsValid(GetOwner()))
	{
		return;
	}

	APlayerController* PC = InputController.Get();
	if (RestartInput)
	{
		PC->PopInputComponent(RestartInput);
		RestartInput->DestroyComponent();
		RestartInput = nullptr;
	}

	RestartInput = NewObject<UInputComponent>(GetOwner());
	if (!ensure(RestartInput))
	{
		return;
	}

	RestartInput->RegisterComponent();

	auto BindPressed = [this](const TArray<FKey>& Keys, void (UTotorisBlockGeneratorComponent::* Handler)())
		{
			for (const FKey& Key : Keys)
			{
				if (Key.IsValid())
				{
					RestartInput->BindKey(Key, IE_Pressed, this, Handler);
				}
			}
		};

	auto BindPressedReleased = [this](
		const TArray<FKey>& Keys,
		void (UTotorisBlockGeneratorComponent::* PressedHandler)(),
		void (UTotorisBlockGeneratorComponent::* ReleasedHandler)())
		{
			for (const FKey& Key : Keys)
			{
				if (Key.IsValid())
				{
					RestartInput->BindKey(Key, IE_Pressed, this, PressedHandler);
					RestartInput->BindKey(Key, IE_Released, this, ReleasedHandler);
				}
			}
		};

	// Keep the existing R debug restart shortcut when R is unused by gameplay.
	// A user binding takes priority, so R remains available as a normal key.
	bool bRUsedByGameplay = false;
	const TArray<const TArray<FKey>*> AllBindingArrays =
	{
		&MoveLeftKeys,
		&MoveRightKeys,
		&SoftDropKeys,
		&HardDropKeys,
		&RotateCWKeys,
		&RotateCCWKeys,
		&Rotate180Keys,
		&HoldKeys
	};

	for (const TArray<FKey>* Keys : AllBindingArrays)
	{
		if (Keys && Keys->Contains(EKeys::R))
		{
			bRUsedByGameplay = true;
			break;
		}
	}

	if (!bRUsedByGameplay)
	{
		RestartInput->BindKey(EKeys::R, IE_Pressed, this, &UTotorisBlockGeneratorComponent::DebugRestart);
	}

	BindPressedReleased(MoveLeftKeys, &UTotorisBlockGeneratorComponent::HorizontalLeftPressed, &UTotorisBlockGeneratorComponent::HorizontalLeftReleased);
	BindPressedReleased(MoveRightKeys, &UTotorisBlockGeneratorComponent::HorizontalRightPressed, &UTotorisBlockGeneratorComponent::HorizontalRightReleased);
	BindPressedReleased(SoftDropKeys, &UTotorisBlockGeneratorComponent::SoftDropPressed, &UTotorisBlockGeneratorComponent::SoftDropReleased);
	BindPressed(HardDropKeys, &UTotorisBlockGeneratorComponent::HardDrop);
	BindPressed(RotateCWKeys, &UTotorisBlockGeneratorComponent::RotateCW);
	BindPressed(RotateCCWKeys, &UTotorisBlockGeneratorComponent::RotateCCW);

	BindPressed(Rotate180Keys, &UTotorisBlockGeneratorComponent::Rotate180);
	BindPressed(HoldKeys, &UTotorisBlockGeneratorComponent::Hold);

	PC->PushInputComponent(RestartInput);
}

void UTotorisBlockGeneratorComponent::AddBlock(ETotorisMino Type, float Right, float Up)
{
	const int32 Index = static_cast<uint8>(Type);

	// /Engine/BasicShapes/Cube is 100 UU per side.
	// The body exactly fills one board cell, so adjacent blocks touch
	// without exposing the background between them.
	const float BodyScale = CellSize * 0.01f;

	// Leave a small dark border around the colored front face.
	// With CellSize = 10 and OutlineThickness = 0.4:
	//
	// Body size = 10.0
	// Face size =  9.2
	//
	// This produces a 0.4 UU outline on each side while keeping the
	// actual block body at the full cell size.
	const float OutlineThickness = 0.4f;
	const float FaceSize = FMath::Max(CellSize - OutlineThickness * 2.0f, 0.1f);
	const float FaceScale = FaceSize * 0.01f;

	// Dark body: exactly fills the entire cell.
	Bodies[Index]->AddInstance(
		FTransform(
			FQuat::Identity,
			FVector(-5.f, Right, Up),
			FVector(0.02f, BodyScale, BodyScale)
		)
	);

	// Colored front face: slightly smaller than the body.
	// The exposed body around it becomes the per-block outline.
	Faces[Index]->AddInstance(
		FTransform(
			FQuat::Identity,
			FVector(-6.1f, Right, Up),
			FVector(0.004f, FaceScale, FaceScale)
		)
	);
}

void UTotorisBlockGeneratorComponent::BuildRenderComponents()
{
	auto CreateRenderComponent =
		[this](
			const FName& ComponentName,
			ETotorisMino Type,
			float ColorMultiplier)
		-> UInstancedStaticMeshComponent*
		{
			UInstancedStaticMeshComponent* Mesh =
				NewObject<UInstancedStaticMeshComponent>(
					GetOwner(),
					ComponentName
				);

			Mesh->SetupAttachment(GetOwner()->GetRootComponent());
			Mesh->SetStaticMesh(CubeMesh);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCastShadow(false);
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->RegisterComponent();

			UMaterialInstanceDynamic* Material =
				UMaterialInstanceDynamic::Create(
					BlockMaterial,
					Mesh
				);

			Material->SetVectorParameterValue(
				TEXT("Color"),
				TotorisGeneration::Color(Type) * ColorMultiplier
			);

			Mesh->SetMaterial(0, Material);

			return Mesh;
		};

	for (uint8 Index = 0; Index < 7; ++Index)
	{
		const ETotorisMino Type =
			static_cast<ETotorisMino>(Index);

		const FString TypeName =
			TotorisGeneration::Name(Type);

		Bodies.Add(
			CreateRenderComponent(
				FName(*FString::Printf(
					TEXT("Mino_%s_Bodies"),
					*TypeName)),
				Type,
				0.35f
			)
		);

		Faces.Add(
			CreateRenderComponent(
				FName(*FString::Printf(
					TEXT("Mino_%s_Faces"),
					*TypeName)),
				Type,
				1.0f
			)
		);

		// Ghost uses the same mesh/material,
		// but much darker colors.
		GhostBodies.Add(
			CreateRenderComponent(
				FName(*FString::Printf(
					TEXT("Ghost_%s_Bodies"),
					*TypeName)),
				Type,
				0.08f
			)
		);

		GhostFaces.Add(
			CreateRenderComponent(
				FName(*FString::Printf(
					TEXT("Ghost_%s_Faces"),
					*TypeName)),
				Type,
				0.25f
			)
		);
	}
}

void UTotorisBlockGeneratorComponent::SpawnFirstAndPreview()
{
	LockedCells.Reset();
	LockedTypes.Reset();
	CheeseCells.Reset();
	PendingGarbageSegments.Reset();
	CheeseRowsOnBoard = 0;
	GarbageRandom.Initialize(bUseFixedSeed ? FixedSeed ^ 0x5A17 : FMath::Rand());
	RunResult = ETotorisRunResult::None;
	ElapsedSeconds = 0.0;
	PlacedPieceCount = 0;
	Score = 0;
	RemainingSprintLines = ClassicSettings.Mode == ETotorisClassicMode::Sprint
		? ClassicSettings.TargetLines : 0;
	RemainingCheeseLines = ClassicSettings.Mode == ETotorisClassicMode::CheeseRace
		? ClassicSettings.CheeseCount : 0;
	QueuedCheeseLines = 0;
	CheeseHoleColumn = INDEX_NONE;

	bGameOver = false;
	if (ClassicSettings.Mode == ETotorisClassicMode::CheeseRace)
	{
		InitializeCheeseBoard();
	}

	bHasHold = false;
	bCanHold = true;

	GravityAccumulator = 0.f;

	HorizontalHeldSeconds = 0.f;
	HorizontalARRAccumulator = 0.f;
	DCDRemainingSeconds = 0.f;
	ActiveHorizontalDirection = 0;

	LastSpinKind = ETotorisSpinKind::None;
	LastSpinMino = ETotorisMino::I;

	LastClearedLineCount = 0;
	TotalClearedLines = 0;

	LastActionName = TEXT("None");

	ComboCount = -1;
	BackToBackCount = 0;

	bLastClearWasBackToBack = false;
	bLastClearWasDifficult = false;
	bLastPerfectClear = false;

	DifficultClearStreak = 0;

	LockTimer = 0.f;
	LockResets = 0;

	FirstBagOrder =
		TotorisGeneration::BagName(
			Sequence.GetFirstBag()
		);

	SpawnMino(Sequence.Draw());
	RebuildRender();
}

TArray<FIntPoint> UTotorisBlockGeneratorComponent::ActiveCells() const
{
	TArray<FIntPoint> Cells = TotorisGeneration::RotationCells(ActiveMino, ActiveRotation);
	for (FIntPoint& Cell : Cells) Cell += ActivePosition;
	return Cells;
}

FIntPoint UTotorisBlockGeneratorComponent::GetGhostPosition() const
{
	FIntPoint GhostPosition = ActivePosition;

	while (IsValidPosition(
		ActiveMino,
		GhostPosition + FIntPoint(0, -1),
		ActiveRotation))
	{
		--GhostPosition.Y;
	}

	return GhostPosition;
}

bool UTotorisBlockGeneratorComponent::IsValidPosition(ETotorisMino Type, const FIntPoint& Position, uint8 Rotation) const
{
	for (FIntPoint Cell : TotorisGeneration::RotationCells(Type, Rotation))
	{
		Cell += Position;
		if (Cell.X < 0 || Cell.X >= TotorisGeneration::BoardWidth || Cell.Y < 1 || Cell.Y > MaxLogicalRows) return false;
		if (LockedCells.Contains(Cell)) return false;
	}
	return true;
}

void UTotorisBlockGeneratorComponent::SpawnMino(ETotorisMino Type)
{
	ActiveMino = Type;
	ActiveRotation = 0;
	ActivePosition = TotorisGeneration::SpawnPosition(Type);
	ActiveColumn = ActivePosition.X;
	ActiveRow = ActivePosition.Y;
	ActivePieceName = TotorisGeneration::Name(Type);
	ActiveSpawnCells = ActiveCells();
	bGrounded = false;
	LockTimer = 0.f;
	LockResets = 0;
	GravityAccumulator = 0.f;
	ResetActiveActionTracking();
	StartDCD();
	if (!IsValidPosition(ActiveMino, ActivePosition, ActiveRotation)) SetGameOver();
}

void UTotorisBlockGeneratorComponent::ResetActiveActionTracking()
{
	bLastActionWasRotation = false;
	bLastRotationWas180 = false;
	LastRotationKickIndex = INDEX_NONE;
}

void UTotorisBlockGeneratorComponent::MarkTranslation()
{
	bLastActionWasRotation = false;
	bLastRotationWas180 = false;
	LastRotationKickIndex = INDEX_NONE;
}

void UTotorisBlockGeneratorComponent::MarkRotation(bool bWas180, int32 KickIndex)
{
	bLastActionWasRotation = true;
	bLastRotationWas180 = bWas180;
	LastRotationKickIndex = KickIndex;
}

void UTotorisBlockGeneratorComponent::RebuildRender()
{
	for (const auto& Mesh : Bodies)
		if (IsValid(Mesh)) Mesh->ClearInstances();

	for (const auto& Mesh : Faces)
		if (IsValid(Mesh)) Mesh->ClearInstances();

	for (const auto& Mesh : GhostBodies)
		if (IsValid(Mesh)) Mesh->ClearInstances();

	for (const auto& Mesh : GhostFaces)
		if (IsValid(Mesh)) Mesh->ClearInstances();

	if (!bGameplayActive)
	{
		return;
	}

	ActiveSpawnCells = ActiveCells();
	ActiveColumn = ActivePosition.X;
	ActiveRow = ActivePosition.Y;

	// Locked blocks
	for (const auto& Pair : LockedTypes)
	{
		AddLogicalBlock(
			Pair.Value,
			Pair.Key
		);
	}

	if (!bGameOver)
	{
		// --------------------------------
		// Ghost piece
		// --------------------------------

		const FIntPoint GhostPosition =
			GetGhostPosition();

		// If the piece is already at the landing position,
		// do not draw a duplicate ghost over the active piece.
		if (GhostPosition != ActivePosition)
		{
			TArray<FIntPoint> GhostCells =
				TotorisGeneration::RotationCells(
					ActiveMino,
					ActiveRotation
				);

			for (FIntPoint& Cell : GhostCells)
			{
				Cell += GhostPosition;

				AddGhostBlock(
					ActiveMino,
					Cell
				);
			}
		}

		// --------------------------------
		// Active piece
		// --------------------------------

		for (const FIntPoint& Cell : ActiveCells())
		{
			AddLogicalBlock(
				ActiveMino,
				Cell
			);
		}
	}
	NextPieceNames.Reset();

	const TArray<ETotorisMino> Next = Sequence.Preview(TotorisGeneration::NextCount);
	for (int32 Slot = 0; Slot < Next.Num(); ++Slot)
	{
		NextPieceNames.Add(TotorisGeneration::Name(Next[Slot]));
		DrawPreviewPiece(Next[Slot], FVector2D(NextCenter.X, NextCenter.Y + (2 - Slot) * NextSlotSpacing));
	}
	if (bHasHold) DrawPreviewPiece(HeldMino, HoldCenter);
}

void UTotorisBlockGeneratorComponent::AddGhostBlock(
	ETotorisMino Type,
	const FIntPoint& Cell)
{
	const int32 Index = static_cast<uint8>(Type);

	const float Right =
		(Cell.X + 0.5f -
			TotorisGeneration::BoardWidth / 2.f)
		* CellSize;

	const float Up =
		(Cell.Y - 0.5f -
			TotorisGeneration::BoardHeight / 2.f)
		* CellSize;

	const float BodyScale = CellSize * 0.01f;

	const float OutlineThickness = 0.4f;
	const float FaceSize =
		FMath::Max(
			CellSize - OutlineThickness * 2.0f,
			0.1f);

	const float FaceScale =
		FaceSize * 0.01f;

	GhostBodies[Index]->AddInstance(
		FTransform(
			FQuat::Identity,
			FVector(-5.f, Right, Up),
			FVector(
				0.02f,
				BodyScale,
				BodyScale)
		)
	);

	GhostFaces[Index]->AddInstance(
		FTransform(
			FQuat::Identity,
			FVector(-6.1f, Right, Up),
			FVector(
				0.004f,
				FaceScale,
				FaceScale)
		)
	);
}

void UTotorisBlockGeneratorComponent::AddLogicalBlock(ETotorisMino Type, const FIntPoint& Cell)
{
	AddBlock(Type, (Cell.X + 0.5f - TotorisGeneration::BoardWidth / 2.f) * CellSize,
		(Cell.Y - 0.5f - TotorisGeneration::BoardHeight / 2.f) * CellSize);
}

void UTotorisBlockGeneratorComponent::DrawPreviewPiece(ETotorisMino Type, const FVector2D& Center)
{
	const TArray<FIntPoint> Shape = TotorisGeneration::Shape(Type);
	FIntPoint Max(0, 0);
	for (const FIntPoint& Cell : Shape) { Max.X = FMath::Max(Max.X, Cell.X); Max.Y = FMath::Max(Max.Y, Cell.Y); }
	for (const FIntPoint& Cell : Shape)
	{
		AddBlock(Type, Center.X + (Cell.X - Max.X / 2.f) * CellSize,
			Center.Y + (Cell.Y - Max.Y / 2.f) * CellSize);
	}
}

void UTotorisBlockGeneratorComponent::UpdateGroundedState()
{
	const bool bWasGrounded = bGrounded;
	bGrounded = !IsValidPosition(ActiveMino, ActivePosition + FIntPoint(0, -1), ActiveRotation);
	if (bGrounded && !bWasGrounded) LockTimer = 0.f;
	if (!bGrounded) LockTimer = 0.f;
}

void UTotorisBlockGeneratorComponent::MoveHorizontal(int32 Direction)
{
	if (!bGameplayActive || bGameOver) return;
	const FIntPoint Candidate = ActivePosition + FIntPoint(Direction, 0);
	if (!IsValidPosition(ActiveMino, Candidate, ActiveRotation)) return;
	const bool bWasGrounded = bGrounded;
	ActivePosition = Candidate;
	ActiveColumn = ActivePosition.X;
	MarkTranslation();
	UpdateGroundedState();
	if (bWasGrounded && bGrounded && LockResets < 15) { LockTimer = 0.f; ++LockResets; }
	RebuildRender();
}

void UTotorisBlockGeneratorComponent::MoveHorizontalToWall(int32 Direction)
{
	if (!bGameplayActive || bGameOver || Direction == 0) return;

	const bool bWasGrounded = bGrounded;
	bool bMoved = false;

	// BoardWidth iterations are enough to reach either wall from any legal
	// position and also avoid an accidental unbounded loop.
	for (int32 Step = 0; Step < TotorisGeneration::BoardWidth; ++Step)
	{
		const FIntPoint Candidate = ActivePosition + FIntPoint(Direction, 0);
		if (!IsValidPosition(ActiveMino, Candidate, ActiveRotation))
		{
			break;
		}

		ActivePosition = Candidate;
		bMoved = true;
	}

	if (!bMoved)
	{
		return;
	}

	ActiveColumn = ActivePosition.X;
	MarkTranslation();
	UpdateGroundedState();

	if (bWasGrounded && bGrounded && LockResets < 15)
	{
		LockTimer = 0.f;
		++LockResets;
	}

	RebuildRender();
}

void UTotorisBlockGeneratorComponent::HorizontalLeftPressed()
{
	if (!bGameplayActive || bGameOver) return;
	bLeftHeld = true;
	ActiveHorizontalDirection = -1;
	HorizontalHeldSeconds = 0.f;
	HorizontalARRAccumulator = 0.f;
	MoveHorizontal(-1);
}

void UTotorisBlockGeneratorComponent::HorizontalLeftReleased()
{
	bLeftHeld = false;
	if (ActiveHorizontalDirection == -1)
	{
		ActiveHorizontalDirection = bRightHeld ? 1 : 0;
		HorizontalHeldSeconds = 0.f;
		HorizontalARRAccumulator = 0.f;
	}
}

void UTotorisBlockGeneratorComponent::HorizontalRightPressed()
{
	if (!bGameplayActive || bGameOver) return;
	bRightHeld = true;
	ActiveHorizontalDirection = 1;
	HorizontalHeldSeconds = 0.f;
	HorizontalARRAccumulator = 0.f;
	MoveHorizontal(1);
}

void UTotorisBlockGeneratorComponent::HorizontalRightReleased()
{
	bRightHeld = false;
	if (ActiveHorizontalDirection == 1)
	{
		ActiveHorizontalDirection = bLeftHeld ? -1 : 0;
		HorizontalHeldSeconds = 0.f;
		HorizontalARRAccumulator = 0.f;
	}
}

void UTotorisBlockGeneratorComponent::TickHorizontalHandling(float DeltaSeconds)
{
	if (ActiveHorizontalDirection == 0) return;

	HorizontalHeldSeconds += FMath::Max(0.f, DeltaSeconds);

	const float PreviousDCD = DCDRemainingSeconds;
	DCDRemainingSeconds = FMath::Max(0.f, DCDRemainingSeconds - DeltaSeconds);
	if (DCDRemainingSeconds > 0.f) return;

	const float ActiveDelta = FMath::Max(0.f, DeltaSeconds - PreviousDCD);
	const float DASSeconds = HorizontalDASMilliseconds * 0.001f;

	if (HorizontalHeldSeconds < DASSeconds) return;

	// ARR 0 is instant auto-repeat: after DAS has charged, move as far as
	// possible in the held direction on every eligible tick.
	if (HorizontalARRMilliseconds == 0)
	{
		HorizontalARRAccumulator = 0.f;
		MoveHorizontalToWall(ActiveHorizontalDirection);
		return;
	}

	const float ARRSeconds = HorizontalARRMilliseconds * 0.001f;

	if (HorizontalHeldSeconds - ActiveDelta < DASSeconds)
	{
		MoveHorizontal(ActiveHorizontalDirection);
		HorizontalARRAccumulator = 0.f;
	}
	else
	{
		HorizontalARRAccumulator += ActiveDelta;
	}

	while (HorizontalARRAccumulator >= ARRSeconds)
	{
		HorizontalARRAccumulator -= ARRSeconds;
		MoveHorizontal(ActiveHorizontalDirection);
	}
}

void UTotorisBlockGeneratorComponent::StartDCD()
{
	DCDRemainingSeconds = HorizontalDCDMilliseconds * 0.001f;
}

void UTotorisBlockGeneratorComponent::Rotate(int32 Direction)
{
	if (!bGameplayActive || bGameOver) return;
	const uint8 CandidateRotation = static_cast<uint8>((ActiveRotation + (Direction > 0 ? 1 : 3)) & 3);
	const TArray<FIntPoint> Kicks = TotorisGeneration::RotationKicks(ActiveMino, ActiveRotation, CandidateRotation);
	FIntPoint AcceptedPosition;
	bool bAccepted = false;
	int32 AcceptedKickIndex = INDEX_NONE;
	for (int32 KickIndex = 0; KickIndex < Kicks.Num(); ++KickIndex)
	{
		const FIntPoint& Kick = Kicks[KickIndex];
		const FIntPoint CandidatePosition = ActivePosition + Kick;
		if (IsValidPosition(ActiveMino, CandidatePosition, CandidateRotation))
		{
			AcceptedPosition = CandidatePosition;
			bAccepted = true;
			AcceptedKickIndex = KickIndex;
			break;
		}
	}
	if (!bAccepted) return;
	const bool bWasGrounded = bGrounded;
	ActivePosition = AcceptedPosition;
	ActiveRotation = CandidateRotation;
	MarkRotation(false, AcceptedKickIndex);
	UpdateGroundedState();
	StartDCD();
	if (bWasGrounded && bGrounded && LockResets < 15) { LockTimer = 0.f; ++LockResets; }
	RebuildRender();
}

void UTotorisBlockGeneratorComponent::RotateCCW() { Rotate(-1); }
void UTotorisBlockGeneratorComponent::RotateCW() { Rotate(1); }

void UTotorisBlockGeneratorComponent::Rotate180()
{
	if (!bGameplayActive || bGameOver) return;
	const uint8 CandidateRotation = static_cast<uint8>((ActiveRotation + 2) & 3);
	const TArray<FIntPoint> Kicks = TotorisGeneration::RotationKicks180(ActiveMino, ActiveRotation, CandidateRotation);
	FIntPoint AcceptedPosition;
	bool bAccepted = false;
	int32 AcceptedKickIndex = INDEX_NONE;
	for (int32 KickIndex = 0; KickIndex < Kicks.Num(); ++KickIndex)
	{
		const FIntPoint& Kick = Kicks[KickIndex];
		const FIntPoint CandidatePosition = ActivePosition + Kick;
		if (IsValidPosition(ActiveMino, CandidatePosition, CandidateRotation))
		{
			AcceptedPosition = CandidatePosition;
			bAccepted = true;
			AcceptedKickIndex = KickIndex;
			break;
		}
	}
	if (!bAccepted) return;
	const bool bWasGrounded = bGrounded;
	ActivePosition = AcceptedPosition;
	ActiveRotation = CandidateRotation;
	MarkRotation(true, AcceptedKickIndex);
	UpdateGroundedState();
	StartDCD();
	if (bWasGrounded && bGrounded && LockResets < 15) { LockTimer = 0.f; ++LockResets; }
	RebuildRender();
}

void UTotorisBlockGeneratorComponent::SoftDropPressed() { if (bGameplayActive && !bGameOver) bSoftDropHeld = true; }
void UTotorisBlockGeneratorComponent::SoftDropReleased() { bSoftDropHeld = false; }

void UTotorisBlockGeneratorComponent::TickGravity(float DeltaSeconds)
{
	if (bSoftDropHeld && bSoftDropInfinite)
	{

		const FIntPoint StartingPosition = ActivePosition;

		while (IsValidPosition(
			ActiveMino,
			ActivePosition + FIntPoint(0, -1),
			ActiveRotation))
		{
			--ActivePosition.Y;
		}

		ActiveRow = ActivePosition.Y;

		if (ActivePosition != StartingPosition)
		{
			MarkTranslation();
		}

		GravityAccumulator = 0.f;
		UpdateGroundedState();
	}
	else
	{
		const float BaseSpeed = bConfiguredStartGravity ? GravityCellsPerSecond : 0.f;
		const float FallSpeed = BaseSpeed + (bConfiguredGravityIncrease
			? GravityIncreaseCellsPerSecondSquared * static_cast<float>(ElapsedSeconds) : 0.f);
		const float EffectiveFallSpeed = bSoftDropHeld
			? FMath::Max(GravityCellsPerSecond, FallSpeed) * static_cast<float>(SoftDropMultiplier)
			: FallSpeed;

		GravityAccumulator += DeltaSeconds * EffectiveFallSpeed;

		while (GravityAccumulator >= 1.f && !bGameOver)
		{
			GravityAccumulator -= 1.f;
			const FIntPoint Candidate = ActivePosition + FIntPoint(0, -1);

			if (IsValidPosition(ActiveMino, Candidate, ActiveRotation))
			{
				ActivePosition = Candidate;
				ActiveRow = ActivePosition.Y;
				MarkTranslation();
				UpdateGroundedState();
			}
			else
			{
				UpdateGroundedState();
				break;
			}
		}
	}

	if (bGrounded && !bGameOver)
	{
		LockTimer += DeltaSeconds;
		if (LockTimer >= LockDelaySeconds)
		{
			LockActiveMino();
		}
	}

	RebuildRender();
}

void UTotorisBlockGeneratorComponent::HardDrop()
{
	if (!bGameplayActive || bGameOver) return;
	const FIntPoint StartingPosition = ActivePosition;
	while (IsValidPosition(ActiveMino, ActivePosition + FIntPoint(0, -1), ActiveRotation))
	{
		ActivePosition.Y--;
	}
	ActiveRow = ActivePosition.Y;
	if (ActivePosition != StartingPosition) MarkTranslation();
	LockActiveMino();
}

void UTotorisBlockGeneratorComponent::Hold()
{
	if (!bGameplayActive || bGameOver || !bCanHold) return;
	const ETotorisMino Previous = ActiveMino;
	if (bHasHold)
	{
		ActiveMino = HeldMino;
		HeldMino = Previous;
		bCanHold = false;
		ActiveRotation = 0;
		ActivePosition = TotorisGeneration::SpawnPosition(ActiveMino);
		ActivePieceName = TotorisGeneration::Name(ActiveMino);
		ResetActiveActionTracking();
		StartDCD();
		UpdateGroundedState();
		if (!IsValidPosition(ActiveMino, ActivePosition, ActiveRotation)) SetGameOver();
	}
	else
	{
		HeldMino = Previous;
		bHasHold = true;
		bCanHold = false;
		SpawnMino(Sequence.Draw());
	}
	RebuildRender();
}

int32 UTotorisBlockGeneratorComponent::ClearCompletedLines()
{
	TArray<int32> CompletedRows;
	CompletedRows.Reserve(4);

	// The logical board includes the hidden spawn area as well as the
	// visible 20 rows. Any completely filled logical row is a valid clear.
	for (int32 Row = 1; Row <= MaxLogicalRows; ++Row)
	{
		bool bComplete = true;
		for (int32 Column = 0; Column < TotorisGeneration::BoardWidth; ++Column)
		{
			if (!LockedCells.Contains(FIntPoint(Column, Row)))
			{
				bComplete = false;
				break;
			}
		}

		if (bComplete)
		{
			CompletedRows.Add(Row);
		}
	}

	if (CompletedRows.Num() == 0)
	{
		return 0;
	}

	int32 ClearedCheeseRows = 0;
	for (int32 Row : CompletedRows)
	{
		for (int32 Column = 0; Column < TotorisGeneration::BoardWidth; ++Column)
		{
			if (CheeseCells.Contains(FIntPoint(Column, Row)))
			{
				++ClearedCheeseRows;
				break;
			}
		}
	}

	TSet<FIntPoint> NewCheeseCells;
	for (const FIntPoint& OldCell : CheeseCells)
	{
		if (CompletedRows.Contains(OldCell.Y)) continue;
		int32 ClearedBelow = 0;
		for (int32 Row : CompletedRows)
		{
			if (Row < OldCell.Y) ++ClearedBelow;
		}
		NewCheeseCells.Add(FIntPoint(OldCell.X, OldCell.Y - ClearedBelow));
	}
	CheeseCells = MoveTemp(NewCheeseCells);
	RemainingCheeseLines = FMath::Max(0, RemainingCheeseLines - ClearedCheeseRows);
	CheeseRowsOnBoard = FMath::Max(0, CheeseRowsOnBoard - ClearedCheeseRows);

	// Rebuild both containers together so the collision set and the
	// per-cell render type map can never disagree after a clear.
	TSet<FIntPoint> NewLockedCells;
	TMap<FIntPoint, ETotorisMino> NewLockedTypes;
	for (const auto& Pair : LockedTypes)
	{
		const FIntPoint OldCell = Pair.Key;

		if (CompletedRows.Contains(OldCell.Y))
		{
			continue;
		}

		int32 ClearedRowsBelow = 0;
		for (const int32 ClearedRow : CompletedRows)
		{
			if (ClearedRow < OldCell.Y)
			{
				++ClearedRowsBelow;
			}
		}

		const FIntPoint NewCell(OldCell.X, OldCell.Y - ClearedRowsBelow);
		NewLockedCells.Add(NewCell);
		NewLockedTypes.Add(NewCell, Pair.Value);
	}

	LockedCells = MoveTemp(NewLockedCells);
	LockedTypes = MoveTemp(NewLockedTypes);

	return CompletedRows.Num();
}

void UTotorisBlockGeneratorComponent::LockActiveMino()
{
	// Spin detection must happen before the active mino is added to LockedCells.
	LastSpinKind = TotorisGeneration::DetectSpin(
		ActiveMino,
		ActivePosition,
		ActiveRotation,
		bLastActionWasRotation,
		bLastRotationWas180,
		LastRotationKickIndex,
		LockedCells,
		MaxLogicalRows);
	LastSpinMino = ActiveMino;

	for (const FIntPoint& Cell : ActiveCells())
	{
		LockedCells.Add(Cell);
		LockedTypes.Add(Cell, ActiveMino);
	}

	++PlacedPieceCount;

	// Resolve the board before classifying Perfect Clear.
	LastClearedLineCount = ClearCompletedLines();
	TotalClearedLines += LastClearedLineCount;
	if (ClassicSettings.Mode == ETotorisClassicMode::Sprint)
	{
		RemainingSprintLines = FMath::Max(0, ClassicSettings.TargetLines - TotalClearedLines);
	}

	bLastPerfectClear =
		LastClearedLineCount > 0 &&
		LockedCells.Num() == 0;

	LastActionName = TotorisGeneration::ActionName(
		LastSpinMino,
		LastSpinKind,
		LastClearedLineCount);

	// TETR.IO-style combo index:
	// first consecutive clear = 0-combo, then 1, 2, ...
	if (LastClearedLineCount > 0)
	{
		++ComboCount;
	}
	else
	{
		ComboCount = -1;
	}

	// For this single-player ruleset, B2B follows TETR.IO's "difficult clear"
	// concept without implementing attack/Surge:
	// - any Spin that clears at least one line
	// - a four-line clear (Tetris)
	// - a Perfect Clear
	//
	// A no-line placement (including a no-line Spin) preserves the current
	// B2B chain but does not advance it. A normal Single/Double/Triple breaks it.
	bLastClearWasDifficult = TotorisGeneration::IsBackToBackEligible(
		LastSpinKind,
		LastClearedLineCount,
		bLastPerfectClear);

	bLastClearWasBackToBack = false;

	if (LastClearedLineCount > 0)
	{
		if (bLastClearWasDifficult)
		{
			bLastClearWasBackToBack = DifficultClearStreak > 0;
			++DifficultClearStreak;
			BackToBackCount = FMath::Max(0, DifficultClearStreak - 1);
		}
		else
		{
			DifficultClearStreak = 0;
			BackToBackCount = 0;
		}
	}

	// One concise gameplay event log per locked piece.
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Totoris action: %s piece=%s spin=%s lines=%d combo=%d b2b=%d b2bAction=%d perfectClear=%d totalLines=%d"),
		*LastActionName,
		*TotorisGeneration::Name(LastSpinMino),
		LastSpinKind == ETotorisSpinKind::Full ? TEXT("Full") :
		LastSpinKind == ETotorisSpinKind::Mini ? TEXT("Mini") : TEXT("None"),
		LastClearedLineCount,
		ComboCount,
		BackToBackCount,
		bLastClearWasBackToBack,
		bLastPerfectClear,
		TotalClearedLines);

	bCanHold = true;

	// Refill only after a subsequent placement without any line clear.
	// A completed target takes precedence over all pending garbage.
	if (ClassicSettings.Mode == ETotorisClassicMode::CheeseRace && RemainingCheeseLines == 0)
	{
		CompleteRun(ETotorisRunResult::Completed);
		RebuildRender();
		return;
	}

	if (ClassicSettings.Mode == ETotorisClassicMode::CheeseRace && LastClearedLineCount == 0)
	{
		RefillCheeseBoard();
	}
	// Incoming attacks are independent of the cheese-race nine-row limit.
	if (!bGameOver) ApplyPendingGarbage();

	if (bGameOver) // Cheese refill can top out the board.
	{
		RebuildRender();
		return;
	}
	if ((ClassicSettings.Mode == ETotorisClassicMode::Sprint && RemainingSprintLines == 0) ||
		(ClassicSettings.Mode == ETotorisClassicMode::CheeseRace && RemainingCheeseLines == 0))
	{
		CompleteRun(ETotorisRunResult::Completed);
		RebuildRender();
		return;
	}

	// No ARE / line-clear delay yet.
	SpawnMino(Sequence.Draw());
	RebuildRender();
}

void UTotorisBlockGeneratorComponent::QueueIncomingGarbage(int32 Lines)
{
	if (Lines > 0 && Lines <= 100000 && bGameplayActive && !bGameOver)
		PendingGarbageSegments.Add(Lines);
}

int32 UTotorisBlockGeneratorComponent::GetPendingGarbageLines() const
{
	int32 Total = 0;
	for (const int32 Segment : PendingGarbageSegments)
		Total += Segment;
	return Total;
}

int32 UTotorisBlockGeneratorComponent::RandomGarbageHole()
{
	return GarbageRandom.RandRange(0, TotorisGeneration::BoardWidth - 1);
}

// Insertion is atomic: an overflow never leaves a partially shifted board.
bool UTotorisBlockGeneratorComponent::InjectGarbageRow(int32 Hole, bool bCheeseRaceRow)
{
	for (const auto& Pair : LockedTypes)
	{
		if (Pair.Key.Y >= MaxLogicalRows)
		{
			CompleteRun(ETotorisRunResult::ToppedOut);
			return false;
		}
	}
	TSet<FIntPoint> ShiftedCells;
	TMap<FIntPoint, ETotorisMino> ShiftedTypes;
	TSet<FIntPoint> ShiftedCheese;
	for (const auto& Pair : LockedTypes)
	{
		const FIntPoint Shifted(Pair.Key.X, Pair.Key.Y + 1);
		ShiftedCells.Add(Shifted);
		ShiftedTypes.Add(Shifted, Pair.Value);
		if (CheeseCells.Contains(Pair.Key)) ShiftedCheese.Add(Shifted);
	}
	LockedCells = MoveTemp(ShiftedCells);
	LockedTypes = MoveTemp(ShiftedTypes);
	CheeseCells = MoveTemp(ShiftedCheese);
	for (int32 Column = 0; Column < TotorisGeneration::BoardWidth; ++Column)
	{
		if (Column == Hole) continue;
		const FIntPoint Cell(Column, 1);
		LockedCells.Add(Cell);
		LockedTypes.Add(Cell, ETotorisMino::O); // Existing temporary garbage visual.
		if (bCheeseRaceRow) CheeseCells.Add(Cell);
	}
	if (bCheeseRaceRow) ++CheeseRowsOnBoard;
	return true;
}

void UTotorisBlockGeneratorComponent::InitializeCheeseBoard()
{
	const int32 InitialRows = FMath::Min(ClassicSettings.CheeseCount, 9);
	QueuedCheeseLines = ClassicSettings.CheeseCount - InitialRows;
	for (int32 Index = 0; Index < InitialRows; ++Index)
	{
		if (!InjectGarbageRow(RandomGarbageHole(), true)) return;
	}
}

void UTotorisBlockGeneratorComponent::RefillCheeseBoard()
{
	while (QueuedCheeseLines > 0 && CheeseRowsOnBoard < 9 && !bGameOver)
	{
		if (!InjectGarbageRow(RandomGarbageHole(), true)) return;
		--QueuedCheeseLines;
	}
}

void UTotorisBlockGeneratorComponent::ApplyPendingGarbage()
{
	// Preserve each attack boundary unless Cheese Garbage is enabled.
	for (const int32 Segment : PendingGarbageSegments)
	{
		const int32 SharedHole = bIncomingCheeseGarbage ? INDEX_NONE : RandomGarbageHole();
		for (int32 Index = 0; Index < Segment; ++Index)
		{
			const int32 Hole = bIncomingCheeseGarbage ? RandomGarbageHole() : SharedHole;
			if (!InjectGarbageRow(Hole, false))
			{
				PendingGarbageSegments.Reset();
				return;
			}
		}
	}
	PendingGarbageSegments.Reset();
}

void UTotorisBlockGeneratorComponent::SetGameOver()
{
	CompleteRun(ETotorisRunResult::ToppedOut);
}

void UTotorisBlockGeneratorComponent::DebugRestart()
{
	if (!bGameplayActive || !HasBegunPlay() || Bodies.Num() != 7) return;
	Sequence.DebugRestart();
	++DebugRestartCount;
	LockedCells.Reset();
	LockedTypes.Reset();
	CheeseCells.Reset();
	bHasHold = false;
	bCanHold = true;
	bSoftDropHeld = false;
	bLeftHeld = false;
	bRightHeld = false;
	ActiveHorizontalDirection = 0;
	HorizontalHeldSeconds = 0.f;
	HorizontalARRAccumulator = 0.f;
	SpawnFirstAndPreview();
}

void UTotorisBlockGeneratorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (InputController.IsValid() && RestartInput) InputController->PopInputComponent(RestartInput);
	if (RestartInput) RestartInput->DestroyComponent();
	for (const auto& Mesh : Bodies)
		if (IsValid(Mesh))
			Mesh->DestroyComponent();

	for (const auto& Mesh : Faces)
		if (IsValid(Mesh))
			Mesh->DestroyComponent();

	for (const auto& Mesh : GhostBodies)
		if (IsValid(Mesh))
			Mesh->DestroyComponent();

	for (const auto& Mesh : GhostFaces)
		if (IsValid(Mesh))
			Mesh->DestroyComponent();

	Bodies.Reset();
	Faces.Reset();
	GhostBodies.Reset();
	GhostFaces.Reset();
	Super::EndPlay(EndPlayReason);
}
