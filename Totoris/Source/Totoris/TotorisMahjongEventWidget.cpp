#include "TotorisMahjongEventWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	// Deliberately long enough to read as a rising firework rather than a flash.
	// A quick launch with a pronounced ease-out makes the firework decelerate
	// noticeably as it settles at the centre point.
	constexpr float RiseSeconds = 1.45f;
	constexpr float HoldSeconds = .50f;
	constexpr float ExplodeSeconds = .32f;
	constexpr float ExplosionSoundOffset = .70f;
	constexpr float TileFlightDuration = .86f;
	constexpr float ReferenceWidth = 1600.f;
	constexpr float ReferenceHeight = 1125.f;

	UImage* AddImage(UWidgetTree* Tree, UCanvasPanel* Parent, const TCHAR* Name)
	{
		UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass(), FName(Name));
		Parent->AddChild(Image);
		Image->SetVisibility(ESlateVisibility::Collapsed);
		Image->SetRenderTransformPivot(FVector2D(.5f, .5f));
		return Image;
	}

	UTexture2D* CreateFireworkTexture(bool bRibbon = false)
	{
		constexpr int32 TextureSize = 256;
		UTexture2D* Texture = UTexture2D::CreateTransient(TextureSize, TextureSize, PF_B8G8R8A8);
		if (!IsValid(Texture) || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.IsEmpty()) return nullptr;

		Texture->SRGB = true;
		Texture->Filter = TF_Bilinear;
		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		FColor* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		for (int32 Y = 0; Y < TextureSize; ++Y)
		{
			for (int32 X = 0; X < TextureSize; ++X)
			{
				const FVector2f UV((X + .5f) / TextureSize - .5f, (Y + .5f) / TextureSize - .5f);
				const float Radius = UV.Length() * 2.f;
				// Keep the smaller projectile readable: the inner 55% of its
				// radius is opaque, with glow confined to the outer portion.
				const float FeatherT = FMath::Clamp((Radius - .55f) / .45f, 0.f, 1.f);
				float Alpha = 1.f - FMath::Pow(FeatherT, 1.75f);
				if (bRibbon)
				{
					// Top is the fresh exhaust; the old tail narrows and dissolves.
					const float Along = (Y + .5f) / TextureSize;
					const float Taper = FMath::Pow(1.f - Along, .7f);
					const float Width = .05f + .32f * Taper;
					const float Side = UV.X / Width;
					const float Wisps = .90f + .10f * FMath::Sin(Along * 39.f + UV.X * 13.f);
					Alpha = FMath::Exp(-Side * Side * 5.f) * FMath::Pow(1.f - Along, 1.4f) * Wisps;
				}
				Pixels[Y * TextureSize + X] = FColor(255, 255, 255, FMath::RoundToInt(Alpha * 255.f));
			}
		}
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}
}

void UTotorisMahjongEventWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MahjongEventRoot"));
	WidgetTree->RootWidget = RootPanel;
	FireworkTexture = CreateFireworkTexture();
	TrailTexture = CreateFireworkTexture(true);
	MahjongSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Totoris/Audio/Gameplay/MahjongSound.MahjongSound"));

	FireworkCore = AddImage(WidgetTree, RootPanel, TEXT("MahjongFireworkCore"));
	FireworkCore->SetBrushFromTexture(FireworkTexture);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(FireworkCore->Slot)) CanvasSlot->SetZOrder(20);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UImage* Trail = AddImage(WidgetTree, RootPanel, *FString::Printf(TEXT("MahjongFireworkTrail_%d"), Index));
		Trail->SetBrushFromTexture(TrailTexture);
		FireworkTrails.Add(Trail);
	}

	static const TCHAR* TileNames[] = { TEXT("1m"), TEXT("1p"), TEXT("1s"), TEXT("9m"), TEXT("9p"), TEXT("9s"),
		TEXT("e"), TEXT("g"), TEXT("n"), TEXT("r"), TEXT("s"), TEXT("w"), TEXT("wh") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(TileNames); ++Index)
	{
		const FString Name(TileNames[Index]);
		TileTextures.Add(LoadObject<UTexture2D>(nullptr,
			*FString::Printf(TEXT("/Game/Totoris/Objects/Textures/Mahjong/%s.%s"), *Name, *Name)));
		UImage* Tile = AddImage(WidgetTree, RootPanel, *FString::Printf(TEXT("MahjongTile_%02d"), Index));
		Tile->SetBrushFromTexture(TileTextures.Last());
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Tile->Slot)) CanvasSlot->SetZOrder(10);
		TileImages.Add(Tile);
	}

	static const TCHAR* EmblemNames[] = { TEXT("Koku"), TEXT("Shi"), TEXT("Mu"), TEXT("Sou") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(EmblemNames); ++Index)
	{
		const FString Name(EmblemNames[Index]);
		EmblemTextures.Add(LoadObject<UTexture2D>(nullptr,
			*FString::Printf(TEXT("/Game/Totoris/Objects/Textures/Mahjong/%s.%s"), *Name, *Name)));
		UImage* Emblem = AddImage(WidgetTree, RootPanel, *FString::Printf(TEXT("MahjongKokushiEmblem_%02d"), Index));
		Emblem->SetBrushFromTexture(EmblemTextures.Last());
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Emblem->Slot)) CanvasSlot->SetZOrder(30);
		EmblemImages.Add(Emblem);
	}
}

void UTotorisMahjongEventWidget::Place(UImage* Image, const FVector2D& Center, const FVector2D& Size)
{
	if (!IsValid(Image)) return;
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image->Slot))
	{
		CanvasSlot->SetPosition(Center);
		CanvasSlot->SetSize(Size);
		CanvasSlot->SetAlignment(FVector2D(.5f, .5f));
	}
}

void UTotorisMahjongEventWidget::InitializeForViewport(const FVector2D& InViewportSize)
{
	ViewportSize = InViewportSize;
	FireworkStart = FVector2D(ViewportSize.X * .5f, ViewportSize.Y * .88f);
	FireworkPeak = FVector2D(ViewportSize.X * .5f, ViewportSize.Y * .5f);
	const float TileHeight = ViewportSize.Y * .145f;
	const UTexture2D* FirstTexture = TileTextures.IsValidIndex(0) ? TileTextures[0].Get() : nullptr;
	const float Aspect = IsValid(FirstTexture) ? static_cast<float>(FirstTexture->GetSizeX()) / FirstTexture->GetSizeY() : .62f;
	TileSize = FVector2D(TileHeight * Aspect, TileHeight);

	// Centers sampled from the supplied 1600x1125 composition. They deliberately
	// leave the play board and its input statistics unobstructed.
	static const FVector2D ReferenceCenters[] = {
		{ 276.f, 118.f }, { 775.f, 105.f }, { 1034.f, 125.f }, { 1534.f, 123.f },
		{ 176.f, 378.f }, { 1503.f, 493.f }, { 206.f, 610.f }, { 125.f, 768.f },
		{ 301.f, 906.f }, { 801.f, 984.f }, { 1034.f, 946.f }, { 1294.f, 906.f }, { 1502.f, 704.f }
	};
	TileDestinations.Reset();
	for (const FVector2D& ReferenceCenter : ReferenceCenters)
	{
		TileDestinations.Add(FVector2D(ReferenceCenter.X / ReferenceWidth * ViewportSize.X,
			ReferenceCenter.Y / ReferenceHeight * ViewportSize.Y));
	}

	static const FVector2D EmblemReferenceCenters[] = {
		{ 290.f, 305.f }, { 290.f, 830.f }, { 1310.f, 305.f }, { 1310.f, 830.f }
	};
	for (int32 Index = 0; Index < EmblemImages.Num(); ++Index)
	{
		const UTexture2D* Texture = EmblemTextures.IsValidIndex(Index) ? EmblemTextures[Index].Get() : nullptr;
		const float EmblemAspect = IsValid(Texture) ? static_cast<float>(Texture->GetSizeX()) / Texture->GetSizeY() : 1.5f;
		const float Width = ViewportSize.X * .31f;
		const FVector2D Centre(EmblemReferenceCenters[Index].X / ReferenceWidth * ViewportSize.X,
			EmblemReferenceCenters[Index].Y / ReferenceHeight * ViewportSize.Y);
		Place(EmblemImages[Index], Centre, FVector2D(Width, Width / EmblemAspect));
	}
}

