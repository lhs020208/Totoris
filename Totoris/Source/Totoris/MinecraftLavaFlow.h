#pragma once

#include "CoreMinimal.h"

// One permanent source, Overworld-style spreading. Each Advance is one
// 30-game-tick (1.5 second) fluid update, independent of rendering frame rate.
namespace MinecraftLava
{
constexpr float CellSize = 100.f;
constexpr float TickSeconds = 1.5f;
constexpr int32 HorizontalRange = 3;
constexpr int32 MaxCells = 512;
constexpr int32 MaxDrop = 32;

struct FCell
{
	int32 Distance = 0;
	bool bFalling = false;
	FIntVector Direction = FIntVector(1, 0, 0);
};

class FFlow
{
public:
	TMap<FIntVector, FCell> Cells;

	void Reset() { Cells.Reset(); }
	void Start() { Reset(); Cells.Add(FIntVector::ZeroValue, FCell{}); }

	// CanFlow tests both the destination volume and the edge between cells.
	// Sweeping the edge is necessary for thin floors/walls in an Unreal level.
	bool Advance(TFunctionRef<bool(const FIntVector&, const FIntVector&)> CanFlow)
	{
		TMap<FIntVector, FCell> Pending;
		const FIntVector Down(0, 0, -1);
		auto Propose = [&](const FIntVector& P, const FCell& Cell)
		{
			if (P.Z < -MaxDrop) return;
			const FCell* Old = Cells.Find(P);
			if (Old && (Old->bFalling || (!Cell.bFalling && Old->Distance <= Cell.Distance))) return;
			const FCell* Other = Pending.Find(P);
			if (Other && (Other->bFalling || (!Cell.bFalling && Other->Distance <= Cell.Distance))) return;
			if (!Old && !Other && Cells.Num() + Pending.Num() >= MaxCells) return;
			Pending.Add(P, Cell);
		};

		for (const auto& Pair : Cells)
		{
			const FIntVector P = Pair.Key;
			const FCell& Cell = Pair.Value;
			if (P.Z > -MaxDrop && CanFlow(P, P + Down))
			{
				Propose(P + Down, FCell{0, true, Down});
				continue;
			}
			const int32 NextDistance = Cell.bFalling ? 1 : Cell.Distance + 1;
			if (NextDistance > HorizontalRange) continue;

			int32 Costs[4];
			int32 BestCost = MAX_int32;
			for (int32 I = 0; I < 4; ++I)
			{
				const FIntVector Next = P + Directions()[I];
				Costs[I] = MAX_int32;
				if (!CanFlow(P, Next)) continue;
				TSet<FIntVector> Visited;
				Visited.Add(P);
				Costs[I] = FindDrop(Next, 0, Visited, CanFlow);
				BestCost = FMath::Min(BestCost, Costs[I]);
			}
			for (int32 I = 0; I < 4; ++I)
			{
				const FIntVector Next = P + Directions()[I];
				if (Costs[I] == BestCost && CanFlow(P, Next))
					Propose(Next, FCell{NextDistance, false, Directions()[I]});
			}
		}
		for (const auto& Pair : Pending) Cells.Add(Pair.Key, Pair.Value);
		return !Pending.IsEmpty();
	}

	static const TArray<FIntVector>& Directions()
	{
		static const TArray<FIntVector> D = {FIntVector(1,0,0), FIntVector(-1,0,0),
			FIntVector(0,1,0), FIntVector(0,-1,0)};
		return D;
	}

private:
	int32 FindDrop(const FIntVector& P, int32 Depth, TSet<FIntVector> Visited,
		TFunctionRef<bool(const FIntVector&, const FIntVector&)> CanFlow) const
	{
		if (CanFlow(P, P + FIntVector(0,0,-1))) return Depth;
		if (Depth >= HorizontalRange - 1) return MAX_int32;
		Visited.Add(P);
		int32 Best = MAX_int32;
		for (const FIntVector& D : Directions())
		{
			const FIntVector Next = P + D;
			if (!Visited.Contains(Next) && CanFlow(P, Next))
				Best = FMath::Min(Best, FindDrop(Next, Depth + 1, Visited, CanFlow));
		}
		return Best;
	}
};
}
