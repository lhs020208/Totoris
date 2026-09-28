#include "TotorisVirtualGarbage.h"

namespace
{
    constexpr double InitialAttackGraceSeconds = 5.0;
    constexpr double BaseAltitudePerSecond = .25;
    constexpr double AltitudePerVirtualAttackLine = 1.0;
    constexpr double Tiny = 1.e-9;
}

const FTotorisVirtualGarbageSimulator::FPreset& FTotorisVirtualGarbageSimulator::PresetForDifficulty(int32 InDifficulty)
{
    static const FPreset Presets[] = {
        { 5, 3.f, 2.0f, 8 }, { 8, 4.f, 1.9f, 8 }, { 12, 5.f, 1.8f, 8 },
        { 18, 6.f, 1.7f, 8 }, { 24, 7.f, 1.6f, 8 }, { 32, 8.f, 1.5f, 12 },
        { 42, 9.f, 1.4f, 12 }, { 53, 10.f, 1.3f, 12 }, { 66, 11.f, 1.2f, 12 },
        { 84, 12.f, 1.1f, 12 }
    };
    return Presets[FMath::Clamp(InDifficulty, 1, 10) - 1];
}

double FTotorisVirtualGarbageSimulator::FloorStartAltitude(int32 InDifficulty)
{
    static const double Starts[] = { 0., 50., 150., 300., 450., 650., 850., 1100., 1350., 1650. };
    return Starts[FMath::Clamp(InDifficulty, 1, 10) - 1];
}

void FTotorisVirtualGarbageSimulator::Reset(const FTotorisVirtualGarbageConfig& InConfig)
{
    Config = InConfig;
    Config.InitialDifficulty = FMath::Clamp(Config.InitialDifficulty, 1, 10);
    Random.Initialize(Config.Seed);
    Packets.Reset();
    ElapsedSeconds = 0.0;
    Intensity = 0.0;
    Threshold = SampleThreshold();
    Difficulty = Config.InitialDifficulty;
    VirtualAltitude = FloorStartAltitude(Difficulty);
    CumulativeVirtualAttackLines = 0.0;
    WarningPulseElapsedSeconds = 0.0;
    LastRecordedPlacementId = INDEX_NONE;
    NextPacketId = 1;
    NextGroupId = 1;
    bAggressivePhase = false;
    PhaseRemainingSeconds = SamplePhaseDuration(false);
}

double FTotorisVirtualGarbageSimulator::RandomUnit()
{
    return FMath::Max(1.e-12, static_cast<double>(Random.FRand()));
}

double FTotorisVirtualGarbageSimulator::SampleThreshold()
{
    return -FMath::Loge(RandomUnit() * RandomUnit()) * .5;
}

double FTotorisVirtualGarbageSimulator::SamplePhaseDuration(bool bAggressive)
{
    return bAggressive ? Random.FRandRange(16.f, 24.f) : Random.FRandRange(24.f, 36.f);
}

int32 FTotorisVirtualGarbageSimulator::SampleEventLines(const FPreset& Preset)
{
    // The floor/ceiling mixture preserves the specified long-run mean exactly
    // while still varying every event.  Packet splitting creates larger bursts.
    const double Mean = static_cast<double>(Preset.TargetAPM) / Preset.EventsPerMinute;
    const int32 Low = FMath::Clamp(FMath::FloorToInt(Mean), 1, Preset.MaxEventLines);
    const int32 High = FMath::Clamp(FMath::CeilToInt(Mean), 1, Preset.MaxEventLines);
    return RandomUnit() < (Mean - Low) ? High : Low;
}

void FTotorisVirtualGarbageSimulator::EmitAttackGroup(double EventTime)
{
    const FPreset& Preset = PresetForDifficulty(Difficulty);
    int32 Remaining = SampleEventLines(Preset);
    const int32 TotalLines = Remaining;
    const int64 GroupId = NextGroupId++;
    const bool bWindup = TotalLines >= 8;
    double Arrival = EventTime + (bWindup ? 1.0 : 0.0);
    int32 PacketIndex = 0;
    while (Remaining > 0)
    {
        const int32 Lines = FMath::Min(4, Remaining);
        FTotorisVirtualGarbagePacket& Packet = Packets.AddDefaulted_GetRef();
        Packet.Id = NextPacketId++;
        Packet.GroupId = GroupId;
        Packet.PacketIndex = PacketIndex++;
        Packet.Lines = Lines;
        Packet.ArrivalTime = Arrival;
        Packet.ActivationTime = Arrival + Preset.ActivationDelaySeconds;
        Remaining -= Lines;
        if (Remaining > 0)
        {
            Arrival += bWindup ? .5 : Random.FRandRange(.25f, .65f);
        }
    }
}

void FTotorisVirtualGarbageSimulator::MarkArrivals()
{
    const int32 PreviousWarning = GetWarningLines();
    for (FTotorisVirtualGarbagePacket& Packet : Packets)
    {
        if (!Packet.bArrived && Packet.ArrivalTime <= ElapsedSeconds + Tiny)
        {
            Packet.bArrived = true;
            Packet.RemainingWarningLines = Packet.Lines;
        }
    }
    if (PreviousWarning == 0 && GetWarningLines() > 0)
    {
        WarningPulseElapsedSeconds = 0.0;
    }
}

