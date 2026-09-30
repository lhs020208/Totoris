#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TotorisMenuSoundBinding.generated.h"

class UTotorisMenuManager;

UCLASS()
class TOTORIS_API UTotorisMenuSoundBinding : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UTotorisMenuManager* InMenuManager, const FString& InMenuName, const FString& InControlName, bool bInFalseButton, bool bInModeSetupCheckBox);

	UFUNCTION()
	void HandleButtonPressed();

	UFUNCTION()
	void HandleCheckStateChanged(bool bIsChecked);

	UFUNCTION()
	void HandleSliderMouseCaptureBegin();

private:
	TWeakObjectPtr<UTotorisMenuManager> MenuManager;
	FString MenuName;
	FString ControlName;
	bool bFalseButton = false;
	bool bModeSetupCheckBox = false;
};
