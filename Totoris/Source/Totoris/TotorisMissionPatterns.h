#pragma once

#include "CoreMinimal.h"

namespace TotorisMissionPatterns
{
	// Top-to-bottom, all ten columns including the required empty border.
	inline constexpr const TCHAR* HeartRows[] = {
		TEXT("..##..##.."), TEXT(".########."), TEXT(".########."),
		TEXT(".########."), TEXT("..######.."), TEXT("...####..."), TEXT("....##....")
	};

	inline bool ContainsHeart(const TSet<FIntPoint>& Cells, int32 MaxRow)
	{
		for (int32 Bottom = 1; Bottom + 6 <= MaxRow; ++Bottom)
		{
			bool bMatches = true;
			for (int32 Row = 0; Row < 7 && bMatches; ++Row)
				for (int32 Column = 0; Column < 10; ++Column)
					if (Cells.Contains(FIntPoint(Column, Bottom + 6 - Row)) != (HeartRows[Row][Column] == TEXT('#')))
					{
						bMatches = false;
						break;
					}
			if (bMatches) return true;
		}
		return false;
	}
}
