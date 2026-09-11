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
}

void UTotorisBlockGeneratorComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!ensure(CubeMesh && BlockMaterial && GetOwner()->GetRootComponent())) return;
	BuildRenderComponents();
	Sequence.Initialize(bUseFixedSeed ? FixedSeed : FMath::Rand());
	DebugRestartCount = 0;
	SpawnFirstAndPreview();

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		InputController = PC;
		RestartInput = NewObject<UInputComponent>(GetOwner(), TEXT("TotorisRestartInput"));
		RestartInput->RegisterComponent();
		RestartInput->BindKey(EKeys::R, IE_Pressed, this, &UTotorisBlockGeneratorComponent::DebugRestart);
		RestartInput->BindKey(EKeys::Left, IE_Pressed, this, &UTotorisBlockGeneratorComponent::HorizontalLeftPressed);
		RestartInput->BindKey(EKeys::Left, IE_Released, this, &UTotorisBlockGeneratorComponent::HorizontalLeftReleased);
		RestartInput->BindKey(EKeys::Right, IE_Pressed, this, &UTotorisBlockGeneratorComponent::HorizontalRightPressed);
		RestartInput->BindKey(EKeys::Right, IE_Released, this, &UTotorisBlockGeneratorComponent::HorizontalRightReleased);
		RestartInput->BindKey(EKeys::Down, IE_Pressed, this, &UTotorisBlockGeneratorComponent::SoftDropPressed);
		RestartInput->BindKey(EKeys::Down, IE_Released, this, &UTotorisBlockGeneratorComponent::SoftDropReleased);
		RestartInput->BindKey(EKeys::SpaceBar, IE_Pressed, this, &UTotorisBlockGeneratorComponent::HardDrop);
		RestartInput->BindKey(EKeys::Z, IE_Pressed, this, &UTotorisBlockGeneratorComponent::RotateCCW);
		RestartInput->BindKey(EKeys::LeftControl, IE_Pressed, this, &UTotorisBlockGeneratorComponent::RotateCCW);
		RestartInput->BindKey(EKeys::X, IE_Pressed, this, &UTotorisBlockGeneratorComponent::RotateCW);
		RestartInput->BindKey(EKeys::Up, IE_Pressed, this, &UTotorisBlockGeneratorComponent::RotateCW);
		RestartInput->BindKey(EKeys::A, IE_Pressed, this, &UTotorisBlockGeneratorComponent::Rotate180);
		RestartInput->BindKey(EKeys::C, IE_Pressed, this, &UTotorisBlockGeneratorComponent::Hold);
		RestartInput->BindKey(EKeys::LeftShift, IE_Pressed, this, &UTotorisBlockGeneratorComponent::Hold);
		PC->PushInputComponent(RestartInput);
	}
}

void UTotorisBlockGeneratorComponent::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);
	if (!bGameOver)
	{
		TickHorizontalHandling(DeltaSeconds);
		TickGravity(DeltaSeconds);
	}
}

void UTotorisBlockGeneratorComponent::BuildRenderComponents()
{
	for (uint8 Index = 0; Index < 7; ++Index)
	{
		const ETotorisMino Type = static_cast<ETotorisMino>(Index);
		for (bool bFace : {false, true})
		{
			const FName ComponentName(*FString::Printf(TEXT("Mino_%s_%s"), *TotorisGeneration::Name(Type), bFace ? TEXT("Faces") : TEXT("Bodies")));
			UInstancedStaticMeshComponent* Mesh = NewObject<UInstancedStaticMeshComponent>(GetOwner(), ComponentName);
			Mesh->SetupAttachment(GetOwner()->GetRootComponent());
			Mesh->SetStaticMesh(CubeMesh);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCastShadow(false);
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->RegisterComponent();
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BlockMaterial, Mesh);
			Material->SetVectorParameterValue(TEXT("Color"), TotorisGeneration::Color(Type) * (bFace ? 1.f : 0.35f));
			Mesh->SetMaterial(0, Material);
			(bFace ? Faces : Bodies).Add(Mesh);
		}
	}
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

