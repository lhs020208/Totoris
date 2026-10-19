#include "TotorisMenuManager.h"
#include "TotorisAudioMix.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/RichTextBlock.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"
#include "TotorisMinoRichTextDecorator.h"
#include "TotorisPlayerController.h"
#include "TotorisMission.h"
#include "TotorisMenuSoundBinding.h"

namespace
{
	FText MakeMissionListRichText(const FText& Source)
	{
		FString Text = Source.ToString();
		// Each list row has room for two lines. Split long descriptions once at
		// the space nearest its centre, keeping both lines visually balanced.
		if (Text.Len() > 24)
		{
			const int32 Centre = Text.Len() / 2;
			int32 BestBreak = INDEX_NONE;
			for (int32 Index = 0; Index < Text.Len(); ++Index)
			{
				if (!FChar::IsWhitespace(Text[Index])) continue;
				if (BestBreak == INDEX_NONE || FMath::Abs(Index - Centre) < FMath::Abs(BestBreak - Centre))
				{
					BestBreak = Index;
				}
			}
			if (BestBreak != INDEX_NONE)
			{
				Text[BestBreak] = TEXT('\n');
			}
		}

		for (const TCHAR Type : FString(TEXT("IOTSZJL")))
		{
			const FString Token = FString::Printf(TEXT("{%c}"), Type);
			const FString Tag = FString::Printf(TEXT("<mino id=\"%c\"/>"), Type);
			Text.ReplaceInline(*Token, *Tag);
		}
		return FText::FromString(Text);
	}

	const TCHAR* MissionListTabName(const ETotorisMissionTier Tier)
	{
		switch (Tier)
		{
		case ETotorisMissionTier::F: return TEXT("FTab");
		case ETotorisMissionTier::E: return TEXT("ETab");
		case ETotorisMissionTier::D: return TEXT("DTab");
		case ETotorisMissionTier::C: return TEXT("CTab");
		case ETotorisMissionTier::B: return TEXT("BTab");
		case ETotorisMissionTier::A: return TEXT("ATab");
		default: return TEXT("");
		}
	}

	bool IsGarbageRelatedMission(const FTotorisMissionDefinition& Mission)
	{
		const FString Id = Mission.Id.ToString();
		// Attack-generation missions also require the garbage-attack game rule.
		return Id.Contains(TEXT("Garbage")) || Id.Contains(TEXT("Send_"));
	}
}

void UTotorisMenuManager::Initialize(ATotorisPlayerController* InOwnerController)
{
	OwnerController = InOwnerController;
	MissionListTier = ETotorisMissionTier::F;
	TrueClickSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Totoris/Audio/UI/ClickSound_True.ClickSound_True"));
	FalseClickSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Totoris/Audio/UI/ClickSound_False.ClickSound_False"));
	MissionWarningSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Totoris/Audio/Gameplay/WarningSound.WarningSound"));

	if (!TrueClickSound || !FalseClickSound || !MissionWarningSound)
	{
		UE_LOG(LogTemp, Warning, TEXT("Totoris UI: one or more click sounds could not be loaded."));
	}
}

bool UTotorisMenuManager::IsMissionListMissionEnabled(const FName MissionId) const
{
	const bool* bEnabled = MissionListMissionEnabled.Find(MissionId);
	return bEnabled == nullptr || *bEnabled;
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
	const bool bShown = ShowWidgetByName(TEXT("WBP_MissionList"), true);
	if (bShown)
	{
		RefreshMissionList();
	}
	return bShown;
}

bool UTotorisMenuManager::ShowSettings(int32 ZOrder)
{
	return ShowWidgetByName(TEXT("WBP_Settings"), false, ZOrder);
}

