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
	bool ShowSettings();
	void HideCurrentMenu();
	void SetMenusHidden(bool bHidden);
	bool RestoreDeferredMenu();

	UUserWidget* GetCurrentWidget() const { return CurrentWidget; }

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

	bool ShowWidgetByName(const TCHAR* WidgetName, bool bPreserveInstance = false);
	TSubclassOf<UUserWidget> LoadWidgetClass(const TCHAR* WidgetName) const;
	void BindClickSounds(UUserWidget* Widget, const FString& MenuWidgetName, TSet<UWidget*>& BoundWidgets);
	void InitializeSettingsSoundControls(UUserWidget* Widget);
	void InitializeMissionListNavigation(UUserWidget* Widget, const FString& MenuWidgetName);
	void RefreshMissionList();
	void SelectMissionListTier(ETotorisMissionTier Tier);

	UFUNCTION()
	void HandleMissionListRequested();

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
	void ShowSoundSettings();

	UFUNCTION()
	void SetMasterVolumeFromSlider(float SliderValue);

	UFUNCTION()
	void SetSitoMode(bool bIsChecked);

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
	TArray<TObjectPtr<UTotorisMenuSoundBinding>> SoundBindings;

	TWeakObjectPtr<ATotorisPlayerController> OwnerController;
};
