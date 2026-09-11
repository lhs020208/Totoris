#include "TotorisGeneration.h"

namespace TotorisGeneration
{
	FString Name(ETotorisMino Type)
	{
		static const TCHAR* Names[] = { TEXT("I"), TEXT("S"), TEXT("Z"), TEXT("T"), TEXT("O"), TEXT("L"), TEXT("J") };
		return Names[static_cast<uint8>(Type)];
	}

	FLinearColor Color(ETotorisMino Type)
	{
		static const FLinearColor Colors[] = {
			{0.03f, 0.8f, 1.0f}, {0.16f, 0.85f, 0.08f}, {0.95f, 0.055f, 0.09f},
			{0.65f, 0.08f, 0.9f}, {1.0f, 0.8f, 0.035f}, {1.0f, 0.32f, 0.035f}, {0.07f, 0.2f, 0.95f}
		};
		return Colors[static_cast<uint8>(Type)];
	}

	TArray<FIntPoint> Shape(ETotorisMino Type)
	{
		// North-facing/SRS spawn orientations; J/L/T have the flat side down.
		switch (Type)
		{
		case ETotorisMino::I: return {{0,0}, {1,0}, {2,0}, {3,0}};
		case ETotorisMino::S: return {{0,0}, {1,0}, {1,1}, {2,1}};
		case ETotorisMino::Z: return {{1,0}, {2,0}, {0,1}, {1,1}};
		case ETotorisMino::T: return {{0,0}, {1,0}, {2,0}, {1,1}};
		case ETotorisMino::O: return {{0,0}, {1,0}, {0,1}, {1,1}};
		case ETotorisMino::L: return {{0,0}, {1,0}, {2,0}, {2,1}};
		case ETotorisMino::J: return {{0,0}, {1,0}, {2,0}, {0,1}};
		}
		checkNoEntry();
		return {};
	}

	TArray<FIntPoint> SpawnCells(ETotorisMino Type)
	{
		TArray<FIntPoint> Cells = Shape(Type);
		int32 MaxX = 0, MaxY = 0;
		for (const FIntPoint& Cell : Cells)
		{
			MaxX = FMath::Max(MaxX, Cell.X);
			MaxY = FMath::Max(MaxY, Cell.Y);
		}
		const int32 Left = (BoardWidth - (MaxX + 1)) / 2;
		for (FIntPoint& Cell : Cells)
		{
			Cell += FIntPoint(Left, SpawnTopRow - MaxY);
		}
		return Cells;
	}

	TArray<ETotorisMino> ShuffleBag(FRandomStream& Random)
	{
		TArray<ETotorisMino> Bag = { ETotorisMino::I, ETotorisMino::S, ETotorisMino::Z,
			ETotorisMino::T, ETotorisMino::O, ETotorisMino::L, ETotorisMino::J };
		for (int32 Index = Bag.Num() - 1; Index > 0; --Index)
		{
			Bag.Swap(Index, Random.RandRange(0, Index));
		}
		return Bag;
	}

	FString BagName(const TArray<ETotorisMino>& Bag)
	{
		FString Result;
		for (ETotorisMino Type : Bag) Result += Name(Type);
		return Result;
	}
}

void FTotorisSevenBag::Initialize(int32 Seed)
{
	Random.Initialize(Seed);
	FirstBag = TotorisGeneration::ShuffleBag(Random);
	Pending = FirstBag;
}

void FTotorisSevenBag::DebugRestart()
{
	check(FirstBag.Num() == 7);
	const ETotorisMino First = FirstBag[0];
	FirstBag.RemoveAt(0);
	FirstBag.Add(First);
	// Discard all lookahead from the previous run. Subsequent bags are freshly shuffled.
	Pending = FirstBag;
}

void FTotorisSevenBag::EnsurePending(int32 Count)
{
	check(FirstBag.Num() == 7);
	while (Pending.Num() < Count)
	{
		Pending.Append(TotorisGeneration::ShuffleBag(Random));
	}
}

ETotorisMino FTotorisSevenBag::Draw()
{
	EnsurePending(1);
	const ETotorisMino Type = Pending[0];
	Pending.RemoveAt(0);
	return Type;
}

TArray<ETotorisMino> FTotorisSevenBag::Preview(int32 Count)
{
	check(Count >= 0);
	EnsurePending(Count);
	TArray<ETotorisMino> Result;
	Result.Append(Pending.GetData(), Count);
	return Result;
}
