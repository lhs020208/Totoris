#pragma once

#include "CoreMinimal.h"
#include "TotorisGeneration.generated.h"

UENUM(BlueprintType)
enum class ETotorisMino : uint8
{
	I, S, Z, T, O, L, J
};

UENUM(BlueprintType)
enum class ETotorisSpinKind : uint8
{
	None,
	Mini,
	Full
};

namespace TotorisGeneration
{
	constexpr int32 BoardWidth = 10;
	constexpr int32 BoardHeight = 20;
	constexpr int32 SpawnTopRow = 22;
	constexpr int32 NextCount = 5;

	FString Name(ETotorisMino Type);
	FLinearColor Color(ETotorisMino Type);
	// Occupied cells only, normalized to bottom-left (0,0), with rows increasing up.
	TArray<FIntPoint> Shape(ETotorisMino Type);
	TArray<FIntPoint> RotationCells(ETotorisMino Type, uint8 Rotation);
	FIntPoint SpawnPosition(ETotorisMino Type);
	TArray<FIntPoint> RotationKicks(ETotorisMino Type, uint8 From, uint8 To);
	TArray<FIntPoint> RotationKicks180(ETotorisMino Type, uint8 From, uint8 To);
	bool IsImmobile(ETotorisMino Type, const FIntPoint& Position, uint8 Rotation,
		const TSet<FIntPoint>& LockedCells, int32 LogicalRows);
	ETotorisSpinKind DetectSpin(ETotorisMino Type, const FIntPoint& Position, uint8 Rotation,
		bool bLastActionWasRotation, bool bLastRotationWas180, int32 LastRotationKickIndex,
		const TSet<FIntPoint>& LockedCells, int32 LogicalRows);
	// Zero-based columns, one-based rows. Odd-width pieces round left to stay on-grid.
	TArray<FIntPoint> SpawnCells(ETotorisMino Type);
	TArray<ETotorisMino> ShuffleBag(FRandomStream& Random);
	FString BagName(const TArray<ETotorisMino>& Bag);
}

// Independent of rendering/input so bag boundaries and debug restarts can be tested.
class FTotorisSevenBag
{
public:
	void Initialize(int32 Seed);
	void DebugRestart();
	ETotorisMino Draw();
	TArray<ETotorisMino> Preview(int32 Count);
	const TArray<ETotorisMino>& GetFirstBag() const { return FirstBag; }

private:
	void EnsurePending(int32 Count);
	FRandomStream Random;
	TArray<ETotorisMino> FirstBag;
	TArray<ETotorisMino> Pending;
};