void UTotorisMahjongEventWidget::SetCircle(const FVector2D& Center, float Scale, float Opacity, bool bTrail)
{
	UImage* Image = bTrail ? nullptr : FireworkCore.Get();
	if (!bTrail && IsValid(Image))
	{
		const float Diameter = ViewportSize.X * .0575f * Scale;
		Place(Image, Center, FVector2D(Diameter));
		Image->SetColorAndOpacity(FLinearColor::White);
		Image->SetRenderOpacity(Opacity);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UTotorisMahjongEventWidget::StartEvent()
{
	bPlaying = true;
	bTilesLaunched = false;
	Phase = EPhase::Rise;
	PhaseSeconds = 0.f;
	TileFlightSeconds = 0.f;
	EmblemSeconds = 0.f;
	const FVector2D Size = GetCachedGeometry().GetLocalSize();
	if (Size.X >= 10.f && Size.Y >= 10.f) InitializeForViewport(Size);
	// A replay starts from a clean center burst; completed tiles remain visible
	// only between events.
	for (UImage* Image : TileImages) if (IsValid(Image)) Image->SetVisibility(ESlateVisibility::Collapsed);
	for (UImage* Image : FireworkTrails) if (IsValid(Image)) Image->SetVisibility(ESlateVisibility::Collapsed);
	for (UImage* Image : EmblemImages) if (IsValid(Image)) Image->SetVisibility(ESlateVisibility::Collapsed);
	if (IsValid(FireworkCore)) FireworkCore->SetVisibility(ESlateVisibility::Collapsed);
}

void UTotorisMahjongEventWidget::TickEmblems(float DeltaTime)
{
	EmblemSeconds += DeltaTime;
	const float Delays[] = { KokuSpawnDelay, ShiSpawnDelay, MuSpawnDelay, SouSpawnDelay };
	for (int32 Index = 0; Index < EmblemImages.Num() && Index < UE_ARRAY_COUNT(Delays); ++Index)
	{
		if (IsValid(EmblemImages[Index]) && EmblemSeconds >= Delays[Index])
		{
			EmblemImages[Index]->SetRenderOpacity(1.f);
			EmblemImages[Index]->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void UTotorisMahjongEventWidget::BeginTileFlight()
{
	bTilesLaunched = true;
	TileFlightSeconds = 0.f;
	for (UImage* Tile : TileImages)
	{
		if (IsValid(Tile)) Tile->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UTotorisMahjongEventWidget::TickTiles(float DeltaTime)
{
	if (!bTilesLaunched) return;
	TileFlightSeconds += DeltaTime;
	const float T = FMath::Clamp(TileFlightSeconds / TileFlightDuration, 0.f, 1.f);
	// Quartic ease-out makes the early flight brisk and the last approach brake hard.
	const float EasedT = 1.f - FMath::Pow(1.f - T, 4.f);
	for (int32 Index = 0; Index < TileImages.Num() && TileDestinations.IsValidIndex(Index); ++Index)
	{
		Place(TileImages[Index], FMath::Lerp(FireworkPeak, TileDestinations[Index], EasedT), TileSize);
	}
}

void UTotorisMahjongEventWidget::FinishEvent()
{
	bPlaying = false;
	Phase = EPhase::Finished;
	for (UImage* Image : FireworkTrails) if (IsValid(Image)) Image->SetVisibility(ESlateVisibility::Collapsed);
	if (IsValid(FireworkCore)) FireworkCore->SetVisibility(ESlateVisibility::Collapsed);
}

void UTotorisMahjongEventWidget::UpdateFireworkTrail(float FlightTime)
{
	// Evaluate historical positions in seconds so the tail follows the actual
	// launch trajectory independently of frame rate, and expires after stopping.
	const auto PositionAt = [this](float Time)
	{
		const float T = FMath::Clamp(Time / RiseSeconds, 0.f, 1.f);
		return FMath::Lerp(FireworkStart, FireworkPeak, 1.f - FMath::Pow(1.f - T, 4.f));
	};
	const FVector2D Head = PositionAt(FlightTime);
	const FVector2D Tail = PositionAt(FMath::Max(0.f, FlightTime - .85f));
	const float Length = Tail.Y - Head.Y;
	const float Fade = FMath::Clamp(Length / (ViewportSize.Y * .035f), 0.f, 1.f);
	for (int32 Index = 0; Index < FireworkTrails.Num(); ++Index)
	{
		UImage* Trail = FireworkTrails[Index];
		const bool bGlow = Index == 0;
		Place(Trail, (Head + Tail) * .5f,
			FVector2D(ViewportSize.X * (bGlow ? .024f : .007f), FMath::Max(Length, 1.f)));
		Trail->SetColorAndOpacity(bGlow ? FLinearColor(.78f, .9f, 1.f, 1.f) : FLinearColor::White);
		Trail->SetRenderOpacity(Fade * (bGlow ? .38f : .95f));
		Trail->SetVisibility(Length > .5f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UTotorisMahjongEventWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bPlaying) return;
	const FVector2D CurrentSize = MyGeometry.GetLocalSize();
	if (ViewportSize.X < 10.f || ViewportSize.Y < 10.f)
	{
		if (CurrentSize.X >= 10.f && CurrentSize.Y >= 10.f) InitializeForViewport(CurrentSize);
		else return;
	}

	PhaseSeconds += InDeltaTime;
	if (Phase == EPhase::Rise)
	{
		const float T = FMath::Clamp(PhaseSeconds / RiseSeconds, 0.f, 1.f);
		// Launch quickly, then bleed speed aggressively near the centre.
		const float Travel = 1.f - FMath::Pow(1.f - T, 4.f);
		const FVector2D Position(FireworkStart.X,
			FireworkStart.Y - (FireworkStart.Y - FireworkPeak.Y) * Travel);
		SetCircle(Position, 1.f, 1.f, false);
		UpdateFireworkTrail(PhaseSeconds);
		if (T >= 1.f) { Phase = EPhase::Hold; PhaseSeconds = 0.f; }
	}
	else if (Phase == EPhase::Hold)
	{
		SetCircle(FireworkPeak, 1.f, 1.f, false);
		UpdateFireworkTrail(RiseSeconds + PhaseSeconds);
		// Preserve the previous sound-start point; hold the orb until its new cue.
		if (PhaseSeconds >= HoldSeconds + ExplodeSeconds)
		{
			if (IsValid(MahjongSound)) UGameplayStatics::PlaySound2D(this, MahjongSound);
			Phase = EPhase::SoundSequence;
			PhaseSeconds = 0.f;
			EmblemSeconds = 0.f;
		}
	}
	else if (Phase == EPhase::SoundSequence)
	{
		// One uninterrupted clock from sound start drives both the burst and
		// emblems. Never reset it when the burst finishes.
		TickEmblems(InDeltaTime);
		UpdateFireworkTrail(RiseSeconds + HoldSeconds + ExplodeSeconds + EmblemSeconds);
		const float BurstElapsed = EmblemSeconds - ExplosionSoundOffset;
		if (BurstElapsed < 0.f)
		{
			SetCircle(FireworkPeak, 1.f, 1.f, false);
		}
		else
		{
			if (!bTilesLaunched) BeginTileFlight();
			const float T = FMath::Clamp(BurstElapsed / ExplodeSeconds, 0.f, 1.f);
			SetCircle(FireworkPeak, 1.f + 4.f * T, 1.f - T, false);
			// Include only time after the .70-second crossing, even on a slow frame.
			TileFlightSeconds = BurstElapsed;
			TickTiles(0.f);
		}
		// 2.25 seconds is the final authored sound cue.  It intentionally does
		// not summon an image, but keeps the preceding 2.10-second cue reachable.
		if (EmblemSeconds >= 2.25f) FinishEvent();
	}
}
