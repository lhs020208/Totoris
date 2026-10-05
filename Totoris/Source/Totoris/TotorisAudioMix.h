#pragma once

// Per-effect output multipliers.  Keep every value at 1.0f to preserve the
// SoundWave asset's current loudness; adjust only these values when balancing
// the imported effects against each other.
namespace TotorisAudioMix
{
    // The existing per-effect values represent the game's 50% master-volume
    // mix.  The UI maps 0..100% to 0.0f..2.0f, keeping the established mix
    // unchanged at the default 50% value (1.0f).
    inline constexpr float DefaultMasterVolume = 1.0f;
    inline float MasterVolume = DefaultMasterVolume;
    inline bool bSitoMode = false;

    inline constexpr float UiClickTrue = 0.3f;
    inline constexpr float UiClickFalse = 0.3f;

    inline constexpr float BlockRotate = 0.5f;
    inline constexpr float BlockSpin = 4.0f;
    inline constexpr float SoftDrop = 0.5f;
    inline constexpr float HardDrop = 1.0f;
    inline constexpr float Hold = 1.0f;
    inline constexpr float Translate = 0.5f;
    inline constexpr float Attack = 1.0f;
    inline constexpr float Garbage = 1.0f;
    inline constexpr float SimpleLineClear = 1.0f;
    inline constexpr float SpinLineClear = 0.3f;
    inline constexpr float QuadLineClear = 1.0f;
    inline constexpr float AllClear = 1.0f;

    inline constexpr float SitoQuadLineClear = 2.0f;
    inline constexpr float SitoSpinLineClear = 2.0f;
    inline constexpr float SitoAllClear = 4.0f;
}
