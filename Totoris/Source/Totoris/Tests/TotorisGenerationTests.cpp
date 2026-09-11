#include "../TotorisGeneration.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisBagTest, "Totoris.Generation.SevenBagAndPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisBagTest::RunTest(const FString& Parameters)
{
	FTotorisSevenBag Bag;
	Bag.Initialize(7391);
	TSet<FString> Orders;
	for (int32 BagIndex = 0; BagIndex < 1000; ++BagIndex)
	{
		TSet<uint8> Types;
		FString Order;
		for (int32 Piece = 0; Piece < 7; ++Piece)
		{
			const auto Preview = Bag.Preview(5); // Deliberately straddles bag boundaries.
			TestEqual(TEXT("NEXT has five entries"), Preview.Num(), 5);
			TestTrue(TEXT("Preview does not consume"), Preview == Bag.Preview(5));
			const auto Drawn = Bag.Draw();
			TestTrue(TEXT("NEXT head equals next draw"), Drawn == Preview[0]);
			Types.Add(static_cast<uint8>(Drawn));
			Order += TotorisGeneration::Name(Drawn);
		}
		TestEqual(TEXT("Every bag contains each of the seven types exactly once"), Types.Num(), 7);
		Orders.Add(Order);
	}
	TestTrue(TEXT("Later bags are shuffled independently"), Orders.Num() > 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisRestartTest, "Totoris.Generation.DebugRestartCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisRestartTest::RunTest(const FString& Parameters)
{
	FTotorisSevenBag Bag;
	Bag.Initialize(2026);
	const FString Original = TotorisGeneration::BagName(Bag.GetFirstBag());
	FString Previous = Original;
	TSet<FString> ActiveTypes;
	for (int32 Run = 0; Run < 7; ++Run)
	{
		// Old consumption / lookahead must not affect the next run's first bag.
		for (int32 Index = 0; Index < 19; ++Index) Bag.Draw();
		Bag.Preview(5);
		Bag.DebugRestart();
		const FString Expected = Previous.Mid(1) + Previous.Left(1);
		TestEqual(TEXT("Restart rotates exactly one position left"), TotorisGeneration::BagName(Bag.GetFirstBag()), Expected);
		const FString Active = TotorisGeneration::Name(Bag.Draw());
		TestEqual(TEXT("Active is first entry"), Active, Expected.Left(1));
		TestEqual(TEXT("NEXT shows entries 2-6"), TotorisGeneration::BagName(Bag.Preview(5)), Expected.Mid(1,5));
		ActiveTypes.Add(Active);
		Previous = Expected;
	}
	TestEqual(TEXT("Seven restarts visit all shapes"), ActiveTypes.Num(), 7);
	TestEqual(TEXT("Seven restarts return to original order"), Previous, Original);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisSpawnTest, "Totoris.Generation.SpawnOrientationsAndRow22",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisSpawnTest::RunTest(const FString& Parameters)
{
	const TArray<TArray<FIntPoint>> Expected = {
		{{3,22},{4,22},{5,22},{6,22}}, // I: columns 4-7, one row
		{{3,21},{4,21},{4,22},{5,22}}, // S
		{{4,21},{5,21},{3,22},{4,22}}, // Z
		{{3,21},{4,21},{5,21},{4,22}}, // T
		{{4,21},{5,21},{4,22},{5,22}}, // O: columns 5-6
		{{3,21},{4,21},{5,21},{5,22}}, // L
		{{3,21},{4,21},{5,21},{3,22}}  // J
	};
	for (uint8 Index = 0; Index < 7; ++Index)
	{
		const auto Type = static_cast<ETotorisMino>(Index);
		const auto Cells = TotorisGeneration::SpawnCells(Type);
		TestTrue(*FString::Printf(TEXT("%s spawn coordinates and orientation"), *TotorisGeneration::Name(Type)), Cells == Expected[Index]);
		TSet<FIntPoint> Unique;
		int32 Highest = 0;
		for (const auto& Cell : Cells)
		{
			Unique.Add(Cell);
			Highest = FMath::Max(Highest, Cell.Y);
			TestTrue(TEXT("Spawn column is within the ten-column board"), Cell.X >= 0 && Cell.X < 10);
		}
		TestEqual(TEXT("Four distinct blocks"), Unique.Num(), 4);
		TestEqual(TEXT("Highest occupied row is 22"), Highest, 22);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisSpinDetectionTest, "Totoris.Generation.AllMiniPlusSpinDetection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisSpinDetectionTest::RunTest(const FString& Parameters)
{
	const FIntPoint TPosition(3, 5);
	const FIntPoint Pivot = TPosition + FIntPoint(1, 1);
	auto Corners = [&](bool bNW, bool bNE, bool bSW, bool bSE)
	{
		TSet<FIntPoint> Locked;
		if (bNW) Locked.Add(Pivot + FIntPoint(-1, 1));
		if (bNE) Locked.Add(Pivot + FIntPoint(1, 1));
		if (bSW) Locked.Add(Pivot + FIntPoint(-1, -1));
		if (bSE) Locked.Add(Pivot + FIntPoint(1, -1));
		return Locked;
	};

	TestEqual(TEXT("T three corners and two front corners is Full"),
		TotorisGeneration::DetectSpin(ETotorisMino::T, TPosition, 0, true, false, 0, Corners(true, true, true, false), 40),
		ETotorisSpinKind::Full);
	TestEqual(TEXT("T three corners and one front corner is Mini"),
		TotorisGeneration::DetectSpin(ETotorisMino::T, TPosition, 0, true, false, 0, Corners(true, false, true, true), 40),
		ETotorisSpinKind::Mini);
	TestEqual(TEXT("T fifth 90-degree kick upgrades to Full"),
		TotorisGeneration::DetectSpin(ETotorisMino::T, TPosition, 0, true, false, 4, Corners(false, true, true, true), 40),
		ETotorisSpinKind::Full);
	TestEqual(TEXT("T 180 kick does not use fifth-kick upgrade"),
		TotorisGeneration::DetectSpin(ETotorisMino::T, TPosition, 0, true, true, 4, Corners(false, true, true, true), 40),
		ETotorisSpinKind::Mini);

		TSet<FIntPoint> ImmobileLocked;
		ImmobileLocked.Add(FIntPoint(2, 6));
		ImmobileLocked.Add(FIntPoint(6, 6));
		ImmobileLocked.Add(FIntPoint(4, 8));
		ImmobileLocked.Add(FIntPoint(3, 5));
	TestEqual(TEXT("T failed three-corner check uses immobile Mini fallback"),
		TotorisGeneration::DetectSpin(ETotorisMino::T, TPosition, 0, true, false, 0, ImmobileLocked, 40),
		ETotorisSpinKind::Mini);
	TestEqual(TEXT("T failed three-corner check while movable is None"),
		TotorisGeneration::DetectSpin(ETotorisMino::T, TPosition, 0, true, false, 0, TSet<FIntPoint>(), 40),
		ETotorisSpinKind::None);

	for (ETotorisMino Type : {ETotorisMino::I, ETotorisMino::S, ETotorisMino::Z, ETotorisMino::L, ETotorisMino::J, ETotorisMino::O})
	{
		TSet<FIntPoint> Locked;
		for (FIntPoint Cell : TotorisGeneration::RotationCells(Type, 0))
		{
			Cell += TPosition;
			Locked.Add(Cell + FIntPoint(-1, 0));
			Locked.Add(Cell + FIntPoint(1, 0));
			Locked.Add(Cell + FIntPoint(0, 1));
			Locked.Add(Cell + FIntPoint(0, -1));
		}
		for (FIntPoint Cell : TotorisGeneration::RotationCells(Type, 0)) Locked.Remove(Cell + TPosition);
		TestEqual(*FString::Printf(TEXT("%s immobile rotation is Mini"), *TotorisGeneration::Name(Type)),
			TotorisGeneration::DetectSpin(Type, TPosition, 0, true, false, 0, Locked, 40), ETotorisSpinKind::Mini);
		TestEqual(*FString::Printf(TEXT("%s movable rotation is None"), *TotorisGeneration::Name(Type)),
			TotorisGeneration::DetectSpin(Type, TPosition, 0, true, false, 0, TSet<FIntPoint>(), 40), ETotorisSpinKind::None);
	}
	TestTrue(TEXT("O rotation keeps the same cells"), TotorisGeneration::RotationCells(ETotorisMino::O, 0) == TotorisGeneration::RotationCells(ETotorisMino::O, 1));
	const TArray<FIntPoint> OKick = TotorisGeneration::RotationKicks(ETotorisMino::O, 0, 1);
	TestEqual(TEXT("O rotation uses one no-op kick"), OKick.Num(), 1);
	TestEqual(TEXT("O rotation kick is zero"), OKick[0], FIntPoint::ZeroValue);

	TestEqual(TEXT("Non-rotation action is not a spin"),
		TotorisGeneration::DetectSpin(ETotorisMino::T, TPosition, 0, false, false, INDEX_NONE, Corners(true, true, true, true), 40),
		ETotorisSpinKind::None);
	const FIntPoint EdgePosition(-1, 5);
	TSet<FIntPoint> EdgeLocked;
	EdgeLocked.Add(FIntPoint(1, 7));
	EdgeLocked.Add(FIntPoint(1, 5));
	TestEqual(TEXT("Board edge counts as an occupied T corner"),
		TotorisGeneration::DetectSpin(ETotorisMino::T, EdgePosition, 0, true, false, 0, EdgeLocked, 40),
		ETotorisSpinKind::Full);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisActionClassificationTest, "Totoris.Generation.ActionClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisActionClassificationTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Single action name"),
		TotorisGeneration::ActionName(ETotorisMino::J, ETotorisSpinKind::None, 1),
		FString(TEXT("Single")));

	TestEqual(TEXT("T-Spin Double action name"),
		TotorisGeneration::ActionName(ETotorisMino::T, ETotorisSpinKind::Full, 2),
		FString(TEXT("T-Spin Double")));

	TestEqual(TEXT("Mini T-Spin Single action name"),
		TotorisGeneration::ActionName(ETotorisMino::T, ETotorisSpinKind::Mini, 1),
		FString(TEXT("Mini T-Spin Single")));

	TestEqual(TEXT("I-Spin Double action name"),
		TotorisGeneration::ActionName(ETotorisMino::I, ETotorisSpinKind::Mini, 2),
		FString(TEXT("I-Spin Double")));

	TestEqual(TEXT("Tetris action name"),
		TotorisGeneration::ActionName(ETotorisMino::I, ETotorisSpinKind::None, 4),
		FString(TEXT("Tetris")));

	TestTrue(TEXT("Tetris is B2B eligible"),
		TotorisGeneration::IsBackToBackEligible(ETotorisSpinKind::None, 4, false));

	TestTrue(TEXT("Spin clear is B2B eligible"),
		TotorisGeneration::IsBackToBackEligible(ETotorisSpinKind::Mini, 1, false));

	TestTrue(TEXT("Perfect Clear is B2B eligible"),
		TotorisGeneration::IsBackToBackEligible(ETotorisSpinKind::None, 2, true));

	TestFalse(TEXT("Normal Double is not B2B eligible"),
		TotorisGeneration::IsBackToBackEligible(ETotorisSpinKind::None, 2, false));

	TestFalse(TEXT("No-line Spin does not advance B2B"),
		TotorisGeneration::IsBackToBackEligible(ETotorisSpinKind::Full, 0, false));

	// Regression coverage for the hexadecimal rotation-transition keys.
	TestEqual(TEXT("T R->0 has all five 90-degree SRS kick tests"),
		TotorisGeneration::RotationKicks(ETotorisMino::T, 1, 0).Num(), 5);

	TestEqual(TEXT("T R->L has all six 180-degree kick tests"),
		TotorisGeneration::RotationKicks180(ETotorisMino::T, 1, 3).Num(), 6);

	return true;
}

#endif
