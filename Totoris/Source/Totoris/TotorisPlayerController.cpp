#include "TotorisPlayerController.h"

#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "TotorisBlockGeneratorComponent.h"
#include "TotorisMenuManager.h"


namespace
{
	constexpr TCHAR HandlingConfigSection[] = TEXT("Totoris.Handling");
	constexpr TCHAR ARRKey[] = TEXT("ARRMilliseconds");
	constexpr TCHAR DASKey[] = TEXT("DASMilliseconds");
	constexpr TCHAR DCDKey[] = TEXT("DCDMilliseconds");
	constexpr TCHAR SDFKey[] = TEXT("SDFMultiplier");
	constexpr TCHAR SDFInfiniteKey[] = TEXT("SDFInfinite");
}

ATotorisPlayerController::ATotorisPlayerController()
{
	bShowMouseCursor = true;
}

void ATotorisPlayerController::BeginPlay()
{
	Super::BeginPlay();

	MenuManager = NewObject<UTotorisMenuManager>(this);
	if (!ensure(MenuManager))
	{
		return;
	}

	MenuManager->Initialize(this);
	LoadHandlingSettings();

	// The room/camera remain untouched. Only gameplay simulation and actors
	// explicitly tagged as gameplay presentation are hidden.
	StopClassicGame();
	ShowMainMenu();
}

void ATotorisPlayerController::ShowMainMenu()
{
	SaveHandlingSettings();
	StopClassicGame();

	if (MenuManager && MenuManager->ShowMainMenu())
	{
		EnterMenuInputMode();
	}
}

void ATotorisPlayerController::ShowModeSelect()
{
	if (MenuManager && MenuManager->ShowModeSelect())
	{
		EnterMenuInputMode();
	}
}

void ATotorisPlayerController::ShowSettings()
{
	if (MenuManager && MenuManager->ShowSettings())
	{
		EnterMenuInputMode();
	}
}

void ATotorisPlayerController::StartClassicGame()
{
	if (MenuManager)
	{
		MenuManager->HideCurrentMenu();
	}

	SetTaggedGameplayVisualsVisible(true);

	if (UTotorisBlockGeneratorComponent* BlockGenerator = FindBlockGenerator())
	{
		BlockGenerator->ApplyHandlingSettings(
			HandlingARRMilliseconds,
			HandlingDASMilliseconds,
			HandlingDCDMilliseconds,
			HandlingSDFMultiplier,
			bHandlingSDFInfinite);
		SaveHandlingSettings();
		BlockGenerator->StartGame();
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Totoris: StartClassicGame could not find UTotorisBlockGeneratorComponent in the current world"));
	}

	EnterGameInputMode();
}

void ATotorisPlayerController::StopClassicGame()
{
	if (UTotorisBlockGeneratorComponent* BlockGenerator = FindBlockGenerator())
	{
		BlockGenerator->StopGame();
	}

	SetTaggedGameplayVisualsVisible(false);
}

void ATotorisPlayerController::QuitGame()
{
	SaveHandlingSettings();

	UKismetSystemLibrary::QuitGame(
		this,
		this,
		EQuitPreference::Quit,
		false);
}

float ATotorisPlayerController::SetARRSliderValue(float SliderValue)
{
	const float Clamped = FMath::Clamp(SliderValue, 0.f, 1.f);
	HandlingARRMilliseconds = FMath::RoundToInt(
		FMath::Lerp(
			static_cast<float>(MaxARRMilliseconds),
			0.f,
			Clamped));

	MarkHandlingDirtyAndApply();
	return GetARRSliderValue();
}

float ATotorisPlayerController::SetDASSliderValue(float SliderValue)
{
	const float Clamped = FMath::Clamp(SliderValue, 0.f, 1.f);
	HandlingDASMilliseconds = FMath::RoundToInt(
		FMath::Lerp(
			static_cast<float>(MaxDASMilliseconds),
			static_cast<float>(MinDASMilliseconds),
			Clamped));

	MarkHandlingDirtyAndApply();
	return GetDASSliderValue();
}

float ATotorisPlayerController::SetDCDSliderValue(float SliderValue)
{
	const float Clamped = FMath::Clamp(SliderValue, 0.f, 1.f);
	HandlingDCDMilliseconds = FMath::RoundToInt(
		FMath::Lerp(
			static_cast<float>(MaxDCDMilliseconds),
			0.f,
			Clamped));

	MarkHandlingDirtyAndApply();
	return GetDCDSliderValue();
}

