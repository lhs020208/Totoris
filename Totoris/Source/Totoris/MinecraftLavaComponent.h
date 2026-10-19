#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MinecraftLavaFlow.h"
#include "MinecraftLavaComponent.generated.h"

class UDynamicMeshComponent;
class UMaterialInterface;

UCLASS(ClassGroup=(Totoris), meta=(BlueprintSpawnableComponent))
class TOTORIS_API UMinecraftLavaComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UMinecraftLavaComponent();
	// SurfacePoint is the bottom centre of the 1m source, in world space.
	UFUNCTION(BlueprintCallable, Category="Totoris|Minecraft")
	void StartLava(FVector SurfacePoint);
	UFUNCTION(BlueprintCallable, Category="Totoris|Minecraft")
	void StopLava();
	void BeginRecede();
	bool HasLava() const;
	bool IsSpreading() const { return bSpreading; }
	bool ContainsPoint(const FVector& WorldPoint) const;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool CanFlow(const FIntVector& From, const FIntVector& To) const;
	void RebuildMesh();
	float CornerHeight(const FIntVector& P, int32 CornerX, int32 CornerY) const;
	MinecraftLava::FFlow Flow;
	FVector Origin = FVector::ZeroVector;
	float Elapsed = 0.f;
	bool bSpreading = false;
	bool bReceding = false;
	UPROPERTY(Transient)
	TObjectPtr<UDynamicMeshComponent> LavaMesh;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LavaMaterial;
};
