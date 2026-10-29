#include "TotorisPlayerController.h"
#include "TotorisCameraManager.h"
#include "TotorisEventAudio.h"
#include "TotorisPreferences.h"
#include "TotorisWindowPlacement.h"

#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundBase.h"
#include "Misc/ConfigCacheIni.h"
#include "TotorisBlockGeneratorComponent.h"
#include "TotorisClassicHUDWidget.h"
#include "TotorisSannabiEventWidget.h"
#include "TotorisMahjongEventWidget.h"
#include "TotorisReAnimalEventWidget.h"
#include "TotorisAudioMix.h"
#include "TotorisMenuManager.h"
#include "SmilemoSitoJumpComponent.h"
#include "DummyDragonFlightComponent.h"
#include "AnjanathDropEventComponent.h"
#include "MinecraftBlockEventComponent.h"
#include "JourneyEventComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "UObject/UObjectIterator.h"
#include "TotorisMission.h"
#include "TotorisMissionPatterns.h"
#include "TotorisSChallengeRules.h"


namespace
{
	constexpr TCHAR HandlingConfigSection[] = TEXT("Totoris.Handling");
	constexpr TCHAR ARRKey[] = TEXT("ARRMilliseconds");
	constexpr TCHAR DASKey[] = TEXT("DASMilliseconds");
	constexpr TCHAR DCDKey[] = TEXT("DCDMilliseconds");
	constexpr TCHAR SDFKey[] = TEXT("SDFMultiplier");
	constexpr TCHAR SDFInfiniteKey[] = TEXT("SDFInfinite");

	constexpr TCHAR KeyBindingsConfigSection[] = TEXT("Totoris.KeyBindings");
	const TCHAR* KeyBindActionConfigNames[] =
	{
		TEXT("MoveLeft"),
		TEXT("MoveRight"),
		TEXT("SoftDrop"),
		TEXT("HardDrop"),
		TEXT("RotateCW"),
		TEXT("RotateCCW"),
		TEXT("Rotate180"),
		TEXT("Hold"),
		TEXT("Restart")
	};

	FLinearColor MissionTierBaseColor(ETotorisMissionTier Tier)
	{
		switch (Tier)
		{
		case ETotorisMissionTier::F: return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("957A97")));
		case ETotorisMissionTier::E: return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("4F65CB")));
		case ETotorisMissionTier::D: return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5EC054")));
		case ETotorisMissionTier::C: return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FFE90F")));
		case ETotorisMissionTier::B: return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FF561B")));
		case ETotorisMissionTier::A: return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FF5EFF")));
		case ETotorisMissionTier::S: return FLinearColor(1.f, 0.f, 0.f, 1.f);
		default: return FLinearColor::White;
		}
	}
}

ATotorisPlayerController::ATotorisPlayerController()
{
	PlayerCameraManagerClass = ATotorisCameraManager::StaticClass();
	bShowMouseCursor = true;
}

void ATotorisPlayerController::BeginPlay()
{
	Super::BeginPlay();
	MissionFailSound = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/Totoris/Audio/Gameplay/MissionFailSound.MissionFailSound"));
	MissionClearSound1 = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/Totoris/Audio/Gameplay/MissionClearSound1.MissionClearSound1"));
	MissionClearSound2 = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/Totoris/Audio/Gameplay/MissionClearSound2.MissionClearSound2"));
	UsodaSound = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/Totoris/Audio/Gameplay/UsodaSound.UsodaSound"));
	HigurashiSound = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/Totoris/Audio/Gameplay/HigurashiSound.HigurashiSound"));
	InitializeSmilemoSitoJumpPath();
	InitializeDummyDragonFlight();
	InitializeAnjanathDropEvent();
	InitializeMinecraftBlockEvent();

	MenuManager = NewObject<UTotorisMenuManager>(this);
	if (!ensure(MenuManager))
	{
		return;
	}

	MenuManager->Initialize(this);
	CommonGameSetupSettings.bQuickStart = TotorisPreferences::ReadBool(TEXT("QuickStart"), false);
	CreateShowWidgetsOverlay();
	LoadHandlingSettings();
	LoadKeyBindings();
    SetClassicMode(ETotorisClassicMode::Endless);

	// The room/camera remain untouched. Only gameplay simulation and actors
	// explicitly tagged as gameplay presentation are hidden.
	StopClassicGame();
	ShowMainMenu();
}

void ATotorisPlayerController::InitializeSmilemoSitoJumpPath()
{
	if (!GetWorld() || IsValid(SmilemoSitoJumpComponent))
	{
		return;
	}

	AActor* SmilemoSito = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		// Use the same persisted identity in editor and packaged builds.
		if (It->ActorHasTag(TEXT("SmilemoSito")))
		{
			SmilemoSito = *It;
			break;
		}
	}

	if (!IsValid(SmilemoSito))
	{
		UE_LOG(LogTemp, Warning, TEXT("Totoris could not find the SmilemoSito actor tag."));
		return;
	}

	// The presentation actor must be movable in the level as well.  Keep this
	// runtime guard so an accidentally restored Static setting cannot silently
	// turn every route update into a rejected transform operation.
	if (USceneComponent* SmilemoRoot = SmilemoSito->GetRootComponent())
	{
		SmilemoRoot->SetMobility(EComponentMobility::Movable);
	}

	SmilemoSitoJumpComponent = NewObject<USmilemoSitoJumpComponent>(SmilemoSito, TEXT("SmilemoSitoJumpComponent"));
	SmilemoSitoJumpComponent->RegisterComponent();
	SmilemoSitoJumpComponent->InitializeRoute({
		FVector(-47690.411846f, -14801.172975f, 114.415751f),
		FVector(-47357.839591f, -14605.093259f, 180.350017f),
		FVector(-47403.853720f, -14053.112804f, 140.970096f),
		FVector(-47328.374086f, -13642.661665f, 188.638375f),
		FVector(-47943.481624f, -13329.632793f, 25.632562f)
	});

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("SmilemoBoss")))
		{
			SmilemoSitoJumpComponent->InitializeBossFollower(*It);
			break;
		}
	}
}

void ATotorisPlayerController::PlaySmilemoSitoJumpPath()
{
	if (!IsValid(SmilemoSitoJumpComponent))
	{
		InitializeSmilemoSitoJumpPath();
	}

	if (IsValid(SmilemoSitoJumpComponent))
	{
		SmilemoSitoJumpComponent->PlayJumpRoute();
	}
}

void ATotorisPlayerController::InitializeDummyDragonFlight()
{
	if (!GetWorld() || IsValid(DummyDragonFlightComponent))
	{
		return;
	}

	AActor* DragonFlightActor = nullptr;
	AActor* WindowPlane = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!DragonFlightActor && It->ActorHasTag(TEXT("DragonFlightActor")))
		{
			DragonFlightActor = *It;
		}
		if (!WindowPlane && It->ActorHasTag(TEXT("DragonWindowTarget")))
		{
			WindowPlane = *It;
		}
	}

	if (!IsValid(DragonFlightActor) || !IsValid(WindowPlane))
	{
		UE_LOG(LogTemp, Warning, TEXT("Totoris could not find Dragon_Model or the Plane2 dragon window target."));
		return;
	}

	DummyDragonFlightComponent = NewObject<UDummyDragonFlightComponent>(DragonFlightActor, TEXT("DummyDragonFlightComponent"));
	DummyDragonFlightComponent->RegisterComponent();
	DummyDragonFlightComponent->InitializeFlight(WindowPlane);
}

void ATotorisPlayerController::PlayDummyDragonFlight()
{
	if (!IsValid(DummyDragonFlightComponent))
	{
		InitializeDummyDragonFlight();
	}
	if (IsValid(DummyDragonFlightComponent))
	{
		DummyDragonFlightComponent->PlayFlight();
	}
}

void ATotorisPlayerController::InitializeAnjanathDropEvent()
{
	if (!GetWorld() || IsValid(AnjanathDropEventComponent))
	{
		return;
	}

	AActor* AnjanathModel = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("AnjanathEventActor")))
		{
			AnjanathModel = *It;
			break;
		}
	}

	if (!IsValid(AnjanathModel))
	{
		UE_LOG(LogTemp, Warning, TEXT("Totoris could not find the Anjanath_Model event actor."));
		return;
	}

	// Prefer an editor-authored component, so its Details-panel settings and
	// optional AnjanathFireOrigin scene/arrow marker remain configurable.
	AnjanathDropEventComponent = AnjanathModel->FindComponentByClass<UAnjanathDropEventComponent>();
	if (!IsValid(AnjanathDropEventComponent))
	{
		// The placed skeletal mesh is kept as a pure presentation actor; create
		// the event driver at runtime so swapping its final mesh stays simple.
		AnjanathDropEventComponent = NewObject<UAnjanathDropEventComponent>(
			AnjanathModel, TEXT("AnjanathDropEventComponent"));
		AnjanathDropEventComponent->ConfigureFireBreath(
			AnjanathFireDurationSeconds, AnjanathFireConeAngleDegrees,
			AnjanathFireParticlesPerSecond, AnjanathSmokeParticlesPerSecond,
			AnjanathMouthLocalOffset);
		AnjanathModel->AddInstanceComponent(AnjanathDropEventComponent);
		AnjanathDropEventComponent->RegisterComponent();
	}
	AnjanathDropEventComponent->InitializeDrop();
}

void ATotorisPlayerController::PlayAnjanathDropEvent()
{
	if (!IsValid(AnjanathDropEventComponent))
	{
		InitializeAnjanathDropEvent();
	}
	if (IsValid(AnjanathDropEventComponent))
	{
		AnjanathDropEventComponent->PlayDrop();
	}
}

void ATotorisPlayerController::InitializeMinecraftBlockEvent()
{
	if (!GetWorld() || IsValid(MinecraftBlockEventComponent))
	{
		return;
	}

	AActor* MinecraftBlock = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("MinecraftHeldBlock")) && It->ActorHasTag(TEXT("MinecraftBlock")))
		{
			MinecraftBlock = *It;
			break;
		}
	}

	if (!IsValid(MinecraftBlock))
	{
		UE_LOG(LogTemp, Warning, TEXT("Totoris could not find the MinecraftBlock event actor."));
		return;
	}

	MinecraftBlockEventComponent = MinecraftBlock->FindComponentByClass<UMinecraftBlockEventComponent>();
	if (!IsValid(MinecraftBlockEventComponent))
	{
		MinecraftBlockEventComponent = NewObject<UMinecraftBlockEventComponent>(
			MinecraftBlock, TEXT("MinecraftBlockEventComponent"));
		MinecraftBlock->AddInstanceComponent(MinecraftBlockEventComponent);
		MinecraftBlockEventComponent->RegisterComponent();
	}
}

void ATotorisPlayerController::PlayMinecraftBlockEvent()
{
	if (!IsValid(MinecraftBlockEventComponent))
	{
		InitializeMinecraftBlockEvent();
	}
	if (IsValid(MinecraftBlockEventComponent))
	{
		MinecraftBlockEventComponent->PlayEvent();
	}
}

void ATotorisPlayerController::PlayMahjongEvent()
{
	if (IsValid(MahjongEventWidget))
	{
		MahjongEventWidget->StartEvent();
	}
}

void ATotorisPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TotorisWindowPlacement::Capture(GetWorld(), true);
	UnbindMissionGameplayEvents();
	UsodaFadeElapsedSeconds = -1.f;
	if (IsValid(UsodaPostProcessVolume))
	{
		UsodaPostProcessVolume->Destroy();
		UsodaPostProcessVolume = nullptr;
	}
	StopUsodaHigurashiSound();
    if (IsValid(ClassicHUD))
    {
        ClassicHUD->RemoveFromParent();
        ClassicHUD = nullptr;
    }

	// Flush pending handling changes even when PIE/game is stopped directly
	// without returning through the Settings Back button.
	SaveHandlingSettings();
	SaveKeyBindings();

	Super::EndPlay(EndPlayReason);
}

void ATotorisPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TotorisWindowPlacement::Capture(GetWorld());
	if (!EventPreviewName.IsNone() && !IsPreviewEventPlaying())
		FinishEventPreview();
	if (UsodaFadeElapsedSeconds >= 0.f)
	{
		UsodaFadeElapsedSeconds += DeltaSeconds;
		const float InversionAlpha = 1.f - FMath::Clamp(UsodaFadeElapsedSeconds / .3f, 0.f, 1.f);
		if (IsValid(UsodaPostProcessVolume))
		{
			// In the colour-grading combine pass the result is
			// Color * Gain + Offset.  Alpha 1 yields 1 - Color; alpha 0
			// yields the original Color, with a direct 0.3 second fade.
			const float Gain = 1.f - 2.f * InversionAlpha;
			UsodaPostProcessVolume->Settings.ColorGain = FVector4(Gain, Gain, Gain, 1.f);
			UsodaPostProcessVolume->Settings.ColorOffset = FVector4(InversionAlpha, InversionAlpha, InversionAlpha, 0.f);
		}
		if (UsodaFadeElapsedSeconds >= .3f)
		{
			UsodaFadeElapsedSeconds = -1.f;
			if (IsValid(UsodaPostProcessVolume))
			{
				UsodaPostProcessVolume->Destroy();
				UsodaPostProcessVolume = nullptr;
			}
		}
	}
	if (HigurashiElapsedSeconds >= 0.f)
	{
		HigurashiElapsedSeconds += DeltaSeconds;
		const float FadeDuration = FMath::Min(5.f, HigurashiDurationSeconds);
		const float FadeStartTime = FMath::Max(0.f, HigurashiDurationSeconds - FadeDuration);
		if (IsValid(HigurashiAudioComponent) && HigurashiElapsedSeconds >= FadeStartTime)
		{
			const float FadeAlpha = 1.f - FMath::Clamp(
				(HigurashiElapsedSeconds - FadeStartTime) / FMath::Max(FadeDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
			HigurashiAudioComponent->SetVolumeMultiplier(
				TotorisAudioMix::Higurashi * TotorisAudioMix::MasterVolume * FadeAlpha);
		}
		if (HigurashiElapsedSeconds >= HigurashiDurationSeconds)
		{
			StopUsodaHigurashiSound();
		}
	}
	if (GameEndDelaySeconds >= 0.f && (GameEndDelaySeconds += DeltaSeconds) >= 1.f)
	{
		GameEndDelaySeconds = -1.f;
		ShowGameEndWidget();
	}
	if (GameEndFadeSeconds >= 0.f && IsValid(GameEndWidget))
	{
		GameEndFadeSeconds += DeltaSeconds;
		GameEndWidget->SetRenderOpacity(FMath::Clamp(GameEndFadeSeconds / .18f, 0.f, 1.f));
		if (GameEndFadeSeconds >= .18f) GameEndFadeSeconds = -1.f;
	}
}

void ATotorisPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ATotorisPlayerController::ShowPauseMenu);
}

