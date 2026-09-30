#include "TotorisMenuSoundBinding.h"

#include "TotorisMenuManager.h"

void UTotorisMenuSoundBinding::Initialize(UTotorisMenuManager* InMenuManager, const FString& InMenuName, const FString& InControlName, const bool bInFalseButton, const bool bInModeSetupCheckBox)
{
	MenuManager = InMenuManager;
	MenuName = InMenuName;
	ControlName = InControlName;
	bFalseButton = bInFalseButton;
	bModeSetupCheckBox = bInModeSetupCheckBox;
}

void UTotorisMenuSoundBinding::HandleButtonPressed()
{
	if (UTotorisMenuManager* Manager = MenuManager.Get())
	{
		Manager->HandleButtonSoundEvent(MenuName, ControlName, TEXT("OnPressed (LButtonDown)"), bFalseButton);
	}
}

void UTotorisMenuSoundBinding::HandleCheckStateChanged(const bool bIsChecked)
{
	if (UTotorisMenuManager* Manager = MenuManager.Get())
	{
		Manager->HandleCheckBoxSoundEvent(MenuName, ControlName, bIsChecked, bModeSetupCheckBox);
	}
}

void UTotorisMenuSoundBinding::HandleSliderMouseCaptureBegin()
{
	if (UTotorisMenuManager* Manager = MenuManager.Get())
	{
		Manager->HandleSliderSoundEvent(MenuName, ControlName);
	}
}
