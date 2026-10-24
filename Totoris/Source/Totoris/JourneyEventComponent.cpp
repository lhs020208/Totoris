#include "JourneyEventComponent.h"
#include "TotorisBlockGeneratorComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "TotorisAudioMix.h"

using namespace UE::Geometry;

UJourneyEventComponent::UJourneyEventComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/Totoris/Objects/Journey/SM_JourneyTraveler.SM_JourneyTraveler"));
    ConstructorHelpers::FObjectFinder<UMaterialInterface> Body(TEXT("/Game/Totoris/Objects/Journey/M_JourneyTraveler.M_JourneyTraveler"));
    ConstructorHelpers::FObjectFinder<UMaterialInterface> Ribbon(TEXT("/Game/Totoris/Objects/Journey/M_JourneyCloth.M_JourneyCloth"));
    ConstructorHelpers::FObjectFinder<UMaterialInterface> Gold(TEXT("/Game/Totoris/Objects/Journey/M_JourneyGold.M_JourneyGold"));
    TravelerAsset = Mesh.Object;
    TravelerMaterial = Body.Object;
    ClothMaterial = Ribbon.Object;
    GoldMaterial = Gold.Object;
    for (const TCHAR* Name : {TEXT("Arrival"), TEXT("Chirp"), TEXT("ClothGather"),
        TEXT("ScarfCharge"), TEXT("FlightWhoosh"), TEXT("Wind")})
    {
        const FString Path = FString::Printf(TEXT("/Game/Totoris/Audio/Journey/SW_Journey%s.SW_Journey%s"), Name, Name);
        ConstructorHelpers::FObjectFinder<USoundWave> Sound(*Path);
        Sounds.Add(Sound.Object);
    }
}

bool UJourneyEventComponent::PlayEvent(UTotorisBlockGeneratorComponent* Game)
{
    StopEvent();
    auto* PC = Cast<APlayerController>(GetOwner());
    if (!PC || !PC->PlayerCameraManager || !IsValid(Game) || !TravelerAsset ||
        !TravelerMaterial || !ClothMaterial || !GoldMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("Journey preview: missing camera, board or prepared assets."));
        return false;
    }
    Board = Game;
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    Presentation = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
    if (!Presentation) return false;
    auto* Root = NewObject<USceneComponent>(Presentation);
    Presentation->SetRootComponent(Root);
    Root->RegisterComponent();
    Traveler = NewObject<UStaticMeshComponent>(Presentation);
    Traveler->SetupAttachment(Root);
    Traveler->SetStaticMesh(TravelerAsset);
    Traveler->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Traveler->SetCastShadow(false);
    Traveler->SetBoundsScale(1.25f);
    Traveler->RegisterComponent();
    Cloth = NewObject<UDynamicMeshComponent>(Presentation);
    Cloth->SetupAttachment(Root);
    Cloth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Cloth->SetCastShadow(false);
    Cloth->RegisterComponent();
    for (UMaterialInterface* Source : {TravelerMaterial.Get(), ClothMaterial.Get(), GoldMaterial.Get()})
        Materials.Add(UMaterialInstanceDynamic::Create(Source, this));
    for (int32 I = 0; I < TravelerAsset->GetStaticMaterials().Num(); ++I) Traveler->SetMaterial(I, Materials[0]);
    Cloth->SetMaterial(0, Materials[1]);
    Cloth->SetMaterial(1, Materials[2]);
    Elapsed = 0.f;
    bPlaying = true;
    SetComponentTickEnabled(true);
    UpdatePresentation();
    WindAudio = StartSound(5, 0.f);
    UpdateAudio();
    UE_LOG(LogTemp, Display, TEXT("Journey gallery preview started (11 seconds)."));
    return true;
}

void UJourneyEventComponent::StopEvent()
{
    bPlaying = false;
    SetComponentTickEnabled(false);
    for (UAudioComponent* Audio : AudioComponents)
    {
        if (IsValid(Audio)) { Audio->Stop(); Audio->DestroyComponent(); }
    }
    AudioComponents.Reset();
    WindAudio = nullptr;
    NextAudioCue = 0;
    if (IsValid(Presentation)) Presentation->Destroy();
    Presentation = nullptr;
    Traveler = nullptr;
    Cloth = nullptr;
    Materials.Reset();
    Board.Reset();
    Elapsed = 0.f;
}

void UJourneyEventComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    StopEvent();
    Super::EndPlay(Reason);
}

void UJourneyEventComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    if (!bPlaying) return;
    Elapsed += FMath::Max(0.f, DeltaTime);
    if (Elapsed >= Duration || !Board.IsValid() || !IsValid(Presentation)) { StopEvent(); return; }
    UpdatePresentation();
    if (bPlaying) UpdateAudio();
}

