#include "TotorisClassicHUDWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "TotorisBlockGeneratorComponent.h"
#include "TotorisGeneration.h"

void UTotorisClassicHUDWidget::SetObservedGame(UTotorisBlockGeneratorComponent* InGame)
{
    ObservedGame = InGame;
}

UTextBlock* UTotorisClassicHUDWidget::AddText(const TCHAR* Name, int32 FontSize, bool bEmphasized)
{
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
    RootPanel->AddChild(Text);
    FSlateFontInfo Font = Text->GetFont();
    Font.Size = FontSize;
    Text->SetFont(Font);
    Text->SetJustification(ETextJustify::Right);
    Text->SetColorAndOpacity(FSlateColor(bEmphasized
        ? FLinearColor(1.f, 1.f, 1.f, .98f)
        : FLinearColor(.76f, .86f, 1.f, .92f)));
    Text->SetShadowOffset(FVector2D(1.f, 2.f));
    Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, .85f));
    return Text;
}

void UTotorisClassicHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ClassicHUDRoot"));
    WidgetTree->RootWidget = RootPanel;
    SetVisibility(ESlateVisibility::HitTestInvisible);

    PiecesLabel = AddText(TEXT("PiecesLabel"), 17, false);
    PiecesLabel->SetText(FText::FromString(TEXT("PIECES")));
    PiecesValue = AddText(TEXT("PiecesValue"), 25, true);
    LinesLabel = AddText(TEXT("LinesLabel"), 17, false);
    LinesLabel->SetText(FText::FromString(TEXT("LINES")));
    LinesValue = AddText(TEXT("LinesValue"), 25, true);
    TimeLabel = AddText(TEXT("TimeLabel"), 17, false);
    TimeLabel->SetText(FText::FromString(TEXT("TIME")));
    TimeValue = AddText(TEXT("TimeValue"), 25, true);

    // #FFFFFF33 over #0C0C0C produces approximately #3D3D3D
    // (alpha 0x33 = 51/255 = 20%). Apply only to the center mode readout;
    // PIECES / LINES / TIME retain their original appearance.
    constexpr float ModeTextAlpha = 0.50f;
    const FSlateColor ModeTextColor(FLinearColor(1.f, 1.f, 1.f, ModeTextAlpha));

    ModeLabel = AddText(TEXT("ModeLabel"), 16, false);
    ModeLabel->SetJustification(ETextJustify::Center);
    ModeLabel->SetColorAndOpacity(ModeTextColor);
    ModeLabel->SetShadowOffset(FVector2D::ZeroVector);
    ModeLabel->SetShadowColorAndOpacity(FLinearColor::Transparent);

    ModeValue = AddText(TEXT("ModeValue"), 72, true);
    ModeValue->SetJustification(ETextJustify::Center);
    ModeValue->SetColorAndOpacity(ModeTextColor);
    ModeValue->SetShadowOffset(FVector2D::ZeroVector);
    ModeValue->SetShadowColorAndOpacity(FLinearColor::Transparent);

    CountdownText = AddText(TEXT("CountdownText"), 76, true);
    CountdownText->SetJustification(ETextJustify::Center);
    CountdownText->SetRenderTransformPivot(FVector2D(.5f, .5f));
    CountdownText->SetShadowOffset(FVector2D(2.f, 3.f));
    CountdownText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, .72f));
    CountdownText->SetVisibility(ESlateVisibility::Collapsed);
}

void UTotorisClassicHUDWidget::Place(UTextBlock* Text, const FVector2D& Position,
    const FVector2D& Size, const FVector2D& Alignment)
{
    if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Text->Slot))
    {
        Slot->SetAutoSize(false);
        Slot->SetAnchors(FAnchors(0.f, 0.f));
        Slot->SetAlignment(Alignment);
        Slot->SetPosition(Position);
        Slot->SetSize(Size);
    }
}

void UTotorisClassicHUDWidget::UpdateFontSize(UTextBlock* Text, int32 Size)
{
    if (Text->GetFont().Size != Size)
    {
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = Size;
        Text->SetFont(Font);
    }
}

void UTotorisClassicHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!IsValid(ObservedGame) || !ObservedGame->IsGameplayActive())
    {
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    APlayerController* PC = GetOwningPlayer();
    AActor* BoardActor = ObservedGame->GetOwner();
    if (!IsValid(PC) || !IsValid(BoardActor)) return;

    // Block rendering is on the owner's local Y/Z plane (front = -X).
    // Use Unreal's widget-space projection rather than fixed 1280x720 offsets.
    const float HalfWidth = .5f * TotorisGeneration::BoardWidth * ObservedGame->CellSize;
    const float HalfHeight = .5f * TotorisGeneration::BoardHeight * ObservedGame->CellSize;
    const FTransform BoardTransform = BoardActor->GetActorTransform();
    FVector2D TopLeft, BottomRight;
    const bool bHasTopLeft = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
        PC, BoardTransform.TransformPosition(FVector(-8.f, -HalfWidth, HalfHeight)), TopLeft, true);
    const bool bHasBottomRight = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
        PC, BoardTransform.TransformPosition(FVector(-8.f, HalfWidth, -HalfHeight)), BottomRight, true);
    if (!bHasTopLeft || !bHasBottomRight)
    {
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    SetVisibility(ESlateVisibility::HitTestInvisible);

    // The camera is usually aimed along the board's local +X. Min/max keeps
    // this stable if a different camera orientation reverses screen axes.
    const float Left = FMath::Min(TopLeft.X, BottomRight.X);
    const float Right = FMath::Max(TopLeft.X, BottomRight.X);
    const float Top = FMath::Min(TopLeft.Y, BottomRight.Y);
    const float Bottom = FMath::Max(TopLeft.Y, BottomRight.Y);
    const float Width = Right - Left;
    const float Height = Bottom - Top;
    if (Width < 10.f || Height < 10.f) return;

    const float Gap = FMath::Clamp(Width * .05f, 9.f, 24.f);
    const float SideWidth = FMath::Clamp(Width * .65f, 120.f, 215.f);
    const float SideX = Left - Gap;
    const int32 SmallFont = FMath::Clamp(FMath::RoundToInt(Width * .060f), 12, 19);
    const int32 BigFont = FMath::Clamp(FMath::RoundToInt(Width * .087f), 17, 30);
    for (UTextBlock* Label : { PiecesLabel.Get(), LinesLabel.Get(), TimeLabel.Get() })
        UpdateFontSize(Label, SmallFont);
    for (UTextBlock* Value : { PiecesValue.Get(), LinesValue.Get(), TimeValue.Get() })
        UpdateFontSize(Value, BigFont);

    const auto PositionMetric = [&](UTextBlock* Label, UTextBlock* Value, float Fraction)
    {
        const float Y = Top + Height * Fraction;
        Place(Label, FVector2D(SideX, Y), FVector2D(SideWidth, 29.f), FVector2D(1.f, 0.f));
        Place(Value, FVector2D(SideX, Y + 22.f), FVector2D(SideWidth, 43.f), FVector2D(1.f, 0.f));
    };
    PositionMetric(PiecesLabel, PiecesValue, .52f);
    PositionMetric(LinesLabel, LinesValue, .67f);
    PositionMetric(TimeLabel, TimeValue, .82f);

    // Countdown: Ready, then 3 / 2 / 1 / GO for one second each.
    // It rises quickly, fades slowly, and only breathes a few percent in scale.
    const double CountdownElapsed = ObservedGame->GetStartCountdownElapsedSecondsForHUD();
    const bool bQuickStart = ObservedGame->IsQuickStartEnabledForHUD();
    const bool bShowCountdown = CountdownElapsed >= 0.0 && CountdownElapsed < (bQuickStart ? 2.0 : 5.0);
    CountdownText->SetVisibility(bShowCountdown
        ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (bShowCountdown)
    {
        const int32 CountdownStage = bQuickStart ? 3 : FMath::Clamp(
            FMath::FloorToInt(CountdownElapsed) - 1, 0, 3);
        const float Phase = static_cast<float>(CountdownElapsed - FMath::FloorToDouble(CountdownElapsed));
        const float FadeIn = FMath::Clamp(Phase / .12f, 0.f, 1.f);
        const float Alpha = Phase < .12f
            ? FMath::InterpEaseOut(0.f, 1.f, FadeIn, 2.f)
            : FMath::Lerp(1.f, 0.f, (Phase - .12f) / .88f);
        const float Scale = 1.f + .055f * FMath::Sin(PI * FMath::Clamp(Phase / .78f, 0.f, 1.f));
        const bool bIsReady = CountdownElapsed < 1.0;
        const bool bIsGo = CountdownStage == 3;
        CountdownText->SetText(FText::FromString(
            bIsReady ? TEXT("Ready") : (bIsGo ? TEXT("GO") : FString::FromInt(3 - CountdownStage))));
        CountdownText->SetColorAndOpacity(FSlateColor(bIsGo
            ? FLinearColor(1.f, .78f, .22f, Alpha)
            : FLinearColor(.56f, .88f, 1.f, Alpha)));
        CountdownText->SetRenderScale(FVector2D(Scale, Scale));
        UpdateFontSize(CountdownText, FMath::Clamp(FMath::RoundToInt(Width * .28f), 46, 124));
        Place(CountdownText,
            FVector2D((Left + Right) * .5f, Top + Height * .42f),
            FVector2D(Width * .92f, Height * .22f), FVector2D(.5f, .5f));
    }

    PiecesValue->SetText(FText::AsNumber(ObservedGame->GetPlacedPieceCountForHUD()));
    LinesValue->SetText(FText::AsNumber(ObservedGame->GetClearedLineCountForHUD()));
    const int64 Milliseconds = FMath::Max<int64>(0,
        FMath::FloorToInt64(ObservedGame->GetElapsedSecondsForHUD() * 1000.0 + 0.000001));
    const int64 Minutes = Milliseconds / 60000;
    const int64 Seconds = (Milliseconds / 1000) % 60;
    TimeValue->SetText(FText::FromString(FString::Printf(TEXT("%lld:%02lld.%03lld"),
        static_cast<long long>(Minutes), static_cast<long long>(Seconds),
        static_cast<long long>(Milliseconds % 1000))));

    const ETotorisClassicMode Mode = ObservedGame->GetActiveClassicModeForHUD();
    ModeLabel->SetVisibility(Mode == ETotorisClassicMode::Endless
        ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    ModeValue->SetVisibility(Mode == ETotorisClassicMode::Endless
        ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    if (Mode == ETotorisClassicMode::Endless) return;

    const float CenterX = (Left + Right) * .5f;
    const float DisplayWidth = Width * .94f;
    const float DisplayY = Top + Height * .145f;
    UpdateFontSize(ModeLabel, FMath::Clamp(FMath::RoundToInt(Width * .055f), 12, 20));
    UpdateFontSize(ModeValue, FMath::Clamp(FMath::RoundToInt(Width * .32f), 38, 102));
    Place(ModeLabel, FVector2D(CenterX, DisplayY), FVector2D(DisplayWidth, 28.f), FVector2D(.5f, 0.f));
    Place(ModeValue, FVector2D(CenterX, DisplayY + 22.f), FVector2D(DisplayWidth, 125.f), FVector2D(.5f, 0.f));

    switch (Mode)
    {
    case ETotorisClassicMode::Sprint:
        ModeLabel->SetText(FText::FromString(TEXT("LINES LEFT")));
        ModeValue->SetText(FText::AsNumber(ObservedGame->GetRemainingSprintLinesForHUD()));
        break;
    case ETotorisClassicMode::Blitz:
    {
        ModeLabel->SetText(FText::FromString(TEXT("TIME LEFT")));
        // Ceil ensures that the timer starts at the full configured second
        // and reaches 0 exactly when the existing Blitz timer expires.
        const int32 SecondsLeft = FMath::Max(0,
            FMath::CeilToInt(ObservedGame->GetRemainingBlitzSecondsForHUD() - 1e-9));
        ModeValue->SetText(FText::FromString(FString::Printf(TEXT("%d:%02d"),
            SecondsLeft / 60, SecondsLeft % 60)));
        break;
    }
    case ETotorisClassicMode::CheeseRace:
        ModeLabel->SetText(FText::FromString(TEXT("CHEESE LEFT")));
        ModeValue->SetText(FText::AsNumber(ObservedGame->GetRemainingCheeseLinesForHUD()));
        break;
    default:
        break;
    }
}
