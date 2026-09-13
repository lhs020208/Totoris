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

	UPROPERTY(Transient)
	TObjectPtr<UTotorisMenuManager> MenuManager;
};