UAudioComponent* UJourneyEventComponent::StartSound(int32 Index, float Volume, float StartTime)
{
    if (!IsValid(Presentation) || !Sounds.IsValidIndex(Index) || !Sounds[Index]) return nullptr;
    auto* Audio = NewObject<UAudioComponent>(Presentation);
    Audio->bAutoActivate = false;
    Audio->bAutoDestroy = false;
    Audio->bAllowSpatialization = false;
    Audio->bIsUISound = false;
    Audio->SetSound(Sounds[Index]);
    Audio->SetVolumeMultiplier(Volume * TotorisAudioMix::MasterVolume);
    Audio->RegisterComponent();
    AudioComponents.Add(Audio);
    Audio->Play(StartTime);
    UE_LOG(LogTemp, Display, TEXT("Journey audio: %s at %.2fs"), *Sounds[Index]->GetName(), Elapsed);
    return Audio;
}

void UJourneyEventComponent::UpdateAudio()
{
    struct FCue { float Time; int32 Sound; float Volume; };
    // Ring begins at 1.6s; gathering/charge at 1.8-3.3s; launch at 3s; ascent at 8s.
    static const FCue Cues[] = {{.15f,0,.22f}, {1.6f,1,.65f}, {2.15f,2,.16f},
        {2.8f,3,.24f}, {3.f,4,.24f}, {8.f,4,.18f}};
    while (NextAudioCue < UE_ARRAY_COUNT(Cues) && Elapsed >= Cues[NextAudioCue].Time)
    {
        const FCue& Cue = Cues[NextAudioCue++];
        const float Offset = Elapsed-Cue.Time;
        // Frame hitches must not play stale cues late or trigger them twice.
        if (Sounds.IsValidIndex(Cue.Sound) && Sounds[Cue.Sound] && Offset < Sounds[Cue.Sound]->Duration)
            StartSound(Cue.Sound, Cue.Volume, Offset);
    }
    if (IsValid(WindAudio))
    {
        const float FadeIn = FMath::SmoothStep(.3f,2.8f,Elapsed);
        const float FadeOut = 1.f-FMath::SmoothStep(8.5f,10.9f,Elapsed);
        const float Flight = FMath::SmoothStep(2.5f,4.f,Elapsed);
        WindAudio->SetVolumeMultiplier(FadeIn*FadeOut*FMath::Lerp(.035f,.13f,Flight)*TotorisAudioMix::MasterVolume);
    }
}

FVector UJourneyEventComponent::ScreenPoint(float X, float Y) const
{
    return FVector(0, (X * 2.f - 1.f) * HalfWidth, (1.f - Y * 2.f) * HalfHeight);
}

FVector UJourneyEventComponent::Path(float Seconds) const
{
    if (Seconds < 3.f)
    {
        const float A = FMath::SmoothStep(0.f, 1.6f, Seconds);
        return ScreenPoint(.84f, FMath::Lerp(.76f, .58f, A) + .007f * FMath::Sin(Seconds * 2.f));
    }
    if (Seconds < 8.f)
    {
        const float A = FMath::SmoothStep(3.f, 8.f, Seconds);
        const float B = 1.f-A;
        // A single continuous arch around the top of the playfield, rather than
        // clipping the traveler at its border. The sides remain fully readable.
        return ScreenPoint(.84f,.58f)*(B*B*B) + ScreenPoint(.84f,-.11f)*(3*B*B*A)
            + ScreenPoint(.22f,-.10f)*(3*B*A*A) + ScreenPoint(.13f,.32f)*(A*A*A);
    }
    const float A = FMath::SmoothStep(8.f, 11.f, Seconds);
    return ScreenPoint(FMath::Lerp(.13f, .08f, A), FMath::Lerp(.32f, -.24f, A));
}

