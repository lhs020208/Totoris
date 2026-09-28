#pragma once

#include "CoreMinimal.h"

// Pure gameplay scheduler for simulated incoming garbage.  It deliberately
// knows nothing about the board, widgets, or garbage-hole policy.
struct TOTORIS_API FTotorisVirtualGarbagePacket
{
    int64 Id = 0;
    int64 GroupId = 0;
    int32 PacketIndex = 0;
    int32 Lines = 0;
    int32 RemainingWarningLines = 0;
    double ArrivalTime = 0.0;
    double ActivationTime = 0.0;
    bool bArrived = false;
    bool bDelivered = false;
};

struct TOTORIS_API FTotorisVirtualGarbageConfig
{
    bool bEnabled = false;
    int32 InitialDifficulty = 5;
    // When false, the initial level is immutable for the entire run.
    bool bDifficultyIncrease = false;
    int32 Seed = 0;
};

class TOTORIS_API FTotorisVirtualGarbageSimulator
{
public:
    void Reset(const FTotorisVirtualGarbageConfig& InConfig);
    void Tick(double DeltaSeconds);
    void RecordPlacementVirtualAttack(double Lines, int32 PlacementId);
    TArray<FTotorisVirtualGarbagePacket> TakeActivatedPacketsForLock(double LockTime);
    void RecordInjectedLines(int64 PacketId, int32 Lines);

    bool IsEnabled() const { return Config.bEnabled; }
    int32 GetDifficulty() const { return Difficulty; }
    int32 GetWarningLines() const;
    float GetWarningPulseAlpha() const;
    double GetVirtualAltitude() const { return VirtualAltitude; }
    double GetCumulativeVirtualAttackLines() const { return CumulativeVirtualAttackLines; }

private:
    struct FPreset
    {
        int32 TargetAPM;
        float EventsPerMinute;
        float ActivationDelaySeconds;
        int32 MaxEventLines;
    };

    static const FPreset& PresetForDifficulty(int32 InDifficulty);
    static double FloorStartAltitude(int32 InDifficulty);
    double RandomUnit();
    double SampleThreshold();
    double SamplePhaseDuration(bool bAggressive);
    int32 SampleEventLines(const FPreset& Preset);
    void EmitAttackGroup(double EventTime);
    void MarkArrivals();
    void UpdateDifficulty();

    FTotorisVirtualGarbageConfig Config;
    FRandomStream Random;
    TArray<FTotorisVirtualGarbagePacket> Packets;
    double ElapsedSeconds = 0.0;
    double PhaseRemainingSeconds = 0.0;
    double Intensity = 0.0;
    double Threshold = 1.0;
    double VirtualAltitude = 0.0;
    double CumulativeVirtualAttackLines = 0.0;
    double WarningPulseElapsedSeconds = 0.0;
    int32 Difficulty = 5;
    int32 LastRecordedPlacementId = INDEX_NONE;
    int64 NextPacketId = 1;
    int64 NextGroupId = 1;
    bool bAggressivePhase = false;
};
