#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TotorisMenuSoundBinding.h"
#include "TotorisMenuManager.generated.h"

class ATotorisPlayerController;
class USoundBase;
class UUserWidget;
class UWidget;

UCLASS()
class TOTORIS_API UTotorisMenuManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ATotorisPlayerController* InOwnerController);

	bool ShowMainMenu();
	bool ShowModeSelect();
	bool ShowModeSetup();
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

	bool ShowWidgetByName(const TCHAR* WidgetName);
	TSubclassOf<UUserWidget> LoadWidgetClass(const TCHAR* WidgetName) const;
	void BindClickSounds(UUserWidget* Widget, const FString& MenuWidgetName, TSet<UWidget*>& BoundWidgets);
	void InitializeSettingsSoundControls(UUserWidget* Widget);

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
