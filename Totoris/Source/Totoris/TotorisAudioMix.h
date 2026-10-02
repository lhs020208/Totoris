#pragma once

// Per-effect output multipliers.  Keep every value at 1.0f to preserve the
// SoundWave asset's current loudness; adjust only these values when balancing
// the imported effects against each other.
namespace TotorisAudioMix
{
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
}
