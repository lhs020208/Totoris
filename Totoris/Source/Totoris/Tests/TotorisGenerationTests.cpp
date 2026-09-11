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
#endif
