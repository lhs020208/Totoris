#pragma once

#include "CoreMinimal.h"

namespace TotorisMissionPatterns
{
	// Top-to-bottom, all ten columns including the required empty border.
	inline constexpr const TCHAR* HeartRows[] = {
		TEXT("..##..##.."), TEXT(".########."), TEXT(".########."),
		TEXT(".########."), TEXT("..######.."), TEXT("...####..."), TEXT("....##....")
	};

	inline constexpr const TCHAR* DiamondRows[] = {
		TEXT("....##...."), TEXT("...####..."), TEXT("..######.."), TEXT(".########."),
		TEXT(".########."), TEXT("..######.."), TEXT("...####..."), TEXT("....##....")
	};

	inline constexpr const TCHAR* HalfSecretGradeRows[] = {
		TEXT("#########."), TEXT("########.#"), TEXT("#######.##"), TEXT("######.###"),
		TEXT("#####.####"), TEXT("####.#####"), TEXT("###.######"), TEXT("##.#######"),
		TEXT("#.########"), TEXT(".#########")
	};

	template <size_t Height>
	inline bool ContainsPattern(const TSet<FIntPoint>& Cells, int32 MaxRow, const TCHAR* const (&Rows)[Height], bool bMirror = false)
	{
		for (int32 Bottom = 1; Bottom + static_cast<int32>(Height) - 1 <= MaxRow; ++Bottom)
		{
			bool bMatches = true;
			for (int32 Row = 0; Row < static_cast<int32>(Height) && bMatches; ++Row)
				for (int32 Column = 0; Column < 10; ++Column)
					if (Cells.Contains(FIntPoint(Column, Bottom + static_cast<int32>(Height) - 1 - Row)) != (Rows[Row][bMirror ? 9 - Column : Column] == TEXT('#')))
					{
						bMatches = false;
						break;
					}
			if (bMatches) return true;
		}
		return false;
	}
	inline bool ContainsHeart(const TSet<FIntPoint>& Cells, int32 MaxRow)
	{
		return ContainsPattern(Cells, MaxRow, HeartRows);
	}
	inline bool ContainsDiamond(const TSet<FIntPoint>& Cells, int32 MaxRow)
	{
		return ContainsPattern(Cells, MaxRow, DiamondRows);
	}
	inline bool ContainsHalfSecretGrade(const TSet<FIntPoint>& Cells, int32 MaxRow)
	{
		return ContainsPattern(Cells, MaxRow, HalfSecretGradeRows)
			|| ContainsPattern(Cells, MaxRow, HalfSecretGradeRows, true);
	}
}