bool UTotorisMenuManager::ShowSCList()
{
	// The authored ten rows and their independent selections survive screen swaps.
	return ShowWidgetByName(TEXT("WBP_SCList"), true);
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
	if (WidgetName == TEXT("WBP_SCList"))
	{
		return ShowSCList();
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

bool UTotorisMenuManager::ShowWidgetByName(const TCHAR* WidgetName, bool bPreserveInstance, int32 ZOrder)
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

	CurrentWidget->AddToViewport(ZOrder);
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

	if (MenuWidgetName == TEXT("WBP_ModeSelect"))
	{
		if (UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(TEXT("ChallengeButton"))))
		{
			Button->OnClicked.AddUniqueDynamic(this, &UTotorisMenuManager::HandleChallengeRequested);
		}
	}
	else if (MenuWidgetName == TEXT("WBP_ModeSetup"))
	{
		if (UButton* SCListButton = Cast<UButton>(Widget->GetWidgetFromName(TEXT("SCListButton"))))
		{
			SCListButton->OnClicked.AddDynamic(this, &UTotorisMenuManager::HandleSCListRequested);
		}
		if (UButton* MissionListButton = Cast<UButton>(Widget->GetWidgetFromName(TEXT("MissionListButton"))))
		{
			MissionListButton->OnClicked.AddDynamic(this, &UTotorisMenuManager::HandleMissionListRequested);
		}
	}
	else if (MenuWidgetName == TEXT("WBP_MissionList") || MenuWidgetName == TEXT("WBP_SCList"))
	{
		if (UButton* BackButton = Cast<UButton>(Widget->GetWidgetFromName(TEXT("BackButton"))))
		{
			BackButton->OnClicked.AddDynamic(this, &UTotorisMenuManager::HandleMissionListBackRequested);
		}

		if (UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(TEXT("FTab"))))
			Button->OnClicked.AddDynamic(this, &UTotorisMenuManager::HandleMissionListFTabRequested);
		if (UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(TEXT("ETab"))))
			Button->OnClicked.AddDynamic(this, &UTotorisMenuManager::HandleMissionListETabRequested);
		if (UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(TEXT("DTab"))))
			Button->OnClicked.AddDynamic(this, &UTotorisMenuManager::HandleMissionListDTabRequested);
		if (UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(TEXT("CTab"))))
			Button->OnClicked.AddDynamic(this, &UTotorisMenuManager::HandleMissionListCTabRequested);
		if (UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(TEXT("BTab"))))
			Button->OnClicked.AddDynamic(this, &UTotorisMenuManager::HandleMissionListBTabRequested);
		if (UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(TEXT("ATab"))))
			Button->OnClicked.AddDynamic(this, &UTotorisMenuManager::HandleMissionListATabRequested);
		if (UButton* LeftButton = Cast<UButton>(Widget->GetWidgetFromName(TEXT("LeftButton"))))
		{
			LeftButton->OnClicked.AddDynamic(this, &UTotorisMenuManager::HandleMissionListPreviousPageRequested);
		}
		if (UButton* RightButton = Cast<UButton>(Widget->GetWidgetFromName(TEXT("RightButton"))))
		{
			RightButton->OnClicked.AddDynamic(this, &UTotorisMenuManager::HandleMissionListNextPageRequested);
		}
		if (UButton* RestoreButton = Cast<UButton>(Widget->GetWidgetFromName(TEXT("RestoreButton"))))
		{
			RestoreButton->OnClicked.AddDynamic(this, &UTotorisMenuManager::HandleMissionListRestoreRequested);
		}
		// SC check states live in its preserved widget, not in the normal mission pool.
		if (MenuWidgetName == TEXT("WBP_SCList")) return;

		if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(TEXT("FCB1")))) CheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::HandleMissionListEntry1Changed);
		if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(TEXT("FCB2")))) CheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::HandleMissionListEntry2Changed);
		if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(TEXT("FCB3")))) CheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::HandleMissionListEntry3Changed);
		if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(TEXT("FCB4")))) CheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::HandleMissionListEntry4Changed);
		if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(TEXT("FCB5")))) CheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::HandleMissionListEntry5Changed);
		if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(TEXT("FCB6")))) CheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::HandleMissionListEntry6Changed);
		if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(TEXT("FCB7")))) CheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::HandleMissionListEntry7Changed);
		if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(TEXT("FCB8")))) CheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::HandleMissionListEntry8Changed);
		if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(TEXT("FCB9")))) CheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::HandleMissionListEntry9Changed);
		if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(TEXT("FCB10")))) CheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::HandleMissionListEntry10Changed);
		if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(TEXT("FCB11")))) CheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::HandleMissionListEntry11Changed);
		if (UCheckBox* CheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(TEXT("FCB12")))) CheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::HandleMissionListEntry12Changed);
	}
}

