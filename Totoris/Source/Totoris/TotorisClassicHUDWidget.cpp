#include "TotorisClassicHUDWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/Image.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TotorisBlockGeneratorComponent.h"
#include "TotorisAudioMix.h"
#include "TotorisGeneration.h"
#include "TotorisPlayerController.h"
#include "TotorisMinoRichTextDecorator.h"
#include "Misc/Paths.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"

namespace
{
    FText ToMinoRichText(const FText& Source, float MinoBrightness)
    {
        FString Markup = Source.ToString();
        for (const TCHAR Type : FString(TEXT("IOTSZJL")))
        {
            const FString Token = FString::Printf(TEXT("{%c}"), Type);
            const FString Tag = FString::Printf(TEXT("<mino id=\"%c\" brightness=\"%.2f\"/>"),
                Type, FMath::Clamp(MinoBrightness, 0.f, 1.f));
            Markup.ReplaceInline(*Token, *Tag);
        }
        return FText::FromString(Markup);
    }

    FTextBlockStyle MakeMissionRichTextStyle(const FString& FontPath, int32 FontSize,
        const FSlateColor& Color, const FLinearColor& OutlineColor)
    {
        // A newly constructed URichTextBlock has no Slate style instance yet.
        // Do not query GetDefaultTextStyle here: it deliberately ensures before
        // RebuildWidget has created that instance.
        FTextBlockStyle Style;
    FSlateFontInfo Font(FontPath, FontSize);
    Font.OutlineSettings = FFontOutlineSettings(1, OutlineColor);
    Style.SetFont(Font);
        Style.SetColorAndOpacity(Color);
        Style.SetShadowOffset(FVector2D::ZeroVector);
        return Style;
    }

    void SetMissionRichTextStyle(URichTextBlock* Text, const FString& FontPath,
        int32 FontSize, const FSlateColor& Color, const FLinearColor& OutlineColor)
    {
        const FTextBlockStyle Style = MakeMissionRichTextStyle(FontPath, FontSize, Color, OutlineColor);
        Text->SetDefaultTextStyle(Style);
    }
}

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

URichTextBlock* UTotorisClassicHUDWidget::AddMissionText(
    const TCHAR* Name, int32 FontSize, const FString& FontPath)
{
    URichTextBlock* Text = WidgetTree->ConstructWidget<URichTextBlock>(
        URichTextBlock::StaticClass(), FName(Name));
    RootPanel->AddChild(Text);
    Text->SetDefaultTextStyle(MakeMissionRichTextStyle(
        FontPath, FontSize, FSlateColor(FLinearColor::White), FLinearColor::Black));
    Text->SetDecorators({ UTotorisMinoRichTextDecorator::StaticClass() });
    Text->SetAutoWrapText(true);
    Text->SetClipping(EWidgetClipping::ClipToBounds);
    return Text;
}

void UTotorisClassicHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    MissionSummonSound = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/Totoris/Audio/Gameplay/MissionSummon.MissionSummon"));
    RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ClassicHUDRoot"));
    WidgetTree->RootWidget = RootPanel;
    SetVisibility(ESlateVisibility::HitTestInvisible);

    // Medium weight keeps the compact mission panel legible while matching
    // the stronger UI type used by the game setup screen.
    const FString MissionFontPath = FPaths::ProjectContentDir() / TEXT("Slate/Fonts/NotoSansCJKkr-Medium.otf");
    for (int32 Index = 0; Index < 3; ++Index)
    {
        URichTextBlock* EnglishText = AddMissionText(
            *FString::Printf(TEXT("MissionPanel_%d_English"), Index), 16, MissionFontPath);
        MissionPanelEnglishText.Add(EnglishText);

        URichTextBlock* KoreanText = AddMissionText(
            *FString::Printf(TEXT("MissionPanel_%d_Korean"), Index), 18, MissionFontPath);
        MissionPanelKoreanText.Add(KoreanText);
    }

	MissionProgressText = AddText(TEXT("MissionPanel_Progress"), 36, true);
	FSlateFontInfo MissionProgressFont(MissionFontPath, 36);
	MissionProgressFont.OutlineSettings = FFontOutlineSettings(1, FLinearColor::White);
	MissionProgressText->SetFont(MissionProgressFont);
	MissionProgressText->SetJustification(ETextJustify::Center);
	MissionProgressText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	MissionProgressText->SetShadowOffset(FVector2D(1.f, 2.f));
	MissionProgressText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, .85f));
	MissionProgressText->SetAutoWrapText(false);
	MissionProgressText->SetVisibility(ESlateVisibility::Collapsed);

	for (int32 Index = 0; Index < 3; ++Index)
	{
		UTextBlock* ChecklistText = AddText(
			*FString::Printf(TEXT("MissionPanel_LineClearChecklist_%d"), Index), 28, true);
		FSlateFontInfo ChecklistFont(MissionFontPath, 28);
		ChecklistFont.OutlineSettings = FFontOutlineSettings(1, FLinearColor(.05f, .05f, .08f, 1.f));
		ChecklistText->SetFont(ChecklistFont);
		ChecklistText->SetText(FText::FromString(
			Index == 0 ? TEXT("Single") : Index == 1 ? TEXT("Double") : TEXT("Triple")));
		ChecklistText->SetJustification(ETextJustify::Center);
		ChecklistText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ChecklistText->SetVisibility(ESlateVisibility::Collapsed);
		MissionLineClearChecklistText.Add(ChecklistText);
	}

	for (int32 Column = 0; Column < TotorisGeneration::BoardWidth; ++Column)
	{
		UImage* ColumnCell = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			FName(*FString::Printf(TEXT("MissionPanel_ColumnChecklist_%d"), Column)));
		RootPanel->AddChild(ColumnCell);
		ColumnCell->SetBrush(FSlateColorBrush(FLinearColor::White));
		ColumnCell->SetVisibility(ESlateVisibility::Collapsed);
		MissionColumnChecklistCells.Add(ColumnCell);
	}

	for (int32 Index = 0; Index < 100; ++Index)
	{
		UImage* Cell = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			FName(*FString::Printf(TEXT("MissionPattern_%d"), Index)));
		RootPanel->AddChild(Cell);
		Cell->SetBrush(FSlateColorBrush(FLinearColor::White));
		Cell->SetVisibility(ESlateVisibility::Collapsed);
		MissionPatternCells.Add(Cell);
	}

	// Seven independent four-cell glyphs. Their positions are resolved in
	// NativeTick so they remain inside the 3D mission panel at every viewport size.
	for (int32 MinoIndex = 0; MinoIndex < 7; ++MinoIndex)
	{
		for (int32 CellIndex = 0; CellIndex < 4; ++CellIndex)
		{
			UImage* MinoCell = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
				FName(*FString::Printf(TEXT("MissionPanel_MinoChecklist_%d_%d"), MinoIndex, CellIndex)));
			RootPanel->AddChild(MinoCell);
			MinoCell->SetBrush(FSlateColorBrush(FLinearColor::White));
			MinoCell->SetVisibility(ESlateVisibility::Collapsed);
			MissionMinoChecklistCells.Add(MinoCell);
		}
	}

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

