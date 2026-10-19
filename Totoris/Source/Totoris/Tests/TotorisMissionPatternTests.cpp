#include "../TotorisMissionPatterns.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisHeartPatternTest, "Totoris.Missions.HeartPattern",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisHeartPatternTest::RunTest(const FString& Parameters)
{
	// Independent expected coordinates, bottom-to-top.
	const uint16 Rows[] = {0x030, 0x078, 0x0FC, 0x1FE, 0x1FE, 0x1FE, 0x0CC};
	for (const int32 Bottom : {1, 8, 14})
	{
		TSet<FIntPoint> Cells;
		for (int32 Row = 0; Row < 7; ++Row)
			for (int32 Col = 0; Col < 10; ++Col)
				if (Rows[Row] & (1 << Col)) Cells.Add(FIntPoint(Col, Bottom + Row));
		TestEqual(TEXT("Heart has forty cells"), Cells.Num(), 40);
		TestTrue(TEXT("Exact heart at any height"), TotorisMissionPatterns::ContainsHeart(Cells, 20));
		Cells.Add(FIntPoint(0, Bottom + 7));
		TestTrue(TEXT("Other rows ignored"), TotorisMissionPatterns::ContainsHeart(Cells, 20));
		Cells.Add(FIntPoint(0, Bottom));
		TestFalse(TEXT("Extra cell in required empty space rejected"), TotorisMissionPatterns::ContainsHeart(Cells, 20));
		Cells.Remove(FIntPoint(0, Bottom));
		Cells.Remove(FIntPoint(4, Bottom));
		TestFalse(TEXT("Missing heart cell rejected"), TotorisMissionPatterns::ContainsHeart(Cells, 20));
	}
	TestFalse(TEXT("Empty board rejected"), TotorisMissionPatterns::ContainsHeart({}, 20));
	return true;
}
#endif
