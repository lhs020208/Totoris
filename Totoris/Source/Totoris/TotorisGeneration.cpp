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
		Rotation &= 3;

		// O does not visually rotate in this implementation.
		if (Type == ETotorisMino::O)
		{
			return Shape(Type);
		}

		TArray<FIntPoint> Cells;
		int32 Size = 3;

		// IMPORTANT:
		// Shape() is normalized to the occupied minos and is suitable for
		// NEXT/HOLD previews, but SRS rotation must preserve the empty rows
		// inside the 3x3 / 4x4 rotation box.
		//
		// Project coordinates use +Y upward.
		switch (Type)
		{
		case ETotorisMino::I:
			// SRS 4x4 box.
			//
			// ....
			// IIII   <- y = 2
			// ....
			// ....
			//
			// Rotation center is (1.5, 1.5).
			Cells = {
				{0, 2},
				{1, 2},
				{2, 2},
				{3, 2}
			};
			Size = 4;
			break;

		case ETotorisMino::S:
			// .SS
			// SS.
			// ...
			Cells = {
				{0, 1},
				{1, 1},
				{1, 2},
				{2, 2}
			};
			break;

		case ETotorisMino::Z:
			// ZZ.
			// .ZZ
			// ...
			Cells = {
				{1, 1},
				{2, 1},
				{0, 2},
				{1, 2}
			};
			break;

		case ETotorisMino::T:
			// .T.
			// TTT
			// ...
			//
			// Rotation center is the middle block of the flat row: (1, 1).
			Cells = {
				{0, 1},
				{1, 1},
				{2, 1},
				{1, 2}
			};
			break;

		case ETotorisMino::L:
			// ..L
			// LLL
			// ...
			Cells = {
				{0, 1},
				{1, 1},
				{2, 1},
				{2, 2}
			};
			break;

		case ETotorisMino::J:
			// J..
			// JJJ
			// ...
			Cells = {
				{0, 1},
				{1, 1},
				{2, 1},
				{0, 2}
			};
			break;

		case ETotorisMino::O:
			checkNoEntry();
			break;
		}

		// Pure clockwise rotation inside the fixed SRS box.
		//
		// For a 3x3 box this rotates around (1,1).
		// For a 4x4 I box this rotates around (1.5,1.5).
		for (int32 Turn = 0; Turn < Rotation; ++Turn)
		{
			for (FIntPoint& Cell : Cells)
			{
				const int32 OldX = Cell.X;
				const int32 OldY = Cell.Y;

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
		if (Type == ETotorisMino::O)
		{
			return { {0, 0} };
		}

		const uint8 Key =
			static_cast<uint8>(((From & 3) << 4) | (To & 3));

		// Standard Guideline SRS for J/L/S/T/Z.
		// Project coordinate system: +Y is upward.
		static const TMap<uint8, TArray<FIntPoint>> JLSTZ =
		{
			// 0 -> R
			{0x01, {{0,0}, {-1,0}, {-1,1}, {0,-2}, {-1,-2}}},

			// R -> 0
			{0x10, {{0,0}, {1,0}, {1,-1}, {0,2}, {1,2}}},

			// R -> 2
			{0x12, {{0,0}, {1,0}, {1,-1}, {0,2}, {1,2}}},

			// 2 -> R
			{0x21, {{0,0}, {-1,0}, {-1,1}, {0,-2}, {-1,-2}}},

			// 2 -> L
			{0x23, {{0,0}, {1,0}, {1,1}, {0,-2}, {1,-2}}},

			// L -> 2
			{0x32, {{0,0}, {-1,0}, {-1,-1}, {0,2}, {-1,2}}},

			// L -> 0
			{0x30, {{0,0}, {-1,0}, {-1,-1}, {0,2}, {-1,2}}},

			// 0 -> L
			{0x03, {{0,0}, {1,0}, {1,1}, {0,-2}, {1,-2}}}
		};

		// TETR.IO SRS+
		//
		// Unlike regular Guideline SRS, the I-piece kick ordering is made
		// symmetrical across the Y axis.
		static const TMap<uint8, TArray<FIntPoint>> I_SRSPlus =
		{
			// 0 -> R
			{0x01, {
				{0,0},
				{1,0},
				{-2,0},
				{-2,-1},
				{1,2}
			}},

			// R -> 0
			{0x10, {
				{0,0},
				{-1,0},
				{2,0},
				{-1,-2},
				{2,1}
			}},

			// R -> 2
			{0x12, {
				{0,0},
				{-1,0},
				{2,0},
				{-1,2},
				{2,-1}
			}},

			// 2 -> R
			{0x21, {
				{0,0},
				{-2,0},
				{1,0},
				{-2,1},
				{1,-2}
			}},

			// 2 -> L
			{0x23, {
				{0,0},
				{2,0},
				{-1,0},
				{2,1},
				{-1,-2}
			}},

			// L -> 2
			{0x32, {
				{0,0},
				{1,0},
				{-2,0},
				{1,2},
				{-2,-1}
			}},

			// L -> 0
			{0x30, {
				{0,0},
				{1,0},
				{-2,0},
				{1,-2},
				{-2,1}
			}},

			// 0 -> L
			{0x03, {
				{0,0},
				{-1,0},
				{2,0},
				{2,-1},
				{-1,2}
			}}
		};

		const TMap<uint8, TArray<FIntPoint>>& Table =
			Type == ETotorisMino::I ? I_SRSPlus : JLSTZ;

		if (const TArray<FIntPoint>* Found = Table.Find(Key))
		{
			return *Found;
		}

		return { {0, 0} };
	}

	TArray<FIntPoint> RotationKicks180(
		ETotorisMino Type,
		uint8 From,
		uint8 To)
	{
		if (Type == ETotorisMino::O)
		{
			return { {0, 0} };
		}

		const uint8 Key =
			static_cast<uint8>(((From & 3) << 4) | (To & 3));

		// TETR.IO-style I-piece 180 kicks.
		if (Type == ETotorisMino::I)
		{
			static const TMap<uint8, TArray<FIntPoint>> I180 =
			{
				// 0 -> 2
				{0x02, {
					{0,0},
					{0,1}
				}},

				// 2 -> 0
				{0x20, {
					{0,0},
					{0,-1}
				}},

				// R -> L
				{0x13, {
					{0,0},
					{1,0}
				}},

				// L -> R
				{0x31, {
					{0,0},
					{-1,0}
				}}
			};

			if (const TArray<FIntPoint>* Found = I180.Find(Key))
			{
				return *Found;
			}

			return { {0, 0} };
		}

		// TETR.IO custom 180-degree kicks for J/L/S/T/Z.
		static const TMap<uint8, TArray<FIntPoint>> JLSTZ180 =
		{
			// 0 -> 2
			{0x02, {
				{0,0},
				{0,1},
				{1,1},
				{-1,1},
				{1,0},
				{-1,0}
			}},

			// 2 -> 0
			{0x20, {
				{0,0},
				{0,-1},
				{-1,-1},
				{1,-1},
				{-1,0},
				{1,0}
			}},

			// R -> L
			{0x13, {
				{0,0},
				{1,0},
				{1,2},
				{1,1},
				{0,2},
				{0,1}
			}},

			// L -> R
			{0x31, {
				{0,0},
				{-1,0},
				{-1,2},
				{-1,1},
				{0,2},
				{0,1}
			}}
		};

		if (const TArray<FIntPoint>* Found = JLSTZ180.Find(Key))
		{
			return *Found;
		}

		return { {0, 0} };
	}

	bool IsImmobile(ETotorisMino Type, const FIntPoint& Position, uint8 Rotation,
		const TSet<FIntPoint>& LockedCells, int32 LogicalRows)
	{
		auto IsValid = [&](const FIntPoint& TestPosition)
		{
			for (FIntPoint Cell : RotationCells(Type, Rotation))
			{
				Cell += TestPosition;
				if (Cell.X < 0 || Cell.X >= BoardWidth || Cell.Y < 1 || Cell.Y > LogicalRows || LockedCells.Contains(Cell)) return false;
			}
			return true;
		};
		return !IsValid(Position + FIntPoint(-1, 0))
			&& !IsValid(Position + FIntPoint(1, 0))
			&& !IsValid(Position + FIntPoint(0, 1))
			&& !IsValid(Position + FIntPoint(0, -1));
	}

	ETotorisSpinKind DetectSpin(ETotorisMino Type, const FIntPoint& Position, uint8 Rotation,
		bool bLastActionWasRotation, bool bLastRotationWas180, int32 LastRotationKickIndex,
		const TSet<FIntPoint>& LockedCells, int32 LogicalRows)
	{
		if (!bLastActionWasRotation) return ETotorisSpinKind::None;
		if (Type != ETotorisMino::T)
		{
			return IsImmobile(Type, Position, Rotation, LockedCells, LogicalRows)
				? ETotorisSpinKind::Mini : ETotorisSpinKind::None;
		}

		auto IsOccupied = [&](const FIntPoint& Cell)
		{
			return Cell.X < 0 || Cell.X >= BoardWidth || Cell.Y < 1 || Cell.Y > LogicalRows || LockedCells.Contains(Cell);
		};
		const FIntPoint Pivot = Position + FIntPoint(1, 1);
		const bool bNW = IsOccupied(Pivot + FIntPoint(-1, 1));
		const bool bNE = IsOccupied(Pivot + FIntPoint(1, 1));
		const bool bSW = IsOccupied(Pivot + FIntPoint(-1, -1));
		const bool bSE = IsOccupied(Pivot + FIntPoint(1, -1));
		const int32 CornerCount = static_cast<int32>(bNW) + static_cast<int32>(bNE) + static_cast<int32>(bSW) + static_cast<int32>(bSE);
		if (CornerCount >= 3)
		{
			bool bFrontA = false;
			bool bFrontB = false;
			switch (Rotation & 3)
			{
			case 0: bFrontA = bNW; bFrontB = bNE; break;
			case 1: bFrontA = bNE; bFrontB = bSE; break;
			case 2: bFrontA = bSW; bFrontB = bSE; break;
			case 3: bFrontA = bNW; bFrontB = bSW; break;
			}
			if ((bFrontA && bFrontB) || (!bLastRotationWas180 && LastRotationKickIndex == 4)) return ETotorisSpinKind::Full;
			return ETotorisSpinKind::Mini;
		}
		return IsImmobile(Type, Position, Rotation, LockedCells, LogicalRows)
			? ETotorisSpinKind::Mini : ETotorisSpinKind::None;
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
