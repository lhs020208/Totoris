#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TotorisSannabiEventWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTexture2D;
class USoundBase;
class UTotorisBlockGeneratorComponent;

// A self-contained, non-interactive fullscreen presentation layer.  It uses
// viewport-space coordinates deliberately: this event is a 2D cut-in, not a
// world-space object and must remain above every gameplay/menu widget.
UCLASS()
class TOTORIS_API UTotorisSannabiEventWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetObservedGame(UTotorisBlockGeneratorComponent* InGame);
    void StartEvent();
    bool IsEventPlaying() const { return bPlaying; }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    enum class EPhase : uint8
    {
        TotoriJump,
        Attack,
        Fall,
        Idle,
        ReadyLine,
        FlyLine,
        FlyArc,
        Finished
    };

    static void Place(UImage* Image, const FVector2D& Position, const FVector2D& Size);
    static FVector2D GetScaledSize(const UTexture2D* Texture, float Scale);
    void SetCharacter(UImage* Image, const FVector2D& Position, const FVector2D& Size, bool bMirrorX = false);
    void HideAll();
    void UpdateLine(const FVector2D& Start, const FVector2D& End, bool bVisible);
    void BeginFall();
    void BeginReady();
    void BeginFly();
    void FinishEvent();
    float GetMissionPanelLeftX() const;
    void TickHitAnimation(float DeltaSeconds);

    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootPanel;
    UPROPERTY(Transient) TObjectPtr<UTotorisBlockGeneratorComponent> ObservedGame;
    UPROPERTY(Transient) TObjectPtr<UImage> TotoriImage;
    UPROPERTY(Transient) TObjectPtr<UImage> AttackImage;
    UPROPERTY(Transient) TObjectPtr<UImage> FallImage;
    UPROPERTY(Transient) TObjectPtr<UImage> IdleImage;
    UPROPERTY(Transient) TObjectPtr<UImage> ReadyImage;
    UPROPERTY(Transient) TObjectPtr<UImage> FlyImage;
    UPROPERTY(Transient) TObjectPtr<UImage> HitImage;
    UPROPERTY(Transient) TObjectPtr<UImage> GrappleLine;

    UPROPERTY(Transient) TObjectPtr<UTexture2D> TotoriTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> AttackTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> FallTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> IdleTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> ReadyTexture;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> FlyTexture;
    UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> HitTextures;
    UPROPERTY(Transient) TObjectPtr<USoundBase> WireSound;
    UPROPERTY(Transient) TObjectPtr<USoundBase> DriftSound;
    UPROPERTY(Transient) TObjectPtr<USoundBase> HitSound;

    EPhase Phase = EPhase::Finished;
    bool bPlaying = false;
    float PhaseSeconds = 0.f;
    FVector2D ViewportSize = FVector2D::ZeroVector;
    float VisualScale = 1.f;
    FVector2D TotoriSize;
    FVector2D AttackSize;
    FVector2D FallSize;
    FVector2D IdleSize;
    FVector2D ReadySize;
    FVector2D FlySize;
    FVector2D HitSize;
    FVector2D CollisionPosition;
    FVector2D HitPosition;
    FVector2D GroundPosition;
    float IdleBrakeInitialVelocityX = 0.f;
    FVector2D ReadyTopRight;
    FVector2D LineEnd;
    FVector2D FlyTopRight;
    FVector2D FlyVelocity;
    float HitAnimationElapsed = 0.f;
    bool bHitAnimationPlaying = false;
};
