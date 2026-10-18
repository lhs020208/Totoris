#include "TotorisReAnimalEventWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UTotorisReAnimalEventWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ReAnimalRoot"));
	WidgetTree->RootWidget = Root;
	Root->SetClipping(EWidgetClipping::ClipToBounds);
	LoadTextures();
	// Four mirrored tiles cover a two-tile scroll cycle without exposing seams.
	// Slice the opaque base into strips so its upper edge blends into the scene.
	for (int32 Layer = 0; Layer < 4; ++Layer)
	{
		const int32 Bands = Layer == 0 ? 24 : 1;
		for (int32 Tile = 0; Tile < 4; ++Tile)
		{
			for (int32 Band = 0; Band < Bands; ++Band)
			{
				UImage* Water = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
					*FString::Printf(TEXT("ReAnimalWater_%d_%d_%d"), Layer, Tile, Band));
				UCanvasPanelSlot* WaterSlot = Root->AddChildToCanvas(Water);
				WaterSlot->SetZOrder(Layer == 3 ? 20 : Layer);
				Water->SetRenderTransformPivot(FVector2D(.5f, .5f));
				Water->SetRenderScale(FVector2D(Tile % 2 ? -1.f : 1.f, 1.f));
				Water->SetVisibility(ESlateVisibility::Collapsed);
				WaterImages.Add(Water);
			}
		}
	}
	BoatImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ReAnimalBoat"));
	Root->AddChildToCanvas(BoatImage)->SetZOrder(10);
	BoatImage->SetRenderTransformPivot(FVector2D(.5f, .72f));
	BoatImage->SetVisibility(ESlateVisibility::Collapsed);
	if (IsValid(BoatTexture)) BoatImage->SetBrushFromTexture(BoatTexture);
}

void UTotorisReAnimalEventWidget::LoadTextures()
{
	if (!IsValid(BoatTexture))
		BoatTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Totoris/Objects/Textures/Reanimal/ReAnimal.ReAnimal"));
	static const TCHAR* Names[] = { TEXT("ReAnimal_OceanBase"), TEXT("ReAnimal_DistantWaveA"),
		TEXT("ReAnimal_DistantWaveB"), TEXT("ReAnimal_ForegroundWave") };
	WaterTextures.SetNum(4);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (!IsValid(WaterTextures[Index]))
			WaterTextures[Index] = LoadObject<UTexture2D>(nullptr,
				*FString::Printf(TEXT("/Game/Totoris/Objects/Textures/Reanimal/%s.%s"), Names[Index], Names[Index]));
	}
}

void UTotorisReAnimalEventWidget::HidePresentation()
{
	if (IsValid(BoatImage)) BoatImage->SetVisibility(ESlateVisibility::Collapsed);
	for (UImage* Water : WaterImages) if (IsValid(Water)) Water->SetVisibility(ESlateVisibility::Collapsed);
}