float ATotorisPlayerController::SetSDFSliderValue(float SliderValue)
{
	const float Clamped = FMath::Clamp(SliderValue, 0.f, 1.f);
	const int32 Step = FMath::Clamp(
		FMath::RoundToInt(Clamped * static_cast<float>(SDFSliderStepCount)),
		0,
		SDFSliderStepCount);

	if (Step >= SDFSliderStepCount)
	{
		bHandlingSDFInfinite = true;
		HandlingSDFMultiplier = MaxSDFMultiplier;
	}
	else
	{
		bHandlingSDFInfinite = false;
		HandlingSDFMultiplier = MinSDFMultiplier + Step;
	}

	MarkHandlingDirtyAndApply();
	return GetSDFSliderValue();
}

float ATotorisPlayerController::GetARRSliderValue() const
{
	return 1.f -
		(static_cast<float>(HandlingARRMilliseconds) /
		 static_cast<float>(MaxARRMilliseconds));
}

float ATotorisPlayerController::GetDASSliderValue() const
{
	const int32 Range = MaxDASMilliseconds - MinDASMilliseconds;
	return static_cast<float>(MaxDASMilliseconds - HandlingDASMilliseconds) /
		static_cast<float>(Range);
}

float ATotorisPlayerController::GetDCDSliderValue() const
{
	return 1.f -
		(static_cast<float>(HandlingDCDMilliseconds) /
		 static_cast<float>(MaxDCDMilliseconds));
}

float ATotorisPlayerController::GetSDFSliderValue() const
{
	if (bHandlingSDFInfinite)
	{
		return 1.f;
	}

	const int32 Step = FMath::Clamp(
		HandlingSDFMultiplier - MinSDFMultiplier,
		0,
		SDFSliderStepCount - 1);

	return static_cast<float>(Step) /
		static_cast<float>(SDFSliderStepCount);
}

FText ATotorisPlayerController::GetARRDisplayText() const
{
	return FText::FromString(
		FString::Printf(TEXT("%d ms"), HandlingARRMilliseconds));
}

FText ATotorisPlayerController::GetDASDisplayText() const
{
	return FText::FromString(
		FString::Printf(TEXT("%d ms"), HandlingDASMilliseconds));
}

FText ATotorisPlayerController::GetDCDDisplayText() const
{
	return FText::FromString(
		FString::Printf(TEXT("%d ms"), HandlingDCDMilliseconds));
}

FText ATotorisPlayerController::GetSDFDisplayText() const
{
	if (bHandlingSDFInfinite)
	{
		return FText::FromString(TEXT("inf"));
	}

	return FText::FromString(
		FString::Printf(TEXT("%d X"), HandlingSDFMultiplier));
}

void ATotorisPlayerController::ResetHandlingDefaults()
{
	HandlingARRMilliseconds = DefaultARRMilliseconds;
	HandlingDASMilliseconds = DefaultDASMilliseconds;
	HandlingDCDMilliseconds = DefaultDCDMilliseconds;
	HandlingSDFMultiplier = DefaultSDFMultiplier;
	bHandlingSDFInfinite = false;

	MarkHandlingDirtyAndApply();
}

void ATotorisPlayerController::LoadHandlingSettings()
{
	HandlingARRMilliseconds = DefaultARRMilliseconds;
	HandlingDASMilliseconds = DefaultDASMilliseconds;
	HandlingDCDMilliseconds = DefaultDCDMilliseconds;
	HandlingSDFMultiplier = DefaultSDFMultiplier;
	bHandlingSDFInfinite = false;

	if (GConfig)
	{
		GConfig->GetInt(
			HandlingConfigSection,
			ARRKey,
			HandlingARRMilliseconds,
			GGameUserSettingsIni);

		GConfig->GetInt(
			HandlingConfigSection,
			DASKey,
			HandlingDASMilliseconds,
			GGameUserSettingsIni);

		GConfig->GetInt(
			HandlingConfigSection,
			DCDKey,
			HandlingDCDMilliseconds,
			GGameUserSettingsIni);

		GConfig->GetInt(
			HandlingConfigSection,
			SDFKey,
			HandlingSDFMultiplier,
			GGameUserSettingsIni);

		GConfig->GetBool(
			HandlingConfigSection,
			SDFInfiniteKey,
			bHandlingSDFInfinite,
			GGameUserSettingsIni);
	}

	HandlingARRMilliseconds =
		FMath::Clamp(HandlingARRMilliseconds, 0, MaxARRMilliseconds);

	HandlingDASMilliseconds =
		FMath::Clamp(
			HandlingDASMilliseconds,
			MinDASMilliseconds,
			MaxDASMilliseconds);

	HandlingDCDMilliseconds =
		FMath::Clamp(HandlingDCDMilliseconds, 0, MaxDCDMilliseconds);

	HandlingSDFMultiplier =
		FMath::Clamp(
			HandlingSDFMultiplier,
			MinSDFMultiplier,
			MaxSDFMultiplier);

	bHandlingSettingsDirty = false;
	ApplyHandlingSettingsToGame();
}

