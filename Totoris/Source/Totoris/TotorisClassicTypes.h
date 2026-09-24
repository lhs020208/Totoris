#pragma once

#include "CoreMinimal.h"
#include "TotorisClassicTypes.generated.h"

UENUM(BlueprintType)
enum class ETotorisClassicMode : uint8
{
    Endless UMETA(DisplayName="Endless"),
    Sprint UMETA(DisplayName="Sprint"),
    Blitz UMETA(DisplayName="Blitz"),
    CheeseRace UMETA(DisplayName="Cheese Race")
};

UENUM(BlueprintType)
enum class ETotorisRunResult : uint8
{
    None UMETA(DisplayName="In Progress"),
    Completed UMETA(DisplayName="Completed"),
    ToppedOut UMETA(DisplayName="Topped Out"),
    TimeExpired UMETA(DisplayName="Time Expired")
};

USTRUCT(BlueprintType)
struct TOTORIS_API FTotorisClassicSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Totoris|Classic")
    ETotorisClassicMode Mode = ETotorisClassicMode::Endless;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Totoris|Classic", meta=(ClampMin="1", ClampMax="1000"))
    int32 TargetLines = 40;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Totoris|Classic", meta=(ClampMin="1", ClampMax="359"))
    int32 LimitTimeSeconds = 120;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Totoris|Classic", meta=(ClampMin="1", ClampMax="100"))
    int32 CheeseCount = 18;
};
