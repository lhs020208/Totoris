#include "TotorisMenuManager.h"
#include "TotorisAudioMix.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TotorisPlayerController.h"
#include "TotorisMenuSoundBinding.h"

void UTotorisMenuManager::Initialize(ATotorisPlayerController* InOwnerController)
{
	OwnerController = InOwnerController;
	TrueClickSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Totoris/Audio/UI/ClickSound_True.ClickSound_True"));
	FalseClickSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Totoris/Audio/UI/ClickSound_False.ClickSound_False"));

	if (!TrueClickSound || !FalseClickSound)
	{
		UE_LOG(LogTemp, Warning, TEXT("Totoris UI: one or more click sounds could not be loaded."));
	}
}

void UTotorisMenuManager::PlayClickSound(USoundBase* Sound) const
{
	if (Sound && OwnerController.IsValid())
	{
		const float VolumeMultiplier = Sound == TrueClickSound
			? TotorisAudioMix::UiClickTrue
			: Sound == FalseClickSound ? TotorisAudioMix::UiClickFalse : 1.0f;
		UGameplayStatics::PlaySound2D(
			OwnerController.Get(),
			Sound,
			VolumeMultiplier * TotorisAudioMix::MasterVolume);
	}
}

void UTotorisMenuManager::PlayTrueClickSound()
{
	PlayClickSound(TrueClickSound);
}

void UTotorisMenuManager::PlayFalseClickSound()
{
	PlayClickSound(FalseClickSound);
}

void UTotorisMenuManager::PlayModeSetupCheckBoxSound(const bool bIsChecked)
{
	PlayClickSound(bIsChecked ? TrueClickSound : FalseClickSound);
}

void UTotorisMenuManager::PlayTrueCheckBoxSound(bool)
{
	PlayClickSound(TrueClickSound);
}

void UTotorisMenuManager::PlaySliderChangeSound()
{
	PlayClickSound(TrueClickSound);
}

void UTotorisMenuManager::HandleButtonSoundEvent(const FString&, const FString&, const TCHAR*, const bool bUseFalseSound)
{
	PlayClickSound(bUseFalseSound ? FalseClickSound : TrueClickSound);
}

void UTotorisMenuManager::HandleCheckBoxSoundEvent(const FString&, const FString&, const bool bIsChecked, const bool bUseStateSound)
{
	const bool bUseFalseSound = bUseStateSound && !bIsChecked;
	PlayClickSound(bUseFalseSound ? FalseClickSound : TrueClickSound);
}

void UTotorisMenuManager::HandleSliderSoundEvent(const FString&, const FString&)
{
	PlayClickSound(TrueClickSound);
}

bool UTotorisMenuManager::ShowMainMenu()
{
	return ShowWidgetByName(TEXT("WBP_MainMenu"));
}

bool UTotorisMenuManager::ShowModeSelect()
{
	return ShowWidgetByName(TEXT("WBP_ModeSelect"));
}

bool UTotorisMenuManager::ShowModeSetup()
{
	return ShowWidgetByName(TEXT("WBP_ModeSetup"), true);
}

bool UTotorisMenuManager::ShowMissionList()
{
	return ShowWidgetByName(TEXT("WBP_MissionList"), true);
}

bool UTotorisMenuManager::ShowSettings()
{
	return ShowWidgetByName(TEXT("WBP_Settings"));
}

void UTotorisMenuManager::HideCurrentMenu()
{
	if (IsValid(CurrentWidget))
	{
		CurrentWidget->RemoveFromParent();
	}

	CurrentWidget = nullptr;
	CurrentMenuWidgetName = NAME_None;
}

void UTotorisMenuManager::SetMenusHidden(bool bHidden)
{
	bMenusHidden = bHidden;
	if (bMenusHidden)
	{
		DeferredMenuWidgetName = CurrentMenuWidgetName;
		HideCurrentMenu();
	}
}

