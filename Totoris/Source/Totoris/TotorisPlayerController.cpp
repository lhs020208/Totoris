#include "TotorisPlayerController.h"

#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "TotorisBlockGeneratorComponent.h"
#include "TotorisClassicHUDWidget.h"
#include "TotorisMenuManager.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"


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
}

ATotorisPlayerController::ATotorisPlayerController()
{
	bShowMouseCursor = true;
}

void ATotorisPlayerController::BeginPlay()
{
	Super::BeginPlay();

	MenuManager = NewObject<UTotorisMenuManager>(this);
	if (!ensure(MenuManager))
	{
		return;
	}

	MenuManager->Initialize(this);
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
	PendingResultSummary = Summary;
	GameEndDelaySeconds = 0.f;
}

void ATotorisPlayerController::ShowGameEndWidget()
{
	if (!IsLocalController()) return;
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
	ShowGameEndOverview();
}

void ATotorisPlayerController::ShowGameEndOverview()
{
	if (UWidgetSwitcher* Switcher = IsValid(GameEndWidget) ? Cast<UWidgetSwitcher>(GameEndWidget->GetWidgetFromName(TEXT("StatsSwitcher"))) : nullptr) Switcher->SetActiveWidgetIndex(0);
}

void ATotorisPlayerController::ShowGameEndFull()
{
	if (UWidgetSwitcher* Switcher = IsValid(GameEndWidget) ? Cast<UWidgetSwitcher>(GameEndWidget->GetWidgetFromName(TEXT("StatsSwitcher"))) : nullptr) Switcher->SetActiveWidgetIndex(1);
}

void ATotorisPlayerController::ShowMainMenu()
{
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
	if (MenuManager && MenuManager->ShowModeSelect())
	{
		EnterMenuInputMode();
	}
}

void ATotorisPlayerController::ShowSettings()
{
	if (MenuManager && MenuManager->ShowSettings())
	{
		EnterMenuInputMode();
	}
}

void ATotorisPlayerController::ShowModeSetup(ETotorisGameSetupMode Mode)
{
	SelectedGameSetupMode = Mode;

	if (MenuManager && MenuManager->ShowModeSetup())
	{
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
}

void ATotorisPlayerController::SetClassicMode(ETotorisClassicMode Mode)
{
    ClassicSettings.Mode = Mode;
    // Set only on selection; changing a checkbox afterwards overrides the preset.
    CommonGameSetupSettings.bGarbageAttack = Mode == ETotorisClassicMode::Endless;
    CommonGameSetupSettings.bGarbageDifficultyIncrease = Mode == ETotorisClassicMode::Endless;
    CommonGameSetupSettings.bCheeseGarbage = false;
    CommonGameSetupSettings.bStartGravity = true;
    CommonGameSetupSettings.bGravityIncrease =
        Mode == ETotorisClassicMode::Endless || Mode == ETotorisClassicMode::Blitz;
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
	if (MenuManager)
	{
		MenuManager->HideCurrentMenu();
	}

	SetTaggedGameplayVisualsVisible(true);

	if (UTotorisBlockGeneratorComponent* BlockGenerator = FindBlockGenerator())
	{
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
			CommonGameSetupSettings.bQuickStart);

		BlockGenerator->StartGame();
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

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void ATotorisPlayerController::EnterGameInputMode()
{
	bShowMouseCursor = false;

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
}
