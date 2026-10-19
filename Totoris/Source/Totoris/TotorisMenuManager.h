#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TotorisMenuSoundBinding.h"
#include "TotorisMenuManager.generated.h"

class ATotorisPlayerController;
class USoundBase;
class UUserWidget;
class UWidget;
enum class ETotorisMissionTier : uint8;

UCLASS()
class TOTORIS_API UTotorisMenuManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ATotorisPlayerController* InOwnerController);

	bool ShowMainMenu();
	bool ShowModeSelect();
	bool ShowModeSetup();
	bool ShowMissionList();
	bool ShowSCList();
	bool ShowSettings(int32 ZOrder = 100);
	void HideCurrentMenu();
	void SetMenusHidden(bool bHidden);
	bool RestoreDeferredMenu();

	UUserWidget* GetCurrentWidget() const { return CurrentWidget; }
	// Mission generation consults this persisted list selection. Entries never
	// visited in the UI retain the enabled-by-default behavior.
	bool IsMissionListMissionEnabled(FName MissionId) const;

	void HandleButtonSoundEvent(const FString& MenuName, const FString& ControlName, const TCHAR* EventName, bool bUseFalseSound);
	void HandleCheckBoxSoundEvent(const FString& MenuName, const FString& ControlName, bool bIsChecked, bool bUseStateSound);
	void HandleSliderSoundEvent(const FString& MenuName, const FString& ControlName);

private:
	UFUNCTION()
	void PlayTrueClickSound();

	UFUNCTION()
	void PlayFalseClickSound();

	UFUNCTION()
	void PlayModeSetupCheckBoxSound(bool bIsChecked);

	UFUNCTION()
	void PlayTrueCheckBoxSound(bool bIsChecked);

	UFUNCTION()
	void PlaySliderChangeSound();

	bool ShowWidgetByName(const TCHAR* WidgetName, bool bPreserveInstance = false, int32 ZOrder = 100);
	TSubclassOf<UUserWidget> LoadWidgetClass(const TCHAR* WidgetName) const;
	void BindClickSounds(UUserWidget* Widget, const FString& MenuWidgetName, TSet<UWidget*>& BoundWidgets);
	void InitializeSettingsSoundControls(UUserWidget* Widget);
	void InitializeMissionListNavigation(UUserWidget* Widget, const FString& MenuWidgetName);
	void RefreshMissionList();
	void SelectMissionListTier(ETotorisMissionTier Tier);
	void SetMissionListEntryEnabled(int32 VisibleIndex, bool bEnabled);
	bool IsMissionListEntryAvailable(const struct FTotorisMissionDefinition& Mission) const;
	void RestoreMissionListDefaults();
	FString GetMissionTierValidationList() const;
	void ShowMissionWarning(const FString& TierList);
	void HideMissionWarning();

	UFUNCTION()
	void HandleMissionListRequested();

	UFUNCTION()
	void HandleSCListRequested();

	UFUNCTION()
	void HandleChallengeRequested();

	UFUNCTION()
	void HandleMissionListBackRequested();

	UFUNCTION()
	void HandleMissionListFTabRequested();
	UFUNCTION()
	void HandleMissionListETabRequested();
	UFUNCTION()
	void HandleMissionListDTabRequested();
	UFUNCTION()
	void HandleMissionListCTabRequested();
	UFUNCTION()
	void HandleMissionListBTabRequested();
	UFUNCTION()
	void HandleMissionListATabRequested();
	UFUNCTION()
	void HandleMissionListPreviousPageRequested();
	UFUNCTION()
	void HandleMissionListNextPageRequested();
	UFUNCTION()
	void HandleMissionListRestoreRequested();
	UFUNCTION()
	void HandleMissionWarningDismissRequested();
	UFUNCTION() void HandleMissionListEntry1Changed(bool bChecked);
	UFUNCTION() void HandleMissionListEntry2Changed(bool bChecked);
	UFUNCTION() void HandleMissionListEntry3Changed(bool bChecked);
	UFUNCTION() void HandleMissionListEntry4Changed(bool bChecked);
	UFUNCTION() void HandleMissionListEntry5Changed(bool bChecked);
	UFUNCTION() void HandleMissionListEntry6Changed(bool bChecked);
	UFUNCTION() void HandleMissionListEntry7Changed(bool bChecked);
	UFUNCTION() void HandleMissionListEntry8Changed(bool bChecked);
	UFUNCTION() void HandleMissionListEntry9Changed(bool bChecked);
	UFUNCTION() void HandleMissionListEntry10Changed(bool bChecked);
	UFUNCTION() void HandleMissionListEntry11Changed(bool bChecked);
	UFUNCTION() void HandleMissionListEntry12Changed(bool bChecked);

	UFUNCTION()
	void ShowSoundSettings();

	UFUNCTION()
	void SetMasterVolumeFromSlider(float SliderValue);

	UFUNCTION()
	void SetSitoMode(bool bIsChecked);

	UFUNCTION()
	void SetMissionEventsEnabled(bool bIsChecked);

	UFUNCTION()
	void RestoreSoundDefaults();

	void PlayClickSound(USoundBase* Sound) const;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentWidget;

	// Mode Setup and Mission List deliberately survive a screen swap. Their
	// controls are authored in UMG, so retaining the same instances preserves
	// every check box, page, and future per-widget selection without copying it.
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UUserWidget>> PreservedMenuWidgets;

	ETotorisMissionTier MissionListTier;
	int32 MissionListPage = 1;
	TMap<FName, bool> MissionListMissionEnabled;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MissionWarningWidget;

	// The Show Widgets overlay suppresses menus without losing which menu the
	// player was viewing, so unchecking it can restore that exact menu.
	bool bMenusHidden = false;
	FName CurrentMenuWidgetName = NAME_None;
	FName DeferredMenuWidgetName = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> TrueClickSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> FalseClickSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> MissionWarningSound;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTotorisMenuSoundBinding>> SoundBindings;

	TWeakObjectPtr<ATotorisPlayerController> OwnerController;
};
