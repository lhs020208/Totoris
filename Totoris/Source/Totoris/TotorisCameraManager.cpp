#include "TotorisCameraManager.h"

void ATotorisCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);
	OutVT.POV.AspectRatio = 16.f / 9.f;
	OutVT.POV.bConstrainAspectRatio = true;
}
