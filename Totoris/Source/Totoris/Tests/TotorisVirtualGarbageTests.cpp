#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../TotorisVirtualGarbage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisVirtualGarbageOptionsTest,
    "Totoris.VirtualGarbage.OptionsAndPackets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisVirtualGarbageOptionsTest::RunTest(const FString& Parameters)
{
    FTotorisVirtualGarbageSimulator Simulator;
    FTotorisVirtualGarbageConfig Config;
    Config.bEnabled = false;
    Config.InitialDifficulty = 4;
    Config.bDifficultyIncrease = true;
    Config.Seed = 12345;
    Simulator.Reset(Config);
    Simulator.Tick(600.0);
    Simulator.RecordPlacementVirtualAttack(10000.0, 1);
    TestEqual(TEXT("Disabled attacks never create warnings"), Simulator.GetWarningLines(), 0);
    TestEqual(TEXT("Disabled attacks never change difficulty"), Simulator.GetDifficulty(), 4);

    Config.bEnabled = true;
    Config.bDifficultyIncrease = false;
    Simulator.Reset(Config);
    // Covers the post-grace transition: attacks must begin accumulating after
    // the initial five seconds rather than getting stuck at that boundary.
    Simulator.Tick(120.0);
    Simulator.RecordPlacementVirtualAttack(10000.0, 1);
    TestEqual(TEXT("Fixed difficulty ignores virtual altitude"), Simulator.GetDifficulty(), 4);
    const TArray<FTotorisVirtualGarbagePacket> Packets = Simulator.TakeActivatedPacketsForLock(600.0);
    TestTrue(TEXT("Enabled simulation produces activated packets"), Packets.Num() > 0);
    for (const FTotorisVirtualGarbagePacket& Packet : Packets)
    {
        TestTrue(TEXT("Packets contain at most four lines"), Packet.Lines >= 1 && Packet.Lines <= 4);
    }

    Config.bDifficultyIncrease = true;
    Simulator.Reset(Config);
    Simulator.RecordPlacementVirtualAttack(10000.0, 1);
    TestEqual(TEXT("Increasing difficulty caps at level ten"), Simulator.GetDifficulty(), 10);
    return true;
}

#endif