void ATotorisPlayerController::ShowPauseMenu()
{
	UTotorisBlockGeneratorComponent* Game = FindBlockGenerator();
	if (!Game || !Game->IsGameplayActive()) return;
	if (!IsValid(PauseWidget))
	{
		const TSubclassOf<UUserWidget> Class = LoadClass<UUserWidget>(nullptr,
			TEXT("/Game/Totoris/UI/Widgets/WBP_Pause.WBP_Pause_C"));
		if (!Class) return;
		PauseWidget = CreateWidget<UUserWidget>(this, Class);
		if (!PauseWidget) return;
		UButton* StartButton = Cast<UButton>(PauseWidget->GetWidgetFromName(TEXT("StartButton")));
		if (!StartButton) { PauseWidget = nullptr; return; }
		StartButton->OnClicked.AddDynamic(this, &ATotorisPlayerController::ResumeFromPause);
		if (UButton* Button = Cast<UButton>(PauseWidget->GetWidgetFromName(TEXT("SettingsButton"))))
			Button->OnClicked.AddDynamic(this, &ATotorisPlayerController::OpenSettingsFromPause);
		if (UButton* Button = Cast<UButton>(PauseWidget->GetWidgetFromName(TEXT("ModSetupButton"))))
			Button->OnClicked.AddDynamic(this, &ATotorisPlayerController::ReturnFromGameEndToModeSetup);
	}
	FlushPressedKeys();
	Game->PauseGameplay();
	if (!Game->IsGameplayPaused()) return;
	if (!PauseWidget->IsInViewport()) PauseWidget->AddToPlayerScreen(10000);
	bShowMouseCursor = true;
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(PauseWidget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

void ATotorisPlayerController::ResumeFromPause()
{
	ApplyHandlingSettingsToGame();
	ApplyKeyBindingsToGame();
	if (IsValid(PauseWidget)) PauseWidget->RemoveFromParent();
	FlushPressedKeys();
	if (UTotorisBlockGeneratorComponent* Game = FindBlockGenerator()) Game->ResumeGameplayWithCountdown();
	EnterGameInputMode();
}

void ATotorisPlayerController::OpenSettingsFromPause()
{
	UTotorisBlockGeneratorComponent* Game = FindBlockGenerator();
	if (!Game || !Game->IsGameplayPaused() || !MenuManager) return;
	if (!MenuManager->ShowSettings(10000)) return;
	UUserWidget* Settings = MenuManager->GetCurrentWidget();
	UButton* Back = Cast<UButton>(Settings->GetWidgetFromName(TEXT("BackButton")));
	if (!Back) { MenuManager->HideCurrentMenu(); return; }
	// Replace the menu-return delegate only on this pause-owned settings instance.
	Back->OnClicked.Clear();
	Back->OnClicked.AddDynamic(this, &ATotorisPlayerController::ReturnToPauseFromSettings);
	bSettingsOpenedFromPause = true;
	if (IsValid(PauseWidget)) PauseWidget->RemoveFromParent();
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Settings->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	bShowMouseCursor = true;
	SetInputMode(Mode);
}

void ATotorisPlayerController::ReturnToPauseFromSettings()
{
	if (!bSettingsOpenedFromPause) return;
	bSettingsOpenedFromPause = false;
	SaveHandlingSettings();
	SaveKeyBindings();
	ApplyHandlingSettingsToGame();
	ApplyKeyBindingsToGame();
	if (MenuManager) MenuManager->HideCurrentMenu();
	ShowPauseMenu();
}

void ATotorisPlayerController::PlayUsodaEvent()
{
	if (!GetWorld())
	{
		return;
	}

	if (IsValid(UsodaPostProcessVolume))
	{
		UsodaPostProcessVolume->Destroy();
		UsodaPostProcessVolume = nullptr;
	}

	UsodaPostProcessVolume = GetWorld()->SpawnActor<APostProcessVolume>();
	if (IsValid(UsodaPostProcessVolume))
	{
		// A real world post-process volume is used here rather than a component
		// attached to the controller, ensuring the renderer registers it as a
		// global, unbound effect in PIE as well as packaged builds.
		UsodaPostProcessVolume->bUnbound = true;
		UsodaPostProcessVolume->bEnabled = true;
		UsodaPostProcessVolume->Priority = 100000.f;
		UsodaPostProcessVolume->BlendWeight = 1.f;
		UsodaPostProcessVolume->Settings.bOverride_ColorGain = true;
		UsodaPostProcessVolume->Settings.bOverride_ColorOffset = true;
		UsodaPostProcessVolume->Settings.ColorGain = FVector4(-1.f, -1.f, -1.f, 1.f);
		UsodaPostProcessVolume->Settings.ColorOffset = FVector4(1.f, 1.f, 1.f, 0.f);
	}

	if (IsValid(UsodaSound))
	{
		TotorisEventAudio::PlaySound2D(this, UsodaSound,
			TotorisAudioMix::Usoda * TotorisAudioMix::MasterVolume);
	}
	StopUsodaHigurashiSound();
	if (IsValid(HigurashiSound))
	{
		// The Usoda cue deliberately uses a fixed ten-second excerpt: five
		// seconds at full level followed by a five-second fade to silence.
		// A shorter source still ends naturally without attempting to extend it.
		HigurashiDurationSeconds = FMath::Min(HigurashiSound->GetDuration(), 10.f);
		HigurashiAudioComponent = UGameplayStatics::SpawnSound2D(
			this,
			HigurashiSound,
			TotorisAudioMix::Higurashi * TotorisAudioMix::MasterVolume,
			1.f,
			0.f,
			nullptr,
			false,
			false);
		HigurashiElapsedSeconds = HigurashiDurationSeconds > 0.f ? 0.f : -1.f;
	}
	UsodaFadeElapsedSeconds = 0.f;
}

void ATotorisPlayerController::StopUsodaHigurashiSound()
{
	HigurashiElapsedSeconds = -1.f;
	HigurashiDurationSeconds = 0.f;
	if (IsValid(HigurashiAudioComponent))
	{
		HigurashiAudioComponent->Stop();
		HigurashiAudioComponent->DestroyComponent();
		HigurashiAudioComponent = nullptr;
	}
}

void ATotorisPlayerController::HandleRunFinished(const FTotorisRunSummary& Summary)
{
	if (!bGameEndEligibleForCurrentRun || !Summary.bValid)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Totoris ignored a result-screen request outside an active run."));
		return;
	}

	PendingResultSummary = Summary;
	// Let the final S-set celebration play; exiting/restarting still cancels it.
	if (SelectedGameSetupMode != ETotorisGameSetupMode::SChallenge || Summary.Result != ETotorisRunResult::Completed)
		StopMissionPresentationEvents();
	GameEndDelaySeconds = 0.f;
}

void ATotorisPlayerController::ShowGameEndWidget()
{
	if (!IsLocalController() || !bGameEndEligibleForCurrentRun || !PendingResultSummary.bValid)
	{
		GameEndDelaySeconds = -1.f;
		return;
	}
	SetShowWidgetsOverlayEnabled(true);
	if (bOtherWidgetsHidden)
	{
		bGameEndHiddenByOverlay = true;
		return;
	}
	bGameEndHiddenByOverlay = false;
	// Match the pre-game presentation state: this also hides the board frame,
	// HOLD/NEXT panels, and any separately tagged gameplay visual actors.
	SetTaggedGameplayVisualsVisible(false);
	if (UTotorisBlockGeneratorComponent* BlockGenerator = FindBlockGenerator())
	{
		// The result screen owns the presentation once FINISH has faded out.
		BlockGenerator->SetGameplayVisible(false);
	}
	if (IsValid(ClassicHUD)) { ClassicHUD->RemoveFromParent(); ClassicHUD = nullptr; }
	GameEndWidget = CreateWidget<UUserWidget>(this, LoadClass<UUserWidget>(nullptr, TEXT("/Game/Totoris/UI/Widgets/WBP_GameEnd.WBP_GameEnd_C")));
	if (!IsValid(GameEndWidget)) return;
	const auto SetText = [this](const TCHAR* Name, const FString& Value)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(GameEndWidget->GetWidgetFromName(Name)))
			Text->SetText(FText::FromString(Value));
	};
	const FTotorisRunStatistics& Stats = PendingResultSummary.Statistics;
	const bool bEndless = SelectedGameSetupMode != ETotorisGameSetupMode::SChallenge &&
		PendingResultSummary.Settings.Mode == ETotorisClassicMode::Endless;
	SetText(TEXT("GameEndText"), bEndless ? TEXT("Finish")
		: PendingResultSummary.Result == ETotorisRunResult::Completed ? TEXT("Game Clear") : TEXT("Game Over"));
	SetText(TEXT("PiecesPlacedValue"), FString::FromInt(PendingResultSummary.PiecesPlaced));
	SetText(TEXT("PiecesPerSecondValue"), FString::Printf(TEXT("%.2f"), Stats.PiecesPerSecond));
	SetText(TEXT("KeysPressedValue"), FString::FromInt(Stats.KeysPressed));
	SetText(TEXT("KeysPerPieceValue"), FString::Printf(TEXT("%.3f"), Stats.KeysPerPiece));
	SetText(TEXT("KeysPerSecondValue"), FString::Printf(TEXT("%.3f"), Stats.KeysPerSecond));
	SetText(TEXT("HoldsValue"), FString::FromInt(Stats.Holds));
	SetText(TEXT("ScoreValue"), FString::Printf(TEXT("%lld"), static_cast<long long>(PendingResultSummary.Score)));
	const int64 Milliseconds = FMath::Max<int64>(0, FMath::RoundToInt64(PendingResultSummary.ElapsedSeconds * 1000.0));
	SetText(TEXT("TimeValue"), FString::Printf(TEXT("%lld:%02lld.%03lld"),
		static_cast<long long>(Milliseconds / 60000), static_cast<long long>((Milliseconds / 1000) % 60), static_cast<long long>(Milliseconds % 1000)));
	SetText(TEXT("LinesValue"), FString::FromInt(PendingResultSummary.LinesCleared));
	SetText(TEXT("LinesPerMinuteValue"), FString::Printf(TEXT("%.2f"), Stats.LinesPerMinute));
	SetText(TEXT("SpinsValue"), FString::FromInt(Stats.TotalSpins));
	SetText(TEXT("MaximumComboValue"), FString::FromInt(Stats.MaximumCombo));
	SetText(TEXT("MaximumBackToBackChainValue"), FString::FromInt(Stats.MaximumBackToBackChain));
	SetText(TEXT("AllClearsValue"), FString::FromInt(Stats.AllClears));
	SetText(TEXT("FinessePercentageValue"), FString::Printf(TEXT("%.2f%%"), Stats.FinessePercent));
	SetText(TEXT("FinesseFaultsValue"), FString::FromInt(Stats.FinesseFaults));
	// FULL uses the same immutable end-of-run snapshot as OVERVIEW.  Keep this
	// population here, before the widget is added, so switching tabs can never
	// reveal placeholder values during the result-screen fade-in.
	SetText(TEXT("SinglesValue"), FString::FromInt(Stats.Singles));
	SetText(TEXT("DoublesValue"), FString::FromInt(Stats.Doubles));
	SetText(TEXT("TriplesValue"), FString::FromInt(Stats.Triples));
	// FullKeysPerPieceValue is the legacy designer name for the QUADS value
	// widget.  Populate both names so a future in-editor rename needs no code
	// change and the currently saved widget receives the right number.
	SetText(TEXT("QuadsValue"), FString::FromInt(Stats.Quads));
	SetText(TEXT("FullKeysPerPieceValue"), FString::FromInt(Stats.Quads));
	SetText(TEXT("FSpinsValue"), FString::FromInt(Stats.FullSpins));
	SetText(TEXT("SpinMinisValue"), FString::FromInt(Stats.SpinMinis));
	SetText(TEXT("SpinMiniSinglesValue"), FString::FromInt(Stats.SpinMiniSingles));
	SetText(TEXT("SpinSinglesValue"), FString::FromInt(Stats.SpinSingles));
	SetText(TEXT("SpinMiniDoublesValue"), FString::FromInt(Stats.SpinMiniDoubles));
	SetText(TEXT("SpinDoublesValue"), FString::FromInt(Stats.SpinDoubles));
	SetText(TEXT("SpinMiniTriplesValue"), FString::FromInt(Stats.SpinMiniTriples));
	SetText(TEXT("SpinTriplesValue"), FString::FromInt(Stats.SpinTriples));
	SetText(TEXT("FAllClearsValue"), FString::FromInt(Stats.AllClears));
	// MissionTab used to be a separate result page. Mission Clear now lives on
	// FULL, so retain the label authored in the widget and only populate its
	// value. The count is scoped to this mission-mode run and resets on restart.
	const bool bMissionMode = SelectedGameSetupMode == ETotorisGameSetupMode::Mission;
	const int32 CompletedSets = SelectedGameSetupMode == ETotorisGameSetupMode::SChallenge
		? CompletedMissionCount : bMissionMode ? CompletedMissionCount / 3 : 0;
	const int32 FinalSetClears = bMissionMode ? CompletedMissionCount % 3 : 0;
	SetText(TEXT("MissionValue"), FString::Printf(TEXT("%d + %d"), CompletedSets, FinalSetClears));
	GameEndWidget->SetRenderOpacity(0.f);
	// Shared by every mode: clear the bottom-right ShowWidgets toggle without
	// changing the result panel's size or internal layout (DPI-scaled units).
	GameEndWidget->SetRenderTranslation(FVector2D(0.f, -20.f));
	GameEndWidget->AddToPlayerScreen(30);
	GameEndFadeSeconds = 0.f;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(GameEndWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	if (UButton* Button = Cast<UButton>(GameEndWidget->GetWidgetFromName(TEXT("OverViewTab")))) Button->OnClicked.AddDynamic(this, &ATotorisPlayerController::ShowGameEndOverview);
	if (UButton* Button = Cast<UButton>(GameEndWidget->GetWidgetFromName(TEXT("FullTab")))) Button->OnClicked.AddDynamic(this, &ATotorisPlayerController::ShowGameEndFull);
	if (UButton* Button = Cast<UButton>(GameEndWidget->GetWidgetFromName(TEXT("RestartTab")))) Button->OnClicked.AddDynamic(this, &ATotorisPlayerController::RestartFromGameEnd);
	if (UButton* Button = Cast<UButton>(GameEndWidget->GetWidgetFromName(TEXT("BackTab")))) Button->OnClicked.AddDynamic(this, &ATotorisPlayerController::ReturnFromGameEndToModeSetup);
	ShowGameEndOverview();
}

void ATotorisPlayerController::DismissGameEndWidget()
{
	GameEndDelaySeconds = -1.f;
	GameEndFadeSeconds = -1.f;
	bGameEndHiddenByOverlay = false;
	bGameEndEligibleForCurrentRun = false;

	// A previous bad transition could create a second GameEnd and overwrite the
	// controller pointer. Remove every result widget in this world, not merely
	// the most recently tracked instance, so old orphaned overlays cannot remain.
	const TSubclassOf<UUserWidget> GameEndClass = LoadClass<UUserWidget>(
		nullptr, TEXT("/Game/Totoris/UI/Widgets/WBP_GameEnd.WBP_GameEnd_C"));
	if (GameEndClass)
	{
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			UUserWidget* Widget = *It;
			if (IsValid(Widget) && Widget->GetWorld() == GetWorld() && Widget->IsA(GameEndClass))
			{
				Widget->RemoveFromParent();
			}
		}
	}

	if (IsValid(GameEndWidget))
	{
		GameEndWidget->RemoveFromParent();
		GameEndWidget = nullptr;
	}
}

void ATotorisPlayerController::ShowGameEndOverview()
{
	if (UWidgetSwitcher* Switcher = IsValid(GameEndWidget) ? Cast<UWidgetSwitcher>(GameEndWidget->GetWidgetFromName(TEXT("StatsSwitcher"))) : nullptr) Switcher->SetActiveWidgetIndex(0);
}

void ATotorisPlayerController::ShowGameEndFull()
{
	if (UWidgetSwitcher* Switcher = IsValid(GameEndWidget) ? Cast<UWidgetSwitcher>(GameEndWidget->GetWidgetFromName(TEXT("StatsSwitcher"))) : nullptr) Switcher->SetActiveWidgetIndex(1);
}

void ATotorisPlayerController::CreateShowWidgetsOverlay()
{
	if (!IsLocalController() || IsValid(ShowWidgetsOverlay))
	{
		return;
	}

	const TSubclassOf<UUserWidget> OverlayClass = LoadClass<UUserWidget>(
		nullptr,
		TEXT("/Game/Totoris/UI/Widgets/WBP_ShowWigets.WBP_ShowWigets_C"));
	if (!OverlayClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Totoris: could not load WBP_ShowWigets."));
		return;
	}

	ShowWidgetsOverlay = CreateWidget<UUserWidget>(this, OverlayClass);
	if (!IsValid(ShowWidgetsOverlay))
	{
		return;
	}

	// The widget's canvas is authored for the bottom-right of the viewport.
	// Its high Z order keeps the opt-out check box reachable when menus hide.
	ShowWidgetsOverlay->AddToPlayerScreen(200);
	LayoutShowWidgetsOverlay();
	if (UCheckBox* CheckBox = Cast<UCheckBox>(ShowWidgetsOverlay->GetWidgetFromName(TEXT("CheckBox"))))
	{
		CheckBox->SetIsChecked(bOtherWidgetsHidden);
		CheckBox->OnCheckStateChanged.AddDynamic(this, &ATotorisPlayerController::HandleShowWidgetsCheckStateChanged);
	}
}

void ATotorisPlayerController::LayoutShowWidgetsOverlay()
{
	if (!IsValid(ShowWidgetsOverlay))
	{
		return;
	}

	const FAnchors BottomRight(1.0f, 1.0f);
	const FVector2D BottomRightAlignment(1.0f, 1.0f);
	const auto LayoutWidget = [this, &BottomRight, &BottomRightAlignment](
		const TCHAR* WidgetName,
		const FVector2D& Position,
		const FVector2D& Size)
	{
		if (UWidget* Widget = ShowWidgetsOverlay->GetWidgetFromName(WidgetName))
		{
			if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
			{
				Slot->SetAnchors(BottomRight);
				Slot->SetAlignment(BottomRightAlignment);
				Slot->SetPosition(Position);
				Slot->SetSize(Size);
			}
		}
	};

	// Keep all three independently-authored canvas children together at the
	// viewport's lower-right corner. Negative positions are inset from it.
	LayoutWidget(TEXT("GlassPanel"), FVector2D(-24.0f, -24.0f), FVector2D(180.0f, 58.0f));
	LayoutWidget(TEXT("Text"), FVector2D(-62.0f, -38.0f), FVector2D(116.0f, 30.0f));
	LayoutWidget(TEXT("CheckBox"), FVector2D(-32.0f, -40.0f), FVector2D(26.0f, 26.0f));
}

void ATotorisPlayerController::SetShowWidgetsOverlayEnabled(bool bEnabled)
{
	if (!IsValid(ShowWidgetsOverlay))
	{
		return;
	}

	ShowWidgetsOverlay->SetIsEnabled(bEnabled);
	ShowWidgetsOverlay->SetRenderOpacity(bEnabled ? 1.0f : 0.35f);
}

void ATotorisPlayerController::HandleShowWidgetsCheckStateChanged(bool bIsChecked)
{
	bOtherWidgetsHidden = bIsChecked;
	if (!EventPreviewName.IsNone())
	{
		SetTaggedGameplayVisualsVisible(!bIsChecked);
		if (auto* Game = FindBlockGenerator()) Game->SetGameplayVisible(!bIsChecked);
		return;
	}
	if (MenuManager)
	{
		MenuManager->SetMenusHidden(bOtherWidgetsHidden);
	}

	if (bOtherWidgetsHidden)
	{
		if (IsValid(GameEndWidget))
		{
			GameEndWidget->RemoveFromParent();
			GameEndWidget = nullptr;
			bGameEndHiddenByOverlay = true;
		}
		return;
	}

	if (bGameEndHiddenByOverlay)
	{
		ShowGameEndWidget();
		return;
	}

	if (MenuManager && MenuManager->RestoreDeferredMenu())
	{
		EnterMenuInputMode();
	}
}

void ATotorisPlayerController::RestartFromGameEnd()
{
	// StartClassicGame uses the controller-owned setup values, which are not
	// modified during a run.  This deliberately starts the same configured mode.
	DismissGameEndWidget();

	StartClassicGame();
}

void ATotorisPlayerController::ReturnFromGameEndToModeSetup()
{
	DismissGameEndWidget();

	// The mode setup widget reads these controller-owned settings when it is
	// reconstructed, so all previous check-box and slider selections remain.
	StopClassicGame();
	ShowModeSetup(SelectedGameSetupMode);
}

void ATotorisPlayerController::ShowMainMenu()
{
	if (bSettingsOpenedFromPause) { ReturnToPauseFromSettings(); return; }
	DismissGameEndWidget();
	SaveHandlingSettings();
	SaveKeyBindings();
	StopClassicGame();

	if (MenuManager && MenuManager->ShowMainMenu())
	{
		EnterMenuInputMode();
	}
}

void ATotorisPlayerController::ShowModeSelect()
{
	DismissGameEndWidget();
	if (MenuManager && MenuManager->ShowModeSelect())
	{
		EnterMenuInputMode();
	}
}

void ATotorisPlayerController::ShowSettings()
{
	DismissGameEndWidget();
	if (MenuManager && MenuManager->ShowSettings())
	{
		EnterMenuInputMode();
	}
}