bool UTotorisMenuManager::RestoreDeferredMenu()
{
	if (bMenusHidden || DeferredMenuWidgetName.IsNone())
	{
		return false;
	}

	const FString WidgetName = DeferredMenuWidgetName.ToString();
	DeferredMenuWidgetName = NAME_None;
	if (WidgetName == TEXT("WBP_ModeSetup"))
	{
		return ShowModeSetup();
	}
	if (WidgetName == TEXT("WBP_MissionList"))
	{
		return ShowMissionList();
	}
	return ShowWidgetByName(*WidgetName);
}

TSubclassOf<UUserWidget> UTotorisMenuManager::LoadWidgetClass(const TCHAR* WidgetName) const
{
	// Codex created the widgets under /Game/Totoris/UI. Prefer a Widgets
	// subfolder, but also accept the UI root so the code is resilient to
	// either layout.
	const TArray<FString> CandidatePaths =
	{
		FString::Printf(
			TEXT("/Game/Totoris/UI/Widgets/%s.%s_C"),
			WidgetName,
			WidgetName),
		FString::Printf(
			TEXT("/Game/Totoris/UI/%s.%s_C"),
			WidgetName,
			WidgetName)
	};

	for (const FString& Path : CandidatePaths)
	{
		if (UClass* LoadedClass = LoadClass<UUserWidget>(nullptr, *Path))
		{
			return LoadedClass;
		}
	}

	UE_LOG(
		LogTemp,
		Error,
		TEXT("Totoris UI: could not load widget class '%s'. Checked /Game/Totoris/UI/Widgets and /Game/Totoris/UI."),
		WidgetName);

	return nullptr;
}

bool UTotorisMenuManager::ShowWidgetByName(const TCHAR* WidgetName, bool bPreserveInstance)
{
	if (!OwnerController.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Totoris UI: menu manager has no valid player controller"));
		return false;
	}

	if (bMenusHidden)
	{
		DeferredMenuWidgetName = FName(WidgetName);
		return false;
	}

	const FName WidgetNameAsName(WidgetName);
	bool bCreatedWidget = false;
	UUserWidget* WidgetToShow = bPreserveInstance
		? PreservedMenuWidgets.FindRef(WidgetNameAsName)
		: nullptr;
	if (!IsValid(WidgetToShow))
	{
		const TSubclassOf<UUserWidget> WidgetClass = LoadWidgetClass(WidgetName);
		if (!WidgetClass)
		{
			return false;
		}

		WidgetToShow = CreateWidget<UUserWidget>(OwnerController.Get(), WidgetClass);
		if (!IsValid(WidgetToShow))
		{
			UE_LOG(LogTemp, Error, TEXT("Totoris UI: failed to create widget '%s'"), WidgetName);
			return false;
		}
		bCreatedWidget = true;
		if (bPreserveInstance)
		{
			PreservedMenuWidgets.Add(WidgetNameAsName, WidgetToShow);
		}
	}

	HideCurrentMenu();
	CurrentWidget = WidgetToShow;

	CurrentWidget->AddToViewport(100);
	CurrentMenuWidgetName = FName(WidgetName);
	if (FCString::Strcmp(WidgetName, TEXT("WBP_Settings")) == 0)
	{
		InitializeSettingsSoundControls(CurrentWidget);
	}

	// Widget construction performs initial slider synchronization. Bind only
	// after that work completes so those programmatic value changes do not
	// masquerade as player interactions or play UI sounds.
	if (bCreatedWidget)
	{
		InitializeMissionListNavigation(CurrentWidget, WidgetName);
		TSet<UWidget*> BoundWidgets;
		BindClickSounds(CurrentWidget, WidgetName, BoundWidgets);
	}
	return true;
}

void UTotorisMenuManager::InitializeMissionListNavigation(
	UUserWidget* Widget, const FString& MenuWidgetName)
{
	if (!IsValid(Widget))
	{
		return;
	}

	if (MenuWidgetName == TEXT("WBP_ModeSetup"))
	{
		if (UButton* MissionListButton = Cast<UButton>(Widget->GetWidgetFromName(TEXT("MissionListButton"))))
		{
			MissionListButton->OnClicked.AddDynamic(this, &UTotorisMenuManager::HandleMissionListRequested);
		}
	}
	else if (MenuWidgetName == TEXT("WBP_MissionList"))
	{
		if (UButton* BackButton = Cast<UButton>(Widget->GetWidgetFromName(TEXT("BackButton"))))
		{
			BackButton->OnClicked.AddDynamic(this, &UTotorisMenuManager::HandleMissionListBackRequested);
		}
	}
}

