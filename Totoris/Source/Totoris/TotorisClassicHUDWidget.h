#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TotorisClassicHUDWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UTextBlock;
class UTotorisBlockGeneratorComponent;

// A fully native, non-interactive overlay. No WBP_HUD asset or Blueprint binding required.
// It projects the existing 3D board into the local player's viewport every frame,
// so the labels track the board even when the viewport size/camera changes.
UCLASS()
class TOTORIS_API UTotorisClassicHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetObservedGame(UTotorisBlockGeneratorComponent* InGame);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UTextBlock* AddText(const TCHAR* Name, int32 FontSize, bool bEmphasized);
    static void Place(UTextBlock* Text, const FVector2D& Position,
        const FVector2D& Size, const FVector2D& Alignment);
    void UpdateFontSize(UTextBlock* Text, int32 Size);

    UPROPERTY(Transient)
    TObjectPtr<UTotorisBlockGeneratorComponent> ObservedGame;

    UPROPERTY(Transient)
    TObjectPtr<UCanvasPanel> RootPanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> PiecesLabel;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> PiecesValue;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> LinesLabel;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> LinesValue;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TimeLabel;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TimeValue;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ModeLabel;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ModeValue;
};
