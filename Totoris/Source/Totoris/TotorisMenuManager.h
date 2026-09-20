#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TotorisMenuManager.generated.h"

class ATotorisPlayerController;
class UUserWidget;

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
	bool ShowWidgetByName(const TCHAR* WidgetName);
	TSubclassOf<UUserWidget> LoadWidgetClass(const TCHAR* WidgetName) const;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentWidget;

	TWeakObjectPtr<ATotorisPlayerController> OwnerController;
};
