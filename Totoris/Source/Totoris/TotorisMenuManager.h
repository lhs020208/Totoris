#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
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
	void PlaySliderChangeSound(float Value);

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

	TWeakObjectPtr<ATotorisPlayerController> OwnerController;
};