void ATotorisPlayerController::SaveHandlingSettings()
{
	if (!bHandlingSettingsDirty || !GConfig)
	{
		return;
	}

	GConfig->SetInt(
		HandlingConfigSection,
		ARRKey,
		HandlingARRMilliseconds,
		GGameUserSettingsIni);

	GConfig->SetInt(
		HandlingConfigSection,
		DASKey,
		HandlingDASMilliseconds,
		GGameUserSettingsIni);

	GConfig->SetInt(
		HandlingConfigSection,
		DCDKey,
		HandlingDCDMilliseconds,
		GGameUserSettingsIni);

	GConfig->SetInt(
		HandlingConfigSection,
		SDFKey,
		HandlingSDFMultiplier,
		GGameUserSettingsIni);

	GConfig->SetBool(
		HandlingConfigSection,
		SDFInfiniteKey,
		bHandlingSDFInfinite,
		GGameUserSettingsIni);

	GConfig->Flush(false, GGameUserSettingsIni);
	bHandlingSettingsDirty = false;
}

void ATotorisPlayerController::ApplyHandlingSettingsToGame()
{
	if (UTotorisBlockGeneratorComponent* BlockGenerator = FindBlockGenerator())
	{
		BlockGenerator->ApplyHandlingSettings(
			HandlingARRMilliseconds,
			HandlingDASMilliseconds,
			HandlingDCDMilliseconds,
			HandlingSDFMultiplier,
			bHandlingSDFInfinite);
	}
}

void ATotorisPlayerController::MarkHandlingDirtyAndApply()
{
	bHandlingSettingsDirty = true;
	ApplyHandlingSettingsToGame();
}

UTotorisBlockGeneratorComponent* ATotorisPlayerController::FindBlockGenerator() const
{
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(
		this,
		AActor::StaticClass(),
		Actors);

	for (AActor* Actor : Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		if (UTotorisBlockGeneratorComponent* Component =
			Actor->FindComponentByClass<UTotorisBlockGeneratorComponent>())
		{
			return Component;
		}
	}

	return nullptr;
}

void ATotorisPlayerController::SetTaggedGameplayVisualsVisible(bool bVisible)
{
	// Whole actors can be tagged when they contain gameplay presentation only.
	TArray<AActor*> TaggedActors;
	UGameplayStatics::GetAllActorsWithTag(
		this,
		GameplayVisualTag,
		TaggedActors);

	for (AActor* Actor : TaggedActors)
	{
		if (IsValid(Actor))
		{
			Actor->SetActorHiddenInGame(!bVisible);
		}
	}

	// Component tags are also supported so a board/grid component can be hidden
	// without hiding an actor that also owns unrelated room geometry.
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(
		this,
		AActor::StaticClass(),
		AllActors);

	for (AActor* Actor : AllActors)
	{
		if (!IsValid(Actor) || Actor->ActorHasTag(GameplayVisualTag))
		{
			continue;
		}

		TArray<UPrimitiveComponent*> PrimitiveComponents;
		Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

		for (UPrimitiveComponent* Component : PrimitiveComponents)
		{
			if (IsValid(Component) && Component->ComponentHasTag(GameplayVisualTag))
			{
				Component->SetVisibility(bVisible, true);
				Component->SetHiddenInGame(!bVisible, true);
			}
		}
	}
}

void ATotorisPlayerController::EnterMenuInputMode()
{
	bShowMouseCursor = true;

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void ATotorisPlayerController::EnterGameInputMode()
{
	bShowMouseCursor = false;

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
}
