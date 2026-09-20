#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "TotorisPlayerController.generated.h"

class UTotorisBlockGeneratorComponent;
class UTotorisMenuManager;

UENUM(BlueprintType)
enum class ETotorisGameSetupMode : uint8
{
	Classic UMETA(DisplayName="Classic"),
	Mission UMETA(DisplayName="Mission"),
	SChallenge UMETA(DisplayName="S Challenge")
};

USTRUCT(BlueprintType)
struct TOTORIS_API FTotorisCommonGameSetupSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Totoris|GameSetup")
	bool bGarbageAttack = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Totoris|GameSetup", meta=(ClampMin="1", ClampMax="10"))
	int32 GarbageDifficulty = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Totoris|GameSetup")
	bool bGarbageDifficultyIncrease = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Totoris|GameSetup")
	bool bCheeseGarbage = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Totoris|GameSetup")
	bool bStartGravity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Totoris|GameSetup")
	bool bGravityIncrease = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Totoris|GameSetup")
	bool bQuickStart = false;
};

UENUM(BlueprintType)
enum class ETotorisKeyBindResult : uint8
{
	Success UMETA(DisplayName="Success"),
	MovedFromOtherAction UMETA(DisplayName="Moved From Other Action"),
	NoChange UMETA(DisplayName="No Change"),
	InvalidActionOrSlot UMETA(DisplayName="Invalid Action Or Slot"),
	KeyNotAllowed UMETA(DisplayName="Key Not Allowed"),
	DuplicateInSameAction UMETA(DisplayName="Duplicate In Same Action"),
	WouldUnbindOtherAction UMETA(DisplayName="Would Unbind Other Action"),
	CannotClearLastKey UMETA(DisplayName="Cannot Clear Last Key")
};