void UTotorisReAnimalEventWidget::UpdateWater(const FVector2D& View, float Surface, float Wave)
{
	const float Tops[] = { .85f, .83f, .85f, .865f };
	const float Heights[] = { .24f, .19f, .22f, .27f };
	const float Speeds[] = { .009f, -.022f, .032f, -.045f };
	const float Opacities[] = { 1.f, .40f, .28f, .96f };
	int32 ImageIndex = 0;
	for (int32 Layer = 0; Layer < 4; ++Layer)
	{
		const int32 Bands = Layer == 0 ? 24 : 1;
		const float TileWidth = View.X * (Layer == 2 ? 1.1f : .95f);
		const float Height = View.Y * Heights[Layer];
		const float Cycle = 2.f * TileWidth;
		const float Offset = FMath::Fmod(FMath::Fmod(ElapsedSeconds * View.X * Speeds[Layer], Cycle) + Cycle, Cycle);
		const float Top = View.Y * (Tops[Layer] + (1.f - Surface) * .20f)
			+ Surface * View.Y * .004f * FMath::Sin(Wave * (.72f + Layer * .11f) + Layer);
		for (int32 Tile = 0; Tile < 4; ++Tile)
		{
			for (int32 Band = 0; Band < Bands; ++Band)
			{
				UImage* Water = WaterImages[ImageIndex++];
				if (!IsValid(WaterTextures[Layer])) continue;
				FSlateBrush Brush = Water->GetBrush();
				Brush.SetResourceObject(WaterTextures[Layer]);
				Brush.DrawAs = ESlateBrushDrawType::Image;
				Brush.SetUVRegion(FBox2D(FVector2D(0.f, static_cast<float>(Band) / Bands),
					FVector2D(1.f, static_cast<float>(Band + 1) / Bands)));
				Water->SetBrush(Brush);
				if (UCanvasPanelSlot* WaterSlot = Cast<UCanvasPanelSlot>(Water->Slot))
				{
					WaterSlot->SetPosition(FVector2D(Tile * TileWidth - Offset, Top + Height * Band / Bands));
					WaterSlot->SetSize(FVector2D(TileWidth + .5f, Height / Bands + .5f));
				}
				const float Edge = Layer == 0 ? FMath::Clamp((Band + .5f) / 5.f, 0.f, 1.f) : 1.f;
				Water->SetRenderOpacity(Surface * Opacities[Layer] * Edge);
				Water->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
	}
}

void UTotorisReAnimalEventWidget::StartEvent()
{
	ElapsedSeconds = 0.f;
	bPlaying = false;
	if (!IsValid(BoatImage)) return;
	HidePresentation();
	// Retry for assets imported after the widget was initialized.
	LoadTextures();
	if (!IsValid(BoatTexture))
	{
		UE_LOG(LogTemp, Warning, TEXT("ReAnimal event: missing /Game/Totoris/Objects/Textures/Reanimal/ReAnimal"));
		return;
	}
	BoatImage->SetBrushFromTexture(BoatTexture);
	for (int32 Index = 0; Index < WaterTextures.Num(); ++Index)
		if (!IsValid(WaterTextures[Index])) UE_LOG(LogTemp, Warning, TEXT("ReAnimal water layer %d could not be loaded."), Index);
	bPlaying = true;
}

void UTotorisReAnimalEventWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bPlaying || !IsValid(BoatImage) || !IsValid(BoatTexture)) return;
	const FVector2D View = MyGeometry.GetLocalSize();
	if (View.X < 10.f || View.Y < 10.f) return;
	ElapsedSeconds += InDeltaTime;
	const float Duration = FMath::Max(1.f, DurationSeconds);
	const float T = FMath::Clamp(ElapsedSeconds / Duration, 0.f, 1.f);
	if (T >= 1.f)
	{
		bPlaying = false;
		HidePresentation();
		return;
	}
	const auto Smooth = [](float Value)
	{
		const float X = FMath::Clamp(Value, 0.f, 1.f);
		return X * X * (3.f - 2.f * X);
	};
	// Spread emergence and recession over the first/last third of the crossing.
	const float Surface = Smooth(T / .32f) * Smooth((1.f - T) / .32f);
	const float Aspect = static_cast<float>(BoatTexture->GetSizeX()) / FMath::Max(1, BoatTexture->GetSizeY());
	const float Width = FMath::Min(View.X * .42f, View.Y * .26f * Aspect);
	const FVector2D Size(Width, Width / Aspect);
	const float Wave = 2.f * PI * ElapsedSeconds / 2.8f;
	UpdateWater(View, Surface, Wave);
	// Analytic wave displacement gives continuous velocity, with brief backward drift.
	const float Surge = Surface * (.035f * FMath::Sin(Wave) + .006f * FMath::Sin(Wave * 1.61f));
	const float Bob = Surface * View.Y * (.010f * FMath::Sin(Wave + .7f)
		+ .004f * FMath::Sin(Wave * 1.73f));
	const FVector2D Center(
		// Include the full image width so entry/exit happen through the side edges.
		FMath::Lerp(View.X + Size.X * .5f + View.X * .03f,
			-Size.X * .5f - View.X * .03f, T) + View.X * Surge,
		// Only a shallow vertical arc; the boat no longer rises from below frame.
		View.Y - Size.Y * .42f + View.Y * (.02f + .055f * (1.f - Surface)) + Bob);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(BoatImage->Slot))
	{
		CanvasSlot->SetAlignment(FVector2D(.5f, .5f));
		CanvasSlot->SetPosition(Center);
		CanvasSlot->SetSize(Size);
	}
	BoatImage->SetRenderTransformAngle(Surface * (2.2f * FMath::Sin(Wave + 1.3f)
		+ .65f * FMath::Sin(Wave * 1.73f + .4f)));
	BoatImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}
