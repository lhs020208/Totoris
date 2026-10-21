#pragma once
#include "TotorisBlockGeneratorComponent.h"

namespace TotorisSChallengeRules
{
inline uint8 AdvanceAllSpins(uint8 Mask, const FTotorisMissionPlacementEvent& Event)
{
    if (Event.ClearedLines > 0 && !Event.bDifficultClear) return 0;
    if (Event.ClearedLines > 0 && Event.SpinKind != ETotorisSpinKind::None && Event.Mino != ETotorisMino::O)
        Mask |= static_cast<uint8>(1u << static_cast<uint8>(Event.Mino));
    return Mask;
}
inline uint8 AdvanceAllTypes(uint8 Mask, const FTotorisMissionPlacementEvent& Event)
{
    if (Event.ClearedLines <= 0 || Event.ComboCount < 0) return 0;
    if (Event.ComboCount == 0) Mask = 0;
    const uint8 Bit = static_cast<uint8>(1u << static_cast<uint8>(Event.Mino));
    return (Mask & Bit) != 0 ? 0 : static_cast<uint8>(Mask | Bit);
}
inline int32 AdvanceTSpinSequence(int32 Stage, const FTotorisMissionPlacementEvent& Event)
{
    if (Event.ClearedLines > 0 && !Event.bDifficultClear) return 0;
    if (Event.SpinKind == ETotorisSpinKind::None) return Stage;
    return Event.Mino == ETotorisMino::T && Event.SpinKind == ETotorisSpinKind::Full
        && Event.ClearedLines == Stage + 1 ? Stage + 1 : 0;
}
inline bool CompletesComboPC(const FTotorisMissionPlacementEvent& Event)
{
    return Event.ClearedLines > 0 && Event.ComboCount >= 5
        && (Event.bPerfectClear || Event.bColorClear);
}
inline bool CompletesUniqueSpins(const FTotorisMissionPlacementEvent& Event)
{
    return Event.ClearedLines > 0 && Event.UniqueSpinCenterCount >= 5;
}
}
