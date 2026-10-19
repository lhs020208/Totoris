#include "MinecraftLavaComponent.h"

#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

using namespace UE::Geometry;

UMinecraftLavaComponent::UMinecraftLavaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMinecraftLavaComponent::StartLava(FVector SurfacePoint)
{
	StopLava();
	if (!GetWorld() || !GetOwner()) return;
	LavaMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Totoris/Objects/Materials/Minecraft/M_MinecraftLava.M_MinecraftLava"));
	if (!LavaMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("Minecraft lava: M_MinecraftLava is missing."));
		return;
	}
	Origin = SurfacePoint + FVector(0, 0, .5f);
	LavaMesh = NewObject<UDynamicMeshComponent>(GetOwner());
	LavaMesh->SetMobility(EComponentMobility::Movable);
	LavaMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LavaMesh->SetCanEverAffectNavigation(false);
	LavaMesh->SetCastShadow(false);
	LavaMesh->RegisterComponent();
	LavaMesh->SetWorldLocation(Origin);
	LavaMesh->SetMaterial(0, LavaMaterial);
	Flow.Start();
	RebuildMesh();
	bSpreading = true;
	SetComponentTickEnabled(true);
	UE_LOG(LogTemp, Display, TEXT("Minecraft lava: source=%s, interval=1.5s, horizontal range=3 cells"), *Origin.ToString());
}

void UMinecraftLavaComponent::StopLava()
{
	bReceding = false;
	bSpreading = false;
	Elapsed = 0.f;
	SetComponentTickEnabled(false);
	Flow.Reset();
	if (IsValid(LavaMesh)) LavaMesh->DestroyComponent();
	LavaMesh = nullptr;
}

void UMinecraftLavaComponent::BeginRecede()
{
	if (!HasLava() || bReceding) return;
	bSpreading = false;
	bReceding = true;
	Elapsed = 0.f;
	// Remove the source first. Remaining flow loses strength on fluid ticks,
	// rather than uniformly fading the entire surface like a transparency effect.
	Flow.Cells.Remove(FIntVector::ZeroValue);
	RebuildMesh();
	SetComponentTickEnabled(true);
	UE_LOG(LogTemp, Log, TEXT("Minecraft lava: source removed; recession started."));
}

bool UMinecraftLavaComponent::HasLava() const
{
	return IsValid(LavaMesh) && !Flow.Cells.IsEmpty();
}

void UMinecraftLavaComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopLava();
	Super::EndPlay(Reason);
}

bool UMinecraftLavaComponent::CanFlow(const FIntVector& From, const FIntVector& To) const
{
	// This event uses an infinite, solid horizontal floor. All scene objects
	// are deliberately ignored; only lateral expansion is permitted.
	return From.Z == 0 && To.Z == 0;
}

bool UMinecraftLavaComponent::ContainsPoint(const FVector& WorldPoint) const
{
	if (!IsValid(LavaMesh)) return false;
	const FVector P = WorldPoint - Origin;
	const FIntVector Cell(FMath::FloorToInt((P.X + 50) / 100), FMath::FloorToInt((P.Y + 50) / 100), 0);
	if (!Flow.Cells.Contains(Cell)) return false;
	const float X = (P.X - Cell.X * 100 + 50) / 100;
	const float Y = (P.Y - Cell.Y * 100 + 50) / 100;
	// Match the two triangles used by RebuildMesh, including sloped surfaces.
	const float A = CornerHeight(Cell,0,0), B = CornerHeight(Cell,1,0);
	const float C = CornerHeight(Cell,1,1), D = CornerHeight(Cell,0,1);
	const float Height = X >= Y ? A + X*(B-A) + Y*(C-B) : A + X*(C-D) + Y*(D-A);
	return P.Z >= -2.f && P.Z <= Height;
}

void UMinecraftLavaComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bReceding)
	{
		Elapsed += FMath::Max(0.f, DeltaTime);
		while (bReceding && Elapsed >= MinecraftLava::TickSeconds)
		{
			Elapsed -= MinecraftLava::TickSeconds;
			TMap<FIntVector, MinecraftLava::FCell> Next;
			for (const auto& Pair : Flow.Cells)
			{
				int32 Distance = MinecraftLava::HorizontalRange + 1;
				for (const FIntVector& Direction : MinecraftLava::FFlow::Directions())
					if (const auto* Neighbor = Flow.Cells.Find(Pair.Key + Direction))
						Distance = FMath::Min(Distance, Neighbor->Distance + 1);
				Distance = FMath::Max(Distance, Pair.Value.Distance);
				if (Distance <= MinecraftLava::HorizontalRange)
				{
					auto Cell = Pair.Value;
					Cell.Distance = Distance;
					Next.Add(Pair.Key, Cell);
				}
			}
			Flow.Cells = MoveTemp(Next);
			UE_LOG(LogTemp, Log, TEXT("Minecraft lava: receding, %d cells remain."), Flow.Cells.Num());
			if (Flow.Cells.IsEmpty()) StopLava();
			else RebuildMesh();
		}
		return;
	}
	if (!bSpreading) return;
	Elapsed += FMath::Max(0.f, DeltaTime);
	while (bSpreading && Elapsed >= MinecraftLava::TickSeconds)
	{
		Elapsed -= MinecraftLava::TickSeconds;
		bSpreading = Flow.Advance([this](const FIntVector& A, const FIntVector& B) { return CanFlow(A, B); });
		if (bSpreading) RebuildMesh();
		UE_LOG(LogTemp, Display, TEXT("Minecraft lava: %d cells, spreading=%d"), Flow.Cells.Num(), bSpreading);
		if (!bSpreading) SetComponentTickEnabled(false);
	}
}

