#pragma once
#include "Camera/PlayerCameraManager.h"
#include "TotorisCameraManager.generated.h"

UCLASS()
class TOTORIS_API ATotorisCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()
protected:
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;
};
