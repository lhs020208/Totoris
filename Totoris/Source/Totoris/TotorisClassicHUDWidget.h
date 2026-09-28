#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TotorisClassicHUDWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UTextBlock;
class UImage;
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
    static FString FormatScore(int64 Score);

    UPROPERTY(Transient)
    TObjectPtr<UTotorisBlockGeneratorComponent> ObservedGame;

    UPROPERTY(Transient)
    TObjectPtr<UCanvasPanel> RootPanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> InputsLabel;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> InputsValue;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> InputsRate;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> PiecesRate;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> FinesseLabel;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> FinesseValue;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> FaultsValue;

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
    TObjectPtr<UTextBlock> ScoreValue;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UImage>> GarbageWarningRows;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SpinActionText;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ClearActionText;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> BackToBackText;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ComboText;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ModeLabel;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ModeValue;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CountdownText;
    double FinishElapsedSeconds = -1.0;
    int32 LastObservedPlacedPieceCount = -1;
    int32 DisplayedBackToBackCount = 0;
    double SpinActionElapsedSeconds = -1.0;
    double ClearActionElapsedSeconds = -1.0;
    double ComboElapsedSeconds = -1.0;
    double BackToBackPopElapsedSeconds = -1.0;
    FLinearColor SpinActionColor = FLinearColor::White;
};
