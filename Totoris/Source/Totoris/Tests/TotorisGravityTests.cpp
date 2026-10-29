#include "../TotorisBlockGeneratorComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisGravityOptionsTest, "Totoris.Gravity.SetupOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTotorisGravityOptionsTest::RunTest(const FString& Parameters)
{
	UTotorisBlockGeneratorComponent* Generator = NewObject<UTotorisBlockGeneratorComponent>();
	const ETotorisClassicMode Modes[] = { ETotorisClassicMode::Endless, ETotorisClassicMode::Sprint,
		ETotorisClassicMode::Blitz, ETotorisClassicMode::CheeseRace };
	for (const auto Mode : Modes)
	{
		for (int32 Flags = 0; Flags < 4; ++Flags)
		{
			const bool bStart = (Flags & 1) != 0;
			const bool bIncrease = (Flags & 2) != 0;
			FTotorisClassicSettings Settings;
			Settings.Mode = Mode;
			Generator->ConfigureClassicGame(Settings, bStart, bIncrease);
			Generator->ElapsedSeconds = 0.;
			Generator->BlitzLevel = 1;
			Generator->ActiveGravityG = Generator->ResolveGravityForNewPiece();
			const float Initial = bStart ? (Mode == ETotorisClassicMode::Blitz ? 0.0167f : 0.02f) : 0.f;
			TestTrue(TEXT("Start Gravity determines initial speed in every mode"),
				FMath::IsNearlyEqual(Generator->GetCurrentGravityG(), Initial));
			Generator->ElapsedSeconds = 600.;
			Generator->BlitzLevel = 8;
			Generator->ActiveGravityG = Generator->ResolveGravityForNewPiece();
			const float Later = Generator->GetCurrentGravityG();
			TestTrue(TEXT("Only enabled Gravity Increase raises speed after time/lines"),
				bIncrease ? Later > Initial : FMath::IsNearlyEqual(Later, Initial));
			Generator->ElapsedSeconds = 1.e6;
			Generator->BlitzLevel = 100;
			Generator->ActiveGravityG = Generator->ResolveGravityForNewPiece();
			TestTrue(TEXT("Gravity never exceeds 20G"), Generator->GetCurrentGravityG() <= 20.f);
		}
	}
	return true;
}
#endif
