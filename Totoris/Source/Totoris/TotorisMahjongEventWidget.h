#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TotorisMahjongEventWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTexture2D;
class USoundBase;

/** Fullscreen mahjong-firework presentation.  All card destinations use normalized
 * viewport coordinates so they retain the authored safe regions at any resolution. */
UCLASS()
class TOTORIS_API UTotorisMahjongEventWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void StartEvent();
	bool IsEventPlaying() const { return bPlaying; }

	// Seconds from the MahjongSound start, independently adjustable for each call.
	UPROPERTY(EditAnywhere, Category = "Mahjong Event|Kokushi") float KokuSpawnDelay = 1.62f;
	UPROPERTY(EditAnywhere, Category = "Mahjong Event|Kokushi") float ShiSpawnDelay = 1.84f;
	UPROPERTY(EditAnywhere, Category = "Mahjong Event|Kokushi") float MuSpawnDelay = 1.96f;
	UPROPERTY(EditAnywhere, Category = "Mahjong Event|Kokushi") float SouSpawnDelay = 2.10f;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	enum class EPhase : uint8 { Rise, Hold, SoundSequence, Finished };

	static void Place(UImage* Image, const FVector2D& Center, const FVector2D& Size);
	void InitializeForViewport(const FVector2D& InViewportSize);
	void SetCircle(const FVector2D& Center, float Scale, float Opacity, bool bTrail);
	void BeginTileFlight();
	void TickTiles(float DeltaTime);
	void TickEmblems(float DeltaTime);
	void FinishEvent();
	void UpdateFireworkTrail(float FlightTime);

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootPanel;
	UPROPERTY(Transient) TObjectPtr<UImage> FireworkCore;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> FireworkTrails;
	// A transient radial-alpha texture guarantees that the rising projectile is
	// visible even when no imported UI brush has been assigned in the editor.
	UPROPERTY(Transient) TObjectPtr<UTexture2D> FireworkTexture;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> TrailTexture;
	UPROPERTY(Transient) TObjectPtr<USoundBase> MahjongSound;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> TileImages;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> TileTextures;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> EmblemImages;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> EmblemTextures;

	EPhase Phase = EPhase::Finished;
	bool bPlaying = false;
	bool bTilesLaunched = false;
	float PhaseSeconds = 0.f;
	float TileFlightSeconds = 0.f;
	float EmblemSeconds = 0.f;
	FVector2D ViewportSize = FVector2D::ZeroVector;
	FVector2D FireworkStart;
	FVector2D FireworkPeak;
	FVector2D TileSize;
	TArray<FVector2D> TileDestinations;
};
