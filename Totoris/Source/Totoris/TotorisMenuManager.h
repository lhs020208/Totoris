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
	void PlayClickSound(USoundBase* Sound) const;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentWidget;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> TrueClickSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> FalseClickSound;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTotorisMenuSoundBinding>> SoundBindings;

	TWeakObjectPtr<ATotorisPlayerController> OwnerController;
};
