#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TotorisMahjongEventWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTexture2D;
class USoundBase;
class UAudioComponent;

/** Fullscreen mahjong-firework presentation.  All card destinations use normalized
 * viewport coordinates so they retain the authored safe regions at any resolution. */
UCLASS()
class TOTORIS_API UTotorisMahjongEventWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void StartEvent();
	bool IsEventPlaying() const { return bPlaying; }

	// Pixels per second at the 1600-wide reference resolution, after braking.
	UPROPERTY(EditAnywhere, Category = "Mahjong Event|Tiles", meta = (ClampMin = "0.0"))
	float TileDriftSpeed = 6.f;
	UPROPERTY(EditAnywhere, Category = "Mahjong Event|Tiles", meta = (ClampMin = "0.0"))
	float TileDisappearInterval = .06f;
	// Seconds from sound start; includes a small delay after the final sound cue.
	UPROPERTY(EditAnywhere, Category = "Mahjong Event|Tiles", meta = (ClampMin = "0.0"))
	float TileDisappearStartDelay = 2.55f;

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
	void TickEmblemFade();
	void FinishEvent();
	void UpdateFireworkTrail(float FlightTime);
	void TickTileRemoval();
	void RemoveTileWithParticles(int32 TileIndex, float BirthTime);
	bool HasLiveTileParticles() const;

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootPanel;
	UPROPERTY(Transient) TObjectPtr<UImage> FireworkCore;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> FireworkTrails;
	// A transient radial-alpha texture guarantees that the rising projectile is
	// visible even when no imported UI brush has been assigned in the editor.
	UPROPERTY(Transient) TObjectPtr<UTexture2D> FireworkTexture;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> TrailTexture;
	UPROPERTY(Transient) TObjectPtr<USoundBase> MahjongSound;
	UPROPERTY(Transient) TObjectPtr<USoundBase> MahjongFireSound;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> MahjongAudioComponent;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> TileImages;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> TileTextures;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> EmblemImages;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> EmblemTextures;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> TileParticleImages;
	struct FTileParticle
	{
		FVector2D Origin;
		FVector2D Velocity;
		float Size;
		float BirthTime;
		float Lifetime;
	};
	TArray<FTileParticle> TileParticles;
	TArray<FVector2D> TilePositions;
	TArray<FVector2D> EmblemStartPositions;
	TArray<FVector2D> EmblemSizes;
	TArray<int32> TileRemovalOrder;
	int32 NextTileRemoval = 0;

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