void UJourneyEventComponent::UpdatePresentation()
{
    auto* PC = Cast<APlayerController>(GetOwner());
    if (!PC || !PC->PlayerCameraManager) { StopEvent(); return; }
    const APlayerCameraManager* Camera = PC->PlayerCameraManager;
    const FVector CameraPos = Camera->GetCameraLocation();
    const FRotator CameraRot = Camera->GetCameraRotation();
    // One real depth in front of ALL panels. The flight arches around the
    // playfield instead of using a contradictory screen-space depth cutout.
    const float Depth = FMath::Max(100.f, FVector::DotProduct(Board->GetOwner()->GetActorLocation() - CameraPos, CameraRot.Vector()) - 65.f);
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0) return;
    HalfWidth = Depth * FMath::Tan(FMath::DegreesToRadians(Camera->GetFOVAngle() * .5f));
    HalfHeight = HalfWidth * float(Height) / float(Width);
    FVector RayOrigin, RayDirection;
    if (PC->DeprojectScreenPositionToWorld(0.f, 0.f, RayOrigin, RayDirection))
    {
        const float Forward = FVector::DotProduct(RayDirection, CameraRot.Vector());
        if (Forward > KINDA_SMALL_NUMBER)
        {
            const float RayDistance = (Depth - FVector::DotProduct(RayOrigin-CameraPos, CameraRot.Vector())) / Forward;
            const FVector Corner = CameraRot.UnrotateVector(RayOrigin + RayDirection*RayDistance - CameraPos);
            HalfWidth = FMath::Abs(Corner.Y);
            HalfHeight = FMath::Abs(Corner.Z);
        }
    }
    TravelerHeight = HalfHeight * .28f;
    Presentation->SetActorLocationAndRotation(CameraPos + CameraRot.Vector() * Depth, CameraRot);
    const float Fade = FMath::SmoothStep(0.f, .8f, Elapsed) * (1.f - FMath::SmoothStep(9.7f, 11.f, Elapsed));
    const float Charge = FMath::SmoothStep(1.8f, 3.3f, Elapsed);
    for (UMaterialInstanceDynamic* Mat : Materials)
    {
        Mat->SetScalarParameterValue(TEXT("Fade"), Fade);
        Mat->SetScalarParameterValue(TEXT("Charge"), Charge);
        Mat->SetScalarParameterValue(TEXT("EventTime"), Elapsed);
        Mat->SetScalarParameterValue(TEXT("Flight"), FMath::SmoothStep(2.6f, 4.f, Elapsed));
    }
    const FVector Center = Path(Elapsed);
    const float Scale = TravelerHeight / FMath::Max(1.f, float(TravelerAsset->GetBounds().BoxExtent.Z * 2));
    const FQuat Rotation = Pose(Elapsed);
    Traveler->SetRelativeRotation(Rotation);
    Traveler->SetRelativeScale3D(FVector(Scale));
    Traveler->SetRelativeLocation(Center - Rotation.RotateVector(TravelerAsset->GetBounds().Origin * Scale));
    UpdateCloth(Center, Fade);
}

FQuat UJourneyEventComponent::Pose(float Seconds) const
{
    const float Launch = FMath::SmoothStep(2.5f, 4.1f, Seconds);
    const float Climb = FMath::SmoothStep(7.2f, 9.6f, Seconds);
    // Rotate about camera-forward, not mesh-local X: the lean must be visible
    // in silhouette. Lead the departure, then straighten into the final ascent.
    const float Lean = 62.f * Launch * (1.f-Climb) +
        4.f * FMath::Sin(Seconds*2.1f) * (1.f-.65f*Launch);
    const float Turn = 90.f + 24.f*Launch*(1.f-Climb) + 3.f*FMath::Sin(Seconds*1.4f);
    return FQuat(FVector::ForwardVector,FMath::DegreesToRadians(Lean)) * FRotator(0,Turn,0).Quaternion();
}

