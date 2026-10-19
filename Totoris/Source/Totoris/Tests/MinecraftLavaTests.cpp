#include "../MinecraftLavaFlow.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMinecraftLavaFlowTest, "Totoris.Minecraft.LavaFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMinecraftLavaFlowTest::RunTest(const FString& Parameters)
{
	using namespace MinecraftLava;
	FFlow Flow;
	auto Flat = [](const FIntVector&, const FIntVector& B) { return B.Z >= 0; };
	Flow.Start();
	TestEqual(TEXT("One source"), Flow.Cells.Num(), 1);
	Flow.Advance(Flat);
	TestEqual(TEXT("First tick reaches only the four direct neighbors"), Flow.Cells.Num(), 5);
	Flow.Advance(Flat);
	TestEqual(TEXT("Second tick reaches Manhattan distance two"), Flow.Cells.Num(), 13);
	Flow.Advance(Flat);
	TestEqual(TEXT("Third tick reaches distance three"), Flow.Cells.Num(), 25);
	TestFalse(TEXT("Flat pool stabilizes"), Flow.Advance(Flat));
	TestFalse(TEXT("No fourth horizontal step"), Flow.Cells.Contains(FIntVector(4,0,0)));
	for (const auto& Pair : Flow.Cells)
		TestTrue(TEXT("No diagonal shortcut"), FMath::Abs(Pair.Key.X) + FMath::Abs(Pair.Key.Y) <= 3);

	Flow.Start();
	auto Wall = [](const FIntVector&, const FIntVector& B) { return B.Z >= 0 && B.X != 1; };
	for (int32 I=0; I<5; ++I) Flow.Advance(Wall);
	TestFalse(TEXT("Solid wall is not filled"), Flow.Cells.Contains(FIntVector(1,0,0)));
	TestFalse(TEXT("Flow cannot pass through a wall"), Flow.Cells.Contains(FIntVector(2,0,0)));

	Flow.Start();
	auto Drop = [](const FIntVector&, const FIntVector& B) { return B.Z >= -2; };
	Flow.Advance(Drop);
	TestEqual(TEXT("Unsupported source first flows downward only"), Flow.Cells.Num(), 2);
	TestTrue(TEXT("First falling cell"), Flow.Cells.Contains(FIntVector(0,0,-1)));
	TestFalse(TEXT("Fall does not cascade two levels in a single tick"), Flow.Cells.Contains(FIntVector(0,0,-2)));
	for (int32 I=0; I<5; ++I) Flow.Advance(Drop);
	TestTrue(TEXT("Landing resets horizontal reach"), Flow.Cells.Contains(FIntVector(3,0,-2)));
	TestFalse(TEXT("No lateral spreading in midair"), Flow.Cells.Contains(FIntVector(1,0,-1)));

	Flow.Start();
	auto Ledge = [](const FIntVector&, const FIntVector& B) { return B.Z >= 0 || (B.X >= 2 && B.Z >= -1); };
	Flow.Advance(Ledge);
	TestEqual(TEXT("Prefer closest route to a drop"), Flow.Cells.Num(), 2);
	TestTrue(TEXT("Ledge direction selected"), Flow.Cells.Contains(FIntVector(1,0,0)));
	Flow.Reset();
	TestEqual(TEXT("Stop clears simulation"), Flow.Cells.Num(), 0);
	Flow.Start();
	TestEqual(TEXT("Replay starts with only one source"), Flow.Cells.Num(), 1);
	return true;
}
#endif
