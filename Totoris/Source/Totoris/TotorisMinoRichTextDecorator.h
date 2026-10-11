#pragma once

#include "CoreMinimal.h"
#include "Components/RichTextBlockDecorator.h"
#include "TotorisMinoRichTextDecorator.generated.h"

// Replaces <mino id="T"/> markup with a compact, colorable tetromino widget.
UCLASS()
class TOTORIS_API UTotorisMinoRichTextDecorator : public URichTextBlockDecorator
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<ITextDecorator> CreateDecorator(URichTextBlock* InOwner) override;
};
