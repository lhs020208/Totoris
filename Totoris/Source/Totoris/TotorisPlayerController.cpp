#include "TotorisPlayerController.h"

#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundBase.h"
#include "Misc/ConfigCacheIni.h"
#include "TotorisBlockGeneratorComponent.h"
#include "TotorisClassicHUDWidget.h"
#include "TotorisAudioMix.h"
#include "TotorisMenuManager.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "UObject/UObjectIterator.h"
#include "TotorisMission.h"


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
		default: return FLinearColor::White;
		}
	}
}

ATotorisPlayerController::ATotorisPlayerController()
{
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

	MenuManager = NewObject<UTotorisMenuManager>(this);
	if (!ensure(MenuManager))
	{
		return;
	}

	MenuManager->Initialize(this);
	CreateShowWidgetsOverlay();
	LoadHandlingSettings();
	LoadKeyBindings();
    SetClassicMode(ETotorisClassicMode::Endless);

	// The room/camera remain untouched. Only gameplay simulation and actors
	// explicitly tagged as gameplay presentation are hidden.
	StopClassicGame();
	ShowMainMenu();
}

void ATotorisPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindMissionGameplayEvents();
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

void ATotorisPlayerController::HandleRunFinished(const FTotorisRunSummary& Summary)
{
	if (!bGameEndEligibleForCurrentRun || !Summary.bValid)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Totoris ignored a result-screen request outside an active run."));
		return;
	}

	PendingResultSummary = Summary;
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
	const bool bEndless = PendingResultSummary.Settings.Mode == ETotorisClassicMode::Endless;
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
	if (UButton* MissionTab = Cast<UButton>(GameEndWidget->GetWidgetFromName(TEXT("MissionTab"))))
	{
		MissionTab->SetIsEnabled(false);
		MissionTab->SetRenderOpacity(.38f);
	}
	GameEndWidget->SetRenderOpacity(0.f);
	GameEndWidget->AddToViewport(30);
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
	ShowWidgetsOverlay->AddToViewport(200);
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
			// WBP_ModeSetup is deliberately preserved while the Mission List is
			// open.  Its Construct event therefore cannot be relied on to select
			// the correct panel when returning to it.
			if (UWidgetSwitcher* ModeSwitcher = Cast<UWidgetSwitcher>(Widget->GetWidgetFromName(TEXT("ModeSpecificSwitcher"))))
			{
				ModeSwitcher->SetActiveWidgetIndex(Mode == ETotorisGameSetupMode::Mission ? 1 : 0);
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
}

bool ATotorisPlayerController::GetQuickStartEnabled() const
{
	return CommonGameSetupSettings.bQuickStart;
}

void ATotorisPlayerController::ResetCommonGameSetupSettings()
{
	CommonGameSetupSettings = FTotorisCommonGameSetupSettings{};
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
			if (!MenuManager || MenuManager->IsMissionListMissionEnabled(Pool[MissionIndex].Id))
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
		const FTotorisMissionDefinition* Definition = TotorisMissions::All().FindByPredicate(
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
	BlockGenerator->SetMissionPanelLineClearChecklist(
		bShowLineClearChecklist,
		(MissionLineClearChecklistMask & 0x01) == 0,
		(MissionLineClearChecklistMask & 0x02) == 0,
		(MissionLineClearChecklistMask & 0x04) == 0);

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
			CurrentId == TEXT("C_Clear_Lines_All_7_Types"))
		{
			RequiredMinoMask = 0x7F;
		}
		else if (CurrentId == TEXT("B_Spin_Clears_3_Pieces"))
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

void ATotorisPlayerController::ForceCompleteMission()
{
	CompleteCurrentMission();
}

void ATotorisPlayerController::CompleteCurrentMission()
{
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
	if (CompletedMissionCount % 3 == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("Mission set completed. Generating next mission set."));
		++MissionDifficulty;
		InitializeMissionRun();
	}
	else
	{
		BeginCurrentMissionTracking();
		RefreshMissionPanel();
	}
}

void ATotorisPlayerController::BeginCurrentMissionTracking()
{
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
	const int32 Slot = ActiveMissions.IndexOfByPredicate(
		[](const FTotorisMissionRuntimeState& Mission) { return !Mission.bCompleted; });
	if (ActiveMissions.IsValidIndex(Slot) && ActiveMissions[Slot].MissionId == TEXT("F_Use_Hold_8_Times"))
	{
		AddCurrentMissionProgress(1);
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
	if (SelectedGameSetupMode != ETotorisGameSetupMode::Mission)
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

void ATotorisPlayerController::BindMissionDebugInput()
{
	if (MissionDebugInput) return;
	MissionDebugInput = NewObject<UInputComponent>(this, TEXT("MissionDebugInput"));
	MissionDebugInput->RegisterComponent();
	MissionDebugInput->Priority = 10000;
	MissionDebugInput->BindKey(EKeys::P, IE_Pressed, this, &ATotorisPlayerController::ForceCompleteMission);
	PushInputComponent(MissionDebugInput);
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
	// Keep the displayed default enabled for the always-on modes.  Only Sprint
	// and Cheese Race consume this setting; Blitz ignores it for its own
	// line/level gravity curve.
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


void ATotorisPlayerController::StartClassicGame()
{
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

		BlockGenerator->ConfigureClassicGame(
			ClassicSettings,
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
		if (SelectedGameSetupMode == ETotorisGameSetupMode::Mission)
		{
			CompletedMissionCount = 0;
			MissionDifficulty = 1;
			InitializeMissionRun();
			BindMissionGameplayEvents();
			BindMissionDebugInput();
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
				ClassicHUD->AddToViewport(20);
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
	bGameEndEligibleForCurrentRun = false;
	UnbindMissionGameplayEvents();
	SetShowWidgetsOverlayEnabled(true);
    if (IsValid(ClassicHUD))
    {
        ClassicHUD->RemoveFromParent();
        ClassicHUD = nullptr;
    }

	if (UTotorisBlockGeneratorComponent* BlockGenerator = FindBlockGenerator())
	{
		BlockGenerator->StopGame();
	}

	SetTaggedGameplayVisualsVisible(false);
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