void ATotorisPlayerController::ShowModeSetup(ETotorisGameSetupMode Mode)
{
	SelectedGameSetupMode = Mode;
	DismissGameEndWidget();

	if (MenuManager && MenuManager->ShowModeSetup())
	{
		if (UUserWidget* Widget = MenuManager->GetCurrentWidget())
		{
			if (auto* QuickStart = Cast<UCheckBox>(Widget->GetWidgetFromName(TEXT("QuickStartCheckBox"))))
				QuickStart->SetIsChecked(GetQuickStartEnabled());
			// WBP_ModeSetup is deliberately preserved while the Mission List is
			// open.  Its Construct event therefore cannot be relied on to select
			// the correct panel when returning to it.
			if (UWidgetSwitcher* ModeSwitcher = Cast<UWidgetSwitcher>(Widget->GetWidgetFromName(TEXT("ModeSpecificSwitcher"))))
			{
				const FName PanelName = Mode == ETotorisGameSetupMode::SChallenge ? TEXT("SCPanel")
					: Mode == ETotorisGameSetupMode::Mission ? TEXT("MissionPanel") : TEXT("ClassicPanel");
				if (UWidget* Panel = Widget->GetWidgetFromName(PanelName))
				{
					ModeSwitcher->SetActiveWidget(Panel);
				}
			}

			const TCHAR* Names[] = { TEXT("FCheckBox"), TEXT("ECheckBox"), TEXT("DCheckBox"), TEXT("CCheckBox"), TEXT("BCheckBox"), TEXT("ACheckBox") };
			for (int32 Index = 0; Index < 6; ++Index)
			{
				if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(Names[Index])))
				{
					const ETotorisMissionTier Tier = static_cast<ETotorisMissionTier>(Index);
					CheckBox->SetIsChecked(IsMissionTierEnabled(Tier));
					// Mode Setup is preserved while the Mission List is open. Re-showing
					// it must not add the same dynamic delegate a second time.
					switch (Tier)
					{
					case ETotorisMissionTier::F:
						CheckBox->OnCheckStateChanged.RemoveDynamic(this, &ATotorisPlayerController::HandleMissionFCheckChanged);
						CheckBox->OnCheckStateChanged.AddDynamic(this, &ATotorisPlayerController::HandleMissionFCheckChanged); break;
					case ETotorisMissionTier::E:
						CheckBox->OnCheckStateChanged.RemoveDynamic(this, &ATotorisPlayerController::HandleMissionECheckChanged);
						CheckBox->OnCheckStateChanged.AddDynamic(this, &ATotorisPlayerController::HandleMissionECheckChanged); break;
					case ETotorisMissionTier::D:
						CheckBox->OnCheckStateChanged.RemoveDynamic(this, &ATotorisPlayerController::HandleMissionDCheckChanged);
						CheckBox->OnCheckStateChanged.AddDynamic(this, &ATotorisPlayerController::HandleMissionDCheckChanged); break;
					case ETotorisMissionTier::C:
						CheckBox->OnCheckStateChanged.RemoveDynamic(this, &ATotorisPlayerController::HandleMissionCCheckChanged);
						CheckBox->OnCheckStateChanged.AddDynamic(this, &ATotorisPlayerController::HandleMissionCCheckChanged); break;
					case ETotorisMissionTier::B:
						CheckBox->OnCheckStateChanged.RemoveDynamic(this, &ATotorisPlayerController::HandleMissionBCheckChanged);
						CheckBox->OnCheckStateChanged.AddDynamic(this, &ATotorisPlayerController::HandleMissionBCheckChanged); break;
					case ETotorisMissionTier::A:
						CheckBox->OnCheckStateChanged.RemoveDynamic(this, &ATotorisPlayerController::HandleMissionACheckChanged);
						CheckBox->OnCheckStateChanged.AddDynamic(this, &ATotorisPlayerController::HandleMissionACheckChanged); break;
					}
				}
			}
		}
		EnterMenuInputMode();
	}
}

ETotorisGameSetupMode ATotorisPlayerController::GetSelectedGameSetupMode() const
{
	return SelectedGameSetupMode;
}

void ATotorisPlayerController::SetCommonGameSetupSettings(
	const FTotorisCommonGameSetupSettings& Settings)
{
	CommonGameSetupSettings = Settings;
	SetQuickStartEnabled(Settings.bQuickStart);
	CommonGameSetupSettings.GarbageDifficulty = FMath::Clamp(
		CommonGameSetupSettings.GarbageDifficulty,
		1,
		10);
}

FTotorisCommonGameSetupSettings ATotorisPlayerController::GetCommonGameSetupSettings() const
{
	return CommonGameSetupSettings;
}

void ATotorisPlayerController::SetGarbageAttackEnabled(bool bEnabled)
{
	CommonGameSetupSettings.bGarbageAttack = bEnabled;
	bGarbageAttackHasUserOverride = true;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Totoris virtual garbage option changed: Enabled=%s (mode=%d)"),
		bEnabled ? TEXT("true") : TEXT("false"),
		static_cast<int32>(ClassicSettings.Mode));
}

bool ATotorisPlayerController::GetGarbageAttackEnabled() const
{
	return CommonGameSetupSettings.bGarbageAttack;
}

int32 ATotorisPlayerController::SetGarbageDifficulty(int32 Difficulty)
{
	CommonGameSetupSettings.GarbageDifficulty = FMath::Clamp(Difficulty, 1, 10);
	return CommonGameSetupSettings.GarbageDifficulty;
}

int32 ATotorisPlayerController::GetGarbageDifficulty() const
{
	return CommonGameSetupSettings.GarbageDifficulty;
}

void ATotorisPlayerController::SetGarbageDifficultyIncreaseEnabled(bool bEnabled)
{
	CommonGameSetupSettings.bGarbageDifficultyIncrease = bEnabled;
	bGarbageDifficultyIncreaseHasUserOverride = true;
}

bool ATotorisPlayerController::GetGarbageDifficultyIncreaseEnabled() const
{
	return CommonGameSetupSettings.bGarbageDifficultyIncrease;
}

void ATotorisPlayerController::SetCheeseGarbageEnabled(bool bEnabled)
{
	CommonGameSetupSettings.bCheeseGarbage = bEnabled;
}

bool ATotorisPlayerController::GetCheeseGarbageEnabled() const
{
	return CommonGameSetupSettings.bCheeseGarbage;
}

void ATotorisPlayerController::SetStartGravityEnabled(bool bEnabled)
{
	CommonGameSetupSettings.bStartGravity = bEnabled;
}

bool ATotorisPlayerController::GetStartGravityEnabled() const
{
	return CommonGameSetupSettings.bStartGravity;
}

void ATotorisPlayerController::SetGravityIncreaseEnabled(bool bEnabled)
{
	CommonGameSetupSettings.bGravityIncrease = bEnabled;
}

bool ATotorisPlayerController::GetGravityIncreaseEnabled() const
{
	return CommonGameSetupSettings.bGravityIncrease;
}

void ATotorisPlayerController::SetQuickStartEnabled(bool bEnabled)
{
	CommonGameSetupSettings.bQuickStart = bEnabled;
	TotorisPreferences::WriteBool(TEXT("QuickStart"), bEnabled);
}

bool ATotorisPlayerController::GetQuickStartEnabled() const
{
	return CommonGameSetupSettings.bQuickStart;
}

void ATotorisPlayerController::ResetCommonGameSetupSettings()
{
	const bool bQuickStart = CommonGameSetupSettings.bQuickStart;
	CommonGameSetupSettings = FTotorisCommonGameSetupSettings{};
	// Quick Start is a client preference, not a mode preset.
	CommonGameSetupSettings.bQuickStart = bQuickStart;
	bClassicModeHasAppliedDefaults = false;
	bGarbageAttackHasUserOverride = false;
	bGarbageDifficultyIncreaseHasUserOverride = false;
}

void ATotorisPlayerController::SetMissionTierEnabled(ETotorisMissionTier Tier, bool bEnabled)
{
	bool* Value = nullptr;
	switch (Tier)
	{
	case ETotorisMissionTier::F: Value = &CommonGameSetupSettings.bMissionF; break;
	case ETotorisMissionTier::E: Value = &CommonGameSetupSettings.bMissionE; break;
	case ETotorisMissionTier::D: Value = &CommonGameSetupSettings.bMissionD; break;
	case ETotorisMissionTier::C: Value = &CommonGameSetupSettings.bMissionC; break;
	case ETotorisMissionTier::B: Value = &CommonGameSetupSettings.bMissionB; break;
	case ETotorisMissionTier::A: Value = &CommonGameSetupSettings.bMissionA; break;
	}
	if (Value) *Value = bEnabled;
}

bool ATotorisPlayerController::IsMissionTierEnabled(ETotorisMissionTier Tier) const
{
	switch (Tier)
	{
	case ETotorisMissionTier::F: return CommonGameSetupSettings.bMissionF;
	case ETotorisMissionTier::E: return CommonGameSetupSettings.bMissionE;
	case ETotorisMissionTier::D: return CommonGameSetupSettings.bMissionD;
	case ETotorisMissionTier::C: return CommonGameSetupSettings.bMissionC;
	case ETotorisMissionTier::B: return CommonGameSetupSettings.bMissionB;
	case ETotorisMissionTier::A: return CommonGameSetupSettings.bMissionA;
	default: return false;
	}
}

bool ATotorisPlayerController::HasAnyMissionTierEnabled() const
{
	return CommonGameSetupSettings.bMissionF || CommonGameSetupSettings.bMissionE ||
		CommonGameSetupSettings.bMissionD || CommonGameSetupSettings.bMissionC ||
		CommonGameSetupSettings.bMissionB || CommonGameSetupSettings.bMissionA;
}

void ATotorisPlayerController::HandleMissionFCheckChanged(bool bChecked) { SetMissionTierEnabled(ETotorisMissionTier::F, bChecked); }
void ATotorisPlayerController::HandleMissionECheckChanged(bool bChecked) { SetMissionTierEnabled(ETotorisMissionTier::E, bChecked); }
void ATotorisPlayerController::HandleMissionDCheckChanged(bool bChecked) { SetMissionTierEnabled(ETotorisMissionTier::D, bChecked); }
void ATotorisPlayerController::HandleMissionCCheckChanged(bool bChecked) { SetMissionTierEnabled(ETotorisMissionTier::C, bChecked); }
void ATotorisPlayerController::HandleMissionBCheckChanged(bool bChecked) { SetMissionTierEnabled(ETotorisMissionTier::B, bChecked); }
void ATotorisPlayerController::HandleMissionACheckChanged(bool bChecked) { SetMissionTierEnabled(ETotorisMissionTier::A, bChecked); }

ETotorisMissionTier ATotorisPlayerController::SelectMissionTier(ETotorisMissionTier DesiredTier)
{
	const int32 Desired = static_cast<int32>(DesiredTier);
	if (IsMissionTierEnabled(DesiredTier)) return DesiredTier;
	int32 Lower = INDEX_NONE, Upper = INDEX_NONE;
	for (int32 Index = Desired - 1; Index >= 0; --Index) if (IsMissionTierEnabled(static_cast<ETotorisMissionTier>(Index))) { Lower = Index; break; }
	for (int32 Index = Desired + 1; Index < 6; ++Index) if (IsMissionTierEnabled(static_cast<ETotorisMissionTier>(Index))) { Upper = Index; break; }
	if (Lower == INDEX_NONE) return static_cast<ETotorisMissionTier>(Upper);
	if (Upper == INDEX_NONE) return static_cast<ETotorisMissionTier>(Lower);
	const int32 LowerDistance = Desired - Lower;
	const int32 UpperDistance = Upper - Desired;
	return MissionRandom.FRand() < static_cast<float>(UpperDistance) / (LowerDistance + UpperDistance)
		? static_cast<ETotorisMissionTier>(Lower) : static_cast<ETotorisMissionTier>(Upper);
}

void ATotorisPlayerController::InitializeMissionRun()
{
	ActiveMissions.Reset();
	LastDisplayedMissionProgressId = NAME_None;
	LastDisplayedMissionProgressValue = 0;
	bMissionComboHoldUsed = false;
	bMissionNoComboSinglesChainContainsSingle = false;
	MissionAlternatePreviousClearState = INDEX_NONE;
	MissionTopStackSeconds = 0.f;
	bMissionHasSameDoubleMino = false;
	MissionSameDoubleMino = ETotorisMino::I;
	MissionLineClearChecklistMask = 0;
	MissionColumnChecklistMask = 0;
	MissionMinoChecklistMask = 0;
	bMissionB2BChainStarted = false;
	MissionB2BCount = 0;
	MissionB2BStartCount = 0;
	bMissionB2BWasBroken = false;
	MissionGarbageFreeSeconds = 0.f;
	MissionNoGarbageCancelSeconds = 0.f;
	if (CompletedMissionCount == 0)
	{
		MissionDifficulty = 1;
		MissionRandom.Initialize(FMath::Rand());
	}

	if (SelectedGameSetupMode == ETotorisGameSetupMode::SChallenge)
	{
		if (CompletedMissionCount == 0)
		{
			SChallengeQueue.Reset();
			const auto& Pool = TotorisMissions::ForTier(ETotorisMissionTier::S);
			for (int32 Index = 0; Index < Pool.Num(); ++Index)
				if (!MenuManager || MenuManager->IsSCMissionEnabled(Index)) SChallengeQueue.Add(Pool[Index]);
			for (int32 Index = SChallengeQueue.Num() - 1; Index > 0; --Index)
				SChallengeQueue.Swap(Index, MissionRandom.RandRange(0, Index));
		}
		if (SChallengeQueue.IsValidIndex(CompletedMissionCount))
		{
			FTotorisMissionRuntimeState State;
			State.MissionId = SChallengeQueue[CompletedMissionCount].Id;
			State.Tier = ETotorisMissionTier::S;
			State.Kind = SChallengeQueue[CompletedMissionCount].Kind;
			State.TargetValue = SChallengeQueue[CompletedMissionCount].TargetValue;
			ActiveMissions.Add(State);
		}
		BeginCurrentMissionTracking();
		LogMissionSet();
		RefreshMissionPanel();
		return;
	}

	// TETR.IO's 1..17 difficulty schedule, expressed as three desired tiers.
	// The mission difficulty is deliberately independent from Garbage/Floor.
	static const ETotorisMissionTier TierSchedule[17][3] =
	{
		{ ETotorisMissionTier::F, ETotorisMissionTier::F, ETotorisMissionTier::F },
		{ ETotorisMissionTier::F, ETotorisMissionTier::F, ETotorisMissionTier::F },
		{ ETotorisMissionTier::F, ETotorisMissionTier::F, ETotorisMissionTier::F },
		{ ETotorisMissionTier::F, ETotorisMissionTier::F, ETotorisMissionTier::E },
		{ ETotorisMissionTier::F, ETotorisMissionTier::E, ETotorisMissionTier::E },
		{ ETotorisMissionTier::E, ETotorisMissionTier::E, ETotorisMissionTier::E },
		{ ETotorisMissionTier::E, ETotorisMissionTier::E, ETotorisMissionTier::D },
		{ ETotorisMissionTier::E, ETotorisMissionTier::D, ETotorisMissionTier::D },
		{ ETotorisMissionTier::D, ETotorisMissionTier::D, ETotorisMissionTier::D },
		{ ETotorisMissionTier::D, ETotorisMissionTier::D, ETotorisMissionTier::C },
		{ ETotorisMissionTier::D, ETotorisMissionTier::C, ETotorisMissionTier::C },
		{ ETotorisMissionTier::C, ETotorisMissionTier::C, ETotorisMissionTier::C },
		{ ETotorisMissionTier::C, ETotorisMissionTier::C, ETotorisMissionTier::B },
		{ ETotorisMissionTier::C, ETotorisMissionTier::B, ETotorisMissionTier::B },
		{ ETotorisMissionTier::B, ETotorisMissionTier::B, ETotorisMissionTier::B },
		{ ETotorisMissionTier::B, ETotorisMissionTier::B, ETotorisMissionTier::A },
		{ ETotorisMissionTier::A, ETotorisMissionTier::B, ETotorisMissionTier::A }
	};

	const int32 ScheduleIndex = FMath::Clamp(MissionDifficulty, 1, 17) - 1;
	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		const ETotorisMissionTier DesiredTier = TierSchedule[ScheduleIndex][Slot];
		const ETotorisMissionTier Tier = SelectMissionTier(DesiredTier);
		const TArray<FTotorisMissionDefinition>& Pool = TotorisMissions::ForTier(Tier);
		if (Pool.Num() == 0) continue;

		// Individual WBP_MissionList selections persist in the menu manager.
		// Missing entries intentionally mean the UI's default (enabled) state.
		TArray<int32> EnabledMissionIndices;
		for (int32 MissionIndex = 0; MissionIndex < Pool.Num(); ++MissionIndex)
		{
			if (TotorisMissions::IsAllowedByGarbageSetting(Pool[MissionIndex], GetGarbageAttackEnabled()) &&
				(!MenuManager || MenuManager->IsMissionListMissionEnabled(Pool[MissionIndex].Id)))
			{
				EnabledMissionIndices.Add(MissionIndex);
			}
		}
		if (EnabledMissionIndices.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("Mission tier %s has no enabled entries."),
				*UEnum::GetValueAsString(Tier));
			continue;
		}

		int32 Choice = MissionRandom.RandRange(0, EnabledMissionIndices.Num() - 1);
		for (int32 Try = 0; Try < EnabledMissionIndices.Num() &&
			ActiveMissions.ContainsByPredicate([&](const FTotorisMissionRuntimeState& S)
			{
				return S.MissionId == Pool[EnabledMissionIndices[Choice]].Id;
			}); ++Try)
		{
			Choice = (Choice + 1) % EnabledMissionIndices.Num();
		}
		const FTotorisMissionDefinition& SelectedMission = Pool[EnabledMissionIndices[Choice]];
		FTotorisMissionRuntimeState State;
		State.MissionId = SelectedMission.Id;
		State.Tier = Tier;
		State.Kind = SelectedMission.Kind;
		State.TargetValue = SelectedMission.TargetValue;
		ActiveMissions.Add(State);
	}
	BeginCurrentMissionTracking();
	LogMissionSet();
	RefreshMissionPanel();
}

void ATotorisPlayerController::LogMissionSet() const
{
	for (const FTotorisMissionRuntimeState& Mission : ActiveMissions)
	{
		UE_LOG(LogTemp, Display, TEXT("Mission: %s: %s"), *UEnum::GetValueAsString(Mission.Tier), *Mission.MissionId.ToString());
	}
}

