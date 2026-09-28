#include "TotorisClassicHUDWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
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

    // A single, board-clipped visual layer for arrived virtual garbage. Rows
    // are constructed once and merely repositioned/tinted as the viewport moves.
    for (int32 Row = 0; Row < TotorisGeneration::BoardHeight; ++Row)
    {
        UImage* WarningRow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
            FName(*FString::Printf(TEXT("GarbageWarningRow_%d"), Row)));
        RootPanel->AddChild(WarningRow);
        WarningRow->SetVisibility(ESlateVisibility::Collapsed);
        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(WarningRow->Slot))
        {
            CanvasSlot->SetZOrder(-20);
        }
        GarbageWarningRows.Add(WarningRow);
    }

    InputsLabel = AddText(TEXT("InputsLabel"), 17, false);
    InputsLabel->SetText(FText::FromString(TEXT("INPUTS")));
    InputsValue = AddText(TEXT("InputsValue"), 25, true);
    InputsRate = AddText(TEXT("InputsRate"), 17, true);
    PiecesRate = AddText(TEXT("PiecesRate"), 17, true);
    FinesseLabel = AddText(TEXT("FinesseLabel"), 17, false);
    FinesseLabel->SetText(FText::FromString(TEXT("FINESSE")));
    FinesseValue = AddText(TEXT("FinesseValue"), 25, true);
    FaultsValue = AddText(TEXT("FaultsValue"), 17, true);
    for (UTextBlock* Text : { FinesseLabel.Get(), FinesseValue.Get(), FaultsValue.Get() })
        Text->SetJustification(ETextJustify::Left);
    PiecesLabel = AddText(TEXT("PiecesLabel"), 17, false);
    PiecesLabel->SetText(FText::FromString(TEXT("PIECES")));
    PiecesValue = AddText(TEXT("PiecesValue"), 25, true);
    LinesLabel = AddText(TEXT("LinesLabel"), 17, false);
    LinesLabel->SetText(FText::FromString(TEXT("LINES")));
    LinesValue = AddText(TEXT("LinesValue"), 25, true);
    TimeLabel = AddText(TEXT("TimeLabel"), 17, false);
    TimeLabel->SetText(FText::FromString(TEXT("TIME")));
    TimeValue = AddText(TEXT("TimeValue"), 25, true);
    ScoreValue = AddText(TEXT("ScoreValue"), 25, true);
    ScoreValue->SetJustification(ETextJustify::Center);

    // Recent lock feedback lives under HOLD, above the regular left-side
    // metrics.  It is driven from the finalized lock result, never inputs.
    SpinActionText = AddText(TEXT("SpinActionText"), 20, true);
    ClearActionText = AddText(TEXT("ClearActionText"), 34, true);
    BackToBackText = AddText(TEXT("BackToBackText"), 19, true);
    ComboText = AddText(TEXT("ComboText"), 28, true);
    for (UTextBlock* Text : { SpinActionText.Get(), ClearActionText.Get(),
        BackToBackText.Get(), ComboText.Get() })
    {
        Text->SetRenderTransformPivot(FVector2D(1.f, .5f));
        Text->SetVisibility(ESlateVisibility::Collapsed);
    }
    BackToBackText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, .81f, .22f, .96f)));

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