void FTotorisVirtualGarbageSimulator::UpdateDifficulty()
{
    if (!Config.bDifficultyIncrease) return;
    while (Difficulty < 10 && VirtualAltitude >= FloorStartAltitude(Difficulty + 1))
    {
        ++Difficulty;
    }
}

void FTotorisVirtualGarbageSimulator::Tick(double DeltaSeconds)
{
    if (!Config.bEnabled || DeltaSeconds <= 0.0) return;
    double Remaining = DeltaSeconds;
    int32 Safety = 0;
    while (Remaining > Tiny && Safety++ < 64)
    {
        const double UntilAttacks = FMath::Max(0.0, InitialAttackGraceSeconds - ElapsedSeconds);
        const double Rate = ElapsedSeconds + Tiny >= InitialAttackGraceSeconds
            ? PresetForDifficulty(Difficulty).EventsPerMinute / 60.0 * (bAggressivePhase ? 1.45 : .70)
            : 0.0;
        const double UntilThreshold = Rate > 0.0 ? FMath::Max(0.0, (Threshold - Intensity) / Rate) : DBL_MAX;
        // The five-second grace is a boundary only while it lies ahead.  Once
        // elapsed, including its zero duration in Min() would permanently
        // select a zero-length step and prevent all future attacks.
        double Step = FMath::Min(Remaining, PhaseRemainingSeconds);
        if (UntilAttacks > Tiny) Step = FMath::Min(Step, UntilAttacks);
        if (Rate > 0.0) Step = FMath::Min(Step, UntilThreshold);
        if (Step <= Tiny)
        {
            if (ElapsedSeconds + Tiny >= InitialAttackGraceSeconds && Intensity + Tiny >= Threshold)
            {
                Intensity -= Threshold;
                EmitAttackGroup(ElapsedSeconds);
                Threshold = SampleThreshold();
                MarkArrivals();
                continue;
            }
            if (PhaseRemainingSeconds <= Tiny)
            {
                bAggressivePhase = !bAggressivePhase;
                PhaseRemainingSeconds = SamplePhaseDuration(bAggressivePhase);
                continue;
            }
            // The five-second boundary has just been reached.
            ElapsedSeconds = InitialAttackGraceSeconds;
            continue;
        }
        if (Rate > 0.0) Intensity += Rate * Step;
        ElapsedSeconds += Step;
        PhaseRemainingSeconds -= Step;
        Remaining -= Step;
        if (Config.bDifficultyIncrease)
        {
            VirtualAltitude += BaseAltitudePerSecond * Step;
            UpdateDifficulty();
        }
        MarkArrivals();
    }
    if (GetWarningLines() > 0) WarningPulseElapsedSeconds += DeltaSeconds;
}

void FTotorisVirtualGarbageSimulator::RecordPlacementVirtualAttack(double Lines, int32 PlacementId)
{
    if (!Config.bEnabled || !Config.bDifficultyIncrease || PlacementId == LastRecordedPlacementId) return;
    LastRecordedPlacementId = PlacementId;
    CumulativeVirtualAttackLines += FMath::Max(0.0, Lines);
    VirtualAltitude += FMath::Max(0.0, Lines) * AltitudePerVirtualAttackLine;
    UpdateDifficulty();
}

TArray<FTotorisVirtualGarbagePacket> FTotorisVirtualGarbageSimulator::TakeActivatedPacketsForLock(double LockTime)
{
    TArray<FTotorisVirtualGarbagePacket> Result;
    if (!Config.bEnabled) return Result;
    for (FTotorisVirtualGarbagePacket& Packet : Packets)
    {
        if (Packet.bArrived && !Packet.bDelivered && Packet.ActivationTime <= LockTime + Tiny)
        {
            Packet.bDelivered = true;
            Result.Add(Packet);
        }
    }
    return Result;
}

void FTotorisVirtualGarbageSimulator::RecordInjectedLines(int64 PacketId, int32 Lines)
{
    for (FTotorisVirtualGarbagePacket& Packet : Packets)
    {
        if (Packet.Id != PacketId) continue;
        Packet.RemainingWarningLines = FMath::Max(0, Packet.RemainingWarningLines - FMath::Max(0, Lines));
        break;
    }
    if (GetWarningLines() == 0) WarningPulseElapsedSeconds = 0.0;
}

int32 FTotorisVirtualGarbageSimulator::GetWarningLines() const
{
    int32 Result = 0;
    for (const FTotorisVirtualGarbagePacket& Packet : Packets) Result += Packet.RemainingWarningLines;
    return Result;
}

float FTotorisVirtualGarbageSimulator::GetWarningPulseAlpha() const
{
    if (GetWarningLines() <= 0) return 0.f;
    const double T = FMath::Fmod(WarningPulseElapsedSeconds, 1.0);
    return static_cast<float>(.5 + .5 * (1.0 - FMath::Abs(2.0 * T - 1.0)));
}