void ATotorisPlayerController::RefreshMissionPanel()
{
	UTotorisBlockGeneratorComponent* BlockGenerator = FindBlockGenerator();
	if (!IsValid(BlockGenerator)) return;

	const int32 CurrentSlot = ActiveMissions.IndexOfByPredicate(
		[](const FTotorisMissionRuntimeState& Mission) { return !Mission.bCompleted; });
	if (ActiveMissions.IsValidIndex(CurrentSlot))
	{
		const FTotorisMissionRuntimeState& CurrentMission = ActiveMissions[CurrentSlot];
		// Do not play for a fresh mission, nor for an attempted reset that was
		// already at zero. Any lower value for the same active mission is a
		// failed sequence/progress reset and receives exactly one fail cue.
		if (LastDisplayedMissionProgressId == CurrentMission.MissionId &&
			CurrentMission.CurrentValue < LastDisplayedMissionProgressValue && MissionFailSound)
		{
			UGameplayStatics::PlaySound2D(this, MissionFailSound,
				TotorisAudioMix::MissionFail * TotorisAudioMix::MasterVolume);
		}
		LastDisplayedMissionProgressId = CurrentMission.MissionId;
		LastDisplayedMissionProgressValue = CurrentMission.CurrentValue;
	}
	else
	{
		LastDisplayedMissionProgressId = NAME_None;
		LastDisplayedMissionProgressValue = 0;
	}

	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		if (!ActiveMissions.IsValidIndex(Slot) || ActiveMissions[Slot].bCompleted)
		{
			BlockGenerator->ClearMissionPanelSlot(Slot);
			continue;
		}

		const FTotorisMissionRuntimeState& State = ActiveMissions[Slot];
		const FTotorisMissionDefinition* Definition = TotorisMissions::ForTier(State.Tier).FindByPredicate(
			[&State](const FTotorisMissionDefinition& Candidate)
			{
				return Candidate.Id == State.MissionId;
			});
		if (Definition)
		{
			BlockGenerator->SetMissionPanelSlot(
				Slot,
				Definition->DisplayText,
				Definition->Description,
				MissionTierBaseColor(State.Tier),
				Slot == CurrentSlot);
			BlockGenerator->SetMissionPanelCounterSlot(
				Slot, State.CurrentValue, State.TargetValue,
				Slot == CurrentSlot && State.Kind == ETotorisMissionKind::Counter);
		}
		else
		{
			BlockGenerator->ClearMissionPanelSlot(Slot);
		}
	}

	const bool bShowLineClearChecklist = ActiveMissions.IsValidIndex(CurrentSlot) &&
		ActiveMissions[CurrentSlot].MissionId == TEXT("D_Clear_Single_Double_Triple");
	BlockGenerator->MissionPatternColors.Reset();
	BlockGenerator->MissionPatternFadingCells.Reset();
	if (ActiveMissions.IsValidIndex(CurrentSlot) && ActiveMissions[CurrentSlot].MissionId == TEXT("S_Quad_Only_I"))
	{
		BlockGenerator->MissionPatternColors.Init(FLinearColor::Transparent, 100);
		for (int32 Index = 60; Index < 100; ++Index)
		{
			BlockGenerator->MissionPatternColors[Index] = TotorisGeneration::Color(ETotorisMino::I);
			if (Index % 10 == 9) BlockGenerator->MissionPatternFadingCells.Add(Index);
		}
	}
	BlockGenerator->SetMissionPanelLineClearChecklist(
		bShowLineClearChecklist,
		(MissionLineClearChecklistMask & 0x01) == 0,
		(MissionLineClearChecklistMask & 0x02) == 0,
		(MissionLineClearChecklistMask & 0x04) == 0);
	if (ActiveMissions.IsValidIndex(CurrentSlot) &&
		(ActiveMissions[CurrentSlot].MissionId == TEXT("S_Build_Heart") || ActiveMissions[CurrentSlot].MissionId == TEXT("S_Build_Diamond") ||
		 ActiveMissions[CurrentSlot].MissionId == TEXT("S_Half_Secret_Grade")))
	{
		const bool bDiamond = ActiveMissions[CurrentSlot].MissionId == TEXT("S_Build_Diamond");
		const bool bHalfSecretGrade = ActiveMissions[CurrentSlot].MissionId == TEXT("S_Half_Secret_Grade");
		const int32 PatternHeight = bHalfSecretGrade ? 10 : (bDiamond ? 8 : 7);
		const TCHAR* const* Rows = bHalfSecretGrade ? TotorisMissionPatterns::HalfSecretGradeRows :
			(bDiamond ? TotorisMissionPatterns::DiamondRows : TotorisMissionPatterns::HeartRows);
		BlockGenerator->MissionPatternColors.Init(FLinearColor::Transparent, 100);
		for (int32 Row = 0; Row < PatternHeight; ++Row)
			for (int32 Column = 0; Column < 10; ++Column)
				if (Rows[Row][Column] == TEXT('#'))
					BlockGenerator->MissionPatternColors[(Row + 10 - PatternHeight) * 10 + Column] = FLinearColor(.58f, .60f, .63f, 1.f);
	}

	uint16 RequiredColumnMask = 0;
	if (ActiveMissions.IsValidIndex(CurrentSlot))
	{
		const FName& CurrentId = ActiveMissions[CurrentSlot].MissionId;
		if (CurrentId == TEXT("F_Place_Blocks_All_10_Columns") ||
			CurrentId == TEXT("C_Clear_Quad_2_Columns"))
		{
			RequiredColumnMask = 0x03FF;
		}
		else if (CurrentId == TEXT("B_Clear_Quads_Both_Edge_Columns"))
		{
			RequiredColumnMask = 0x0201;
		}
	}
	BlockGenerator->SetMissionPanelColumnChecklist(
		RequiredColumnMask != 0, RequiredColumnMask, MissionColumnChecklistMask);

	uint8 RequiredMinoMask = 0;
	bool bMinoChecklistIsSpin = false;
	if (ActiveMissions.IsValidIndex(CurrentSlot))
	{
		const FName& CurrentId = ActiveMissions[CurrentSlot].MissionId;
		if (CurrentId == TEXT("F_Place_All_7_Types") ||
			CurrentId == TEXT("C_Clear_Lines_All_7_Types") ||
			CurrentId == TEXT("S_All_Types_Consecutive"))
		{
			RequiredMinoMask = 0x7F;
		}
		else if (CurrentId == TEXT("B_Spin_Clears_3_Pieces") || CurrentId == TEXT("S_All_Spins_B2B"))
		{
			// O does not spin, so it must never be rendered for a spin checklist.
			RequiredMinoMask = 0x6F;
			bMinoChecklistIsSpin = true;
		}
	}
	BlockGenerator->SetMissionPanelMinoChecklist(
		RequiredMinoMask != 0, RequiredMinoMask, MissionMinoChecklistMask,
		bMinoChecklistIsSpin);
}

void ATotorisPlayerController::NotifySChallengeMissionCompleted(FName MissionId)
{
	if (SelectedGameSetupMode == ETotorisGameSetupMode::SChallenge && ActiveMissions.Num() == 1 &&
		!ActiveMissions[0].bCompleted && ActiveMissions[0].MissionId == MissionId)
	{
		CompleteCurrentMission();
	}
}