FString UTotorisClassicHUDWidget::FormatScore(int64 Score)
{
    // Avoid culture-dependent number separators: the gameplay display is
    // intentionally always English-style, e.g. 1,000,000.
    FString Result = FString::Printf(TEXT("%lld"), static_cast<long long>(FMath::Max<int64>(0, Score)));
    for (int32 Index = Result.Len() - 3; Index > 0; Index -= 3)
    {
        Result.InsertAt(Index, TEXT(','));
    }
    return Result;
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

    const int32 WarningRows = FMath::Clamp(ObservedGame->GetVirtualGarbageWarningLinesForHUD(),
        0, TotorisGeneration::BoardHeight);
    const float WarningRowHeight = Height / TotorisGeneration::BoardHeight;
    const float WarningAlpha = ObservedGame->GetVirtualGarbageWarningPulseForHUD();
    for (int32 Row = 0; Row < GarbageWarningRows.Num(); ++Row)
    {
        UImage* Warning = GarbageWarningRows[Row];
        const bool bVisible = Row < WarningRows;
        Warning->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (!bVisible) continue;
        // The 0.50..1.00 shared pulse modulates a deliberately translucent
        // red layer, preserving the board, grid and active mino beneath it.
        Warning->SetColorAndOpacity(FLinearColor(.92f, .035f, .055f, .34f * WarningAlpha));
        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Warning->Slot))
        {
            CanvasSlot->SetAutoSize(false);
            CanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
            CanvasSlot->SetAlignment(FVector2D::ZeroVector);
            CanvasSlot->SetPosition(FVector2D(Left, Bottom - (Row + 1) * WarningRowHeight));
            CanvasSlot->SetSize(FVector2D(Width, WarningRowHeight));
        }
    }

    const float Gap = FMath::Clamp(Width * .05f, 9.f, 24.f);
    const float SideWidth = FMath::Clamp(Width * .65f, 120.f, 215.f);
    const float SideX = Left - Gap;
    const int32 SmallFont = FMath::Clamp(FMath::RoundToInt(Width * .060f), 12, 19);
    const int32 BigFont = FMath::Clamp(FMath::RoundToInt(Width * .087f), 17, 30);
    for (UTextBlock* Label : { InputsLabel.Get(), PiecesLabel.Get(), LinesLabel.Get(), TimeLabel.Get(),
        InputsRate.Get(), PiecesRate.Get(), FinesseLabel.Get(), FaultsValue.Get() })
        UpdateFontSize(Label, SmallFont);
    for (UTextBlock* Value : { InputsValue.Get(), PiecesValue.Get(), LinesValue.Get(), TimeValue.Get(), FinesseValue.Get() })
        UpdateFontSize(Value, BigFont);

    const auto PositionMetric = [&](UTextBlock* Label, UTextBlock* Value, float Fraction)
    {
        const float Y = Top + Height * Fraction;
        Place(Label, FVector2D(SideX, Y), FVector2D(SideWidth, 29.f), FVector2D(1.f, 0.f));
        Place(Value, FVector2D(SideX, Y + 22.f), FVector2D(SideWidth, 43.f), FVector2D(1.f, 0.f));
    };
    // Reserve the upper-left column for spin/clear feedback below HOLD.
    PositionMetric(InputsLabel, InputsValue, .48f);
    PositionMetric(PiecesLabel, PiecesValue, .60f);
    PositionMetric(LinesLabel, LinesValue, .72f);
    PositionMetric(TimeLabel, TimeValue, .84f);

    // Keep the count large and its rate smaller on the same baseline.
    const float RateWidth = SmallFont * 5.8f;
    const auto PositionRate = [&](UTextBlock* Count, UTextBlock* Rate, float Fraction)
    {
        const float Y = Top + Height * Fraction + 22.f;
        Place(Count, FVector2D(SideX - RateWidth - 5.f, Y),
            FVector2D(SideWidth - RateWidth - 5.f, 43.f), FVector2D(1.f, 0.f));
        Place(Rate, FVector2D(SideX, Y + (BigFont - SmallFont) * 1.2f),
            FVector2D(RateWidth, 32.f), FVector2D(1.f, 0.f));
    };
    PositionRate(InputsValue, InputsRate, .48f);
    PositionRate(PiecesValue, PiecesRate, .60f);
    const float CenterX = (Left + Right) * .5f;
    UpdateFontSize(ScoreValue, FMath::Clamp(FMath::RoundToInt(Width * .075f), 17, 32));
    Place(ScoreValue, FVector2D(CenterX, Bottom + 3.f),
        FVector2D(Width * 1.8f, 42.f), FVector2D(.5f, 0.f));
    const float FinesseX = Right + Gap;
    const float FinesseY = Top + Height * .835f;
    Place(FinesseLabel, FVector2D(FinesseX, FinesseY), FVector2D(SideWidth, 29.f), FVector2D::ZeroVector);
    Place(FinesseValue, FVector2D(FinesseX, FinesseY + 22.f), FVector2D(SideWidth, 43.f), FVector2D::ZeroVector);
    Place(FaultsValue, FVector2D(FinesseX, FinesseY + 64.f), FVector2D(SideWidth, 32.f), FVector2D::ZeroVector);

    const float ActionTop = Top + Height * .235f;
    const float ActionWidth = SideWidth * 1.18f;
    const int32 SpinFont = FMath::Clamp(FMath::RoundToInt(Width * .068f), 14, 24);
    const int32 ClearFont = FMath::Clamp(FMath::RoundToInt(Width * .125f), 24, 43);
    const int32 B2BFont = FMath::Clamp(FMath::RoundToInt(Width * .072f), 14, 25);
    const int32 ComboFont = FMath::Clamp(FMath::RoundToInt(Width * .10f), 20, 35);
    UpdateFontSize(SpinActionText, SpinFont);
    UpdateFontSize(ClearActionText, ClearFont);
    UpdateFontSize(BackToBackText, B2BFont);
    UpdateFontSize(ComboText, ComboFont);
    Place(SpinActionText, FVector2D(SideX, ActionTop), FVector2D(ActionWidth, 31.f), FVector2D(1.f, 0.f));
    Place(ClearActionText, FVector2D(SideX, ActionTop + SpinFont * 1.22f), FVector2D(ActionWidth, 50.f), FVector2D(1.f, 0.f));
    Place(BackToBackText, FVector2D(SideX, ActionTop + SpinFont * 1.22f + ClearFont * 1.15f), FVector2D(ActionWidth, 32.f), FVector2D(1.f, 0.f));
    Place(ComboText, FVector2D(SideX, ActionTop + SpinFont * 1.22f + ClearFont * 1.15f + B2BFont * 1.28f), FVector2D(ActionWidth, 42.f), FVector2D(1.f, 0.f));

    // Countdown: Ready, then 3 / 2 / 1 / GO for one second each.
    // It rises quickly, fades slowly, and only breathes a few percent in scale.
    const bool bFinished = ObservedGame->GetRunResult() != ETotorisRunResult::None;
    if (bFinished && FinishElapsedSeconds < 0.0) FinishElapsedSeconds = 0.0;
    if (bFinished) FinishElapsedSeconds += InDeltaTime;
    const double CountdownElapsed = ObservedGame->GetStartCountdownElapsedSecondsForHUD();
    const bool bQuickStart = ObservedGame->IsQuickStartEnabledForHUD();
    const bool bShowCountdown = bFinished ? FinishElapsedSeconds < 1.0 : CountdownElapsed >= 0.0 && CountdownElapsed < (bQuickStart ? 2.0 : 5.0);
    CountdownText->SetVisibility(bShowCountdown
        ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (bShowCountdown)
    {
        const int32 CountdownStage = bQuickStart ? 3 : FMath::Clamp(
            FMath::FloorToInt(CountdownElapsed) - 1, 0, 3);
        const float Phase = bFinished ? static_cast<float>(FinishElapsedSeconds) : static_cast<float>(CountdownElapsed - FMath::FloorToDouble(CountdownElapsed));
        const float FadeIn = FMath::Clamp(Phase / .12f, 0.f, 1.f);
        const float Alpha = Phase < .12f
            ? FMath::InterpEaseOut(0.f, 1.f, FadeIn, 2.f)
            : FMath::Lerp(1.f, 0.f, (Phase - .12f) / .88f);
        const float Scale = 1.f + .055f * FMath::Sin(PI * FMath::Clamp(Phase / .78f, 0.f, 1.f));
        const bool bIsReady = !bFinished && CountdownElapsed < 1.0;
        const bool bIsGo = CountdownStage == 3;
        CountdownText->SetText(FText::FromString(
            bFinished ? TEXT("FINISH") : (bIsReady ? TEXT("Ready") : (bIsGo ? TEXT("GO") : FString::FromInt(3 - CountdownStage)))));
        CountdownText->SetColorAndOpacity(FSlateColor(bIsGo
            ? FLinearColor(1.f, .78f, .22f, Alpha)
            : FLinearColor(.56f, .88f, 1.f, Alpha)));
        // Countdown / FINISH follows the same true fade-out rule as the
        // recent-action messages: no opaque black shadow may remain behind.
        CountdownText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, .72f * Alpha));
        CountdownText->SetRenderScale(FVector2D(Scale, Scale));
        UpdateFontSize(CountdownText, FMath::Clamp(FMath::RoundToInt(Width * .28f), 46, 124));
        Place(CountdownText,
            FVector2D((Left + Right) * .5f, Top + Height * .42f),
            FVector2D(Width * .92f, Height * .22f), FVector2D(.5f, .5f));
    }

    const FTotorisRunStatistics Stats = ObservedGame->GetRunStatistics();
    InputsValue->SetText(FText::AsNumber(Stats.KeysPressed));
    InputsRate->SetText(FText::FromString(FString::Printf(TEXT("%.2f/P"), Stats.KeysPerPiece)));
    PiecesValue->SetText(FText::AsNumber(ObservedGame->GetPlacedPieceCountForHUD()));
    PiecesRate->SetText(FText::FromString(FString::Printf(TEXT("%.2f/S"), Stats.PiecesPerSecond)));
    FinesseValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f%%"), Stats.FinessePercent)));
    FaultsValue->SetText(FText::FromString(FString::Printf(TEXT("%d FAULTS"), Stats.FinesseFaults)));
    LinesValue->SetText(FText::AsNumber(ObservedGame->GetClearedLineCountForHUD()));
    ScoreValue->SetText(FText::FromString(FormatScore(ObservedGame->GetScoreForResults())));

    const int32 PlacedPieces = ObservedGame->GetPlacedPieceCountForHUD();
    if (LastObservedPlacedPieceCount < 0)
    {
        LastObservedPlacedPieceCount = PlacedPieces;
    }
    else if (PlacedPieces < LastObservedPlacedPieceCount)
    {
        // A new game/restart must not retain feedback from the old board.
        LastObservedPlacedPieceCount = PlacedPieces;
        DisplayedBackToBackCount = 0;
        SpinActionElapsedSeconds = ClearActionElapsedSeconds = ComboElapsedSeconds = -1.0;
        BackToBackPopElapsedSeconds = -1.0;
        SpinActionText->SetVisibility(ESlateVisibility::Collapsed);
        ClearActionText->SetVisibility(ESlateVisibility::Collapsed);
        BackToBackText->SetVisibility(ESlateVisibility::Collapsed);
        ComboText->SetVisibility(ESlateVisibility::Collapsed);
    }
    else if (PlacedPieces > LastObservedPlacedPieceCount)
    {
        LastObservedPlacedPieceCount = PlacedPieces;
        const ETotorisSpinKind SpinKind = ObservedGame->GetLastSpinKindForHUD();
        if (SpinKind != ETotorisSpinKind::None)
        {
            SpinActionColor = TotorisGeneration::Color(ObservedGame->GetLastSpinMinoForHUD());
            SpinActionText->SetText(FText::FromString(FString::Printf(TEXT("%s%s - SPIN"),
                SpinKind == ETotorisSpinKind::Mini ? TEXT("MINI ") : TEXT(""),
                *TotorisGeneration::Name(ObservedGame->GetLastSpinMinoForHUD()))));
            SpinActionElapsedSeconds = 0.0;
        }

        const int32 ClearedLines = ObservedGame->GetLastClearedLineCountForHUD();
        if (ClearedLines > 0)
        {
            static const TCHAR* ClearNames[] = { TEXT(""), TEXT("SINGLE"), TEXT("DOUBLE"), TEXT("TRIPLE"), TEXT("QUAD") };
            ClearActionText->SetText(FText::FromString(ClearNames[FMath::Clamp(ClearedLines, 1, 4)]));
            ClearActionElapsedSeconds = 0.0;
        }

        const int32 BackToBack = ObservedGame->GetBackToBackCountForHUD();
        if (BackToBack >= 1)
        {
            if (BackToBack != DisplayedBackToBackCount) BackToBackPopElapsedSeconds = 0.0;
            DisplayedBackToBackCount = BackToBack;
            BackToBackText->SetText(FText::FromString(FString::Printf(TEXT("B2B x %d"), BackToBack)));
        }
        else
        {
            DisplayedBackToBackCount = 0;
            BackToBackPopElapsedSeconds = -1.0;
            BackToBackText->SetVisibility(ESlateVisibility::Collapsed);
        }

        // ComboCount is zero-based internally: its displayed streak is one-based.
        const int32 DisplayCombo = ObservedGame->GetComboCountForHUD() + 1;
        if (DisplayCombo >= 2)
        {
            ComboText->SetText(FText::FromString(FString::Printf(TEXT("%d COMBO"), DisplayCombo)));
            ComboElapsedSeconds = 0.0;
        }
    }

    const auto UpdateTransientAction = [InDeltaTime](UTextBlock* Text, double& Elapsed,
        const FLinearColor& BaseColor)
    {
        if (Elapsed < 0.0) return;
        Elapsed += InDeltaTime;
        if (Elapsed >= 2.0)
        {
            Text->SetVisibility(ESlateVisibility::Collapsed);
            Elapsed = -1.0;
            return;
        }
        const float Seconds = static_cast<float>(Elapsed);
        const float PopProgress = FMath::Clamp(Seconds / .16f, 0.f, 1.f);
        const float Scale = FMath::Lerp(1.13f, 1.f, FMath::InterpEaseOut(0.f, 1.f, PopProgress, 2.f));
        const float Alpha = Seconds <= 1.f ? 1.f : 1.f - (Seconds - 1.f);
        Text->SetVisibility(ESlateVisibility::HitTestInvisible);
        Text->SetColorAndOpacity(FSlateColor(FLinearColor(BaseColor.R, BaseColor.G, BaseColor.B, Alpha)));
        // The normal HUD text has an opaque drop shadow. Fade it with the
        // glyph so the tail becomes transparent instead of looking black.
        Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, .85f * Alpha));
        Text->SetRenderScale(FVector2D(Scale, Scale));
        Text->SetRenderTranslation(FVector2D(0.f, -5.f * (1.f - PopProgress)));
    };
    UpdateTransientAction(SpinActionText, SpinActionElapsedSeconds, SpinActionColor);
    UpdateTransientAction(ClearActionText, ClearActionElapsedSeconds, FLinearColor::White);
    UpdateTransientAction(ComboText, ComboElapsedSeconds, FLinearColor::White);
    if (DisplayedBackToBackCount >= 1)
    {
        if (BackToBackPopElapsedSeconds >= 0.0) BackToBackPopElapsedSeconds += InDeltaTime;
        const float PopProgress = BackToBackPopElapsedSeconds < 0.0 ? 1.f :
            FMath::Clamp(static_cast<float>(BackToBackPopElapsedSeconds) / .16f, 0.f, 1.f);
        const float Scale = FMath::Lerp(1.12f, 1.f, FMath::InterpEaseOut(0.f, 1.f, PopProgress, 2.f));
        BackToBackText->SetVisibility(ESlateVisibility::HitTestInvisible);
        BackToBackText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, .81f, .22f, .96f)));
        BackToBackText->SetRenderScale(FVector2D(Scale, Scale));
        BackToBackText->SetRenderTranslation(FVector2D(0.f, -4.f * (1.f - PopProgress)));
    }
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
