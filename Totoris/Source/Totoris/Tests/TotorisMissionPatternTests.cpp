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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisDiamondPatternTest, "Totoris.Missions.DiamondPattern",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisDiamondPatternTest::RunTest(const FString& Parameters)
{
	const uint16 Rows[] = {0x030, 0x078, 0x0FC, 0x1FE, 0x1FE, 0x0FC, 0x078, 0x030};
	for (const int32 Bottom : {1, 7, 13})
	{
		TSet<FIntPoint> Cells;
		for (int32 Row = 0; Row < 8; ++Row)
			for (int32 Col = 0; Col < 10; ++Col)
				if (Rows[Row] & (1 << Col)) Cells.Add(FIntPoint(Col, Bottom + Row));
		TestTrue(TEXT("Exact diamond at any height"), TotorisMissionPatterns::ContainsDiamond(Cells, 20));
		TestFalse(TEXT("Diamond is not heart"), TotorisMissionPatterns::ContainsHeart(Cells, 20));
		Cells.Add(FIntPoint(0, Bottom + 8));
		TestTrue(TEXT("Other rows ignored"), TotorisMissionPatterns::ContainsDiamond(Cells, 20));
		Cells.Add(FIntPoint(0, Bottom));
		TestFalse(TEXT("Extra cell rejected"), TotorisMissionPatterns::ContainsDiamond(Cells, 20));
		Cells.Remove(FIntPoint(0, Bottom));
		Cells.Remove(FIntPoint(4, Bottom));
		TestFalse(TEXT("Missing cell rejected"), TotorisMissionPatterns::ContainsDiamond(Cells, 20));
	}
	TestFalse(TEXT("Empty board rejected"), TotorisMissionPatterns::ContainsDiamond({}, 20));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisHalfSecretGradePatternTest, "Totoris.Missions.HalfSecretGradePattern",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisHalfSecretGradePatternTest::RunTest(const FString& Parameters)
{
	for (const bool bMirror : {false, true})
		for (const int32 Bottom : {1, 6, 11})
		{
			TSet<FIntPoint> Cells;
			// Independent construction: nine occupied cells per row and a diagonal hole.
			// No type map is supplied, just like garbage and normal blocks in LockedCells.
			for (int32 Row = 0; Row < 10; ++Row)
				for (int32 Col = 0; Col < 10; ++Col)
					if (Col != (bMirror ? 9 - Row : Row)) Cells.Add(FIntPoint(Col, Bottom + Row));
			TestEqual(TEXT("Ninety occupied cells"), Cells.Num(), 90);
			TestTrue(TEXT("Both orientations at any height"), TotorisMissionPatterns::ContainsHalfSecretGrade(Cells, 20));
			TestFalse(TEXT("Only nine rows available"), TotorisMissionPatterns::ContainsHalfSecretGrade(Cells, Bottom + 8));
			Cells.Add(FIntPoint(0, Bottom + 10));
			TestTrue(TEXT("Other rows ignored"), TotorisMissionPatterns::ContainsHalfSecretGrade(Cells, 20));
			const FIntPoint Hole(bMirror ? 9 : 0, Bottom);
			Cells.Add(Hole);
			TestFalse(TEXT("Filled diagonal hole rejected"), TotorisMissionPatterns::ContainsHalfSecretGrade(Cells, 20));
			Cells.Remove(Hole);
			Cells.Remove(FIntPoint(5, Bottom));
			TestFalse(TEXT("Missing occupied cell rejected"), TotorisMissionPatterns::ContainsHalfSecretGrade(Cells, 20));
		}
	TestFalse(TEXT("Empty board rejected"), TotorisMissionPatterns::ContainsHalfSecretGrade({}, 20));
	return true;
}
#endif