void UJourneyEventComponent::UpdateCloth(const FVector& Center, float Fade)
{
    FDynamicMesh3 Mesh;
    Mesh.EnableAttributes();
    Mesh.Attributes()->EnableMaterialID();
    auto* UV = Mesh.Attributes()->PrimaryUV();
    auto* Normals = Mesh.Attributes()->PrimaryNormals();
    auto Quad = [&](FVector A, FVector B, FVector C, FVector D, float U0, float U1, int32 Material)
    {
        const FVector P[] = {A,B,C,D};
        const FVector2f Tex[] = {FVector2f(U0,0),FVector2f(U0,1),FVector2f(U1,1),FVector2f(U1,0)};
        int32 V[4], U[4], N[4];
        for (int32 I=0; I<4; ++I) { V[I]=Mesh.AppendVertex(FVector3d(P[I])); U[I]=UV->AppendElement(Tex[I]); N[I]=Normals->AppendElement(FVector3f(-1,0,0)); }
        for (int32 I=1; I<3; ++I)
        {
            const int32 T=Mesh.AppendTriangle(V[0],V[I],V[I+1]);
            UV->SetTriangle(T,FIndex3i(U[0],U[I],U[I+1]));
            Normals->SetTriangle(T,FIndex3i(N[0],N[I],N[I+1]));
            Mesh.Attributes()->GetMaterialID()->SetValue(T,Material);
        }
    };
    // A segmented ribbon with analytic waves, independent of the imported model's topology.
    auto Ribbon = [&](FVector Start, float Length, float Width, float Phase, bool Creature, FVector Tail)
    {
        constexpr int32 Segments = 32;
        auto Edge = [&](float T, float Side)
        {
            const float Envelope = Creature ? FMath::Lerp(.08f,1.f,FMath::Pow(FMath::Sin(T*PI),2.f)) : FMath::Lerp(1.f,.45f,T);
            const float Wave = FMath::Sin(T*8.f-Elapsed*3.f+Phase);
            const FVector Across = FVector::CrossProduct(FVector::ForwardVector,Tail).GetSafeNormal();
            const FVector History = Creature ? FVector::ZeroVector :
                (Path(FMath::Max(0.f,Elapsed-T*.8f))-Center).GetClampedToMaxSize(Length*.8f)*.22f;
            return Start + Tail*Length*T + History + Across*(Wave*Length*.10f*T + Side*Width*Envelope)
                + FVector(FMath::Sin(T*7.f-Elapsed*2.f+Phase)*Width*.4f*T,0,0);
        };
        for (int32 I=0; I<Segments; ++I)
        {
            const float A=float(I)/Segments, B=float(I+1)/Segments;
            Quad(Edge(A,-1),Edge(A,1),Edge(B,1),Edge(B,-1),A,B,0);
        }
    };
    const float Moving = FMath::SmoothStep(2.7f,4.f,Elapsed);
    FVector Tail = (Path(FMath::Max(0.f,Elapsed-.3f))-Path(Elapsed+.05f)).GetSafeNormal();
    Tail = FMath::Lerp(FVector::RightVector,Tail,Moving).GetSafeNormal();
    if (Tail.IsNearlyZero()) Tail = FVector::RightVector;
    const FVector Neck = Center + Pose(Elapsed).RotateVector(FVector(0,-TravelerHeight*.06f,TravelerHeight*.28f));
    Ribbon(Neck, TravelerHeight*1.6f, TravelerHeight*.055f,0,false,Tail);
    const float Gather = FMath::SmoothStep(1.8f,3.5f,Elapsed);
    for (int32 I=0; I<5; ++I)
    {
        const float Phase = I*1.73f;
        const FVector Flight = Path(FMath::Min(11.f,Elapsed + .30f + I*.17f));
        const FVector Offset(8+I*2, FMath::Cos(Phase)*TravelerHeight*.8f,
            TravelerHeight*(.65f + I*.16f)+FMath::Sin(Elapsed*2.f+Phase)*TravelerHeight*.12f);
        const FVector Waiting = ScreenPoint(.94f+I*.045f,.38f+I*.07f);
        FVector CreatureCenter = FMath::Lerp(Waiting,Flight+Offset,Gather);
        // The flock stays visible when the traveler takes the high arch.
        CreatureCenter.Z = FMath::Min(CreatureCenter.Z, HalfHeight-TravelerHeight*(.28f+.15f*(I%3)));
        Ribbon(CreatureCenter,TravelerHeight*(.45f+I*.04f),TravelerHeight*.09f,Phase,true,Tail);
    }
    // One expanding chirp ring and its floating diamond, visible only beside the board.
    const float Call = FMath::Clamp((Elapsed-1.6f)/1.2f,0.f,1.f);
    if (Elapsed > 1.6f && Elapsed < 2.8f)
    {
        const FVector Origin = Center+FVector(-2,0,TravelerHeight*.56f);
        const float Radius = TravelerHeight*FMath::Lerp(.08f,.50f,Call);
        const float Thickness = TravelerHeight*.009f*(1.f-Call);
        for (int32 I=0; I<64; ++I)
        {
            const float A=2*PI*I/64, B=2*PI*(I+1)/64;
            const FVector DA(0,FMath::Cos(A),FMath::Sin(A)), DB(0,FMath::Cos(B),FMath::Sin(B));
            Quad(Origin+DA*(Radius-Thickness),Origin+DA*(Radius+Thickness),Origin+DB*(Radius+Thickness),Origin+DB*(Radius-Thickness),0,1,1);
        }
        const float R=TravelerHeight*.055f;
        Quad(Origin+FVector(0,-R,0),Origin+FVector(0,0,R),Origin+FVector(0,R,0),Origin+FVector(0,0,-R),0,1,1);
    }
    // Sparse gold motes following the cloth; no persistent particles after cleanup.
    for (int32 I=0; I<18; ++I)
    {
        const float Age=FMath::Frac(Elapsed*.45f+I*.137f);
        const FVector P=Path(FMath::Max(0.f,Elapsed-Age*1.6f))+FVector(-3,
            FMath::Sin(I*5.1f)*TravelerHeight*.5f, FMath::Cos(I*2.7f)*TravelerHeight*.6f);
        const float R=TravelerHeight*.008f*FMath::Sin(Age*PI)*Gather*Fade;
        Quad(P+FVector(0,-R,0),P+FVector(0,0,R),P+FVector(0,R,0),P+FVector(0,0,-R),0,1,1);
    }
    Cloth->SetMesh(MoveTemp(Mesh));
}
