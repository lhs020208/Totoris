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
	PrimaryComponentTick.bCanEverTick = false;
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
		PC->PushInputComponent(RestartInput);
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
	for (const auto& Mesh : Bodies) Mesh->ClearInstances();
	for (const auto& Mesh : Faces) Mesh->ClearInstances();
	FirstBagOrder = TotorisGeneration::BagName(Sequence.GetFirstBag());
	const ETotorisMino Active = Sequence.Draw();
	ActivePieceName = TotorisGeneration::Name(Active);
	ActiveSpawnCells = TotorisGeneration::SpawnCells(Active);
	for (const FIntPoint& Cell : ActiveSpawnCells)
	{
		AddBlock(Active, (Cell.X + 0.5f - TotorisGeneration::BoardWidth / 2.f) * CellSize,
			(Cell.Y - 0.5f - TotorisGeneration::BoardHeight / 2.f) * CellSize);
	}

	NextPieceNames.Reset();
	const TArray<ETotorisMino> Next = Sequence.Preview(TotorisGeneration::NextCount);
	for (int32 Slot = 0; Slot < Next.Num(); ++Slot)
	{
		const ETotorisMino Type = Next[Slot];
		NextPieceNames.Add(TotorisGeneration::Name(Type));
		const TArray<FIntPoint> Shape = TotorisGeneration::Shape(Type);
		FIntPoint Max(0, 0);
		for (const FIntPoint& Cell : Shape)
		{
			Max.X = FMath::Max(Max.X, Cell.X);
			Max.Y = FMath::Max(Max.Y, Cell.Y);
		}
		const float SlotCenter = NextCenter.Y + (2 - Slot) * NextSlotSpacing;
		for (const FIntPoint& Cell : Shape)
		{
			AddBlock(Type, NextCenter.X + (Cell.X - Max.X / 2.f) * CellSize,
				SlotCenter + (Cell.Y - Max.Y / 2.f) * CellSize);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("Totoris generation: restart=%d firstBag=%s active=%s next=%s topRow=22"),
		DebugRestartCount, *FirstBagOrder, *ActivePieceName, *FString::Join(NextPieceNames, TEXT("")));
}

void UTotorisBlockGeneratorComponent::DebugRestart()
{
	if (!HasBegunPlay() || Bodies.Num() != 7) return;
	Sequence.DebugRestart();
	++DebugRestartCount;
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