UCLASS()
class TOTORIS_API ATotorisPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATotorisPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="Totoris|UI")
	void ShowMainMenu();

	UFUNCTION(BlueprintCallable, Category="Totoris|UI")
	void ShowModeSelect();

	UFUNCTION(BlueprintCallable, Category="Totoris|UI")
	void ShowSettings();

	UFUNCTION(BlueprintCallable, Category="Totoris|UI")
	void ShowModeSetup(ETotorisGameSetupMode Mode);

	UFUNCTION(BlueprintPure, Category="Totoris|GameSetup")
	ETotorisGameSetupMode GetSelectedGameSetupMode() const;

	UFUNCTION(BlueprintCallable, Category="Totoris|GameSetup")
	void SetCommonGameSetupSettings(const FTotorisCommonGameSetupSettings& Settings);

	UFUNCTION(BlueprintPure, Category="Totoris|GameSetup")
	FTotorisCommonGameSetupSettings GetCommonGameSetupSettings() const;

	// Blueprint-friendly accessors for the Mode Setup widget. These update the
	// pending setup only; gameplay systems consume the values when a game is
	// started. Disabled UI options intentionally retain their stored values.
	UFUNCTION(BlueprintCallable, Category="Totoris|GameSetup|Common")
	void SetGarbageAttackEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Totoris|GameSetup|Common")
	bool GetGarbageAttackEnabled() const;

	UFUNCTION(BlueprintCallable, Category="Totoris|GameSetup|Common")
	int32 SetGarbageDifficulty(int32 Difficulty);

	UFUNCTION(BlueprintPure, Category="Totoris|GameSetup|Common")
	int32 GetGarbageDifficulty() const;

	UFUNCTION(BlueprintCallable, Category="Totoris|GameSetup|Common")
	void SetGarbageDifficultyIncreaseEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Totoris|GameSetup|Common")
	bool GetGarbageDifficultyIncreaseEnabled() const;

	UFUNCTION(BlueprintCallable, Category="Totoris|GameSetup|Common")
	void SetCheeseGarbageEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Totoris|GameSetup|Common")
	bool GetCheeseGarbageEnabled() const;

	UFUNCTION(BlueprintCallable, Category="Totoris|GameSetup|Common")
	void SetStartGravityEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Totoris|GameSetup|Common")
	bool GetStartGravityEnabled() const;

	UFUNCTION(BlueprintCallable, Category="Totoris|GameSetup|Common")
	void SetGravityIncreaseEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Totoris|GameSetup|Common")
	bool GetGravityIncreaseEnabled() const;

	UFUNCTION(BlueprintCallable, Category="Totoris|GameSetup|Common")
	void SetQuickStartEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Totoris|GameSetup|Common")
	bool GetQuickStartEnabled() const;

	UFUNCTION(BlueprintCallable, Category="Totoris|GameSetup|Common")
	void ResetCommonGameSetupSettings();

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

	// ActionIndex matches the Blueprint EKeyBindAction order:
	// 0 MoveLeft, 1 MoveRight, 2 SoftDrop, 3 HardDrop,
	// 4 RotateCW, 5 RotateCCW, 6 Rotate180, 7 Hold.
	// SlotIndex is 1..3 to match the Settings UI slot numbering.
	UFUNCTION(BlueprintPure, Category="Totoris|KeyBindings")
	FKey GetKeyBinding(uint8 ActionIndex, int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category="Totoris|KeyBindings")
	FText GetKeyBindingDisplayText(uint8 ActionIndex, int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category="Totoris|KeyBindings")
	bool IsKeyAllowedForBinding(FKey Key) const;

	UFUNCTION(BlueprintCallable, Category="Totoris|KeyBindings")
	ETotorisKeyBindResult SetKeyBinding(uint8 ActionIndex, int32 SlotIndex, FKey NewKey);

	UFUNCTION(BlueprintCallable, Category="Totoris|KeyBindings")
	ETotorisKeyBindResult ClearKeyBinding(uint8 ActionIndex, int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category="Totoris|KeyBindings")
	void ResetKeyBindingsToDefaults();

	// UI helpers for interpreting SetKeyBinding/ClearKeyBinding results.
	// Success, MovedFromOtherAction and NoChange are treated as completed
	// operations; the remaining values are failures that should keep a
	// rebind prompt open and display the returned feedback text.
	UFUNCTION(BlueprintPure, Category="Totoris|KeyBindings")
	bool IsKeyBindResultSuccessful(ETotorisKeyBindResult Result) const;

	UFUNCTION(BlueprintPure, Category="Totoris|KeyBindings")
	FText GetKeyBindResultFeedbackText(ETotorisKeyBindResult Result) const;

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

	void InitializeDefaultKeyBindings();
	void LoadKeyBindings();
	void SaveKeyBindings(bool bForce = false);
	void ApplyKeyBindingsToGame();
	void MarkKeyBindingsDirtyApplyAndSave();
	bool IsValidKeyBindingLocation(int32 ActionIndex, int32 SlotIndex) const;
	bool AreKeyBindingsValid() const;
	int32 CountBoundKeys(int32 ActionIndex) const;
	bool FindBoundKey(const FKey& Key, int32& OutActionIndex, int32& OutSlotZeroBased) const;
	FText MakeKeyDisplayText(const FKey& Key) const;

	UPROPERTY(Transient)
	TObjectPtr<UTotorisMenuManager> MenuManager;

	ETotorisGameSetupMode SelectedGameSetupMode = ETotorisGameSetupMode::Classic;
	FTotorisCommonGameSetupSettings CommonGameSetupSettings;

	int32 HandlingARRMilliseconds = 33;
	int32 HandlingDASMilliseconds = 167;
	int32 HandlingDCDMilliseconds = 17;
	int32 HandlingSDFMultiplier = 6;
	bool bHandlingSDFInfinite = false;
	bool bHandlingSettingsDirty = false;

	static constexpr int32 KeyBindActionCount = 8;
	static constexpr int32 KeyBindSlotsPerAction = 3;
	TArray<FKey> KeyBindings[KeyBindActionCount];
	bool bKeyBindingsDirty = false;

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