void UTotorisMenuManager::HandleMissionListRequested()
{
	if (ShowMissionList() && OwnerController.IsValid())
	{
		OwnerController->EnterMenuInputMode();
	}
}

void UTotorisMenuManager::HandleSCListRequested()
{
	if (ShowSCList() && OwnerController.IsValid())
	{
		OwnerController->EnterMenuInputMode();
	}
}

void UTotorisMenuManager::HandleChallengeRequested()
{
	if (OwnerController.IsValid())
	{
		OwnerController->ShowModeSetup(ETotorisGameSetupMode::SChallenge);
	}
}

void UTotorisMenuManager::HandleMissionListBackRequested()
{
	FString TierList;
	if (CurrentMenuWidgetName == TEXT("WBP_SCList"))
	{
		int32 EnabledCount = 0;
		for (int32 Index = 1; Index <= 10; ++Index)
		{
			if (UCheckBox* CheckBox = Cast<UCheckBox>(CurrentWidget->GetWidgetFromName(
				FName(*FString::Printf(TEXT("FCB%d"), Index)))))
			{
				EnabledCount += CheckBox->IsChecked() ? 1 : 0;
			}
		}
		if (EnabledCount == 0) TierList = TEXT("S");
	}
	else
	{
		TierList = GetMissionTierValidationList();
	}
	if (!TierList.IsEmpty())
	{
		ShowMissionWarning(TierList);
		return;
	}

	if (OwnerController.IsValid())
	{
		OwnerController->ShowModeSetup(OwnerController->GetSelectedGameSetupMode());
	}
}

void UTotorisMenuManager::SelectMissionListTier(ETotorisMissionTier Tier)
{
	MissionListTier = Tier;
	MissionListPage = 1;
	RefreshMissionList();
}

void UTotorisMenuManager::HandleMissionListFTabRequested() { SelectMissionListTier(ETotorisMissionTier::F); }
void UTotorisMenuManager::HandleMissionListETabRequested() { SelectMissionListTier(ETotorisMissionTier::E); }
void UTotorisMenuManager::HandleMissionListDTabRequested() { SelectMissionListTier(ETotorisMissionTier::D); }
void UTotorisMenuManager::HandleMissionListCTabRequested() { SelectMissionListTier(ETotorisMissionTier::C); }
void UTotorisMenuManager::HandleMissionListBTabRequested() { SelectMissionListTier(ETotorisMissionTier::B); }
void UTotorisMenuManager::HandleMissionListATabRequested() { SelectMissionListTier(ETotorisMissionTier::A); }

void UTotorisMenuManager::HandleMissionListPreviousPageRequested()
{
	const int32 MaxPage = MissionListTier == ETotorisMissionTier::A ? 1 : 2;
	MissionListPage = FMath::Clamp(MissionListPage - 1, 1, MaxPage);
	RefreshMissionList();
}

void UTotorisMenuManager::HandleMissionListNextPageRequested()
{
	const int32 MaxPage = MissionListTier == ETotorisMissionTier::A ? 1 : 2;
	MissionListPage = FMath::Clamp(MissionListPage + 1, 1, MaxPage);
	RefreshMissionList();
}

bool UTotorisMenuManager::IsMissionListEntryAvailable(const FTotorisMissionDefinition& Mission) const
{
	return OwnerController.IsValid()
		&& OwnerController->IsMissionTierEnabled(Mission.Tier)
		&& (OwnerController->GetGarbageAttackEnabled() || !IsGarbageRelatedMission(Mission));
}

void UTotorisMenuManager::SetMissionListEntryEnabled(const int32 VisibleIndex, const bool bEnabled)
{
	const TArray<FTotorisMissionDefinition>& Missions = TotorisMissions::ForTier(MissionListTier);
	const int32 MissionIndex = (MissionListPage - 1) * 12 + VisibleIndex;
	if (!Missions.IsValidIndex(MissionIndex))
	{
		return;
	}

	const FTotorisMissionDefinition& Mission = Missions[MissionIndex];
	MissionListMissionEnabled.Add(Mission.Id, IsMissionListEntryAvailable(Mission) && bEnabled);
}

