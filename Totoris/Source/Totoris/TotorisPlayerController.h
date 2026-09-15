#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TotorisPlayerController.generated.h"

class UTotorisBlockGeneratorComponent;
class UTotorisMenuManager;

UCLASS()
class TOTORIS_API ATotorisPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATotorisPlayerController();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category="Totoris|UI")
	void ShowMainMenu();

	UFUNCTION(BlueprintCallable, Category="Totoris|UI")
	void ShowModeSelect();

	UFUNCTION(BlueprintCallable, Category="Totoris|UI")
	void ShowSettings();

	UFUNCTION(BlueprintCallable, Category="Totoris|Gameplay")
	void StartClassicGame();

	UFUNCTION(BlueprintCallable, Category="Totoris|Gameplay")
	void StopClassicGame();

	UFUNCTION(BlueprintCallable, Category="Totoris|UI")
	void QuitGame();

	// Handling sliders use normalized 0..1 values, left = slower and
	// right = faster. The returned value is the snapped normalized value.
	UFUNCTION(BlueprintCallable, Category="Totoris|Handling")
	float SetARRSliderValue(float SliderValue);

	UFUNCTION(BlueprintCallable, Category="Totoris|Handling")
	float SetDASSliderValue(float SliderValue);

	UFUNCTION(BlueprintCallable, Category="Totoris|Handling")
	float SetDCDSliderValue(float SliderValue);

	UFUNCTION(BlueprintCallable, Category="Totoris|Handling")
	float SetSDFSliderValue(float SliderValue);

	UFUNCTION(BlueprintPure, Category="Totoris|Handling")
	float GetARRSliderValue() const;

	UFUNCTION(BlueprintPure, Category="Totoris|Handling")
	float GetDASSliderValue() const;

	UFUNCTION(BlueprintPure, Category="Totoris|Handling")
	float GetDCDSliderValue() const;

	UFUNCTION(BlueprintPure, Category="Totoris|Handling")
	float GetSDFSliderValue() const;

	UFUNCTION(BlueprintPure, Category="Totoris|Handling")
	FText GetARRDisplayText() const;

	UFUNCTION(BlueprintPure, Category="Totoris|Handling")
	FText GetDASDisplayText() const;

	UFUNCTION(BlueprintPure, Category="Totoris|Handling")
	FText GetDCDDisplayText() const;

	UFUNCTION(BlueprintPure, Category="Totoris|Handling")
	FText GetSDFDisplayText() const;

	UFUNCTION(BlueprintCallable, Category="Totoris|Handling")
	void ResetHandlingDefaults();

protected:
	// Actors carrying this tag are treated as board/HOLD/NEXT presentation
	// actors and are hidden while a menu is on screen. Do not apply this tag
	// to room/background actors.
	UPROPERTY(EditDefaultsOnly, Category="Totoris|UI")
	FName GameplayVisualTag = TEXT("TotorisGameplayVisual");

private:
	UTotorisBlockGeneratorComponent* FindBlockGenerator() const;
	void SetTaggedGameplayVisualsVisible(bool bVisible);
	void EnterMenuInputMode();
	void EnterGameInputMode();

	void LoadHandlingSettings();
	void SaveHandlingSettings();
	void ApplyHandlingSettingsToGame();
	void MarkHandlingDirtyAndApply();

	UPROPERTY(Transient)
	TObjectPtr<UTotorisMenuManager> MenuManager;

	int32 HandlingARRMilliseconds = 33;
	int32 HandlingDASMilliseconds = 167;
	int32 HandlingDCDMilliseconds = 17;
	int32 HandlingSDFMultiplier = 6;
	bool bHandlingSDFInfinite = false;
	bool bHandlingSettingsDirty = false;

	static constexpr int32 DefaultARRMilliseconds = 33;
	static constexpr int32 DefaultDASMilliseconds = 167;
	static constexpr int32 DefaultDCDMilliseconds = 17;
	static constexpr int32 DefaultSDFMultiplier = 6;

	static constexpr int32 MaxARRMilliseconds = 83;
	static constexpr int32 MinDASMilliseconds = 17;
	static constexpr int32 MaxDASMilliseconds = 333;
	static constexpr int32 MaxDCDMilliseconds = 333;

	static constexpr int32 MinSDFMultiplier = 5;
	static constexpr int32 MaxSDFMultiplier = 40;
	static constexpr int32 SDFSliderStepCount = 36; // 5X..40X plus inf.
};