void UTotorisMenuManager::HandleMissionListRequested()
{
	if (ShowMissionList() && OwnerController.IsValid())
	{
		OwnerController->EnterMenuInputMode();
	}
}

void UTotorisMenuManager::HandleMissionListBackRequested()
{
	if (OwnerController.IsValid())
	{
		OwnerController->ShowModeSetup(OwnerController->GetSelectedGameSetupMode());
	}
}

void UTotorisMenuManager::InitializeSettingsSoundControls(UUserWidget* Widget)
{
	if (!IsValid(Widget))
	{
		return;
	}

	if (UButton* SoundTab = Cast<UButton>(Widget->GetWidgetFromName(TEXT("SoundTab"))))
	{
		SoundTab->OnClicked.AddDynamic(this, &UTotorisMenuManager::ShowSoundSettings);
	}

	if (USlider* VolumeSlider = Cast<USlider>(Widget->GetWidgetFromName(TEXT("VolumeSlider"))))
	{
		// This widget is authored as a 0..100 slider, with one tick per percent.
		VolumeSlider->SetStepSize(1.0f);
		VolumeSlider->SetValue(FMath::Clamp(TotorisAudioMix::MasterVolume * 50.0f, 0.0f, 100.0f));
		VolumeSlider->OnValueChanged.AddDynamic(this, &UTotorisMenuManager::SetMasterVolumeFromSlider);
	}

	if (UCheckBox* SitoModeCheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(TEXT("SitoModeCheckBox"))))
	{
		SitoModeCheckBox->SetIsChecked(TotorisAudioMix::bSitoMode);
		SitoModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::SetSitoMode);
	}

	if (UButton* RestoreButton = Cast<UButton>(Widget->GetWidgetFromName(TEXT("RestoreButton"))))
	{
		RestoreButton->OnClicked.AddDynamic(this, &UTotorisMenuManager::RestoreSoundDefaults);
	}

	SetMasterVolumeFromSlider(TotorisAudioMix::MasterVolume * 50.0f);
}

void UTotorisMenuManager::ShowSoundSettings()
{
	if (!IsValid(CurrentWidget))
	{
		return;
	}

	if (UWidgetSwitcher* SettingsSwitcher = Cast<UWidgetSwitcher>(CurrentWidget->GetWidgetFromName(TEXT("SettingsSwitcher"))))
	{
		if (UWidget* SoundPanel = CurrentWidget->GetWidgetFromName(TEXT("SoundPanel")))
		{
			SettingsSwitcher->SetActiveWidget(SoundPanel);
		}
	}
}

void UTotorisMenuManager::SetMasterVolumeFromSlider(float SliderValue)
{
	const float VolumePercent = FMath::Clamp(SliderValue, 0.0f, 100.0f);
	TotorisAudioMix::MasterVolume = VolumePercent / 50.0f;

	if (!IsValid(CurrentWidget))
	{
		return;
	}

	if (UTextBlock* VolumeValueText = Cast<UTextBlock>(CurrentWidget->GetWidgetFromName(TEXT("VolumeValueText"))))
	{
		VolumeValueText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(VolumePercent))));
	}
}

void UTotorisMenuManager::SetSitoMode(bool bIsChecked)
{
	TotorisAudioMix::bSitoMode = bIsChecked;
}