void UTotorisClassicHUDWidget::Place(UWidget* Text, const FVector2D& Position,
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

UImage* UTotorisClassicHUDWidget::AcquireMissionTextParticleWidget()
{
    for (UImage* Particle : MissionTextParticlePool)
    {
        if (IsValid(Particle) && Particle->GetVisibility() == ESlateVisibility::Collapsed)
        {
            return Particle;
        }
    }

    UImage* Particle = WidgetTree->ConstructWidget<UImage>(
        UImage::StaticClass(),
        FName(*FString::Printf(TEXT("MissionTextParticle_%d"), MissionTextParticlePool.Num())));
    RootPanel->AddChild(Particle);
    Particle->SetBrush(FSlateColorBrush(FLinearColor::White));
    Particle->SetVisibility(ESlateVisibility::Collapsed);
    if (UCanvasPanelSlot* ParticleCanvasSlot = Cast<UCanvasPanelSlot>(Particle->Slot))
    {
        ParticleCanvasSlot->SetZOrder(30);
    }
    MissionTextParticlePool.Add(Particle);
    return Particle;
}

void UTotorisClassicHUDWidget::SpawnMissionTextParticles(
    int32 MissionSlot, const FLinearColor& Color, bool bFloatUp)
{
    if (LastMissionTextSizes[MissionSlot].X <= 1.f || LastMissionTextSizes[MissionSlot].Y <= 1.f)
    {
        return;
    }

    // The source is distributed across the full rendered sentence width, not
    // from its center. Inline mino glyphs deliberately use this same mission
    // text color rather than their individual tetromino colors.
    const int32 ParticleCount = FMath::Clamp(
        FMath::RoundToInt(LastMissionTextSizes[MissionSlot].X / 7.f), 32, 72);
    for (int32 Index = 0; Index < ParticleCount; ++Index)
    {
        UImage* Widget = AcquireMissionTextParticleWidget();
        if (!IsValid(Widget)) continue;

        FTotorisMissionTextParticle& Particle = ActiveMissionTextParticles.AddDefaulted_GetRef();
        Particle.Widget = Widget;
        Particle.StartPosition = LastMissionTextOrigins[MissionSlot] + FVector2D(
            FMath::FRandRange(2.f, FMath::Max(3.f, LastMissionTextSizes[MissionSlot].X - 2.f)),
            FMath::FRandRange(1.f, FMath::Max(2.f, LastMissionTextSizes[MissionSlot].Y - 1.f)));
        Particle.Age = FMath::FRandRange(-.035f, .0f);
        Particle.Lifetime = bFloatUp ? FMath::FRandRange(.62f, .92f) : FMath::FRandRange(.48f, .72f);
        Particle.Size = FMath::FRandRange(3.5f, 7.5f);
        Particle.DriftPhase = FMath::FRandRange(0.f, 2.f * PI);
        Particle.bFloatUp = bFloatUp;
        // Preserve the tier color, but lift it toward white by a restrained,
        // randomized amount so the particles read as illuminated dust rather
        // than flat colored squares.
        Particle.Color = FMath::Lerp(Color, FLinearColor::White, FMath::FRandRange(.18f, .38f));
        if (bFloatUp)
        {
            Particle.Velocity = FVector2D(FMath::FRandRange(-68.f, 68.f), FMath::FRandRange(-225.f, -108.f));
        }
        else
        {
            const float Angle = FMath::FRandRange(0.f, 2.f * PI);
            Particle.Velocity = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) *
                FMath::FRandRange(145.f, 330.f);
        }
        Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

void UTotorisClassicHUDWidget::TickMissionTextParticles(float DeltaSeconds)
{
    for (int32 Index = ActiveMissionTextParticles.Num() - 1; Index >= 0; --Index)
    {
        FTotorisMissionTextParticle& Particle = ActiveMissionTextParticles[Index];
        Particle.Age += DeltaSeconds;
        const float Progress = FMath::Clamp(Particle.Age / Particle.Lifetime, 0.f, 1.f);
        if (!IsValid(Particle.Widget) || Progress >= 1.f)
        {
            if (IsValid(Particle.Widget)) Particle.Widget->SetVisibility(ESlateVisibility::Collapsed);
            ActiveMissionTextParticles.RemoveAtSwap(Index);
            continue;
        }

        const FVector2D Acceleration = Particle.bFloatUp
            ? FVector2D(0.f, -48.f)
            : FVector2D(0.f, 250.f);
        FVector2D Position = Particle.StartPosition + Particle.Velocity * Particle.Age +
            .5f * Acceleration * Particle.Age * Particle.Age;
        if (Particle.bFloatUp)
        {
            Position.X += FMath::Sin(Particle.DriftPhase + Particle.Age * 9.f) * 10.f * Progress;
        }
        const float Alpha = FMath::Square(1.f - Progress) * FMath::Lerp(1.25f, 1.f, Progress);
        Particle.Widget->SetColorAndOpacity(FLinearColor(
            Particle.Color.R, Particle.Color.G, Particle.Color.B, Particle.Color.A * Alpha));
        Particle.Widget->SetRenderTransformPivot(FVector2D(.5f, .5f));
        const float Scale = Particle.bFloatUp ? FMath::Lerp(1.18f, .28f, Progress) : FMath::Lerp(1.28f, .48f, Progress);
        Particle.Widget->SetRenderScale(FVector2D(Scale, Scale));
        Place(Particle.Widget, Position, FVector2D(Particle.Size, Particle.Size), FVector2D(.5f, .5f));
    }
}

void UTotorisClassicHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    const auto* PreviewController = Cast<ATotorisPlayerController>(GetOwningPlayer());
    const bool bPreview = PreviewController && PreviewController->IsEventPreviewActive();
    SetRenderOpacity(bPreview && !PreviewController->IsEventPreviewVisible() ? 0.f : 1.f);
    if (!IsValid(ObservedGame) || (!ObservedGame->IsGameplayActive() && !bPreview))
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
    TickMissionTextParticles(InDeltaTime);

    // The camera is usually aimed along the board's local +X. Min/max keeps
    // this stable if a different camera orientation reverses screen axes.
    const float Left = FMath::Min(TopLeft.X, BottomRight.X);
    const float Right = FMath::Max(TopLeft.X, BottomRight.X);
    const float Top = FMath::Min(TopLeft.Y, BottomRight.Y);
    const float Bottom = FMath::Max(TopLeft.Y, BottomRight.Y);
    const float Width = Right - Left;
    const float Height = Bottom - Top;
    if (Width < 10.f || Height < 10.f) return;

    FVector2D MissionTop, MissionBottom;
    const bool bMissionPanelProjected = ObservedGame->IsMissionPanelVisible()
        && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            PC, BoardTransform.TransformPosition(FVector(-8.f, 170.f - HalfWidth, HalfHeight)), MissionTop, true)
        && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            PC, BoardTransform.TransformPosition(FVector(-8.f, 170.f + HalfWidth, -HalfHeight)), MissionBottom, true);
    // Reveal only after the first GO. Keep visibility and intro/sound state
    // through pause and the resume countdown; a new run resets the latch.
    const bool bMissionVisible = bMissionPanelProjected && ObservedGame->HasStartedGameplayThisRun();

    const bool bMissionGameOver = ObservedGame->GetRunResult() != ETotorisRunResult::None;
    if (bMissionGameOver && !bMissionGameOverParticlesSpawned)
    {
        for (int32 Index = 0; Index < 3; ++Index)
        {
            if (bMissionSlotPopulatedLastFrame[Index])
            {
                SpawnMissionTextParticles(Index, LastMissionTextColors[Index], true);
                MissionTextSuppressedMask |= static_cast<uint8>(1u << Index);
            }
        }
        bMissionGameOverParticlesSpawned = true;
    }
    else if (!bMissionGameOver)
    {
        bMissionGameOverParticlesSpawned = false;
        MissionTextSuppressedMask = 0;
    }

    int32 ActiveMissionSlot = INDEX_NONE;
    int32 PopulatedMissionSlots = 0;
    FString MissionSetSignature;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const FString EnglishString = ObservedGame->MissionPanelEnglish[Index].ToString();
        const FString KoreanString = ObservedGame->MissionPanelKorean[Index].ToString();
        if (!EnglishString.IsEmpty() || !KoreanString.IsEmpty())
        {
            if (ActiveMissionSlot == INDEX_NONE) ActiveMissionSlot = Index;
            ++PopulatedMissionSlots;
        }
        MissionSetSignature += EnglishString + TEXT("\u001f") + KoreanString + TEXT("\u001e");
    }

    const bool bIsNewCompleteMissionSet = PopulatedMissionSlots == 3 &&
        MissionSetSignature != LastMissionPanelSetSignature;
    if (!bMissionVisible)
    {
        bMissionPanelIntroVisibleLastFrame = false;
        MissionPanelIntroElapsedSeconds = 0.0;
    }
    else if (!bMissionPanelIntroVisibleLastFrame || bIsNewCompleteMissionSet)
    {
        bMissionPanelIntroVisibleLastFrame = true;
        MissionPanelIntroElapsedSeconds = 0.0;
        LastMissionPanelSetSignature = MissionSetSignature;
        MissionSummonPlayedMask = 0;
    }
    else
    {
        MissionPanelIntroElapsedSeconds += InDeltaTime;
    }
    if (PopulatedMissionSlots == 0)
    {
        LastMissionPanelSetSignature.Reset();
    }

    if (!bMissionVisible)
    {
        for (int32 Index = 0; Index < 3; ++Index)
        {
            bMissionSlotPopulatedLastFrame[Index] = false;
            LastMissionSlotSignatures[Index].Reset();
        }
    }
    else if (!bMissionGameOver)
    {
        // A card that existed on the prior frame but has now been cleared was
        // just completed. Its saved last-frame text rectangle is our emitter.
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const FString CurrentSignature = ObservedGame->MissionPanelEnglish[Index].ToString() +
                TEXT("\u001f") + ObservedGame->MissionPanelKorean[Index].ToString();
            const bool bPopulatedNow = !CurrentSignature.Equals(TEXT("\u001f"));
            const bool bCardWasReplaced = bMissionSlotPopulatedLastFrame[Index] && bPopulatedNow &&
                CurrentSignature != LastMissionSlotSignatures[Index];
            if (bMissionSlotPopulatedLastFrame[Index] && (!bPopulatedNow || bCardWasReplaced))
            {
                SpawnMissionTextParticles(Index, LastMissionTextColors[Index], false);
            }
            bMissionSlotPopulatedLastFrame[Index] = bPopulatedNow;
            LastMissionSlotSignatures[Index] = CurrentSignature;
        }
    }

    const auto ApplyMissionIntro = [this](UWidget* Widget, int32 MissionSlot)
    {
        constexpr float StaggerSeconds = .12f;
        constexpr float FadeAndSlideSeconds = .24f;
        const float Elapsed = static_cast<float>(MissionPanelIntroElapsedSeconds) -
            StaggerSeconds * FMath::Max(0, MissionSlot);
        const float LinearProgress = FMath::Clamp(Elapsed / FadeAndSlideSeconds, 0.f, 1.f);
        const float Progress = FMath::InterpEaseOut(0.f, 1.f, LinearProgress, 2.f);
        Widget->SetRenderOpacity(Progress);
        Widget->SetRenderTranslation(FVector2D(FMath::Lerp(42.f, 0.f, Progress), 0.f));
    };

    // One summon cue per card, synchronized with the staggered visual reveal.
    // The bitmask prevents repeated playback across the several HUD ticks that
    // each card spends at its start threshold.
    if (bMissionVisible && MissionSummonSound)
    {
        constexpr float StaggerSeconds = .12f;
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const uint8 CardBit = static_cast<uint8>(1u << Index);
            if ((MissionSummonPlayedMask & CardBit) == 0 &&
                !ObservedGame->MissionPanelEnglish[Index].IsEmpty() &&
                MissionPanelIntroElapsedSeconds >= StaggerSeconds * Index)
            {
                UGameplayStatics::PlaySound2D(this, MissionSummonSound,
                    TotorisAudioMix::MissionSummon * TotorisAudioMix::MasterVolume);
                MissionSummonPlayedMask |= CardBit;
            }
        }
    }
    const FString MissionFontPath = FPaths::ProjectContentDir() / TEXT("Slate/Fonts/NotoSansCJKkr-Medium.otf");
    for (int32 Index = 0; Index < MissionPanelEnglishText.Num(); ++Index)
    {
        URichTextBlock* EnglishText = MissionPanelEnglishText[Index];
        URichTextBlock* KoreanText = MissionPanelKoreanText[Index];
        const bool bSuppressCardText = (MissionTextSuppressedMask & static_cast<uint8>(1u << Index)) != 0;
        EnglishText->SetVisibility(bMissionVisible && !bSuppressCardText ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        KoreanText->SetVisibility(bMissionVisible && !bSuppressCardText ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (!bMissionVisible) continue;
        ApplyMissionIntro(EnglishText, Index);
        ApplyMissionIntro(KoreanText, Index);
        const float PanelWidth = FMath::Abs(MissionBottom.X - MissionTop.X);
        const float PanelHeight = FMath::Abs(MissionBottom.Y - MissionTop.Y);
        const float MissionPadding = PanelWidth * .035f;
        const float SlotHeight = (PanelHeight * .5f - MissionPadding * 2.f) / 3.f;
        const float SlotTop = FMath::Min(MissionTop.Y, MissionBottom.Y) + MissionPadding + Index * SlotHeight;
        const float TextWidth = PanelWidth - 2.f * MissionPadding;
        const float MinoBrightness = ObservedGame->MissionPanelMinoBrightness[Index];
        const FText English = ToMinoRichText(ObservedGame->MissionPanelEnglish[Index], MinoBrightness);
        const FText Korean = ToMinoRichText(ObservedGame->MissionPanelKorean[Index], MinoBrightness);
        if (!EnglishText->GetText().EqualTo(English)) EnglishText->SetText(English);
        if (!KoreanText->GetText().EqualTo(Korean)) KoreanText->SetText(Korean);
        EnglishText->SetWrapTextAt(TextWidth);
        KoreanText->SetWrapTextAt(TextWidth);

        // Keep the group within its fixed slot. Long English/Korean pairs
        // shrink together, while English remains smaller than Korean.
        const int32 BaseEnglishSize = FMath::Max(8, FMath::RoundToInt(PanelWidth * .041f));
        const int32 BaseKoreanSize = FMath::Max(9, FMath::RoundToInt(PanelWidth * .049f));
        const float TextGap = MissionPadding * .18f;
        float EnglishHeight = 0.f;
        float KoreanHeight = 0.f;
        const FSlateColor MissionColor(ObservedGame->MissionPanelColors[Index]);
		const FLinearColor MissionOutlineColor = ObservedGame->MissionPanelOutlineColors[Index];
        for (float Scale = 1.f; Scale >= .45f; Scale -= .05f)
        {
            SetMissionRichTextStyle(EnglishText, MissionFontPath,
                FMath::Max(8, FMath::RoundToInt(BaseEnglishSize * Scale)), MissionColor, MissionOutlineColor);
            SetMissionRichTextStyle(KoreanText, MissionFontPath,
                FMath::Max(9, FMath::RoundToInt(BaseKoreanSize * Scale)), MissionColor, MissionOutlineColor);
            EnglishText->ForceLayoutPrepass();
            KoreanText->ForceLayoutPrepass();
            EnglishHeight = EnglishText->GetDesiredSize().Y;
            KoreanHeight = KoreanText->GetDesiredSize().Y;
            if (EnglishHeight + TextGap + KoreanHeight <= SlotHeight)
            {
                break;
            }
        }

        // The Korean line begins immediately after the wrapped English text.
        // Slot anchors remain fixed, leaving all inter-mission whitespace intact.
        Place(EnglishText, FVector2D(FMath::Min(MissionTop.X, MissionBottom.X) + MissionPadding, SlotTop),
            FVector2D(TextWidth, EnglishHeight), FVector2D::ZeroVector);
        Place(KoreanText, FVector2D(FMath::Min(MissionTop.X, MissionBottom.X) + MissionPadding,
            SlotTop + EnglishHeight + TextGap),
            FVector2D(TextWidth, KoreanHeight), FVector2D::ZeroVector);

        LastMissionTextOrigins[Index] = FVector2D(
            FMath::Min(MissionTop.X, MissionBottom.X) + MissionPadding, SlotTop);
        LastMissionTextSizes[Index] = FVector2D(TextWidth,
            FMath::Max(1.f, EnglishHeight + TextGap + KoreanHeight));
        LastMissionTextColors[Index] = ObservedGame->MissionPanelColors[Index];
    }

	int32 CounterSlot = INDEX_NONE;
	if (bMissionVisible)
	{
		for (int32 Index = 0; Index < 3; ++Index)
		{
			if (ObservedGame->MissionPanelCounterVisible[Index])
			{
				CounterSlot = Index;
				break;
			}
		}
	}

	if (CounterSlot == INDEX_NONE)
	{
		MissionProgressText->SetVisibility(ESlateVisibility::Collapsed);
		MissionProgressFailElapsedSeconds = -1.0;
		LastMissionProgressCounterSlot = INDEX_NONE;
		LastMissionProgressCurrentValue = 0;
		LastMissionProgressSignature.Reset();
	}
	else
	{
		const int32 CurrentValue = FMath::Clamp(ObservedGame->MissionPanelCounterCurrentValues[CounterSlot], 0, 999);
		const int32 TargetValue = FMath::Clamp(ObservedGame->MissionPanelCounterTargetValues[CounterSlot], 0, 999);
		const FString ProgressMissionSignature = ObservedGame->MissionPanelEnglish[CounterSlot].ToString() +
			TEXT("\u001f") + ObservedGame->MissionPanelKorean[CounterSlot].ToString();
		if (LastMissionProgressCounterSlot == CounterSlot &&
			LastMissionProgressSignature == ProgressMissionSignature &&
			CurrentValue < LastMissionProgressCurrentValue)
		{
			// The controller only lowers a counter for a failed sequence. A
			// value already at zero cannot reach this branch, so 0 -> 0 remains
			// visually silent.
			MissionProgressFailElapsedSeconds = 0.0;
		}
		LastMissionProgressCounterSlot = CounterSlot;
		LastMissionProgressCurrentValue = CurrentValue;
		LastMissionProgressSignature = ProgressMissionSignature;
		if (MissionProgressFailElapsedSeconds >= 0.0)
		{
			MissionProgressFailElapsedSeconds += InDeltaTime;
		}
		// Each number owns a fixed three-character field. Noto Sans uses tabular
		// figures, so [, / and ] stay fixed from 0/1 through 100/100.
		const FText ProgressText = FText::FromString(FString::Printf(
			TEXT("[ %3d / %3d ]"), CurrentValue, TargetValue));
		if (!MissionProgressText->GetText().EqualTo(ProgressText))
		{
			MissionProgressText->SetText(ProgressText);
		}
		FSlateFontInfo ProgressFont = MissionProgressText->GetFont();
		ProgressFont.OutlineSettings = FFontOutlineSettings(1,
			ObservedGame->MissionPanelOutlineColors[CounterSlot]);
		MissionProgressText->SetFont(ProgressFont);

		const float PanelWidth = FMath::Abs(MissionBottom.X - MissionTop.X);
		const float PanelHeight = FMath::Abs(MissionBottom.Y - MissionTop.Y);
		const float AvailableWidth = PanelWidth * .92f;
		const int32 RequestedFontSize = FMath::Clamp(FMath::RoundToInt(PanelWidth * .13f), 18, 52);
		int32 ProgressFontSize = RequestedFontSize;
		for (; ProgressFontSize >= 12; --ProgressFontSize)
		{
			UpdateFontSize(MissionProgressText, ProgressFontSize);
			MissionProgressText->ForceLayoutPrepass();
			if (MissionProgressText->GetDesiredSize().X <= AvailableWidth)
			{
				break;
			}
		}

		MissionProgressText->SetVisibility(ESlateVisibility::HitTestInvisible);
		ApplyMissionIntro(MissionProgressText, CounterSlot);
		// A restrained 10% pulse avoids disturbing the fixed progress layout.
		constexpr float FailFlashSeconds = .22f;
		const float FailProgress = MissionProgressFailElapsedSeconds >= 0.0
			? FMath::Clamp(static_cast<float>(MissionProgressFailElapsedSeconds / FailFlashSeconds), 0.f, 1.f)
			: 1.f;
		const float FailScale = MissionProgressFailElapsedSeconds >= 0.0
			? 1.f + .10f * FMath::Sin(PI * FailProgress)
			: 1.f;
		MissionProgressText->SetRenderTransformPivot(FVector2D(.5f, .5f));
		MissionProgressText->SetRenderScale(FVector2D(FailScale, FailScale));
		MissionProgressText->SetColorAndOpacity(FSlateColor(
			FMath::Lerp(FLinearColor(1.f, .12f, .12f, 1.f), FLinearColor::White, FailProgress)));
		if (MissionProgressFailElapsedSeconds >= FailFlashSeconds)
		{
			MissionProgressFailElapsedSeconds = -1.0;
		}
		Place(MissionProgressText,
			FVector2D((MissionTop.X + MissionBottom.X) * .5f,
				FMath::Min(MissionTop.Y, MissionBottom.Y) + PanelHeight * .75f),
			FVector2D(AvailableWidth, FMath::Max(24.f, ProgressFontSize * 1.45f)),
			FVector2D(.5f, .5f));
	}

	const bool bShowLineClearChecklist = bMissionVisible &&
		ObservedGame->bMissionPanelLineClearChecklistVisible;
	const float ChecklistPanelWidth = FMath::Abs(MissionBottom.X - MissionTop.X);
	const float ChecklistPanelHeight = FMath::Abs(MissionBottom.Y - MissionTop.Y);
	const float ChecklistCenterX = (MissionTop.X + MissionBottom.X) * .5f;
	const float ChecklistTop = FMath::Min(MissionTop.Y, MissionBottom.Y);
	const int32 ChecklistFontSize = FMath::Clamp(
		FMath::RoundToInt(ChecklistPanelWidth * .115f), 16, 42);
	for (int32 Index = 0; Index < MissionLineClearChecklistText.Num(); ++Index)
	{
		UTextBlock* ChecklistText = MissionLineClearChecklistText[Index];
		const bool bPending = bShowLineClearChecklist &&
			ObservedGame->MissionPanelLineClearChecklistPending[Index];
		ChecklistText->SetVisibility(bPending ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!bPending) continue;
		ApplyMissionIntro(ChecklistText, ActiveMissionSlot);
		UpdateFontSize(ChecklistText, ChecklistFontSize);
		Place(ChecklistText,
			FVector2D(ChecklistCenterX, ChecklistTop + ChecklistPanelHeight * (.60f + .14f * Index)),
			FVector2D(ChecklistPanelWidth * .92f, FMath::Max(24.f, ChecklistFontSize * 1.4f)),
			FVector2D(.5f, .5f));
	}

	const bool bShowColumnChecklist = bMissionVisible &&
		ObservedGame->bMissionPanelColumnChecklistVisible;
	const float ColumnChecklistPanelWidth = FMath::Abs(MissionBottom.X - MissionTop.X);
	const float ColumnChecklistPanelHeight = FMath::Abs(MissionBottom.Y - MissionTop.Y);
	const float ColumnChecklistLeft = FMath::Min(MissionTop.X, MissionBottom.X);
	const float ColumnChecklistTop = FMath::Min(MissionTop.Y, MissionBottom.Y);
	const float ColumnChecklistAvailableWidth = ColumnChecklistPanelWidth * .88f;
	const float ColumnChecklistGap = FMath::Clamp(ColumnChecklistAvailableWidth * .008f, 1.f, 3.f);
	const float ColumnChecklistCellWidth = (ColumnChecklistAvailableWidth -
		ColumnChecklistGap * (TotorisGeneration::BoardWidth - 1)) / TotorisGeneration::BoardWidth;
	const float ColumnChecklistCellHeight = FMath::Clamp(ColumnChecklistPanelHeight * .05f, 14.f, 40.f);
	for (int32 Column = 0; Column < MissionColumnChecklistCells.Num(); ++Column)
	{
		UImage* ColumnCell = MissionColumnChecklistCells[Column];
		ColumnCell->SetVisibility(bShowColumnChecklist ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!bShowColumnChecklist) continue;
		ApplyMissionIntro(ColumnCell, ActiveMissionSlot);
		const uint16 ColumnBit = static_cast<uint16>(1u << Column);
		const bool bRequiredAndPending =
			(ObservedGame->MissionPanelRequiredColumnMask & ColumnBit) != 0 &&
			(ObservedGame->MissionPanelCompletedColumnMask & ColumnBit) == 0;
		ColumnCell->SetColorAndOpacity(bRequiredAndPending
			? FLinearColor::White
			: FLinearColor(.34f, .34f, .36f, 1.f));
		Place(ColumnCell,
			FVector2D(ColumnChecklistLeft + ColumnChecklistPanelWidth * .06f +
				Column * (ColumnChecklistCellWidth + ColumnChecklistGap),
				ColumnChecklistTop + ColumnChecklistPanelHeight * .75f),
			FVector2D(ColumnChecklistCellWidth, ColumnChecklistCellHeight), FVector2D::ZeroVector);
	}

	const bool bShowPattern = bMissionVisible && ObservedGame->MissionPatternColors.Num() == 100;
	MissionPatternFadeTime = bShowPattern ? FMath::Fmod(MissionPatternFadeTime + InDeltaTime, 2.f) : 0.f;
	const float PatternFade = .5f - .5f * FMath::Cos(PI * MissionPatternFadeTime);
	const float PatternSize = FMath::Min(ColumnChecklistPanelWidth * .88f, ColumnChecklistPanelHeight * .50f);
	const float PatternCellSize = PatternSize / 10.f;
	const FVector2D PatternOrigin(ColumnChecklistLeft + (ColumnChecklistPanelWidth - PatternSize) * .5f,
		ColumnChecklistTop + ColumnChecklistPanelHeight * .94f - PatternSize);
	const FLinearColor EmptyPatternColor(.025f, .045f, .075f, .8f);
	for (int32 Index = 0; Index < MissionPatternCells.Num(); ++Index)
	{
		UImage* Cell = MissionPatternCells[Index];
		Cell->SetVisibility(bShowPattern ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!bShowPattern) continue;
		ApplyMissionIntro(Cell, ActiveMissionSlot);
		const FLinearColor Target = ObservedGame->MissionPatternColors[Index];
		const float Alpha = Target.A == 0.f ? 0.f :
			(ObservedGame->MissionPatternFadingCells.Contains(Index) ? PatternFade : 1.f);
		// Explicit square outlines remain visible even when the fill fades out.
		// Keep widget tint white so it does not multiply the outline color.
		Cell->SetColorAndOpacity(FLinearColor::White);
		Cell->SetBrush(FSlateRoundedBoxBrush(
			FMath::Lerp(EmptyPatternColor, Target, Alpha), 0.f,
			FLinearColor(.08f, .20f, .26f, 1.f), 1.25f));
		Place(Cell, PatternOrigin + FVector2D(Index % 10, Index / 10) * PatternCellSize,
			FVector2D(PatternCellSize, PatternCellSize), FVector2D::ZeroVector);
	}

	const bool bShowMinoChecklist = bMissionVisible &&
		ObservedGame->bMissionPanelMinoChecklistVisible;
	const float MinoChecklistPanelWidth = FMath::Abs(MissionBottom.X - MissionTop.X);
	const float MinoChecklistPanelHeight = FMath::Abs(MissionBottom.Y - MissionTop.Y);
	const float MinoChecklistLeft = FMath::Min(MissionTop.X, MissionBottom.X);
	const float MinoChecklistTop = FMath::Min(MissionTop.Y, MissionBottom.Y);
	const float MinoChecklistWidth = MinoChecklistPanelWidth * .78f;
	const float MinoChecklistHeight = MinoChecklistPanelHeight * .50f;
	const float MinoChecklistCellSize = FMath::Max(4.f, FMath::Min(
		MinoChecklistWidth / 8.f, MinoChecklistHeight / 11.f));
	const float MinoChecklistOriginX = MinoChecklistLeft +
		(MinoChecklistPanelWidth - MinoChecklistCellSize * 8.f) * .5f;
	const float MinoChecklistOriginY = MinoChecklistTop + MinoChecklistPanelHeight * .46f;
	// I/T, Z/S, L/J, then O centered below them. This mirrors the compact
	// two-column reference layout while preserving every tetromino's true shape.
	static const FIntPoint ChecklistOrigins[7] = {
		FIntPoint(0, 0), FIntPoint(0, 3), FIntPoint(0, 6), FIntPoint(5, 0),
		FIntPoint(3, 9), FIntPoint(5, 6), FIntPoint(5, 3)
	};
	for (int32 MinoIndex = 0; MinoIndex < 7; ++MinoIndex)
	{
		const uint8 MinoBit = static_cast<uint8>(1u << MinoIndex);
		const bool bMinoPending = bShowMinoChecklist &&
			(ObservedGame->MissionPanelRequiredMinoMask & MinoBit) != 0 &&
			(ObservedGame->MissionPanelCompletedMinoMask & MinoBit) == 0;
		const TArray<FIntPoint> Shape = TotorisGeneration::Shape(
			static_cast<ETotorisMino>(MinoIndex));
		for (int32 CellIndex = 0; CellIndex < 4; ++CellIndex)
		{
			UImage* MinoCell = MissionMinoChecklistCells[MinoIndex * 4 + CellIndex];
			MinoCell->SetVisibility(bMinoPending ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (!bMinoPending || !Shape.IsValidIndex(CellIndex)) continue;
			ApplyMissionIntro(MinoCell, ActiveMissionSlot);
			const FIntPoint& Cell = Shape[CellIndex];
			const FIntPoint& Origin = ChecklistOrigins[MinoIndex];
			MinoCell->SetColorAndOpacity(TotorisGeneration::Color(static_cast<ETotorisMino>(MinoIndex)));
			Place(MinoCell, FVector2D(
				MinoChecklistOriginX + (Origin.X + Cell.X) * MinoChecklistCellSize,
				MinoChecklistOriginY + (Origin.Y + (1 - Cell.Y)) * MinoChecklistCellSize),
				FVector2D(MinoChecklistCellSize - 1.f, MinoChecklistCellSize - 1.f), FVector2D::ZeroVector);
		}
	}

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
    const bool bShowCountdown = !bPreview && (bFinished ? FinishElapsedSeconds < 1.0 : CountdownElapsed >= 0.0 && CountdownElapsed < (bQuickStart ? 2.0 : 5.0));
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
