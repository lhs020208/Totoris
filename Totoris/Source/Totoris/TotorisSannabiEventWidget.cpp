#include "TotorisSannabiEventWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Brushes/SlateColorBrush.h"
#include "TotorisBlockGeneratorComponent.h"
#include "TotorisGeneration.h"
#include "TotorisAudioMix.h"

namespace
{
    constexpr float ReferenceWidth = 1280.f;
    constexpr float ReferenceHeight = 720.f;
    constexpr float CharacterScale = .60f;
    // All moving sections run at half the original presentation speed.  The
    // matching ballistic accelerations below are quartered, preserving the
    // intended landing range while doubling their travel time.
    constexpr float TotoriJumpSeconds = 1.48f;
    constexpr float AttackSeconds = .36f;
    constexpr float IdleSeconds = .50f;
    constexpr float GrappleAngleDegrees = 35.f;
    constexpr float GrappleSpeed = 1800.f;

    UImage* AddImage(UWidgetTree* Tree, UCanvasPanel* Parent, const TCHAR* Name)
    {
        UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass(), FName(Name));
        Parent->AddChild(Image);
        Image->SetVisibility(ESlateVisibility::Collapsed);
        return Image;
    }
}

void UTotorisSannabiEventWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::HitTestInvisible);

    TotoriTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Totoris/Objects/Textures/Characters/SannabiTotori.SannabiTotori"));
    AttackTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Totoris/Objects/Textures/Characters/SannabiAttack.SannabiAttack"));
    FallTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Totoris/Objects/Textures/Characters/SannabiFall.SannabiFall"));
    IdleTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Totoris/Objects/Textures/Characters/SannabiIdle.SannabiIdle"));
    ReadyTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Totoris/Objects/Textures/Characters/SannabiReady.SannabiReady"));
    FlyTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Totoris/Objects/Textures/Characters/SannabiFly.SannabiFly"));
    WireSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Totoris/Audio/Gameplay/SannabiWireSound.SannabiWireSound"));

    RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SannabiEventRoot"));
    WidgetTree->RootWidget = RootPanel;
    TotoriImage = AddImage(WidgetTree, RootPanel, TEXT("SannabiTotori"));
    AttackImage = AddImage(WidgetTree, RootPanel, TEXT("SannabiAttack"));
    FallImage = AddImage(WidgetTree, RootPanel, TEXT("SannabiFall"));
    IdleImage = AddImage(WidgetTree, RootPanel, TEXT("SannabiIdle"));
    ReadyImage = AddImage(WidgetTree, RootPanel, TEXT("SannabiReady"));
    FlyImage = AddImage(WidgetTree, RootPanel, TEXT("SannabiFly"));
    GrappleLine = AddImage(WidgetTree, RootPanel, TEXT("SannabiGrappleLine"));
    GrappleLine->SetBrush(FSlateColorBrush(FLinearColor(.54f, .57f, .62f, .96f)));
    GrappleLine->SetRenderTransformPivot(FVector2D(0.f, .5f));

    if (TotoriImage) TotoriImage->SetBrushFromTexture(TotoriTexture);
    if (AttackImage) AttackImage->SetBrushFromTexture(AttackTexture);
    if (FallImage) FallImage->SetBrushFromTexture(FallTexture);
    if (IdleImage) IdleImage->SetBrushFromTexture(IdleTexture);
    if (ReadyImage) ReadyImage->SetBrushFromTexture(ReadyTexture);
    if (FlyImage) FlyImage->SetBrushFromTexture(FlyTexture);
}

void UTotorisSannabiEventWidget::SetObservedGame(UTotorisBlockGeneratorComponent* InGame)
{
    ObservedGame = InGame;
}

FVector2D UTotorisSannabiEventWidget::GetScaledSize(const UTexture2D* Texture, float Scale)
{
    return IsValid(Texture)
        ? FVector2D(Texture->GetSizeX(), Texture->GetSizeY()) * Scale
        : FVector2D(100.f, 100.f) * Scale;
}