void UTotorisMenuManager::RestoreMissionListDefaults()
{
	if (CurrentMenuWidgetName == TEXT("WBP_SCList") && IsValid(CurrentWidget))
	{
		for (int32 Index = 1; Index <= 10; ++Index)
		{
			if (UCheckBox* CheckBox = Cast<UCheckBox>(CurrentWidget->GetWidgetFromName(
				FName(*FString::Printf(TEXT("FCB%d"), Index)))))
			{
				CheckBox->SetIsChecked(true);
			}
		}
		return;
	}
	for (const FTotorisMissionDefinition& Mission : TotorisMissions::All())
	{
		MissionListMissionEnabled.Add(Mission.Id, IsMissionListEntryAvailable(Mission));
	}
	RefreshMissionList();
}

void UTotorisMenuManager::HandleMissionListRestoreRequested() { RestoreMissionListDefaults(); }
void UTotorisMenuManager::HandleMissionWarningDismissRequested() { HideMissionWarning(); }
void UTotorisMenuManager::HandleMissionListEntry1Changed(bool bChecked) { SetMissionListEntryEnabled(0, bChecked); }
void UTotorisMenuManager::HandleMissionListEntry2Changed(bool bChecked) { SetMissionListEntryEnabled(1, bChecked); }
void UTotorisMenuManager::HandleMissionListEntry3Changed(bool bChecked) { SetMissionListEntryEnabled(2, bChecked); }
void UTotorisMenuManager::HandleMissionListEntry4Changed(bool bChecked) { SetMissionListEntryEnabled(3, bChecked); }
void UTotorisMenuManager::HandleMissionListEntry5Changed(bool bChecked) { SetMissionListEntryEnabled(4, bChecked); }
void UTotorisMenuManager::HandleMissionListEntry6Changed(bool bChecked) { SetMissionListEntryEnabled(5, bChecked); }
void UTotorisMenuManager::HandleMissionListEntry7Changed(bool bChecked) { SetMissionListEntryEnabled(6, bChecked); }
void UTotorisMenuManager::HandleMissionListEntry8Changed(bool bChecked) { SetMissionListEntryEnabled(7, bChecked); }
void UTotorisMenuManager::HandleMissionListEntry9Changed(bool bChecked) { SetMissionListEntryEnabled(8, bChecked); }
void UTotorisMenuManager::HandleMissionListEntry10Changed(bool bChecked) { SetMissionListEntryEnabled(9, bChecked); }
void UTotorisMenuManager::HandleMissionListEntry11Changed(bool bChecked) { SetMissionListEntryEnabled(10, bChecked); }
void UTotorisMenuManager::HandleMissionListEntry12Changed(bool bChecked) { SetMissionListEntryEnabled(11, bChecked); }

FString UTotorisMenuManager::GetMissionTierValidationList() const
{
	if (!OwnerController.IsValid())
	{
		return FString();
	}

	const TCHAR* TierLabels[] = { TEXT("F"), TEXT("E"), TEXT("D"), TEXT("C"), TEXT("B"), TEXT("A") };
	TArray<FString> InvalidTiers;
	for (int32 TierIndex = 0; TierIndex < UE_ARRAY_COUNT(TierLabels); ++TierIndex)
	{
		const ETotorisMissionTier Tier = static_cast<ETotorisMissionTier>(TierIndex);
		if (!OwnerController->IsMissionTierEnabled(Tier))
		{
			continue;
		}

		int32 EnabledMissionCount = 0;
		for (const FTotorisMissionDefinition& Mission : TotorisMissions::ForTier(Tier))
		{
			if (!IsMissionListEntryAvailable(Mission))
			{
				continue;
			}
			const bool bEnabled = MissionListMissionEnabled.Contains(Mission.Id)
				? MissionListMissionEnabled.FindRef(Mission.Id) : true;
			EnabledMissionCount += bEnabled ? 1 : 0;
		}

		// Three enabled entries is the allowed minimum; warn only at two or less.
		if (EnabledMissionCount <= 2)
		{
			InvalidTiers.Add(TierLabels[TierIndex]);
		}
	}

	return FString::Join(InvalidTiers, TEXT(", "));
}

