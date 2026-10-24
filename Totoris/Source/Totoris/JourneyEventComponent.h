#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JourneyEventComponent.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UDynamicMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTotorisBlockGeneratorComponent;
class USoundWave;
class UAudioComponent;

/** Gallery-only Journey vignette. No skeletal animation is required. */
UCLASS(ClassGroup=(Totoris))
class TOTORIS_API UJourneyEventComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UJourneyEventComponent();
    bool PlayEvent(UTotorisBlockGeneratorComponent* Game);
    void StopEvent();
    bool IsEventPlaying() const { return bPlaying; }
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    FVector Path(float Seconds) const;
    FVector ScreenPoint(float X, float Y) const;
    void UpdatePresentation();
    void UpdateCloth(const FVector& TravelerCenter, float Fade);
    FQuat Pose(float Seconds) const;
    void UpdateAudio();
    UAudioComponent* StartSound(int32 Index, float Volume, float StartTime = 0.f);

    UPROPERTY() TArray<TObjectPtr<USoundWave>> Sounds;
    UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> AudioComponents;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> WindAudio;
    int32 NextAudioCue = 0;

    UPROPERTY() TObjectPtr<UStaticMesh> TravelerAsset;
    UPROPERTY() TObjectPtr<UMaterialInterface> TravelerMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> ClothMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> GoldMaterial;
    UPROPERTY(Transient) TObjectPtr<AActor> Presentation;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Traveler;
    UPROPERTY(Transient) TObjectPtr<UDynamicMeshComponent> Cloth;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
    TWeakObjectPtr<UTotorisBlockGeneratorComponent> Board;
    float Elapsed = 0.f;
    float HalfWidth = 1.f;
    float HalfHeight = 1.f;
    float TravelerHeight = 1.f;
    bool bPlaying = false;
    static constexpr float Duration = 11.f;
};