void UTotorisSannabiEventWidget::Place(UImage* Image, const FVector2D& Position, const FVector2D& Size)
{
    if (!IsValid(Image)) return;
    if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Image->Slot))
    {
        Slot->SetPosition(Position);
        Slot->SetSize(Size);
        Slot->SetAlignment(FVector2D::ZeroVector);
    }
}

void UTotorisSannabiEventWidget::SetCharacter(UImage* Image, const FVector2D& Position,
    const FVector2D& Size, bool bMirrorX)
{
    if (!IsValid(Image)) return;
    Place(Image, Position, Size);
    Image->SetRenderTransformPivot(FVector2D(.5f, .5f));
    Image->SetRenderScale(FVector2D(bMirrorX ? -1.f : 1.f, 1.f));
    Image->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UTotorisSannabiEventWidget::HideAll()
{
    for (UImage* Image : { TotoriImage.Get(), AttackImage.Get(), FallImage.Get(), IdleImage.Get(), ReadyImage.Get(), FlyImage.Get(), GrappleLine.Get() })
    {
        if (IsValid(Image)) Image->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UTotorisSannabiEventWidget::StartEvent()
{
    const FVector2D CurrentSize = GetCachedGeometry().GetLocalSize();
    if (CurrentSize.X < 10.f || CurrentSize.Y < 10.f)
    {
        // The first viewport tick will have a valid geometry; retain this state
        // and initialize there rather than assuming a desktop resolution.
        bPlaying = true;
        Phase = EPhase::TotoriJump;
        PhaseSeconds = 0.f;
        return;
    }

    ViewportSize = CurrentSize;
    VisualScale = CharacterScale * FMath::Min(ViewportSize.X / ReferenceWidth, ViewportSize.Y / ReferenceHeight);
    TotoriSize = GetScaledSize(TotoriTexture, VisualScale);
    AttackSize = GetScaledSize(AttackTexture, VisualScale);
    FallSize = GetScaledSize(FallTexture, VisualScale);
    IdleSize = GetScaledSize(IdleTexture, VisualScale);
    ReadySize = GetScaledSize(ReadyTexture, VisualScale);
    FlySize = GetScaledSize(FlyTexture, VisualScale);
    HideAll();
    bPlaying = true;
    Phase = EPhase::TotoriJump;
    PhaseSeconds = 0.f;
}

void UTotorisSannabiEventWidget::UpdateLine(const FVector2D& Start, const FVector2D& End, bool bVisible)
{
    if (!IsValid(GrappleLine)) return;
    if (!bVisible)
    {
        GrappleLine->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    const FVector2D Delta = End - Start;
    const float Length = Delta.Size();
    Place(GrappleLine, Start, FVector2D(Length, FMath::Max(2.f, 3.f * VisualScale)));
    GrappleLine->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
    GrappleLine->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UTotorisSannabiEventWidget::BeginFall()
{
    if (IsValid(TotoriImage)) TotoriImage->SetVisibility(ESlateVisibility::Collapsed);
    if (IsValid(AttackImage)) AttackImage->SetVisibility(ESlateVisibility::Collapsed);
    Phase = EPhase::Fall;
    PhaseSeconds = 0.f;
}

void UTotorisSannabiEventWidget::BeginReady()
{
    if (IsValid(IdleImage)) IdleImage->SetVisibility(ESlateVisibility::Collapsed);
    ReadyTopRight = GroundPosition + FVector2D(ReadySize.X, 0.f);
    SetCharacter(ReadyImage, ReadyTopRight - FVector2D(ReadySize.X, 0.f), ReadySize);
    // The wire effect begins with the line launch, not when Fly reaches it.
    if (IsValid(WireSound))
    {
        UGameplayStatics::PlaySound2D(this, WireSound, TotorisAudioMix::SannabiWire);
    }
    Phase = EPhase::ReadyLine;
    PhaseSeconds = 0.f;
}

void UTotorisSannabiEventWidget::BeginFly()
{
    if (IsValid(ReadyImage)) ReadyImage->SetVisibility(ESlateVisibility::Collapsed);
    FlyTopRight = ReadyTopRight;
    SetCharacter(FlyImage, FlyTopRight - FVector2D(FlySize.X, 0.f), FlySize);
    const float Angle = FMath::DegreesToRadians(GrappleAngleDegrees);
    FlyVelocity = FVector2D(FMath::Cos(Angle), -FMath::Sin(Angle)) * GrappleSpeed;
    Phase = EPhase::FlyLine;
    PhaseSeconds = 0.f;
}

void UTotorisSannabiEventWidget::FinishEvent()
{
    HideAll();
    Phase = EPhase::Finished;
    bPlaying = false;
}

float UTotorisSannabiEventWidget::GetMissionPanelLeftX() const
{
    // Use the actual projected edge of the 3D mission board.  This avoids
    // guessing from a viewport percentage, which drifts under PIE DPI scaling
    // and differs between aspect ratios.
    APlayerController* PlayerController = GetOwningPlayer();
    const AActor* BoardActor = IsValid(ObservedGame) ? ObservedGame->GetOwner() : nullptr;
    if (IsValid(PlayerController) && IsValid(BoardActor) && ObservedGame->IsMissionPanelVisible())
    {
        const float HalfWidth = .5f * TotorisGeneration::BoardWidth * ObservedGame->CellSize;
        const float HalfHeight = .5f * TotorisGeneration::BoardHeight * ObservedGame->CellSize;
        FVector2D MissionTop;
        FVector2D MissionBottom;
        const FTransform BoardTransform = BoardActor->GetActorTransform();
        if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
                PlayerController,
                BoardTransform.TransformPosition(FVector(-8.f, 170.f - HalfWidth, HalfHeight)), MissionTop, true)
            && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
                PlayerController,
                BoardTransform.TransformPosition(FVector(-8.f, 170.f + HalfWidth, -HalfHeight)), MissionBottom, true))
        {
            return FMath::Min(MissionTop.X, MissionBottom.X);
        }
    }

    // Classical mode does not have the mission board; retain a sensible line
    // destination for the temporary game-start development trigger.
    return ViewportSize.X * .64f;
}

void UTotorisSannabiEventWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!bPlaying) return;

    if (ViewportSize.X < 10.f || ViewportSize.Y < 10.f)
    {
        StartEvent();
        return;
    }

    PhaseSeconds += InDeltaTime;
    // Start remains in the lower-right quadrant, but is pulled inward so the
    // high point reads as a jump into the middle of the play screen.
    const FVector2D TotoriStart(ViewportSize.X * .84f, ViewportSize.Y + TotoriSize.Y * .15f);
    const FVector2D TotoriApex(ViewportSize.X * .52f, ViewportSize.Y * .50f);

    switch (Phase)
    {
    case EPhase::TotoriJump:
    {
        const float T = FMath::Clamp(PhaseSeconds / TotoriJumpSeconds, 0.f, 1.f);
        // Narrow, high parabola: x moves over a short span while y follows
        // y = apex + (start - apex) * (1 - t)^2.
        const FVector2D Position(
            FMath::Lerp(TotoriStart.X, TotoriApex.X, T),
            TotoriApex.Y + (TotoriStart.Y - TotoriApex.Y) * FMath::Square(1.f - T));
        SetCharacter(TotoriImage, Position, TotoriSize, true);
        // Attack enters only during the final quarter of the ascent.  It is
        // timed to meet Totori exactly at the apex rather than pausing Totori
        // there while the projectile catches up.
        constexpr float AttackStartProgress = .76f;
        if (T >= AttackStartProgress)
        {
            const float AttackT = FMath::Clamp((T - AttackStartProgress) / (1.f - AttackStartProgress), 0.f, 1.f);
            const FVector2D AttackStart(ViewportSize.X + AttackSize.X,
                TotoriApex.Y + (TotoriSize.Y - AttackSize.Y) * .5f);
            const FVector2D AttackEnd(TotoriApex.X + (TotoriSize.X - AttackSize.X) * .5f, AttackStart.Y);
            SetCharacter(AttackImage, FMath::Lerp(AttackStart, AttackEnd, AttackT), AttackSize, true);
        }
        if (T >= 1.f)
        {
            CollisionPosition = Position;
            BeginFall();
        }
        break;
    }
    case EPhase::Attack:
    {
        SetCharacter(TotoriImage, CollisionPosition, TotoriSize, true);
        const float T = FMath::Clamp(PhaseSeconds / AttackSeconds, 0.f, 1.f);
        const FVector2D AttackStart(ViewportSize.X + AttackSize.X, CollisionPosition.Y + (TotoriSize.Y - AttackSize.Y) * .5f);
        const FVector2D AttackEnd(CollisionPosition.X + (TotoriSize.X - AttackSize.X) * .5f, AttackStart.Y);
        SetCharacter(AttackImage, FMath::Lerp(AttackStart, AttackEnd, T), AttackSize, true);
        if (T >= 1.f) BeginFall();
        break;
    }
    case EPhase::Fall:
    {
        // A touch heavier than the globally slowed ballistic tuning so Fall
        // reads as a deliberate drop instead of a prolonged float.
        const float Gravity = 350.f * VisualScale;
        const float HorizontalSpeed = 215.f * VisualScale;
        const FVector2D Position = CollisionPosition + FVector2D(-HorizontalSpeed * PhaseSeconds, .5f * Gravity * FMath::Square(PhaseSeconds));
        SetCharacter(FallImage, Position, FallSize);
        if (Position.Y + FallSize.Y >= ViewportSize.Y)
        {
            GroundPosition = FVector2D(Position.X, ViewportSize.Y - IdleSize.Y);
            if (IsValid(FallImage)) FallImage->SetVisibility(ESlateVisibility::Collapsed);
            SetCharacter(IdleImage, GroundPosition, IdleSize);
            Phase = EPhase::Idle;
            PhaseSeconds = 0.f;
        }
        break;
    }
    case EPhase::Idle:
        SetCharacter(IdleImage, GroundPosition, IdleSize);
        if (PhaseSeconds >= IdleSeconds) BeginReady();
        break;
    case EPhase::ReadyLine:
    {
        const float Angle = FMath::DegreesToRadians(GrappleAngleDegrees);
        const FVector2D Direction(FMath::Cos(Angle), -FMath::Sin(Angle));
        const float MissionPanelLeftX = GetMissionPanelLeftX();
        const float TargetDistance = FMath::Max(0.f, (MissionPanelLeftX - ReadyTopRight.X) / Direction.X);
        const float CurrentDistance = FMath::Min(TargetDistance, PhaseSeconds * GrappleSpeed);
        LineEnd = ReadyTopRight + Direction * CurrentDistance;
        UpdateLine(ReadyTopRight, LineEnd, true);
        if (CurrentDistance >= TargetDistance) BeginFly();
        break;
    }
    case EPhase::FlyLine:
    {
        const FVector2D PreviousTopRight = FlyTopRight;
        FlyTopRight += FlyVelocity * InDeltaTime;
        SetCharacter(FlyImage, FlyTopRight - FVector2D(FlySize.X, 0.f), FlySize);
        UpdateLine(FlyTopRight, LineEnd, true);
        const float Total = (LineEnd - ReadyTopRight).Size();
        if ((FlyTopRight - ReadyTopRight).Size() >= Total || FVector2D::DotProduct(FlyTopRight - LineEnd, FlyVelocity) >= 0.f)
        {
            FlyTopRight = LineEnd;
            SetCharacter(FlyImage, FlyTopRight - FVector2D(FlySize.X, 0.f), FlySize);
            UpdateLine(FlyTopRight, LineEnd, false);
            Phase = EPhase::FlyArc;
            PhaseSeconds = 0.f;
        }
        break;
    }
    case EPhase::FlyArc:
    {
        const float Gravity = 387.5f * VisualScale;
        const FVector2D Position = LineEnd + FlyVelocity * PhaseSeconds + FVector2D(0.f, .5f * Gravity * FMath::Square(PhaseSeconds));
        SetCharacter(FlyImage, Position - FVector2D(FlySize.X, 0.f), FlySize);
        if (Position.X - FlySize.X > ViewportSize.X || Position.Y > ViewportSize.Y + FlySize.Y)
        {
            FinishEvent();
        }
        break;
    }
    default:
        break;
    }
}