void UTotorisMenuManager::ShowMissionWarning(const FString& TierList)
{
	if (!OwnerController.IsValid())
	{
		return;
	}

	if (!IsValid(MissionWarningWidget))
	{
		const TSubclassOf<UUserWidget> WarningClass = LoadWidgetClass(TEXT("WBP_MissionWarning"));
		if (!WarningClass)
		{
			return;
		}

		MissionWarningWidget = CreateWidget<UUserWidget>(OwnerController.Get(), WarningClass);
		if (!IsValid(MissionWarningWidget))
		{
			return;
		}

		if (UButton* DismissButton = Cast<UButton>(MissionWarningWidget->GetWidgetFromName(TEXT("DismissButton"))))
		{
			DismissButton->SetBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.f));
			DismissButton->OnClicked.AddDynamic(this, &UTotorisMenuManager::HandleMissionWarningDismissRequested);
		}
	}

	const bool bWasAlreadyVisible = MissionWarningWidget->IsInViewport();
	if (UTextBlock* MainText = Cast<UTextBlock>(MissionWarningWidget->GetWidgetFromName(TEXT("MainText"))))
	{
		TArray<FString> InvalidTiers;
		TierList.ParseIntoArray(InvalidTiers, TEXT(", "), true);
		const bool bNeedsTwoLines = InvalidTiers.Num() >= 3;
		MainText->SetJustification(ETextJustify::Center);
		const FString WarningText = TierList == TEXT("S")
			? TEXT("S 티어 미션을 최소 1개 활성화 해주세요")
			: bNeedsTwoLines
			? FString::Printf(TEXT("%s 티어 미션을\n최소 3개 활성화 해주세요."), *TierList)
			: FString::Printf(TEXT("%s 티어 미션을 최소 3개 활성화 해주세요."), *TierList);
		MainText->SetText(FText::FromString(WarningText));
	}

	// The full-screen dismiss button sits behind the message visuals.  Make the
	// visual overlay pass pointer input through so any click still reaches it.
	if (UBorder* RebindOverlay = Cast<UBorder>(MissionWarningWidget->GetWidgetFromName(TEXT("RebindOverlay"))))
	{
		RebindOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	MissionWarningWidget->AddToViewport(200);
	if (!bWasAlreadyVisible && MissionWarningSound)
	{
		UGameplayStatics::PlaySound2D(OwnerController.Get(), MissionWarningSound,
			TotorisAudioMix::MissionWarning * TotorisAudioMix::MasterVolume);
	}
}

void UTotorisMenuManager::HideMissionWarning()
{
	if (IsValid(MissionWarningWidget))
	{
		MissionWarningWidget->RemoveFromParent();
	}
}