void ATotorisPlayerController::CompleteCurrentMission()
{
	if (SelectedGameSetupMode != ETotorisGameSetupMode::Mission && SelectedGameSetupMode != ETotorisGameSetupMode::SChallenge) return;
	const auto* Game = FindBlockGenerator();
	if (!Game || !Game->IsGameplayActive() || Game->HasFinishedRunSummary() || Game->IsGameplayPaused()) return;
	if (ActiveMissions.Num() == 0) return;
	const int32 Slot = ActiveMissions.IndexOfByPredicate([](const FTotorisMissionRuntimeState& M) { return !M.bCompleted; });
	if (Slot == INDEX_NONE) return;
	ActiveMissions[Slot].CurrentValue = ActiveMissions[Slot].TargetValue;
	ActiveMissions[Slot].bCompleted = true;
	// Deliberately dispatch both layers independently in the same completion
	// frame, allowing each imported SoundWave to retain its own mix level.
	if (MissionClearSound1)
	{
		UGameplayStatics::PlaySound2D(this, MissionClearSound1,
			TotorisAudioMix::MissionClear1 * TotorisAudioMix::MasterVolume);
	}
	if (MissionClearSound2)
	{
		UGameplayStatics::PlaySound2D(this, MissionClearSound2,
			TotorisAudioMix::MissionClear2 * TotorisAudioMix::MasterVolume);
	}
	++CompletedMissionCount;
	int32 Remaining = 0;
	for (const FTotorisMissionRuntimeState& Mission : ActiveMissions)
	{
		if (!Mission.bCompleted)
		{
			++Remaining;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("Mission completed: %s; %d mission(s) remaining"), *ActiveMissions[Slot].MissionId.ToString(), Remaining);
	if (Remaining == 0)
	{
		if (TotorisAudioMix::bMissionEventsEnabled)
		{
			TriggerRandomMissionSetCompletionEvent();
		}
		UE_LOG(LogTemp, Display, TEXT("Mission set completed. Generating next mission set."));
		if (SelectedGameSetupMode == ETotorisGameSetupMode::SChallenge && CompletedMissionCount >= SChallengeQueue.Num())
		{
			RefreshMissionPanel();
			if (auto* Generator = FindBlockGenerator()) Generator->CompleteChallengeRun();
			return;
		}
		++MissionDifficulty;
		InitializeMissionRun();
	}
	else
	{
		BeginCurrentMissionTracking();
		RefreshMissionPanel();
	}
}

void ATotorisPlayerController::TriggerRandomMissionSetCompletionEvent()
{
	// Add future events here.  We draw from idle candidates first so a set
	// completion never restarts its own visual event while another one is free.
	// Only when every event is still running does the fallback deliberately
	// select one active event; each start routine resets that event immediately.
	enum class EMissionCompletionEvent : uint8 { Smilemo, Sannabi, Usoda, Dragon, Anjanath, Mahjong, ReAnimal, Minecraft, Journey };
	TArray<EMissionCompletionEvent> AllEvents;
	TArray<EMissionCompletionEvent> IdleEvents;
	AllEvents.Reserve(9);
	IdleEvents.Reserve(9);
	AllEvents.Add(EMissionCompletionEvent::Journey);
	if (!IsValid(JourneyEventComponent) || !JourneyEventComponent->IsEventPlaying())
	{
		IdleEvents.Add(EMissionCompletionEvent::Journey);
	}

	if (IsValid(SmilemoSitoJumpComponent))
	{
		AllEvents.Add(EMissionCompletionEvent::Smilemo);
		if (!SmilemoSitoJumpComponent->IsJumpRoutePlaying())
		{
			IdleEvents.Add(EMissionCompletionEvent::Smilemo);
		}
	}
	AllEvents.Add(EMissionCompletionEvent::Usoda);
	if (!IsUsodaEventPlaying())
	{
		IdleEvents.Add(EMissionCompletionEvent::Usoda);
	}
	if (IsValid(DummyDragonFlightComponent))
	{
		AllEvents.Add(EMissionCompletionEvent::Dragon);
		if (!DummyDragonFlightComponent->IsFlightPlaying())
		{
			IdleEvents.Add(EMissionCompletionEvent::Dragon);
		}
	}
	if (IsValid(SannabiEventWidget))
	{
		AllEvents.Add(EMissionCompletionEvent::Sannabi);
		if (!SannabiEventWidget->IsEventPlaying())
		{
			IdleEvents.Add(EMissionCompletionEvent::Sannabi);
		}
	}
	if (IsValid(AnjanathDropEventComponent))
	{
		AllEvents.Add(EMissionCompletionEvent::Anjanath);
		if (!AnjanathDropEventComponent->IsEventPlaying())
		{
			IdleEvents.Add(EMissionCompletionEvent::Anjanath);
		}
	}
	if (IsValid(MahjongEventWidget))
	{
		AllEvents.Add(EMissionCompletionEvent::Mahjong);
		if (!MahjongEventWidget->IsEventPlaying())
		{
			IdleEvents.Add(EMissionCompletionEvent::Mahjong);
		}
	}
	if (IsValid(ReAnimalEventWidget))
	{
		AllEvents.Add(EMissionCompletionEvent::ReAnimal);
		if (!ReAnimalEventWidget->IsEventPlaying())
		{
			IdleEvents.Add(EMissionCompletionEvent::ReAnimal);
		}
	}
	if (IsValid(MinecraftBlockEventComponent))
	{
		AllEvents.Add(EMissionCompletionEvent::Minecraft);
		if (!MinecraftBlockEventComponent->IsEventPlaying())
		{
			IdleEvents.Add(EMissionCompletionEvent::Minecraft);
		}
	}

	const TArray<EMissionCompletionEvent>& CandidateEvents =
		IdleEvents.Num() > 0 ? IdleEvents : AllEvents;
	if (CandidateEvents.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Mission completion event skipped: no event is available."));
		return;
	}

	const EMissionCompletionEvent SelectedEvent = CandidateEvents[
		MissionRandom.RandRange(0, CandidateEvents.Num() - 1)];
	switch (SelectedEvent)
	{
	case EMissionCompletionEvent::Smilemo:
		// PlayJumpRoute resets both actors to their recorded starts, so selecting
		// it from an all-active fallback ends and restarts the route at once.
		PlaySmilemoSitoJumpPath();
		break;
	case EMissionCompletionEvent::Sannabi:
		// StartEvent collapses all prior phase visuals before beginning Totori's
		// entrance, making an all-active fallback an immediate clean restart.
		SannabiEventWidget->StartEvent();
		break;
	case EMissionCompletionEvent::Usoda:
		// Replaying it cleanly replaces the temporary volume and restarts both
		// of its audio cues, matching the restart policy of the other events.
		PlayUsodaEvent();
		break;
	case EMissionCompletionEvent::Dragon:
		PlayDummyDragonFlight();
		break;
	case EMissionCompletionEvent::Anjanath:
		PlayAnjanathDropEvent();
		break;
	case EMissionCompletionEvent::Mahjong:
		PlayMahjongEvent();
		break;
	case EMissionCompletionEvent::ReAnimal:
		ReAnimalEventWidget->StartEvent();
		break;
	case EMissionCompletionEvent::Minecraft:
		PlayMinecraftBlockEvent();
		break;
	case EMissionCompletionEvent::Journey:
		PlayJourneyEvent();
		break;
	default:
		break;
	}
}

bool ATotorisPlayerController::IsUsodaEventPlaying() const
{
	return UsodaFadeElapsedSeconds >= 0.f || HigurashiElapsedSeconds >= 0.f;
}

void ATotorisPlayerController::BeginCurrentMissionTracking()
{
	if (auto* Generator = FindBlockGenerator())
	{
		for (auto& Mission : ActiveMissions)
		{
			if (Mission.bCompleted) continue;
			if (Mission.MissionId == TEXT("S_Unique_Spin_Centers")) Generator->CurrentPieceSpinCenters.Reset();
			if (Mission.MissionId == TEXT("S_B2B_100")) Mission.CurrentValue = Generator->GetBackToBackCountForHUD();
			if (Mission.MissionId == TEXT("S_Combo_10_Perfect_Clear")) Mission.CurrentValue = FMath::Max(0, Generator->GetComboCountForHUD());
			break;
		}
	}
	bMissionComboHoldUsed = false;
	bMissionNoComboSinglesChainContainsSingle = false;
	MissionAlternatePreviousClearState = INDEX_NONE;
	MissionTopStackSeconds = 0.f;
	bMissionHasSameDoubleMino = false;
	MissionSameDoubleMino = ETotorisMino::I;
	MissionLineClearChecklistMask = 0;
	MissionColumnChecklistMask = 0;
	MissionMinoChecklistMask = 0;
	bMissionB2BChainStarted = false;
	MissionB2BCount = 0;
	bMissionB2BWasBroken = false;
	MissionB2BStartCount = 0;
	MissionGarbageFreeSeconds = 0.f;
	MissionNoGarbageCancelSeconds = 0.f;

	const int32 Slot = ActiveMissions.IndexOfByPredicate(
		[](const FTotorisMissionRuntimeState& Mission) { return !Mission.bCompleted; });
	if (ActiveMissions.IsValidIndex(Slot) &&
		(ActiveMissions[Slot].MissionId == TEXT("C_Reach_B2B_4") ||
			ActiveMissions[Slot].MissionId == TEXT("A_Reach_B2B_10")))
	{
		if (UTotorisBlockGeneratorComponent* BlockGenerator = FindBlockGenerator())
		{
			MissionB2BStartCount = BlockGenerator->GetBackToBackCountForHUD();
		}
	}
}

void ATotorisPlayerController::AddCurrentMissionProgress(int32 Amount)
{
	if (Amount <= 0) return;
	const int32 Slot = ActiveMissions.IndexOfByPredicate(
		[](const FTotorisMissionRuntimeState& Mission) { return !Mission.bCompleted; });
	if (!ActiveMissions.IsValidIndex(Slot)) return;

	FTotorisMissionRuntimeState& Mission = ActiveMissions[Slot];
	if (Mission.Kind != ETotorisMissionKind::Counter || Mission.TargetValue <= 0) return;
	Mission.CurrentValue = FMath::Clamp(Mission.CurrentValue + Amount, 0, Mission.TargetValue);
	if (Mission.CurrentValue >= Mission.TargetValue)
	{
		CompleteCurrentMission();
	}
	else
	{
		RefreshMissionPanel();
	}
}

void ATotorisPlayerController::HandleMissionHoldUsed()
{
	if (auto* Generator = FindBlockGenerator()) Generator->CurrentPieceSpinCenters.Reset();
	const int32 Slot = ActiveMissions.IndexOfByPredicate(
		[](const FTotorisMissionRuntimeState& Mission) { return !Mission.bCompleted; });
	if (ActiveMissions.IsValidIndex(Slot) && ActiveMissions[Slot].MissionId == TEXT("F_Use_Hold_8_Times"))
	{
		AddCurrentMissionProgress(1);
	}
	else if (ActiveMissions.IsValidIndex(Slot) && ActiveMissions[Slot].MissionId == TEXT("S_Unique_Spin_Centers"))
	{
		ActiveMissions[Slot].CurrentValue = 0;
		RefreshMissionPanel();
	}
	else if (ActiveMissions.IsValidIndex(Slot) && ActiveMissions[Slot].MissionId == TEXT("D_Perform_3_Combo_No_Hold"))
	{
		bMissionComboHoldUsed = true;
		if (ActiveMissions[Slot].CurrentValue != 0)
		{
			ActiveMissions[Slot].CurrentValue = 0;
			RefreshMissionPanel();
		}
	}
}

void ATotorisPlayerController::HandleMissionSuccessfulRotation()
{
	const int32 Slot = ActiveMissions.IndexOfByPredicate(
		[](const FTotorisMissionRuntimeState& Mission) { return !Mission.bCompleted; });
	if (ActiveMissions.IsValidIndex(Slot) && ActiveMissions[Slot].MissionId == TEXT("S_Unique_Spin_Centers"))
	{
		if (auto* Generator = FindBlockGenerator())
		{
			ActiveMissions[Slot].CurrentValue = Generator->GetUniqueSpinCenterCount();
			RefreshMissionPanel();
		}
		return;
	}
	if (ActiveMissions.IsValidIndex(Slot) &&
		(ActiveMissions[Slot].MissionId == TEXT("F_Rotate_20_Times") ||
			ActiveMissions[Slot].MissionId == TEXT("D_Rotate_80_Times") ||
			ActiveMissions[Slot].MissionId == TEXT("B_Rotate_300_Times")))
	{
		AddCurrentMissionProgress(1);
	}
}

void ATotorisPlayerController::HandleMissionGameRestarted()
{
	StopMissionPresentationEvents();
	if (SelectedGameSetupMode != ETotorisGameSetupMode::Mission && SelectedGameSetupMode != ETotorisGameSetupMode::SChallenge)
	{
		return;
	}

	// The in-game restart key resets the generator in place instead of passing
	// through StartClassicGame, so reset controller-owned mission state here.
	CompletedMissionCount = 0;
	MissionDifficulty = 1;
	InitializeMissionRun();
	UE_LOG(LogTemp, Display, TEXT("Mission run restarted: generated a fresh mission set."));
}

void ATotorisPlayerController::HandleMissionTopStackTick(bool bTopOccupied, bool bNoGarbageOnBoard, float DeltaSeconds)
{
	const int32 Slot = ActiveMissions.IndexOfByPredicate(
		[](const FTotorisMissionRuntimeState& Mission) { return !Mission.bCompleted; });
	if (!ActiveMissions.IsValidIndex(Slot)) return;

	FTotorisMissionRuntimeState& Mission = ActiveMissions[Slot];
	if (Mission.MissionId == TEXT("C_Stack_Top_3_Rows_3_Seconds"))
	{
		if (!bTopOccupied)
		{
			MissionTopStackSeconds = 0.f;
			if (Mission.CurrentValue != 0)
			{
				Mission.CurrentValue = 0;
				RefreshMissionPanel();
			}
			return;
		}

		MissionTopStackSeconds += DeltaSeconds;
		const int32 NewValue = FMath::Clamp(FMath::FloorToInt(MissionTopStackSeconds), 0, Mission.TargetValue);
		if (NewValue != Mission.CurrentValue)
		{
			Mission.CurrentValue = NewValue;
			if (Mission.CurrentValue >= Mission.TargetValue) CompleteCurrentMission(); else RefreshMissionPanel();
		}
		return;
	}

	if (Mission.MissionId == TEXT("B_No_Garbage_4_Seconds"))
	{
		if (!bNoGarbageOnBoard)
		{
			MissionGarbageFreeSeconds = 0.f;
			if (Mission.CurrentValue != 0)
			{
				Mission.CurrentValue = 0;
				RefreshMissionPanel();
			}
			return;
		}
		MissionGarbageFreeSeconds += DeltaSeconds;
		const int32 NewValue = FMath::Clamp(FMath::FloorToInt(MissionGarbageFreeSeconds), 0, Mission.TargetValue);
		if (NewValue != Mission.CurrentValue)
		{
			Mission.CurrentValue = NewValue;
			if (Mission.CurrentValue >= Mission.TargetValue) CompleteCurrentMission(); else RefreshMissionPanel();
		}
		return;
	}

	if (Mission.MissionId == TEXT("B_Dont_Cancel_Garbage_8_Seconds"))
	{
		MissionNoGarbageCancelSeconds += DeltaSeconds;
		const int32 NewValue = FMath::Clamp(FMath::FloorToInt(MissionNoGarbageCancelSeconds), 0, Mission.TargetValue);
		if (NewValue != Mission.CurrentValue)
		{
			Mission.CurrentValue = NewValue;
			if (Mission.CurrentValue >= Mission.TargetValue) CompleteCurrentMission(); else RefreshMissionPanel();
		}
	}
}

void ATotorisPlayerController::HandleMissionPieceLocked(const FTotorisMissionPlacementEvent& Event)
{
	const int32 Slot = ActiveMissions.IndexOfByPredicate(
		[](const FTotorisMissionRuntimeState& Mission) { return !Mission.bCompleted; });
	if (!ActiveMissions.IsValidIndex(Slot)) return;

	FTotorisMissionRuntimeState& Mission = ActiveMissions[Slot];
	const FName& Id = Mission.MissionId;
	const FString MissionIdString = Id.ToString();
	if (Id == TEXT("S_All_Spins_B2B") || Id == TEXT("S_All_Types_Consecutive"))
	{
		const bool bSpinChecklist = Id == TEXT("S_All_Spins_B2B");
		MissionMinoChecklistMask = bSpinChecklist
			? TotorisSChallengeRules::AdvanceAllSpins(MissionMinoChecklistMask, Event)
			: TotorisSChallengeRules::AdvanceAllTypes(MissionMinoChecklistMask, Event);
		Mission.CurrentValue = 0;
		for (int32 Index = 0; Index < 7; ++Index)
			if ((MissionMinoChecklistMask & (1u << Index)) != 0) ++Mission.CurrentValue;
		const uint8 RequiredMask = bSpinChecklist ? 0x6F : 0x7F;
		if (MissionMinoChecklistMask == RequiredMask) NotifySChallengeMissionCompleted(Id);
		else RefreshMissionPanel();
		return;
	}
	if (Id == TEXT("S_TSpins_Sequence_B2B") || Id == TEXT("S_B2B_100") ||
		Id == TEXT("S_Combo_10_Perfect_Clear") || Id == TEXT("S_Unique_Spin_Centers"))
	{
		bool bComplete = false;
		if (Id == TEXT("S_TSpins_Sequence_B2B"))
		{
			Mission.CurrentValue = TotorisSChallengeRules::AdvanceTSpinSequence(Mission.CurrentValue, Event);
			bComplete = Mission.CurrentValue >= 3;
		}
		else if (Id == TEXT("S_B2B_100"))
		{
			Mission.CurrentValue = Event.BackToBackCount;
			bComplete = Mission.CurrentValue >= 100;
		}
		else if (Id == TEXT("S_Combo_10_Perfect_Clear"))
		{
			Mission.CurrentValue = FMath::Max(0, Event.ComboCount);
			bComplete = TotorisSChallengeRules::CompletesComboPC(Event);
		}
		else
		{
			bComplete = TotorisSChallengeRules::CompletesUniqueSpins(Event);
			Mission.CurrentValue = bComplete ? Event.UniqueSpinCenterCount : 0;
		}
		if (bComplete) NotifySChallengeMissionCompleted(Id); else RefreshMissionPanel();
		return;
	}
	if (Id == TEXT("S_Half_Secret_Grade"))
	{
		if (Event.bBuiltHalfSecretGrade) NotifySChallengeMissionCompleted(Id);
		return;
	}
	if (Id == TEXT("S_Build_Diamond"))
	{
		if (Event.bBuiltDiamond) NotifySChallengeMissionCompleted(Id);
		return;
	}
	if (Id == TEXT("S_Build_Heart"))
	{
		if (Event.bBuiltHeart) NotifySChallengeMissionCompleted(Id);
		return;
	}
	if (Id == TEXT("S_Quad_Only_I"))
	{
		if (Event.ClearedLines == 4 && Event.bClearedQuadOnlyI) NotifySChallengeMissionCompleted(Id);
		return;
	}
	if (!MissionIdString.StartsWith(TEXT("F_")) && !MissionIdString.StartsWith(TEXT("E_")) && !MissionIdString.StartsWith(TEXT("D_")) && !MissionIdString.StartsWith(TEXT("C_")) && !MissionIdString.StartsWith(TEXT("B_")) && !MissionIdString.StartsWith(TEXT("A_"))) return;

	if (Id == TEXT("F_Perform_3_Combo"))
	{
		const int32 NewValue = FMath::Clamp(Event.ComboCount, 0, Mission.TargetValue);
		if (NewValue != Mission.CurrentValue)
		{
			Mission.CurrentValue = NewValue;
			if (Mission.CurrentValue >= Mission.TargetValue) CompleteCurrentMission(); else RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("F_Clear_2_Singles_In_Row"))
	{
		if (Event.ClearedLines == 1)
		{
			AddCurrentMissionProgress(1);
		}
		else if (Mission.CurrentValue != 0)
		{
			Mission.CurrentValue = 0;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("E_Perform_5_Combo"))
	{
		const int32 NewValue = FMath::Clamp(Event.ComboCount, 0, Mission.TargetValue);
		if (NewValue != Mission.CurrentValue)
		{
			Mission.CurrentValue = NewValue;
			if (Mission.CurrentValue >= Mission.TargetValue) CompleteCurrentMission(); else RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("A_Perform_7_Combo"))
	{
		const int32 NewValue = FMath::Clamp(Event.ComboCount, 0, Mission.TargetValue);
		if (NewValue != Mission.CurrentValue)
		{
			Mission.CurrentValue = NewValue;
			if (Mission.CurrentValue >= Mission.TargetValue) CompleteCurrentMission(); else RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("E_Place_2_O_Pieces_In_Row"))
	{
		if (Event.Mino == ETotorisMino::O)
		{
			AddCurrentMissionProgress(1);
		}
		else if (Mission.CurrentValue != 0)
		{
			Mission.CurrentValue = 0;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("E_Place_12_Counterclockwise_Only"))
	{
		// Only rotations made by a mino that actually locks matter. A rotated
		// mino discarded through HOLD never raises this placement event.
		if (Event.bUsedClockwiseOr180)
		{
			if (Mission.CurrentValue != 0)
			{
				Mission.CurrentValue = 0;
				RefreshMissionPanel();
			}
		}
		else
		{
			AddCurrentMissionProgress(1);
		}
		return;
	}

	if (Id == TEXT("E_Clear_6_Singles_No_Combo"))
	{
		// Only a combo chain that contains two Singles invalidates this mission.
		// Doubles/Triples/Quads are valid separators within a chain: e.g.
		// Single -> Double is fine, but its later combo Single resets progress.
		// After the combo breaks, Double -> combo Single is valid again.
		if (Event.ClearedLines == 0)
		{
			bMissionNoComboSinglesChainContainsSingle = false;
			return;
		}

		const bool bIsSingle = Event.ClearedLines == 1;
		if (Event.ComboCount == 0)
		{
			// A fresh line-clear chain can contribute exactly one counted Single.
			bMissionNoComboSinglesChainContainsSingle = bIsSingle;
			if (bIsSingle)
			{
				AddCurrentMissionProgress(1);
			}
		}
		else
		{
			if (bIsSingle)
			{
				const bool bComboChainAlreadyHadSingle = bMissionNoComboSinglesChainContainsSingle;
				bMissionNoComboSinglesChainContainsSingle = true;
				if (bComboChainAlreadyHadSingle)
				{
					if (Mission.CurrentValue != 0)
					{
						Mission.CurrentValue = 0;
						RefreshMissionPanel();
					}
				}
				else
				{
					// A combo begun by a Double/Triple/Quad is allowed; this
					// is its first Single, so it still advances the counter.
					AddCurrentMissionProgress(1);
				}
			}
		}
		return;
	}

	if (Id == TEXT("E_5_Columns_Same_Height"))
	{
		const int32 NewValue = FMath::Clamp(Event.LongestEqualColumnHeightRun, 0, Mission.TargetValue);
		if (NewValue != Mission.CurrentValue)
		{
			Mission.CurrentValue = NewValue;
			if (Mission.CurrentValue >= Mission.TargetValue) CompleteCurrentMission(); else RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("C_Place_25_No_Hold"))
	{
		if (!Event.bUsedHold)
		{
			AddCurrentMissionProgress(1);
		}
		else if (Mission.CurrentValue != 0)
		{
			Mission.CurrentValue = 0;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("C_Reach_B2B_4") || Id == TEXT("A_Reach_B2B_10"))
	{
		// Continuing an existing chain advances from the B2B value captured
		// when this mission became active (x10 -> x14 requires four levels).
		// If the chain breaks, the replacement chain must be built from x0.
		if (!bMissionB2BWasBroken && Event.BackToBackCount >= MissionB2BStartCount)
		{
			MissionB2BCount = Event.BackToBackCount - MissionB2BStartCount;
		}
		else
		{
			bMissionB2BWasBroken = true;
			if (Event.ClearedLines > 0 && !Event.bDifficultClear)
			{
				bMissionB2BChainStarted = false;
				MissionB2BCount = 0;
			}
			else if (Event.bDifficultClear)
			{
				if (bMissionB2BChainStarted)
				{
					++MissionB2BCount;
				}
				else
				{
					bMissionB2BChainStarted = true;
					MissionB2BCount = 0;
				}
			}
		}
		const int32 NewValue = FMath::Clamp(MissionB2BCount, 0, Mission.TargetValue);
		if (NewValue != Mission.CurrentValue)
		{
			Mission.CurrentValue = NewValue;
			if (Mission.CurrentValue >= Mission.TargetValue) CompleteCurrentMission(); else RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("A_Clear_2_SZ_Spin_Doubles") || Id == TEXT("A_Clear_2_LJ_Spin_Doubles"))
	{
		const bool bCorrectMino = Id == TEXT("A_Clear_2_SZ_Spin_Doubles")
			? (Event.Mino == ETotorisMino::S || Event.Mino == ETotorisMino::Z)
			: (Event.Mino == ETotorisMino::L || Event.Mino == ETotorisMino::J);
		if (bCorrectMino && Event.SpinKind != ETotorisSpinKind::None && Event.ClearedLines == 2)
		{
			AddCurrentMissionProgress(1);
		}
		else if (Mission.CurrentValue != 0)
		{
			Mission.CurrentValue = 0;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("A_Clear_4_Spins_One_Combo"))
	{
		if (Event.ComboCount < 0)
		{
			if (Mission.CurrentValue != 0)
			{
				Mission.CurrentValue = 0;
				RefreshMissionPanel();
			}
		}
		else if (Event.SpinKind != ETotorisSpinKind::None && Event.ClearedLines > 0)
		{
			AddCurrentMissionProgress(1);
		}
		return;
	}

	if (Id == TEXT("C_Use_Hold_12_In_Row"))
	{
		if (Event.bUsedHold)
		{
			AddCurrentMissionProgress(1);
		}
		else if (Mission.CurrentValue != 0)
		{
			Mission.CurrentValue = 0;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("C_Place_10_Continuous_Soft_Drop"))
	{
		if (Event.bSoftDropHeld)
		{
			AddCurrentMissionProgress(1);
		}
		else if (Mission.CurrentValue != 0)
		{
			Mission.CurrentValue = 0;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("C_Clear_10_No_T_or_I"))
	{
		if (Event.ClearedLines == 0) return;
		if (Event.Mino != ETotorisMino::T && Event.Mino != ETotorisMino::I)
		{
			AddCurrentMissionProgress(Event.ClearedLines);
		}
		else if (Mission.CurrentValue != 0)
		{
			Mission.CurrentValue = 0;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("C_Clear_2_O_Doubles_In_Row"))
	{
		if (Event.Mino == ETotorisMino::O && Event.ClearedLines == 2)
		{
			AddCurrentMissionProgress(1);
		}
		else if (Mission.CurrentValue != 0)
		{
			Mission.CurrentValue = 0;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("C_Clear_3_Doubles_Same_Piece"))
	{
		// Non-clearing placements are neutral: only a subsequent line clear can
		// continue or break the same-mino Double sequence.
		if (Event.ClearedLines == 0)
		{
			return;
		}
		if (Event.ClearedLines != 2)
		{
			bMissionHasSameDoubleMino = false;
			if (Mission.CurrentValue != 0)
			{
				Mission.CurrentValue = 0;
				RefreshMissionPanel();
			}
		}
		else if (!bMissionHasSameDoubleMino || MissionSameDoubleMino == Event.Mino)
		{
			MissionSameDoubleMino = Event.Mino;
			bMissionHasSameDoubleMino = true;
			AddCurrentMissionProgress(1);
		}
		else
		{
			MissionSameDoubleMino = Event.Mino;
			bMissionHasSameDoubleMino = true;
			Mission.CurrentValue = 1;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("C_Clear_2_Spins_One_Combo"))
	{
		if (Event.ComboCount < 0)
		{
			if (Mission.CurrentValue != 0)
			{
				Mission.CurrentValue = 0;
				RefreshMissionPanel();
			}
		}
		else if (Event.SpinKind != ETotorisSpinKind::None && Event.ClearedLines > 0)
		{
			AddCurrentMissionProgress(1);
		}
		return;
	}

	if (Id == TEXT("C_Place_6_No_DAS_Release"))
	{
		if (Event.bDASHeldWithoutRelease)
		{
			AddCurrentMissionProgress(1);
		}
		else if (Mission.CurrentValue != 0)
		{
			Mission.CurrentValue = 0;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("B_Place_5_No_Move_Rotate"))
	{
		if (!Event.bMovedHorizontally && !Event.bWasRotated)
		{
			AddCurrentMissionProgress(1);
		}
		else if (Mission.CurrentValue != 0)
		{
			Mission.CurrentValue = 0;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("B_Clear_2_Quads_In_Row"))
	{
		if (Event.ClearedLines == 4)
		{
			AddCurrentMissionProgress(1);
		}
		else if (Mission.CurrentValue != 0)
		{
			Mission.CurrentValue = 0;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("B_Clear_8_Singles_Only"))
	{
		if (Event.bUsedHold || (Event.ClearedLines > 0 && Event.ClearedLines != 1))
		{
			if (Mission.CurrentValue != 0)
			{
				Mission.CurrentValue = 0;
				RefreshMissionPanel();
			}
		}
		else if (Event.ClearedLines == 1)
		{
			AddCurrentMissionProgress(1);
		}
		return;
	}

	if (Id == TEXT("B_Dont_Cancel_Garbage_8_Seconds"))
	{
		if (Event.CancelledGarbageLines > 0)
		{
			MissionNoGarbageCancelSeconds = 0.f;
			if (Mission.CurrentValue != 0)
			{
				Mission.CurrentValue = 0;
				RefreshMissionPanel();
			}
		}
		return;
	}

	if (Id == TEXT("B_Clear_Single_Double_Triple_Quad"))
	{
		if (Event.ClearedLines == 0) return;
		const int32 ExpectedClear = Mission.CurrentValue + 1;
		if (Event.ClearedLines == ExpectedClear)
		{
			AddCurrentMissionProgress(1);
		}
		else
		{
			Mission.CurrentValue = 0;
			if (Event.ClearedLines == 1)
			{
				AddCurrentMissionProgress(1);
			}
			else
			{
				RefreshMissionPanel();
			}
		}
		return;
	}

	if (Id == TEXT("D_Place_3_No_Move_No_Rotate"))
	{
		if (!Event.bMovedHorizontally && !Event.bWasRotated)
		{
			AddCurrentMissionProgress(1);
		}
		else if (Mission.CurrentValue != 0)
		{
			Mission.CurrentValue = 0;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("D_Place_14_No_Line_Clear"))
	{
		if (Event.ClearedLines == 0)
		{
			AddCurrentMissionProgress(1);
		}
		else if (Mission.CurrentValue != 0)
		{
			Mission.CurrentValue = 0;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("D_Clear_2_Singles_In_Row_SZ"))
	{
		if ((Event.Mino == ETotorisMino::S || Event.Mino == ETotorisMino::Z) && Event.ClearedLines == 1)
		{
			AddCurrentMissionProgress(1);
		}
		else if (Mission.CurrentValue != 0)
		{
			Mission.CurrentValue = 0;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("D_Perform_3_Combo_No_Hold"))
	{
		// A new zero-combo clear begins a fresh candidate chain. Holding before
		// that clear is harmless; holding after it invalidates the chain.
		if (Event.ComboCount == 0)
		{
			bMissionComboHoldUsed = false;
		}
		if (bMissionComboHoldUsed)
		{
			return;
		}
		const int32 NewValue = FMath::Clamp(Event.ComboCount, 0, Mission.TargetValue);
		if (NewValue != Mission.CurrentValue)
		{
			Mission.CurrentValue = NewValue;
			if (Mission.CurrentValue >= Mission.TargetValue) CompleteCurrentMission(); else RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("D_Alternate_Clear_Not_Clear_6"))
	{
		const int32 CurrentClearState = Event.ClearedLines > 0 ? 1 : 0;
		const int32 PreviousClearState = MissionAlternatePreviousClearState;
		// Store this before AddCurrentMissionProgress: reaching the target can
		// synchronously generate a new mission set and reset sequence state.
		MissionAlternatePreviousClearState = CurrentClearState;
		if (PreviousClearState == INDEX_NONE || PreviousClearState != CurrentClearState)
		{
			AddCurrentMissionProgress(1);
		}
		else
		{
			Mission.CurrentValue = 1;
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("D_Clear_Single_Double_Triple"))
	{
		if (Event.ClearedLines >= 1 && Event.ClearedLines <= 3)
		{
			MissionLineClearChecklistMask |= static_cast<uint8>(1 << (Event.ClearedLines - 1));
			if (MissionLineClearChecklistMask == 0x07)
			{
				CompleteCurrentMission();
			}
			else
			{
				RefreshMissionPanel();
			}
		}
		return;
	}

	if (Id == TEXT("F_Place_Blocks_All_10_Columns"))
	{
		MissionColumnChecklistMask |= Event.PlacedColumnMask;
		if ((MissionColumnChecklistMask & 0x03FF) == 0x03FF)
		{
			CompleteCurrentMission();
		}
		else
		{
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("C_Clear_Quad_2_Columns"))
	{
		MissionColumnChecklistMask |= Event.QuadWellColumnMask;
		int32 CompletedColumnCount = 0;
		for (int32 Column = 0; Column < TotorisGeneration::BoardWidth; ++Column)
		{
			if ((MissionColumnChecklistMask & static_cast<uint16>(1u << Column)) != 0)
			{
				++CompletedColumnCount;
			}
		}
		if (CompletedColumnCount >= 2)
		{
			CompleteCurrentMission();
		}
		else if (Event.QuadWellColumnMask != 0)
		{
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("B_Clear_Quads_Both_Edge_Columns"))
	{
		constexpr uint16 EdgeColumnMask = 0x0201;
		MissionColumnChecklistMask |= Event.QuadWellColumnMask & EdgeColumnMask;
		if ((MissionColumnChecklistMask & EdgeColumnMask) == EdgeColumnMask)
		{
			CompleteCurrentMission();
		}
		else if ((Event.QuadWellColumnMask & EdgeColumnMask) != 0)
		{
			RefreshMissionPanel();
		}
		return;
	}

	const uint8 MinoBit = static_cast<uint8>(1u << static_cast<uint8>(Event.Mino));
	if (Id == TEXT("F_Place_All_7_Types"))
	{
		MissionMinoChecklistMask |= MinoBit;
		if (MissionMinoChecklistMask == 0x7F)
		{
			CompleteCurrentMission();
		}
		else
		{
			RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("C_Clear_Lines_All_7_Types"))
	{
		if (Event.ClearedLines > 0)
		{
			MissionMinoChecklistMask |= MinoBit;
			if (MissionMinoChecklistMask == 0x7F) CompleteCurrentMission(); else RefreshMissionPanel();
		}
		return;
	}

	if (Id == TEXT("B_Spin_Clears_3_Pieces"))
	{
		if (Event.Mino != ETotorisMino::O && Event.SpinKind != ETotorisSpinKind::None && Event.ClearedLines > 0)
		{
			MissionMinoChecklistMask |= MinoBit;
			int32 CompletedMinoCount = 0;
			for (int32 MinoIndex = 0; MinoIndex < 7; ++MinoIndex)
			{
				CompletedMinoCount += (MissionMinoChecklistMask & static_cast<uint8>(1u << MinoIndex)) != 0 ? 1 : 0;
			}
			if (CompletedMinoCount >= 3) CompleteCurrentMission(); else RefreshMissionPanel();
		}
		return;
	}

	int32 Progress = 0;
	if (Id == TEXT("F_Clear_2_Doubles") && Event.ClearedLines == 2) Progress = 1;
	else if (Id == TEXT("F_Clear_Quad") && Event.ClearedLines == 4) Progress = 1;
	else if (Id == TEXT("F_Clear_6_Lines")) Progress = Event.ClearedLines;
	else if (Id == TEXT("F_Clear_Single_O_Piece") && Event.Mino == ETotorisMino::O && Event.ClearedLines == 1) Progress = 1;
	else if (Id == TEXT("F_Clear_Double_O_Piece") && Event.Mino == ETotorisMino::O && Event.ClearedLines == 2) Progress = 1;
	else if (Id == TEXT("F_Clear_Double_SZ_Piece") && (Event.Mino == ETotorisMino::S || Event.Mino == ETotorisMino::Z) && Event.ClearedLines == 2) Progress = 1;
	else if (Id == TEXT("F_Clear_Triple_LJ_Piece") && (Event.Mino == ETotorisMino::L || Event.Mino == ETotorisMino::J) && Event.ClearedLines == 3) Progress = 1;
	else if (Id == TEXT("F_Clear_3_Lines_Holding_I") && Event.bHoldingI) Progress = Event.ClearedLines;
	else if (Id == TEXT("F_Clear_Unrotated_Piece") && !Event.bWasRotated && Event.ClearedLines > 0) Progress = 1;
	else if (Id == TEXT("F_Clear_Piece_Touching_Wall") && Event.bTouchedSideWall && Event.ClearedLines > 0) Progress = 1;
	else if (Id == TEXT("F_Clear_Piece_From_Hold") && Event.bTakenFromHold && Event.ClearedLines > 0) Progress = 1;
	else if (Id == TEXT("E_Perform_Any_Spin") && Event.SpinKind != ETotorisSpinKind::None) Progress = 1;
	else if (Id == TEXT("E_Clear_T_Spin_Single") && Event.Mino == ETotorisMino::T && Event.SpinKind == ETotorisSpinKind::Full && Event.ClearedLines == 1) Progress = 1;
	else if (Id == TEXT("E_Clear_T_Spin_Double") && Event.Mino == ETotorisMino::T && Event.SpinKind == ETotorisSpinKind::Full && Event.ClearedLines == 2) Progress = 1;
	else if (Id == TEXT("E_Clear_SZ_Spin") && (Event.Mino == ETotorisMino::S || Event.Mino == ETotorisMino::Z) && Event.SpinKind != ETotorisSpinKind::None && Event.ClearedLines > 0) Progress = 1;
	else if (Id == TEXT("E_Clear_LJ_Spin") && (Event.Mino == ETotorisMino::L || Event.Mino == ETotorisMino::J) && Event.SpinKind != ETotorisSpinKind::None && Event.ClearedLines > 0) Progress = 1;
	else if (Id == TEXT("E_Clear_2_Lines_Horizontal_I") && Event.Mino == ETotorisMino::I && (Event.FinalRotation & 1) == 0) Progress = Event.ClearedLines;
	else if (Id == TEXT("E_Place_20_Pieces")) Progress = 1;
	else if (Id == TEXT("E_Send_6_Attack")) Progress = Event.AttackLines;
	else if (Id == TEXT("E_Place_Vertical_I_Open_Sides") && Event.bVerticalIOpenSides) Progress = 1;
	else if (Id == TEXT("D_Clear_4_Doubles") && Event.ClearedLines == 2) Progress = 1;
	else if (Id == TEXT("D_Clear_2_Doubles_SZ") && (Event.Mino == ETotorisMino::S || Event.Mino == ETotorisMino::Z) && Event.ClearedLines == 2) Progress = 1;
	else if (Id == TEXT("D_Clear_2_Triples_LJ") && (Event.Mino == ETotorisMino::L || Event.Mino == ETotorisMino::J) && Event.ClearedLines == 3) Progress = 1;
	else if (Id == TEXT("D_Clear_I_Spin") && Event.Mino == ETotorisMino::I && Event.SpinKind != ETotorisSpinKind::None && Event.ClearedLines > 0) Progress = 1;
	else if (Id == TEXT("D_Clear_Quad_Upper_Half") && Event.ClearedLines == 4 && Event.bQuadInUpperHalf) Progress = 1;
	else if (Id == TEXT("D_Clear_Quad_On_2_Combo") && Event.ClearedLines == 4 && Event.ComboCount >= 2) Progress = 1;
	else if (Id == TEXT("D_Perform_3_Nonclearing_Spins") && Event.SpinKind != ETotorisSpinKind::None && Event.ClearedLines == 0) Progress = 1;
	else if (Id == TEXT("D_Perform_2_SZLJ_Spins") && (Event.Mino == ETotorisMino::S || Event.Mino == ETotorisMino::Z || Event.Mino == ETotorisMino::L || Event.Mino == ETotorisMino::J) && Event.SpinKind != ETotorisSpinKind::None) Progress = 1;
	else if (Id == TEXT("D_Clear_Line_Exactly_2_Colors") && Event.bClearedLineWithExactlyTwoColors) Progress = 1;
	else if (Id == TEXT("C_Clear_T_Spin_Triple") && Event.Mino == ETotorisMino::T && Event.SpinKind == ETotorisSpinKind::Full && Event.ClearedLines == 3) Progress = 1;
	else if (Id == TEXT("C_Clear_3_Triples") && Event.ClearedLines == 3) Progress = 1;
	else if (Id == TEXT("C_Clear_SZ_Spin_Triple") && (Event.Mino == ETotorisMino::S || Event.Mino == ETotorisMino::Z) && Event.SpinKind != ETotorisSpinKind::None && Event.ClearedLines == 3) Progress = 1;
	else if (Id == TEXT("C_Clear_4_T_Spin_Minis") && Event.Mino == ETotorisMino::T && Event.SpinKind == ETotorisSpinKind::Mini && Event.ClearedLines > 0) Progress = 1;
	else if (Id == TEXT("C_Send_14_Attack")) Progress = Event.AttackLines;
	else if (Id == TEXT("C_Clear_Garbage_LJ_Spin") && (Event.Mino == ETotorisMino::L || Event.Mino == ETotorisMino::J) && Event.SpinKind != ETotorisSpinKind::None && Event.bClearIncludedGarbage) Progress = Event.ClearedLines;
	else if (Id == TEXT("C_Clear_Garbage_SZ_Spin") && (Event.Mino == ETotorisMino::S || Event.Mino == ETotorisMino::Z) && Event.SpinKind != ETotorisSpinKind::None && Event.bClearIncludedGarbage) Progress = Event.ClearedLines;
	else if (Id == TEXT("C_Place_3_O_Column_1") && Event.bPlacedOInFirstColumn) Progress = 1;
	else if (Id == TEXT("C_Clear_Single_I_No_Move_Rotate") && Event.Mino == ETotorisMino::I && Event.ClearedLines == 1 && !Event.bMovedHorizontally && !Event.bWasRotated) Progress = 1;
	else if (Id == TEXT("C_Stack_At_Or_Above_Row_22") && Event.bStackAtOrAboveRow22) Progress = 1;
	else if (Id == TEXT("B_Clear_6_Lines_O_Pieces") && Event.Mino == ETotorisMino::O) Progress = Event.ClearedLines;
	else if (Id == TEXT("B_Clear_4_Quads") && Event.ClearedLines == 4) Progress = 1;
	else if (Id == TEXT("B_Clear_LJ_Spin_Triple") && (Event.Mino == ETotorisMino::L || Event.Mino == ETotorisMino::J) && Event.SpinKind != ETotorisSpinKind::None && Event.ClearedLines == 3) Progress = 1;
	else if (Id == TEXT("B_T_Spin_Double_Up") && Event.Mino == ETotorisMino::T && Event.SpinKind == ETotorisSpinKind::Full && Event.ClearedLines == 2 && Event.FinalRotation == 0) Progress = 1;
	else if (Id == TEXT("B_Clear_Double_O_No_Move_Rotate") && Event.Mino == ETotorisMino::O && Event.ClearedLines == 2 && !Event.bMovedHorizontally && !Event.bWasRotated) Progress = 1;
	else if (Id == TEXT("B_Place_3_T_No_Rotate") && Event.Mino == ETotorisMino::T && !Event.bWasRotated) Progress = 1;
	else if (Id == TEXT("B_T_Spin_Double_On_2_Combo") && Event.Mino == ETotorisMino::T && Event.SpinKind == ETotorisSpinKind::Full && Event.ClearedLines == 2 && Event.ComboCount >= 2) Progress = 1;
	else if (Id == TEXT("B_Clear_Line_One_Color") && Event.bClearedLineWithOneColor) Progress = 1;
	else if (Id == TEXT("B_Clear_Line_5_Colors") && Event.bClearedLineWithFiveColors) Progress = 1;
	else if (Id == TEXT("B_Clear_2_Non_Adjacent_Lines") && Event.bClearedTwoNonAdjacentLines) Progress = 1;
	else if (Id == TEXT("B_Clear_Line_Row_18_Or_Above") && Event.bClearedLineAtOrAboveRow18) Progress = 1;
	else if (Id == TEXT("A_Clear_I_Spin_Double") && Event.Mino == ETotorisMino::I && Event.SpinKind != ETotorisSpinKind::None && Event.ClearedLines == 2) Progress = 1;
	else if (Id == TEXT("A_Perform_Color_Clear") && Event.bColorClear) Progress = 1;
	else if (Id == TEXT("A_Clear_40_Lines")) Progress = Event.ClearedLines;
	else if (Id == TEXT("A_T_Spin_Double_Triple_Edge") && Event.bTSpinDoubleOrTripleCenteredInEdgeColumn) Progress = 1;
	else if (Id == TEXT("A_Clear_Line_All_7_Colors") && Event.bClearedLineWithAllSevenMinoColors) Progress = 1;
	else if (Id == TEXT("A_Perform_Perfect_Clear") && Event.bPerfectClear) Progress = 1;
	AddCurrentMissionProgress(Progress);
}

void ATotorisPlayerController::BindMissionGameplayEvents()
{
	UnbindMissionGameplayEvents();
	UTotorisBlockGeneratorComponent* BlockGenerator = FindBlockGenerator();
	if (!IsValid(BlockGenerator)) return;
	MissionEventSource = BlockGenerator;
	if (SelectedGameSetupMode == ETotorisGameSetupMode::SChallenge)
	{
		MissionPieceLockedHandle = BlockGenerator->OnMissionPieceLocked.AddUObject(this, &ATotorisPlayerController::HandleMissionPieceLocked);
		MissionGameRestartedHandle = BlockGenerator->OnMissionGameRestarted.AddUObject(this, &ATotorisPlayerController::HandleMissionGameRestarted);
		return;
	}
	MissionPieceLockedHandle = BlockGenerator->OnMissionPieceLocked.AddUObject(this, &ATotorisPlayerController::HandleMissionPieceLocked);
	MissionHoldUsedHandle = BlockGenerator->OnMissionHoldUsed.AddUObject(this, &ATotorisPlayerController::HandleMissionHoldUsed);
	MissionSuccessfulRotationHandle = BlockGenerator->OnMissionSuccessfulRotation.AddUObject(this, &ATotorisPlayerController::HandleMissionSuccessfulRotation);
	MissionGameRestartedHandle = BlockGenerator->OnMissionGameRestarted.AddUObject(this, &ATotorisPlayerController::HandleMissionGameRestarted);
	MissionTopStackTickHandle = BlockGenerator->OnMissionTopStackTick.AddUObject(this, &ATotorisPlayerController::HandleMissionTopStackTick);
}

void ATotorisPlayerController::UnbindMissionGameplayEvents()
{
	if (UTotorisBlockGeneratorComponent* BlockGenerator = MissionEventSource.Get())
	{
		BlockGenerator->OnMissionPieceLocked.Remove(MissionPieceLockedHandle);
		BlockGenerator->OnMissionHoldUsed.Remove(MissionHoldUsedHandle);
		BlockGenerator->OnMissionSuccessfulRotation.Remove(MissionSuccessfulRotationHandle);
		BlockGenerator->OnMissionGameRestarted.Remove(MissionGameRestartedHandle);
		BlockGenerator->OnMissionTopStackTick.Remove(MissionTopStackTickHandle);
	}
	MissionEventSource.Reset();
	MissionPieceLockedHandle.Reset();
	MissionHoldUsedHandle.Reset();
	MissionSuccessfulRotationHandle.Reset();
	MissionGameRestartedHandle.Reset();
	MissionTopStackTickHandle.Reset();
}

void ATotorisPlayerController::SetClassicMode(ETotorisClassicMode Mode)
{
    const bool bModeChanged = !bClassicModeHasAppliedDefaults || ClassicSettings.Mode != Mode;
    ClassicSettings.Mode = Mode;
	// Widgets can invoke this again from their Start handler.  Reapplying a
	// preset in that path was overwriting the user's Garbage Attack checkbox:
	// Endless became enabled again, while Sprint became disabled again.
	if (!bModeChanged)
	{
		return;
	}

    // Defaults are assigned only on an actual mode selection; changing a
    // checkbox afterwards deliberately overrides the selected preset.
	if (!bGarbageAttackHasUserOverride)
	{
		CommonGameSetupSettings.bGarbageAttack = Mode == ETotorisClassicMode::Endless;
	}
	if (!bGarbageDifficultyIncreaseHasUserOverride)
	{
		CommonGameSetupSettings.bGarbageDifficultyIncrease = Mode == ETotorisClassicMode::Endless;
	}
    CommonGameSetupSettings.bCheeseGarbage = false;
    CommonGameSetupSettings.bStartGravity = true;
	// Endless and Blitz default to increasing gravity. The player may disable
	// either gravity option after selecting the mode.
	CommonGameSetupSettings.bGravityIncrease =
		Mode == ETotorisClassicMode::Endless || Mode == ETotorisClassicMode::Blitz;
	bClassicModeHasAppliedDefaults = true;
}

int32 ATotorisPlayerController::SetSprintTargetLines(int32 Lines)
{
    ClassicSettings.TargetLines = FMath::Clamp(Lines, 1, 1000);
    return ClassicSettings.TargetLines;
}

int32 ATotorisPlayerController::SetBlitzLimitTimeSeconds(int32 Seconds)
{
    ClassicSettings.LimitTimeSeconds = FMath::Clamp(Seconds, 1, 359);
    return ClassicSettings.LimitTimeSeconds;
}

int32 ATotorisPlayerController::SetCheeseRaceCount(int32 Count)
{
    ClassicSettings.CheeseCount = FMath::Clamp(Count, 1, 100);
    return ClassicSettings.CheeseCount;
}


void ATotorisPlayerController::StartEventPreview(FName EventName)
{
	if (!EventPreviewName.IsNone()) return;
	const TArray<FName> Supported = {TEXT("Smilemo"), TEXT("SANABI"), TEXT("Higurashi_Onikakushi"),
		TEXT("Palworld"), TEXT("MonsterHunter"), TEXT("MahjongSoul"), TEXT("REANIMAL"), TEXT("Minecraft"), TEXT("Journey")};
	if (!Supported.Contains(EventName)) return;
	auto* Game = FindBlockGenerator();
	if (!Game) return;
	StopClassicGame();
	if (MenuManager) MenuManager->HideCurrentMenu();
	EventPreviewName = EventName;
	CreateEventPreviewBackButton();
	// StopGame clears every mino without spawning a piece or starting a countdown.
	Game->PrepareEventPreview();
	SetTaggedGameplayVisualsVisible(!bOtherWidgetsHidden);
	Game->SetGameplayVisible(!bOtherWidgetsHidden);
	ClassicHUD = CreateWidget<UTotorisClassicHUDWidget>(this, UTotorisClassicHUDWidget::StaticClass());
	if (ClassicHUD)
	{
		ClassicHUD->SetObservedGame(Game);
		ClassicHUD->AddToPlayerScreen(20);
	}
	SannabiEventWidget = CreateWidget<UTotorisSannabiEventWidget>(this, UTotorisSannabiEventWidget::StaticClass());
	if (SannabiEventWidget)
	{
		SannabiEventWidget->SetObservedGame(Game);
		SannabiEventWidget->AddToPlayerScreen(1000);
	}
	MahjongEventWidget = CreateWidget<UTotorisMahjongEventWidget>(this, UTotorisMahjongEventWidget::StaticClass());
	if (MahjongEventWidget) MahjongEventWidget->AddToPlayerScreen(1100);
	ReAnimalEventWidget = CreateWidget<UTotorisReAnimalEventWidget>(this, UTotorisReAnimalEventWidget::StaticClass());
	if (ReAnimalEventWidget) ReAnimalEventWidget->AddToPlayerScreen(1200);
	SetShowWidgetsOverlayEnabled(true);
	EnterMenuInputMode();
	if (EventName == TEXT("Smilemo")) PlaySmilemoSitoJumpPath();
	else if (EventName == TEXT("SANABI") && SannabiEventWidget) SannabiEventWidget->StartEvent();
	else if (EventName == TEXT("Higurashi_Onikakushi")) PlayUsodaEvent();
	else if (EventName == TEXT("Palworld")) PlayDummyDragonFlight();
	else if (EventName == TEXT("MonsterHunter")) PlayAnjanathDropEvent();
	else if (EventName == TEXT("MahjongSoul")) PlayMahjongEvent();
	else if (EventName == TEXT("REANIMAL") && ReAnimalEventWidget) ReAnimalEventWidget->StartEvent();
	else if (EventName == TEXT("Minecraft")) PlayMinecraftBlockEvent();
	else if (EventName == TEXT("Journey")) PlayJourneyEvent();
}

void ATotorisPlayerController::PlayJourneyEvent()
{
	if (auto* Game = FindBlockGenerator())
	{
		if (!IsValid(JourneyEventComponent))
		{
			JourneyEventComponent = NewObject<UJourneyEventComponent>(this);
			AddInstanceComponent(JourneyEventComponent);
			JourneyEventComponent->RegisterComponent();
		}
		JourneyEventComponent->PlayEvent(Game);
	}
}

bool ATotorisPlayerController::IsPreviewEventPlaying() const
{
	if (EventPreviewName == TEXT("Journey")) return IsValid(JourneyEventComponent) && JourneyEventComponent->IsEventPlaying();
	if (EventPreviewName == TEXT("Smilemo")) return IsValid(SmilemoSitoJumpComponent) && SmilemoSitoJumpComponent->IsJumpRoutePlaying();
	if (EventPreviewName == TEXT("SANABI")) return IsValid(SannabiEventWidget) && SannabiEventWidget->IsEventPlaying();
	if (EventPreviewName == TEXT("Higurashi_Onikakushi")) return IsUsodaEventPlaying();
	if (EventPreviewName == TEXT("Palworld")) return IsValid(DummyDragonFlightComponent) && DummyDragonFlightComponent->IsFlightPlaying();
	if (EventPreviewName == TEXT("MonsterHunter")) return IsValid(AnjanathDropEventComponent) && AnjanathDropEventComponent->IsEventPlaying();
	if (EventPreviewName == TEXT("MahjongSoul")) return IsValid(MahjongEventWidget) && MahjongEventWidget->IsEventPlaying();
	if (EventPreviewName == TEXT("REANIMAL")) return IsValid(ReAnimalEventWidget) && ReAnimalEventWidget->IsEventPlaying();
	if (EventPreviewName == TEXT("Minecraft")) return IsValid(MinecraftBlockEventComponent) && MinecraftBlockEventComponent->IsEventPlaying();
	return false;
}

void ATotorisPlayerController::CreateEventPreviewBackButton()
{
	// Use the authored gallery button (including its label/style), not a second
	// independently styled copy. Only the button is attached to this overlay.
	UClass* GalleryClass = LoadClass<UUserWidget>(nullptr,
		TEXT("/Game/Totoris/UI/Widgets/WBP_Event.WBP_Event_C"));
	if (!GalleryClass) return;
	EventPreviewBackOverlay = CreateWidget<UUserWidget>(this, GalleryClass);
	if (!EventPreviewBackOverlay) return;
	UCanvasPanel* Root = Cast<UCanvasPanel>(EventPreviewBackOverlay->GetRootWidget());
	UButton* Back = Cast<UButton>(EventPreviewBackOverlay->GetWidgetFromName(TEXT("BackButton")));
	if (!Root || !Back) { EventPreviewBackOverlay = nullptr; return; }
	FVector2D Size(140.f, 58.f);
	if (auto* AuthoredSlot = Cast<UCanvasPanelSlot>(Back->Slot))
		Size = AuthoredSlot->GetSize();
	Back->RemoveFromParent();
	Root->ClearChildren();
	UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Back);
	Slot->SetAnchors(FAnchors(1.f, 0.f));
	Slot->SetAlignment(FVector2D(1.f, 0.f));
	Slot->SetPosition(FVector2D(-24.f, 24.f));
	Slot->SetSize(Size);
	Back->OnClicked.Clear();
	Back->OnClicked.AddDynamic(this, &ATotorisPlayerController::FinishEventPreview);
	Back->SetVisibility(ESlateVisibility::Visible);
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	EventPreviewBackOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	// Stay accessible above event overlays, even when the board UI is hidden.
	EventPreviewBackOverlay->AddToPlayerScreen(2000);
}

void ATotorisPlayerController::FinishEventPreview()
{
	if (EventPreviewName.IsNone()) return;
	if (EventPreviewBackOverlay)
	{
		EventPreviewBackOverlay->RemoveFromParent();
		EventPreviewBackOverlay = nullptr;
	}
	// Returning to the gallery is not a cancellation. In particular Smilemo's
	// boss keeps advancing after the jump route has completed.
	EventPreviewName = NAME_None;
	if (ClassicHUD) { ClassicHUD->RemoveFromParent(); ClassicHUD = nullptr; }
	if (auto* Game = FindBlockGenerator()) Game->StopGame();
	SetTaggedGameplayVisualsVisible(false);
	SetShowWidgetsOverlayEnabled(true);
	bOtherWidgetsHidden = false;
	if (ShowWidgetsOverlay)
		if (auto* Check = Cast<UCheckBox>(ShowWidgetsOverlay->GetWidgetFromName(TEXT("CheckBox"))))
			Check->SetIsChecked(false);
	if (MenuManager)
	{
		MenuManager->SetMenusHidden(false);
		MenuManager->ShowEventGallery();
	}
	EnterMenuInputMode();
}

void ATotorisPlayerController::StartClassicGame()
{
	StopMissionPresentationEvents();
	if (SelectedGameSetupMode == ETotorisGameSetupMode::SChallenge && MenuManager)
	{
		bool bAnyEnabled = false;
		for (int32 Index = 0; Index < 10; ++Index) bAnyEnabled |= MenuManager->IsSCMissionEnabled(Index);
		if (!bAnyEnabled) { MenuManager->ShowSCList(); EnterMenuInputMode(); return; }
	}
	if (SelectedGameSetupMode == ETotorisGameSetupMode::Mission && !HasAnyMissionTierEnabled())
	{
		UE_LOG(LogTemp, Warning, TEXT("Totoris mission game start blocked: no mission tier is enabled."));
		return;
	}
	if (MenuManager)
	{
		MenuManager->HideCurrentMenu();
	}
	SetShowWidgetsOverlayEnabled(false);

	SetTaggedGameplayVisualsVisible(true);

	if (UTotorisBlockGeneratorComponent* BlockGenerator = FindBlockGenerator())
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Totoris virtual garbage starting: Mode=%d Enabled=%s Difficulty=%d Increase=%s"),
			static_cast<int32>(ClassicSettings.Mode),
			CommonGameSetupSettings.bGarbageAttack ? TEXT("true") : TEXT("false"),
			CommonGameSetupSettings.GarbageDifficulty,
			CommonGameSetupSettings.bGarbageDifficultyIncrease ? TEXT("true") : TEXT("false"));

		BlockGenerator->ApplyHandlingSettings(
			HandlingARRMilliseconds,
			HandlingDASMilliseconds,
			HandlingDCDMilliseconds,
			HandlingSDFMultiplier,
			bHandlingSDFInfinite);

		BlockGenerator->ApplyKeyBindings(
			KeyBindings[0],
			KeyBindings[1],
			KeyBindings[2],
			KeyBindings[3],
			KeyBindings[4],
			KeyBindings[5],
			KeyBindings[6],
			KeyBindings[7],
			KeyBindings[8]);

		SaveHandlingSettings();
		SaveKeyBindings();

		FTotorisClassicSettings RunSettings = ClassicSettings;
		if (SelectedGameSetupMode == ETotorisGameSetupMode::SChallenge) RunSettings.Mode = ETotorisClassicMode::Endless;
		BlockGenerator->ConfigureClassicGame(
			RunSettings,
			CommonGameSetupSettings.bStartGravity,
			CommonGameSetupSettings.bGravityIncrease,
			CommonGameSetupSettings.bCheeseGarbage,
			CommonGameSetupSettings.bQuickStart,
			SelectedGameSetupMode == ETotorisGameSetupMode::Mission ||
			SelectedGameSetupMode == ETotorisGameSetupMode::SChallenge);
		BlockGenerator->ConfigureVirtualGarbage(
			CommonGameSetupSettings.bGarbageAttack,
			CommonGameSetupSettings.GarbageDifficulty,
			CommonGameSetupSettings.bGarbageDifficultyIncrease);

		BlockGenerator->StartGame();
		bGameEndEligibleForCurrentRun = BlockGenerator->IsGameplayActive();
		if (SelectedGameSetupMode == ETotorisGameSetupMode::Mission || SelectedGameSetupMode == ETotorisGameSetupMode::SChallenge)
		{
			CompletedMissionCount = 0;
			MissionDifficulty = 1;
			InitializeMissionRun();
			BindMissionGameplayEvents();
		}
		else
		{
			UnbindMissionGameplayEvents();
		}
		BlockGenerator->OnRunFinished.RemoveDynamic(this, &ATotorisPlayerController::HandleRunFinished);
		BlockGenerator->OnRunFinished.AddDynamic(this, &ATotorisPlayerController::HandleRunFinished);
		if (IsValid(GameEndWidget)) { GameEndWidget->RemoveFromParent(); GameEndWidget = nullptr; }

		if (IsLocalController() && BlockGenerator->IsGameplayActive())
		{
			if (IsValid(ClassicHUD))
			{
				ClassicHUD->RemoveFromParent();
				ClassicHUD = nullptr;
			}

			ClassicHUD = CreateWidget<UTotorisClassicHUDWidget>(
				this,
				UTotorisClassicHUDWidget::StaticClass());

			if (IsValid(ClassicHUD))
			{
				ClassicHUD->SetObservedGame(BlockGenerator);
				ClassicHUD->AddToPlayerScreen(20);
			}

			// The overlay remains dormant at game start.  It is retained above every
			// UI layer so a later mission-set event can begin without constructing a
			// new viewport stack during active play.
			if (IsValid(SannabiEventWidget))
			{
				SannabiEventWidget->RemoveFromParent();
				SannabiEventWidget = nullptr;
			}
			SannabiEventWidget = CreateWidget<UTotorisSannabiEventWidget>(this,
				UTotorisSannabiEventWidget::StaticClass());
			if (IsValid(SannabiEventWidget))
			{
				SannabiEventWidget->SetObservedGame(BlockGenerator);
				SannabiEventWidget->AddToPlayerScreen(1000);
			}
			if (IsValid(MahjongEventWidget))
			{
				MahjongEventWidget->RemoveFromParent();
				MahjongEventWidget = nullptr;
			}
			MahjongEventWidget = CreateWidget<UTotorisMahjongEventWidget>(this,
				UTotorisMahjongEventWidget::StaticClass());
			if (IsValid(MahjongEventWidget))
			{
				MahjongEventWidget->AddToPlayerScreen(1100);
			}
			if (IsValid(ReAnimalEventWidget)) ReAnimalEventWidget->RemoveFromParent();
			ReAnimalEventWidget = CreateWidget<UTotorisReAnimalEventWidget>(this,
				UTotorisReAnimalEventWidget::StaticClass());
			if (IsValid(ReAnimalEventWidget))
			{
				ReAnimalEventWidget->AddToPlayerScreen(1200);
			}
		}
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Totoris: StartClassicGame could not find UTotorisBlockGeneratorComponent in the current world"));
	}

	EnterGameInputMode();
}


void ATotorisPlayerController::StopClassicGame()
{
	if (EventPreviewBackOverlay)
	{
		EventPreviewBackOverlay->RemoveFromParent();
		EventPreviewBackOverlay = nullptr;
	}
	EventPreviewName = NAME_None;
	if (bSettingsOpenedFromPause && MenuManager) MenuManager->HideCurrentMenu();
	bSettingsOpenedFromPause = false;
	StopMissionPresentationEvents();
	if (IsValid(PauseWidget)) PauseWidget->RemoveFromParent();
	if (IsValid(ReAnimalEventWidget))
	{
		ReAnimalEventWidget->RemoveFromParent();
		ReAnimalEventWidget = nullptr;
	}
	UsodaFadeElapsedSeconds = -1.f;
	if (IsValid(UsodaPostProcessVolume))
	{
		UsodaPostProcessVolume->Destroy();
		UsodaPostProcessVolume = nullptr;
	}
	StopUsodaHigurashiSound();
	bGameEndEligibleForCurrentRun = false;
	UnbindMissionGameplayEvents();
	SetShowWidgetsOverlayEnabled(true);
    if (IsValid(ClassicHUD))
    {
        ClassicHUD->RemoveFromParent();
        ClassicHUD = nullptr;
    }
	if (IsValid(SannabiEventWidget))
	{
		SannabiEventWidget->RemoveFromParent();
		SannabiEventWidget = nullptr;
	}
	if (IsValid(MahjongEventWidget))
	{
		MahjongEventWidget->RemoveFromParent();
		MahjongEventWidget = nullptr;
	}

	if (UTotorisBlockGeneratorComponent* BlockGenerator = FindBlockGenerator())
	{
		BlockGenerator->StopGame();
	}

	SetTaggedGameplayVisualsVisible(false);
}

void ATotorisPlayerController::StopMissionPresentationEvents()
{
	if (IsValid(JourneyEventComponent)) JourneyEventComponent->StopEvent();
	if (IsValid(SmilemoSitoJumpComponent)) SmilemoSitoJumpComponent->StopEvent();
	if (IsValid(DummyDragonFlightComponent)) DummyDragonFlightComponent->StopEvent();
	if (IsValid(AnjanathDropEventComponent)) AnjanathDropEventComponent->StopEvent();
	if (IsValid(MinecraftBlockEventComponent)) MinecraftBlockEventComponent->StopEvent();
	if (IsValid(SannabiEventWidget)) SannabiEventWidget->StopEvent();
	if (IsValid(MahjongEventWidget)) MahjongEventWidget->StopEvent();
	if (IsValid(ReAnimalEventWidget)) ReAnimalEventWidget->StopEvent();
	TotorisEventAudio::Stop(this);
	StopUsodaHigurashiSound();
	UsodaFadeElapsedSeconds = -1.f;
	if (IsValid(UsodaPostProcessVolume)) { UsodaPostProcessVolume->Destroy(); UsodaPostProcessVolume = nullptr; }
}

void ATotorisPlayerController::QuitGame()
{
	SaveHandlingSettings();
	SaveKeyBindings();

	UKismetSystemLibrary::QuitGame(
		this,
		this,
		EQuitPreference::Quit,
		false);
}

float ATotorisPlayerController::SetARRSliderValue(float SliderValue)
{
	const float Clamped = FMath::Clamp(SliderValue, 0.f, 1.f);
	HandlingARRMilliseconds = FMath::RoundToInt(
		FMath::Lerp(
			static_cast<float>(MaxARRMilliseconds),
			0.f,
			Clamped));

	MarkHandlingDirtyAndApply();
	return GetARRSliderValue();
}

float ATotorisPlayerController::SetDASSliderValue(float SliderValue)
{
	const float Clamped = FMath::Clamp(SliderValue, 0.f, 1.f);
	HandlingDASMilliseconds = FMath::RoundToInt(
		FMath::Lerp(
			static_cast<float>(MaxDASMilliseconds),
			static_cast<float>(MinDASMilliseconds),
			Clamped));

	MarkHandlingDirtyAndApply();
	return GetDASSliderValue();
}

float ATotorisPlayerController::SetDCDSliderValue(float SliderValue)
{
	const float Clamped = FMath::Clamp(SliderValue, 0.f, 1.f);
	HandlingDCDMilliseconds = FMath::RoundToInt(
		FMath::Lerp(
			static_cast<float>(MaxDCDMilliseconds),
			0.f,
			Clamped));

	MarkHandlingDirtyAndApply();
	return GetDCDSliderValue();
}

float ATotorisPlayerController::SetSDFSliderValue(float SliderValue)
{
	const float Clamped = FMath::Clamp(SliderValue, 0.f, 1.f);
	const int32 Step = FMath::Clamp(
		FMath::RoundToInt(Clamped * static_cast<float>(SDFSliderStepCount)),
		0,
		SDFSliderStepCount);

	if (Step >= SDFSliderStepCount)
	{
		bHandlingSDFInfinite = true;
		HandlingSDFMultiplier = MaxSDFMultiplier;
	}
	else
	{
		bHandlingSDFInfinite = false;
		HandlingSDFMultiplier = MinSDFMultiplier + Step;
	}

	MarkHandlingDirtyAndApply();
	return GetSDFSliderValue();
}

float ATotorisPlayerController::GetARRSliderValue() const
{
	return 1.f -
		(static_cast<float>(HandlingARRMilliseconds) /
		 static_cast<float>(MaxARRMilliseconds));
}

float ATotorisPlayerController::GetDASSliderValue() const
{
	const int32 Range = MaxDASMilliseconds - MinDASMilliseconds;
	return static_cast<float>(MaxDASMilliseconds - HandlingDASMilliseconds) /
		static_cast<float>(Range);
}

float ATotorisPlayerController::GetDCDSliderValue() const
{
	return 1.f -
		(static_cast<float>(HandlingDCDMilliseconds) /
		 static_cast<float>(MaxDCDMilliseconds));
}

float ATotorisPlayerController::GetSDFSliderValue() const
{
	if (bHandlingSDFInfinite)
	{
		return 1.f;
	}

	const int32 Step = FMath::Clamp(
		HandlingSDFMultiplier - MinSDFMultiplier,
		0,
		SDFSliderStepCount - 1);

	return static_cast<float>(Step) /
		static_cast<float>(SDFSliderStepCount);
}

FText ATotorisPlayerController::GetARRDisplayText() const
{
	return FText::FromString(
		FString::Printf(TEXT("%d ms"), HandlingARRMilliseconds));
}

FText ATotorisPlayerController::GetDASDisplayText() const
{
	return FText::FromString(
		FString::Printf(TEXT("%d ms"), HandlingDASMilliseconds));
}

FText ATotorisPlayerController::GetDCDDisplayText() const
{
	return FText::FromString(
		FString::Printf(TEXT("%d ms"), HandlingDCDMilliseconds));
}

FText ATotorisPlayerController::GetSDFDisplayText() const
{
	if (bHandlingSDFInfinite)
	{
		return FText::FromString(TEXT("inf"));
	}

	return FText::FromString(
		FString::Printf(TEXT("%d X"), HandlingSDFMultiplier));
}

void ATotorisPlayerController::ResetHandlingDefaults()
{
	HandlingARRMilliseconds = DefaultARRMilliseconds;
	HandlingDASMilliseconds = DefaultDASMilliseconds;
	HandlingDCDMilliseconds = DefaultDCDMilliseconds;
	HandlingSDFMultiplier = DefaultSDFMultiplier;
	bHandlingSDFInfinite = false;

	MarkHandlingDirtyAndApply();
}

FKey ATotorisPlayerController::GetKeyBinding(uint8 ActionIndex, int32 SlotIndex) const
{
	if (!IsValidKeyBindingLocation(ActionIndex, SlotIndex))
	{
		return FKey();
	}

	return KeyBindings[ActionIndex][SlotIndex - 1];
}

FText ATotorisPlayerController::GetKeyBindingDisplayText(uint8 ActionIndex, int32 SlotIndex) const
{
	return MakeKeyDisplayText(GetKeyBinding(ActionIndex, SlotIndex));
}

bool ATotorisPlayerController::IsKeyAllowedForBinding(FKey Key) const
{
	if (!Key.IsValid())
	{
		return false;
	}

	if (Key == EKeys::Escape ||
		Key == EKeys::Delete ||
		Key == EKeys::BackSpace ||
		Key == EKeys::AnyKey)
	{
		return false;
	}

	if (Key.IsMouseButton() || Key.IsGamepadKey() || Key.IsTouch() ||
		!Key.IsDigital() || !Key.IsBindableToActions())
	{
		return false;
	}

	// OnPreviewKeyDown normally supplies keyboard keys only, but reject named
	// pointer/controller inputs too so the backend remains keyboard-only.
	const FString KeyName = Key.GetFName().ToString();
	if (KeyName.Contains(TEXT("Mouse"), ESearchCase::IgnoreCase) ||
		KeyName.Contains(TEXT("Gamepad"), ESearchCase::IgnoreCase) ||
		KeyName.Contains(TEXT("Touch"), ESearchCase::IgnoreCase) ||
		KeyName.Contains(TEXT("MotionController"), ESearchCase::IgnoreCase))
	{
		return false;
	}

	return true;
}

ETotorisKeyBindResult ATotorisPlayerController::SetKeyBinding(
	uint8 ActionIndex,
	int32 SlotIndex,
	FKey NewKey)
{
	if (!IsValidKeyBindingLocation(ActionIndex, SlotIndex))
	{
		return ETotorisKeyBindResult::InvalidActionOrSlot;
	}

	if (!IsKeyAllowedForBinding(NewKey))
	{
		return ETotorisKeyBindResult::KeyNotAllowed;
	}

	const int32 SlotZeroBased = SlotIndex - 1;
	if (KeyBindings[ActionIndex][SlotZeroBased] == NewKey)
	{
		return ETotorisKeyBindResult::NoChange;
	}

	int32 ExistingActionIndex = INDEX_NONE;
	int32 ExistingSlotZeroBased = INDEX_NONE;
	const bool bAlreadyBound = FindBoundKey(
		NewKey,
		ExistingActionIndex,
		ExistingSlotZeroBased);

	if (bAlreadyBound)
	{
		if (ExistingActionIndex == ActionIndex)
		{
			return ETotorisKeyBindResult::DuplicateInSameAction;
		}

		if (CountBoundKeys(ExistingActionIndex) <= 1)
		{
			return ETotorisKeyBindResult::WouldUnbindOtherAction;
		}

		KeyBindings[ExistingActionIndex][ExistingSlotZeroBased] = FKey();
	}

	KeyBindings[ActionIndex][SlotZeroBased] = NewKey;
	MarkKeyBindingsDirtyApplyAndSave();

	return bAlreadyBound
		? ETotorisKeyBindResult::MovedFromOtherAction
		: ETotorisKeyBindResult::Success;
}

ETotorisKeyBindResult ATotorisPlayerController::ClearKeyBinding(
	uint8 ActionIndex,
	int32 SlotIndex)
{
	if (!IsValidKeyBindingLocation(ActionIndex, SlotIndex))
	{
		return ETotorisKeyBindResult::InvalidActionOrSlot;
	}

	const int32 SlotZeroBased = SlotIndex - 1;
	if (!KeyBindings[ActionIndex][SlotZeroBased].IsValid())
	{
		return ETotorisKeyBindResult::NoChange;
	}

	if (CountBoundKeys(ActionIndex) <= 1)
	{
		return ETotorisKeyBindResult::CannotClearLastKey;
	}

	KeyBindings[ActionIndex][SlotZeroBased] = FKey();
	MarkKeyBindingsDirtyApplyAndSave();
	return ETotorisKeyBindResult::Success;
}

void ATotorisPlayerController::ResetKeyBindingsToDefaults()
{
	InitializeDefaultKeyBindings();
	MarkKeyBindingsDirtyApplyAndSave();
}

bool ATotorisPlayerController::IsKeyBindResultSuccessful(
	ETotorisKeyBindResult Result) const
{
	switch (Result)
	{
	case ETotorisKeyBindResult::Success:
	case ETotorisKeyBindResult::MovedFromOtherAction:
	case ETotorisKeyBindResult::NoChange:
		return true;

	default:
		return false;
	}
}

FText ATotorisPlayerController::GetKeyBindResultFeedbackText(
	ETotorisKeyBindResult Result) const
{
	switch (Result)
	{
	case ETotorisKeyBindResult::DuplicateInSameAction:
		return NSLOCTEXT(
			"TotorisKeyBindings",
			"DuplicateInSameAction",
			"이 동작에 이미 등록된 키입니다.");

	case ETotorisKeyBindResult::WouldUnbindOtherAction:
		return NSLOCTEXT(
			"TotorisKeyBindings",
			"WouldUnbindOtherAction",
			"다른 동작의 유일한 키라 사용할 수 없습니다.");

	case ETotorisKeyBindResult::CannotClearLastKey:
		return NSLOCTEXT(
			"TotorisKeyBindings",
			"CannotClearLastKey",
			"각 동작에는 최소 1개의 키가 필요합니다.");

	case ETotorisKeyBindResult::KeyNotAllowed:
		return NSLOCTEXT(
			"TotorisKeyBindings",
			"KeyNotAllowed",
			"사용할 수 없는 키입니다.");

	case ETotorisKeyBindResult::InvalidActionOrSlot:
		return NSLOCTEXT(
			"TotorisKeyBindings",
			"InvalidActionOrSlot",
			"잘못된 키 슬롯입니다.");

	case ETotorisKeyBindResult::Success:
	case ETotorisKeyBindResult::MovedFromOtherAction:
	case ETotorisKeyBindResult::NoChange:
	default:
		return FText::GetEmpty();
	}
}

void ATotorisPlayerController::InitializeDefaultKeyBindings()
{
	for (int32 ActionIndex = 0; ActionIndex < KeyBindActionCount; ++ActionIndex)
	{
		KeyBindings[ActionIndex].SetNum(KeyBindSlotsPerAction);
		for (FKey& Key : KeyBindings[ActionIndex])
		{
			Key = FKey();
		}
	}

	KeyBindings[0][0] = EKeys::Left;
	KeyBindings[1][0] = EKeys::Right;
	KeyBindings[2][0] = EKeys::Down;
	KeyBindings[3][0] = EKeys::SpaceBar;
	KeyBindings[4][0] = EKeys::X;
	KeyBindings[4][1] = EKeys::Up;
	KeyBindings[5][0] = EKeys::Z;
	KeyBindings[5][1] = EKeys::LeftControl;
	KeyBindings[6][0] = EKeys::A;
	KeyBindings[7][0] = EKeys::C;
	KeyBindings[7][1] = EKeys::LeftShift;
	KeyBindings[8][0] = EKeys::R;
}

void ATotorisPlayerController::LoadKeyBindings()
{
	InitializeDefaultKeyBindings();

	bool bReadAnySetting = false;
	if (GConfig)
	{
		for (int32 ActionIndex = 0; ActionIndex < KeyBindActionCount; ++ActionIndex)
		{
			for (int32 SlotZeroBased = 0; SlotZeroBased < KeyBindSlotsPerAction; ++SlotZeroBased)
			{
				const FString ConfigKey = FString::Printf(
					TEXT("%s%d"),
					KeyBindActionConfigNames[ActionIndex],
					SlotZeroBased + 1);

				FString StoredKeyName;
				if (GConfig->GetString(
					KeyBindingsConfigSection,
					*ConfigKey,
					StoredKeyName,
					GGameUserSettingsIni))
				{
					bReadAnySetting = true;
					KeyBindings[ActionIndex][SlotZeroBased] = StoredKeyName.IsEmpty()
						? FKey()
						: FKey(FName(*StoredKeyName));
				}
			}
		}
	}

	bKeyBindingsDirty = false;

	if (bReadAnySetting && !AreKeyBindingsValid())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Totoris: invalid saved key bindings detected; restoring defaults"));
		InitializeDefaultKeyBindings();
		bKeyBindingsDirty = true;
		SaveKeyBindings(true);
	}

	ApplyKeyBindingsToGame();
}

void ATotorisPlayerController::SaveKeyBindings(bool bForce)
{
	if ((!bForce && !bKeyBindingsDirty) || !GConfig)
	{
		return;
	}

	for (int32 ActionIndex = 0; ActionIndex < KeyBindActionCount; ++ActionIndex)
	{
		for (int32 SlotZeroBased = 0; SlotZeroBased < KeyBindSlotsPerAction; ++SlotZeroBased)
		{
			const FString ConfigKey = FString::Printf(
				TEXT("%s%d"),
				KeyBindActionConfigNames[ActionIndex],
				SlotZeroBased + 1);

			const FKey& Key = KeyBindings[ActionIndex][SlotZeroBased];
			const FString StoredKeyName = Key.IsValid()
				? Key.GetFName().ToString()
				: FString();

			GConfig->SetString(
				KeyBindingsConfigSection,
				*ConfigKey,
				*StoredKeyName,
				GGameUserSettingsIni);
		}
	}

	GConfig->Flush(false, GGameUserSettingsIni);
	bKeyBindingsDirty = false;
}

void ATotorisPlayerController::ApplyKeyBindingsToGame()
{
	if (UTotorisBlockGeneratorComponent* BlockGenerator = FindBlockGenerator())
	{
		BlockGenerator->ApplyKeyBindings(
			KeyBindings[0],
			KeyBindings[1],
			KeyBindings[2],
			KeyBindings[3],
			KeyBindings[4],
			KeyBindings[5],
			KeyBindings[6],
			KeyBindings[7],
			KeyBindings[8]);
	}
}

void ATotorisPlayerController::MarkKeyBindingsDirtyApplyAndSave()
{
	bKeyBindingsDirty = true;
	ApplyKeyBindingsToGame();
	SaveKeyBindings();
}

bool ATotorisPlayerController::IsValidKeyBindingLocation(
	int32 ActionIndex,
	int32 SlotIndex) const
{
	return ActionIndex >= 0 &&
		ActionIndex < KeyBindActionCount &&
		SlotIndex >= 1 &&
		SlotIndex <= KeyBindSlotsPerAction &&
		KeyBindings[ActionIndex].Num() == KeyBindSlotsPerAction;
}

bool ATotorisPlayerController::AreKeyBindingsValid() const
{
	TSet<FName> SeenKeys;

	for (int32 ActionIndex = 0; ActionIndex < KeyBindActionCount; ++ActionIndex)
	{
		if (KeyBindings[ActionIndex].Num() != KeyBindSlotsPerAction ||
			CountBoundKeys(ActionIndex) < 1)
		{
			return false;
		}

		for (const FKey& Key : KeyBindings[ActionIndex])
		{
			if (!Key.IsValid())
			{
				continue;
			}

			if (!IsKeyAllowedForBinding(Key) || SeenKeys.Contains(Key.GetFName()))
			{
				return false;
			}

			SeenKeys.Add(Key.GetFName());
		}
	}

	return true;
}

int32 ATotorisPlayerController::CountBoundKeys(int32 ActionIndex) const
{
	if (ActionIndex < 0 || ActionIndex >= KeyBindActionCount)
	{
		return 0;
	}

	int32 Count = 0;
	for (const FKey& Key : KeyBindings[ActionIndex])
	{
		if (Key.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

bool ATotorisPlayerController::FindBoundKey(
	const FKey& Key,
	int32& OutActionIndex,
	int32& OutSlotZeroBased) const
{
	OutActionIndex = INDEX_NONE;
	OutSlotZeroBased = INDEX_NONE;

	if (!Key.IsValid())
	{
		return false;
	}

	for (int32 ActionIndex = 0; ActionIndex < KeyBindActionCount; ++ActionIndex)
	{
		for (int32 SlotZeroBased = 0; SlotZeroBased < KeyBindSlotsPerAction; ++SlotZeroBased)
		{
			if (KeyBindings[ActionIndex][SlotZeroBased] == Key)
			{
				OutActionIndex = ActionIndex;
				OutSlotZeroBased = SlotZeroBased;
				return true;
			}
		}
	}

	return false;
}

FText ATotorisPlayerController::MakeKeyDisplayText(const FKey& Key) const
{
	if (!Key.IsValid())
	{
		return FText::FromString(TEXT("Empty"));
	}

	if (Key == EKeys::Left) return FText::FromString(TEXT("←"));
	if (Key == EKeys::Right) return FText::FromString(TEXT("→"));
	if (Key == EKeys::Down) return FText::FromString(TEXT("↓"));
	if (Key == EKeys::Up) return FText::FromString(TEXT("↑"));
	if (Key == EKeys::SpaceBar) return FText::FromString(TEXT("SPACE"));
	if (Key == EKeys::LeftControl) return FText::FromString(TEXT("LCTRL"));
	if (Key == EKeys::RightControl) return FText::FromString(TEXT("RCTRL"));
	if (Key == EKeys::LeftShift) return FText::FromString(TEXT("LSHIFT"));
	if (Key == EKeys::RightShift) return FText::FromString(TEXT("RSHIFT"));
	if (Key == EKeys::LeftAlt) return FText::FromString(TEXT("LALT"));
	if (Key == EKeys::RightAlt) return FText::FromString(TEXT("RALT"));

	return Key.GetDisplayName(false);
}

void ATotorisPlayerController::LoadHandlingSettings()
{
	HandlingARRMilliseconds = DefaultARRMilliseconds;
	HandlingDASMilliseconds = DefaultDASMilliseconds;
	HandlingDCDMilliseconds = DefaultDCDMilliseconds;
	HandlingSDFMultiplier = DefaultSDFMultiplier;
	bHandlingSDFInfinite = false;

	if (GConfig)
	{
		GConfig->GetInt(
			HandlingConfigSection,
			ARRKey,
			HandlingARRMilliseconds,
			GGameUserSettingsIni);

		GConfig->GetInt(
			HandlingConfigSection,
			DASKey,
			HandlingDASMilliseconds,
			GGameUserSettingsIni);

		GConfig->GetInt(
			HandlingConfigSection,
			DCDKey,
			HandlingDCDMilliseconds,
			GGameUserSettingsIni);

		GConfig->GetInt(
			HandlingConfigSection,
			SDFKey,
			HandlingSDFMultiplier,
			GGameUserSettingsIni);

		GConfig->GetBool(
			HandlingConfigSection,
			SDFInfiniteKey,
			bHandlingSDFInfinite,
			GGameUserSettingsIni);
	}

	HandlingARRMilliseconds =
		FMath::Clamp(HandlingARRMilliseconds, 0, MaxARRMilliseconds);

	HandlingDASMilliseconds =
		FMath::Clamp(
			HandlingDASMilliseconds,
			MinDASMilliseconds,
			MaxDASMilliseconds);

	HandlingDCDMilliseconds =
		FMath::Clamp(HandlingDCDMilliseconds, 0, MaxDCDMilliseconds);

	HandlingSDFMultiplier =
		FMath::Clamp(
			HandlingSDFMultiplier,
			MinSDFMultiplier,
			MaxSDFMultiplier);

	bHandlingSettingsDirty = false;
	ApplyHandlingSettingsToGame();
}

void ATotorisPlayerController::SaveHandlingSettings()
{
	if (!bHandlingSettingsDirty || !GConfig)
	{
		return;
	}

	GConfig->SetInt(
		HandlingConfigSection,
		ARRKey,
		HandlingARRMilliseconds,
		GGameUserSettingsIni);

	GConfig->SetInt(
		HandlingConfigSection,
		DASKey,
		HandlingDASMilliseconds,
		GGameUserSettingsIni);

	GConfig->SetInt(
		HandlingConfigSection,
		DCDKey,
		HandlingDCDMilliseconds,
		GGameUserSettingsIni);

	GConfig->SetInt(
		HandlingConfigSection,
		SDFKey,
		HandlingSDFMultiplier,
		GGameUserSettingsIni);

	GConfig->SetBool(
		HandlingConfigSection,
		SDFInfiniteKey,
		bHandlingSDFInfinite,
		GGameUserSettingsIni);

	GConfig->Flush(false, GGameUserSettingsIni);
	bHandlingSettingsDirty = false;
}

void ATotorisPlayerController::ApplyHandlingSettingsToGame()
{
	if (UTotorisBlockGeneratorComponent* BlockGenerator = FindBlockGenerator())
	{
		BlockGenerator->ApplyHandlingSettings(
			HandlingARRMilliseconds,
			HandlingDASMilliseconds,
			HandlingDCDMilliseconds,
			HandlingSDFMultiplier,
			bHandlingSDFInfinite);
	}
}

void ATotorisPlayerController::MarkHandlingDirtyAndApply()
{
	bHandlingSettingsDirty = true;
	ApplyHandlingSettingsToGame();
}

UTotorisBlockGeneratorComponent* ATotorisPlayerController::FindBlockGenerator() const
{
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(
		this,
		AActor::StaticClass(),
		Actors);

	for (AActor* Actor : Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		if (UTotorisBlockGeneratorComponent* Component =
			Actor->FindComponentByClass<UTotorisBlockGeneratorComponent>())
		{
			return Component;
		}
	}

	return nullptr;
}

void ATotorisPlayerController::SetTaggedGameplayVisualsVisible(bool bVisible)
{
	// Whole actors can be tagged when they contain gameplay presentation only.
	TArray<AActor*> TaggedActors;
	UGameplayStatics::GetAllActorsWithTag(
		this,
		GameplayVisualTag,
		TaggedActors);

	for (AActor* Actor : TaggedActors)
	{
		if (IsValid(Actor))
		{
			Actor->SetActorHiddenInGame(!bVisible);
		}
	}

	// Component tags are also supported so a board/grid component can be hidden
	// without hiding an actor that also owns unrelated room geometry.
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(
		this,
		AActor::StaticClass(),
		AllActors);

	for (AActor* Actor : AllActors)
	{
		if (!IsValid(Actor) || Actor->ActorHasTag(GameplayVisualTag))
		{
			continue;
		}

		TArray<UPrimitiveComponent*> PrimitiveComponents;
		Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

		for (UPrimitiveComponent* Component : PrimitiveComponents)
		{
			if (IsValid(Component) && Component->ComponentHasTag(GameplayVisualTag))
			{
				Component->SetVisibility(bVisible, true);
				Component->SetHiddenInGame(!bVisible, true);
			}
		}
	}
}

void ATotorisPlayerController::EnterMenuInputMode()
{
	bShowMouseCursor = true;

	// Keep UI as the first recipient, but allow unhandled developer/test keys
	// (such as Totori's J/K/L jump checks) to reach PlayerController input.
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void ATotorisPlayerController::EnterGameInputMode()
{
	bShowMouseCursor = false;

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
}
