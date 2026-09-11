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
		// SRS uses a 4x4 I-piece box; the spawn cells occupy its second row.
		case ETotorisMino::I: return {{0,1}, {1,1}, {2,1}, {3,1}};
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
		const FIntPoint Position = SpawnPosition(Type);
		TArray<FIntPoint> Cells = RotationCells(Type, 0);
		for (FIntPoint& Cell : Cells)
		{
			Cell += Position;
		}
		return Cells;
	}

	TArray<FIntPoint> RotationCells(ETotorisMino Type, uint8 Rotation)
	{
		TArray<FIntPoint> Cells = Shape(Type);
		if (Type == ETotorisMino::O || (Rotation & 3) == 0) return Cells;
		const int32 Size = Type == ETotorisMino::I ? 4 : 3;
		for (int32 Turn = 0; Turn < (Rotation & 3); ++Turn)
		{
			for (FIntPoint& Cell : Cells)
			{
				const int32 OldX = Cell.X;
				const int32 OldY = Cell.Y;
				// Project coordinates use +Y upward, so this is a clockwise turn.
				Cell.X = OldY;
				Cell.Y = Size - 1 - OldX;
			}
		}
		return Cells;
	}

	FIntPoint SpawnPosition(ETotorisMino Type)
	{
		const TArray<FIntPoint> Cells = RotationCells(Type, 0);
		int32 MaxX = 0, MaxY = 0;
		for (const FIntPoint& Cell : Cells)
		{
			MaxX = FMath::Max(MaxX, Cell.X);
			MaxY = FMath::Max(MaxY, Cell.Y);
		}
		return FIntPoint((BoardWidth - (MaxX + 1)) / 2, SpawnTopRow - MaxY);
	}

	TArray<FIntPoint> RotationKicks(ETotorisMino Type, uint8 From, uint8 To)
	{
		if (Type == ETotorisMino::O) return {{0, 0}};
		const uint8 Key = static_cast<uint8>(((From & 3) << 2) | (To & 3));
		static const TMap<uint8, TArray<FIntPoint>> JLSTZ = {
			{0x01, {{0,0},{-1,0},{-1,1},{0,-2},{-1,-2}}},
			{0x10, {{0,0},{1,0},{1,-1},{0,2},{1,2}}},
			{0x12, {{0,0},{1,0},{1,-1},{0,2},{1,2}}},
			{0x21, {{0,0},{-1,0},{-1,1},{0,-2},{-1,-2}}},
			{0x23, {{0,0},{1,0},{1,1},{0,-2},{1,-2}}},
			{0x32, {{0,0},{-1,0},{-1,-1},{0,2},{-1,2}}},
			{0x30, {{0,0},{-1,0},{-1,-1},{0,2},{-1,2}}},
			{0x03, {{0,0},{1,0},{1,1},{0,-2},{1,-2}}}
		};
		static const TMap<uint8, TArray<FIntPoint>> I_SRSPlus = {
			{0x01, {{0,0},{-2,0},{1,0},{1,2},{-2,-1}}},
			{0x10, {{0,0},{2,0},{-1,0},{2,1},{-1,-2}}},
			{0x12, {{0,0},{-1,0},{2,0},{-1,2},{2,-1}}},
			{0x21, {{0,0},{-2,0},{1,0},{-2,1},{1,-1}}},
			{0x23, {{0,0},{2,0},{-1,0},{2,1},{-1,-1}}},
			{0x32, {{0,0},{1,0},{-2,0},{1,2},{-2,-1}}},
			{0x30, {{0,0},{-2,0},{1,0},{-2,1},{1,-2}}},
			{0x03, {{0,0},{2,0},{-1,0},{-1,2},{2,-1}}}
		};
		const TMap<uint8, TArray<FIntPoint>>& Table = Type == ETotorisMino::I ? I_SRSPlus : JLSTZ;
		if (const TArray<FIntPoint>* Found = Table.Find(Key)) return *Found;
		return {{0, 0}};
	}

	TArray<FIntPoint> RotationKicks180(ETotorisMino Type, uint8 From, uint8 To)
	{
		if (Type == ETotorisMino::O) return {{0, 0}};
		const uint8 Key = static_cast<uint8>(((From & 3) << 2) | (To & 3));
		static const TMap<uint8, TArray<FIntPoint>> Table = {
			{0x02, {{0,0},{0,1},{1,1},{-1,1},{1,0},{-1,0}}},
			{0x20, {{0,0},{0,-1},{-1,-1},{1,-1},{-1,0},{1,0}}},
			{0x13, {{0,0},{1,0},{0,2},{1,1},{0,2},{0,1}}},
			{0x31, {{0,0},{-1,0},{0,2},{-1,1},{0,2},{0,1}}}
		};
		if (const TArray<FIntPoint>* Found = Table.Find(Key)) return *Found;
		return {{0, 0}};
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