void UTotorisMenuManager::RestoreSoundDefaults()
{
	if (!IsValid(CurrentWidget))
	{
		return;
	}

	const UWidgetSwitcher* SettingsSwitcher = Cast<UWidgetSwitcher>(CurrentWidget->GetWidgetFromName(TEXT("SettingsSwitcher")));
	if (!SettingsSwitcher || SettingsSwitcher->GetActiveWidget() != CurrentWidget->GetWidgetFromName(TEXT("SoundPanel")))
	{
		return;
	}

	TotorisAudioMix::MasterVolume = TotorisAudioMix::DefaultMasterVolume;
	if (USlider* VolumeSlider = Cast<USlider>(CurrentWidget->GetWidgetFromName(TEXT("VolumeSlider"))))
	{
		VolumeSlider->SetValue(50.0f);
	}
	if (UCheckBox* SitoModeCheckBox = Cast<UCheckBox>(CurrentWidget->GetWidgetFromName(TEXT("SitoModeCheckBox"))))
	{
		SitoModeCheckBox->SetIsChecked(false);
	}
	SetSitoMode(false);
	SetMasterVolumeFromSlider(50.0f);
}

void UTotorisMenuManager::BindClickSounds(
	UUserWidget* Widget,
	const FString& MenuWidgetName,
	TSet<UWidget*>& BoundWidgets)
{
	if (!IsValid(Widget) || !Widget->WidgetTree)
	{
		return;
	}

	TArray<UWidget*> Widgets;
	Widget->WidgetTree->GetAllWidgets(Widgets);

	for (UWidget* Candidate : Widgets)
	{
		if (!IsValid(Candidate) || BoundWidgets.Contains(Candidate))
		{
			continue;
		}

		BoundWidgets.Add(Candidate);

		if (UButton* Button = Cast<UButton>(Candidate))
		{
			// Slate has distinct Pressed (mouse-down) and Clicked (mouse-up)
			// sounds.  The menu manager is the single source of click audio,
			// so clear both before binding our mouse-down handler.
			FButtonStyle ButtonStyle = Button->GetStyle();
			ButtonStyle.PressedSlateSound = FSlateSound();
			ButtonStyle.ClickedSlateSound = FSlateSound();
			Button->SetStyle(ButtonStyle);

			const FString ButtonName = Button->GetName();
			const bool bUseFalseSound =
				(MenuWidgetName == TEXT("WBP_MainMenu") && ButtonName == TEXT("QuitButton")) ||
				((MenuWidgetName == TEXT("WBP_Settings") || MenuWidgetName == TEXT("WBP_ModeSelect") || MenuWidgetName == TEXT("WBP_ModeSetup")) && ButtonName == TEXT("BackButton"));

			UTotorisMenuSoundBinding* Binding = NewObject<UTotorisMenuSoundBinding>(this);
			Binding->Initialize(this, MenuWidgetName, ButtonName, bUseFalseSound, false);
			SoundBindings.Add(Binding);
			Button->OnPressed.AddDynamic(Binding, &UTotorisMenuSoundBinding::HandleButtonPressed);
		}
		else if (UCheckBox* CheckBox = Cast<UCheckBox>(Candidate))
		{
			UTotorisMenuSoundBinding* Binding = NewObject<UTotorisMenuSoundBinding>(this);
			Binding->Initialize(this, MenuWidgetName, CheckBox->GetName(), false, MenuWidgetName == TEXT("WBP_ModeSetup"));
			SoundBindings.Add(Binding);
			CheckBox->OnCheckStateChanged.AddDynamic(Binding, &UTotorisMenuSoundBinding::HandleCheckStateChanged);
		}
		else if (USlider* Slider = Cast<USlider>(Candidate))
		{
			UTotorisMenuSoundBinding* Binding = NewObject<UTotorisMenuSoundBinding>(this);
			Binding->Initialize(this, MenuWidgetName, Slider->GetName(), false, false);
			SoundBindings.Add(Binding);
			Slider->OnMouseCaptureBegin.AddDynamic(Binding, &UTotorisMenuSoundBinding::HandleSliderMouseCaptureBegin);
		}

		if (UUserWidget* NestedWidget = Cast<UUserWidget>(Candidate))
		{
			BindClickSounds(NestedWidget, MenuWidgetName, BoundWidgets);
		}
	}
}
