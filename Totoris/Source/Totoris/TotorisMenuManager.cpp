#include "TotorisMenuManager.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/Widget.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TotorisPlayerController.h"

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
		UGameplayStatics::PlaySound2D(OwnerController.Get(), Sound);
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

void UTotorisMenuManager::PlaySliderChangeSound(float)
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
	return ShowWidgetByName(TEXT("WBP_ModeSetup"));
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

bool UTotorisMenuManager::ShowWidgetByName(const TCHAR* WidgetName)
{
	if (!OwnerController.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Totoris UI: menu manager has no valid player controller"));
		return false;
	}

	const TSubclassOf<UUserWidget> WidgetClass = LoadWidgetClass(WidgetName);
	if (!WidgetClass)
	{
		return false;
	}

	HideCurrentMenu();

	CurrentWidget = CreateWidget<UUserWidget>(
		OwnerController.Get(),
		WidgetClass);

	if (!IsValid(CurrentWidget))
	{
		UE_LOG(LogTemp, Error, TEXT("Totoris UI: failed to create widget '%s'"), WidgetName);
		return false;
	}

	TSet<UWidget*> BoundWidgets;
	BindClickSounds(CurrentWidget, WidgetName, BoundWidgets);
	CurrentWidget->AddToViewport(100);
	return true;
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

			if (bUseFalseSound)
			{
				Button->OnPressed.AddDynamic(this, &UTotorisMenuManager::PlayFalseClickSound);
			}
			else
			{
				Button->OnPressed.AddDynamic(this, &UTotorisMenuManager::PlayTrueClickSound);
			}
		}
		else if (UCheckBox* CheckBox = Cast<UCheckBox>(Candidate))
		{
			if (MenuWidgetName == TEXT("WBP_ModeSetup"))
			{
				CheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::PlayModeSetupCheckBoxSound);
			}
			else
			{
				CheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::PlayTrueCheckBoxSound);
			}
		}
		else if (USlider* Slider = Cast<USlider>(Candidate))
		{
			Slider->OnValueChanged.AddDynamic(this, &UTotorisMenuManager::PlaySliderChangeSound);
		}

		if (UUserWidget* NestedWidget = Cast<UUserWidget>(Candidate))
		{
			BindClickSounds(NestedWidget, MenuWidgetName, BoundWidgets);
		}
	}
}
