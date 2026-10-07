#pragma once

#include "CoreMinimal.h"
#include "TotorisMission.generated.h"

UENUM(BlueprintType)
enum class ETotorisMissionTier : uint8
{
	F,
	E,
	D,
	C,
	B,
	A
};

// Reserved for the future mission evaluator. The initial mission data only
// contains identity, display text, and tier information.
UENUM(BlueprintType)
enum class ETotorisMissionKind : uint8
{
	None,
	Counter,
	Sequence,
	Event,
	BoardState,
	TimedState
};

USTRUCT(BlueprintType)
struct TOTORIS_API FTotorisMissionDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totoris|Mission")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totoris|Mission")
	ETotorisMissionTier Tier = ETotorisMissionTier::F;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totoris|Mission")
	ETotorisMissionKind Kind = ETotorisMissionKind::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totoris|Mission")
	FText DisplayText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totoris|Mission")
	FText Description;

	// Reserved targets/parameters for the future evaluator.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totoris|Mission")
	int32 TargetValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totoris|Mission")
	int32 SecondaryTargetValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Totoris|Mission")
	FName Parameter;
};

namespace TotorisMissions
{
	TOTORIS_API const TArray<FTotorisMissionDefinition>& All();
	TOTORIS_API const TArray<FTotorisMissionDefinition>& ForTier(ETotorisMissionTier Tier);
}
