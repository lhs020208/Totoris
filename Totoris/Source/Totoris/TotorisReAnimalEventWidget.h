#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TotorisReAnimalEventWidget.generated.h"

class UImage;
class UTexture2D;

UCLASS()
class TOTORIS_API UTotorisReAnimalEventWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void StartEvent();
	bool IsEventPlaying() const { return bPlaying; }

	UPROPERTY(EditAnywhere, Category="ReAnimal Event", meta=(ClampMin="1.0"))
	float DurationSeconds = 12.f;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void LoadTextures();
	void HidePresentation();
	void UpdateWater(const FVector2D& View, float Surface, float Wave);
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> WaterTextures;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> WaterImages;
	UPROPERTY(Transient) TObjectPtr<UImage> BoatImage;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> BoatTexture;
	float ElapsedSeconds = 0.f;
	bool bPlaying = false;
};