float UMinecraftLavaComponent::CornerHeight(const FIntVector& P, int32 CornerX, int32 CornerY) const
{
	float Sum = 0.f;
	int32 Weight = 0;
	// Each corner uses the same four cells from either side of a seam, so the
	// surface slopes continuously instead of rendering a stack of flat cubes.
	for (int32 X = CornerX - 1; X <= CornerX; ++X)
	for (int32 Y = CornerY - 1; Y <= CornerY; ++Y)
	{
		const FIntVector N = P + FIntVector(X,Y,0);
		if (const MinecraftLava::FCell* Cell = Flow.Cells.Find(N))
		{
			if (Flow.Cells.Contains(N + FIntVector(0,0,1))) return 100.f;
			const float Height = Cell->bFalling ? 88.8889f : (8.f - 2.f * Cell->Distance) * (100.f / 9.f);
			const int32 W = Cell->Distance == 0 ? 10 : 1;
			Sum += Height * W;
			Weight += W;
		}
	}
	return Weight ? Sum / Weight : 0.f;
}

void UMinecraftLavaComponent::RebuildMesh()
{
	if (!LavaMesh) return;
	FDynamicMesh3 Mesh;
	Mesh.EnableAttributes();
	auto* UV = Mesh.Attributes()->PrimaryUV();
	auto* Normals = Mesh.Attributes()->PrimaryNormals();
	auto AddQuad = [&](const FVector3d& A, const FVector3d& B, const FVector3d& C, const FVector3d& D,
		const FVector2f& UA, const FVector2f& UB, const FVector2f& UC, const FVector2f& UD)
	{
		const int32 V[4] = {Mesh.AppendVertex(A), Mesh.AppendVertex(B), Mesh.AppendVertex(C), Mesh.AppendVertex(D)};
		const int32 U[4] = {UV->AppendElement(UA), UV->AppendElement(UB), UV->AppendElement(UC), UV->AppendElement(UD)};
		const FVector3f N = FVector3f(VectorUtil::Normal(A, B, C));
		const int32 Normal[4] = {Normals->AppendElement(N), Normals->AppendElement(N), Normals->AppendElement(N), Normals->AppendElement(N)};
		const int32 T0 = Mesh.AppendTriangle(V[0], V[1], V[2]);
		const int32 T1 = Mesh.AppendTriangle(V[0], V[2], V[3]);
		UV->SetTriangle(T0, FIndex3i(U[0],U[1],U[2]));
		UV->SetTriangle(T1, FIndex3i(U[0],U[2],U[3]));
		Normals->SetTriangle(T0, FIndex3i(Normal[0],Normal[1],Normal[2]));
		Normals->SetTriangle(T1, FIndex3i(Normal[0],Normal[2],Normal[3]));
	};
	for (const auto& Pair : Flow.Cells)
	{
		const FIntVector P = Pair.Key;
		const FVector3d Base = FVector3d(P) * 100.0;
		const FVector3d Bottom[4] = {Base+FVector3d(-50,-50,0), Base+FVector3d(50,-50,0),
			Base+FVector3d(50,50,0), Base+FVector3d(-50,50,0)};
		const FVector3d Top[4] = {Bottom[0]+FVector3d(0,0,CornerHeight(P,0,0)), Bottom[1]+FVector3d(0,0,CornerHeight(P,1,0)),
			Bottom[2]+FVector3d(0,0,CornerHeight(P,1,1)), Bottom[3]+FVector3d(0,0,CornerHeight(P,0,1))};
		FVector2f Direction(Pair.Value.Direction.X, Pair.Value.Direction.Y);
		if (Direction.IsNearlyZero()) Direction = FVector2f(1,0);
		FVector2f Tex[4];
		for (int32 I = 0; I < 4; ++I)
		{
			const FVector2f Pos(Top[I].X / 100.f, Top[I].Y / 100.f);
			Tex[I] = FVector2f(Pos.X * Direction.X + Pos.Y * Direction.Y,
				-Pos.X * Direction.Y + Pos.Y * Direction.X);
		}
		if (!Flow.Cells.Contains(P + FIntVector(0,0,1)))
			AddQuad(Top[0],Top[1],Top[2],Top[3],Tex[0],Tex[1],Tex[2],Tex[3]);
		const FIntVector Neighbors[4] = {FIntVector(0,-1,0),FIntVector(1,0,0),FIntVector(0,1,0),FIntVector(-1,0,0)};
		for (int32 I = 0; I < 4; ++I)
		{
			if (Flow.Cells.Contains(P + Neighbors[I])) continue;
			const int32 J = (I+1)%4;
			// U increases downward, so the same panner makes the waterfall fall.
			AddQuad(Bottom[I],Bottom[J],Top[J],Top[I],FVector2f(-Bottom[I].Z/100.f,0),
				FVector2f(-Bottom[J].Z/100.f,1),FVector2f(-Top[J].Z/100.f,1),FVector2f(-Top[I].Z/100.f,0));
		}
	}
	LavaMesh->SetMesh(MoveTemp(Mesh));
}