void UTotorisMenuManager::RefreshMissionList()
{
	const TObjectPtr<UUserWidget>* WidgetEntry = PreservedMenuWidgets.Find(TEXT("WBP_MissionList"));
	UUserWidget* Widget = WidgetEntry ? WidgetEntry->Get() : nullptr;
	if (!IsValid(Widget))
	{
		return;
	}

	const TArray<FTotorisMissionDefinition>& Missions = TotorisMissions::ForTier(MissionListTier);
	const int32 MaxPage = MissionListTier == ETotorisMissionTier::A ? 1 : 2;
	MissionListPage = FMath::Clamp(MissionListPage, 1, MaxPage);
	const int32 FirstMissionIndex = (MissionListPage - 1) * 12;
	for (int32 TextIndex = 0; TextIndex < 12; ++TextIndex)
	{
		const FName TextName(*FString::Printf(TEXT("FText%d"), TextIndex + 1));
		const int32 MissionIndex = FirstMissionIndex + TextIndex;
		const bool bHasMission = Missions.IsValidIndex(MissionIndex);
		const FTotorisMissionDefinition* Mission = bHasMission ? &Missions[MissionIndex] : nullptr;
		const bool bAvailable = Mission && IsMissionListEntryAvailable(*Mission);
		if (Mission && !MissionListMissionEnabled.Contains(Mission->Id))
		{
			// Mission-list defaults are enabled. Rule-disabled entries override
			// that default immediately and remain explicitly unchecked.
			MissionListMissionEnabled.Add(Mission->Id, bAvailable);
		}
		else if (Mission && !bAvailable)
		{
			MissionListMissionEnabled.Add(Mission->Id, false);
		}
		const bool bChecked = Mission && bAvailable && MissionListMissionEnabled.FindRef(Mission->Id);
		if (URichTextBlock* MissionText = Cast<URichTextBlock>(Widget->GetWidgetFromName(TextName)))
		{
			const int32 CharacterCount = bHasMission ? Missions[MissionIndex].DisplayText.ToString().Len() : 0;
			// These rows are deliberately only two lines tall. Scale long entries
			// down proportionally instead of allowing a third line to overlap the
			// following mission row.
			const int32 MissionFontSize = FMath::Clamp(
				FMath::RoundToInt(14.f * 24.f / FMath::Max(24, CharacterCount)), 8, 14);
			FTextBlockStyle Style;
			Style.SetFont(FSlateFontInfo(
				FPaths::ProjectContentDir() / TEXT("Slate/Fonts/NotoSansCJKkr-Medium.otf"), MissionFontSize));
			Style.SetColorAndOpacity(FSlateColor(bAvailable
				? FLinearColor::White : FLinearColor(0.32f, 0.35f, 0.42f, 1.0f)));
			MissionText->SetDefaultTextStyle(Style);
			MissionText->SetDecorators({ UTotorisMinoRichTextDecorator::StaticClass() });
			MissionText->SetAutoWrapText(true);
			MissionText->SetWrapTextAt(0.f);
			MissionText->SetVisibility(bHasMission ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			MissionText->SetText(bHasMission ? MakeMissionListRichText(Mission->DisplayText) : FText::GetEmpty());
		}

		const FName CheckBoxName(*FString::Printf(TEXT("FCB%d"), TextIndex + 1));
		if (UCheckBox* MissionCheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(CheckBoxName)))
		{
			MissionCheckBox->SetVisibility(bHasMission ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			MissionCheckBox->SetIsEnabled(bAvailable);
			MissionCheckBox->SetIsChecked(bChecked);
		}
	}

	if (UTextBlock* PageText = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("PageText"))))
	{
		PageText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), MissionListPage, MaxPage)));
	}

	const TCHAR* ActiveTab = MissionListTabName(MissionListTier);
	const TCHAR* TabNames[] = { TEXT("FTab"), TEXT("ETab"), TEXT("DTab"), TEXT("CTab"), TEXT("BTab"), TEXT("ATab") };
	for (const TCHAR* TabName : TabNames)
	{
		if (UButton* Tab = Cast<UButton>(Widget->GetWidgetFromName(TabName)))
		{
			const bool bActive = FCString::Strcmp(TabName, ActiveTab) == 0;
			Tab->SetBackgroundColor(bActive
				? FLinearColor(0.20f, 0.58f, 0.90f, 1.0f)
				: FLinearColor(0.43f, 0.50f, 0.70f, 0.62f));
		}
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

	if (UCheckBox* EventCheckBox = Cast<UCheckBox>(Widget->GetWidgetFromName(TEXT("EventCheckBox"))))
	{
		EventCheckBox->SetIsChecked(TotorisAudioMix::bMissionEventsEnabled);
		EventCheckBox->OnCheckStateChanged.AddDynamic(this, &UTotorisMenuManager::SetMissionEventsEnabled);
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

void UTotorisMenuManager::SetMissionEventsEnabled(bool bIsChecked)
{
	TotorisAudioMix::bMissionEventsEnabled = bIsChecked;
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
	if (UCheckBox* EventCheckBox = Cast<UCheckBox>(CurrentWidget->GetWidgetFromName(TEXT("EventCheckBox"))))
	{
		EventCheckBox->SetIsChecked(true);
	}
	SetSitoMode(false);
	SetMissionEventsEnabled(true);
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
				((MenuWidgetName == TEXT("WBP_Settings") || MenuWidgetName == TEXT("WBP_ModeSelect") ||
					MenuWidgetName == TEXT("WBP_ModeSetup") || MenuWidgetName == TEXT("WBP_MissionList") ||
					MenuWidgetName == TEXT("WBP_SCList")) &&
					ButtonName == TEXT("BackButton"));

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