void UTotorisBlockGeneratorComponent::SpawnFirstAndPreview()
{
	LockedCells.Reset();
	LockedTypes.Reset();
	bGameOver = false;
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
	FirstBagOrder = TotorisGeneration::BagName(Sequence.GetFirstBag());
	SpawnMino(Sequence.Draw());
	RebuildRender();
}

TArray<FIntPoint> UTotorisBlockGeneratorComponent::ActiveCells() const
{
	TArray<FIntPoint> Cells = TotorisGeneration::RotationCells(ActiveMino, ActiveRotation);
	for (FIntPoint& Cell : Cells) Cell += ActivePosition;
	return Cells;
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
	ActiveSpawnCells = ActiveCells();
	ActiveColumn = ActivePosition.X;
	ActiveRow = ActivePosition.Y;
	for (const auto& Mesh : Bodies) Mesh->ClearInstances();
	for (const auto& Mesh : Faces) Mesh->ClearInstances();
	for (const auto& Pair : LockedTypes) AddLogicalBlock(Pair.Value, Pair.Key);
	if (!bGameOver)
	{
		for (const FIntPoint& Cell : ActiveCells()) AddLogicalBlock(ActiveMino, Cell);
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
	if (bGameOver) return;
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

void UTotorisBlockGeneratorComponent::HorizontalLeftPressed()
{
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
	if (HorizontalHeldSeconds < HorizontalDASSeconds) return;

	if (HorizontalHeldSeconds - ActiveDelta < HorizontalDASSeconds)
	{
		MoveHorizontal(ActiveHorizontalDirection);
		HorizontalARRAccumulator = 0.f;
	}
	else
	{
		HorizontalARRAccumulator += ActiveDelta;
	}

	while (HorizontalARRAccumulator >= HorizontalARRSeconds)
	{
		HorizontalARRAccumulator -= HorizontalARRSeconds;
		MoveHorizontal(ActiveHorizontalDirection);
	}
}

void UTotorisBlockGeneratorComponent::StartDCD()
{
	DCDRemainingSeconds = HorizontalDCDSeconds;
}

void UTotorisBlockGeneratorComponent::Rotate(int32 Direction)
{
	if (bGameOver) return;
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
	if (bGameOver) return;
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

void UTotorisBlockGeneratorComponent::SoftDropPressed() { bSoftDropHeld = true; }
void UTotorisBlockGeneratorComponent::SoftDropReleased() { bSoftDropHeld = false; }

void UTotorisBlockGeneratorComponent::TickGravity(float DeltaSeconds)
{
	GravityAccumulator += DeltaSeconds * (bSoftDropHeld ? SoftDropCellsPerSecond : GravityCellsPerSecond);
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
	if (bGrounded && !bGameOver)
	{
		LockTimer += DeltaSeconds;
		if (LockTimer >= LockDelaySeconds) LockActiveMino();
	}
	RebuildRender();
}

void UTotorisBlockGeneratorComponent::HardDrop()
{
	if (bGameOver) return;
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
	if (bGameOver || !bCanHold) return;
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

	// Resolve the board before classifying Perfect Clear.
	LastClearedLineCount = ClearCompletedLines();
	TotalClearedLines += LastClearedLineCount;

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

	// No ARE / line-clear delay yet.
	SpawnMino(Sequence.Draw());
	RebuildRender();
}

void UTotorisBlockGeneratorComponent::SetGameOver()
{
	bGameOver = true;
	UE_LOG(LogTemp, Warning, TEXT("Totoris game over: spawn position is blocked"));
}

void UTotorisBlockGeneratorComponent::DebugRestart()
{
	if (!HasBegunPlay() || Bodies.Num() != 7) return;
	Sequence.DebugRestart();
	++DebugRestartCount;
	LockedCells.Reset();
	LockedTypes.Reset();
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
	for (const auto& Mesh : Bodies) if (IsValid(Mesh)) Mesh->DestroyComponent();
	for (const auto& Mesh : Faces) if (IsValid(Mesh)) Mesh->DestroyComponent();
	Bodies.Reset();
	Faces.Reset();
	Super::EndPlay(EndPlayReason);
}
