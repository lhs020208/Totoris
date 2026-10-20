#pragma once
#include "TotorisBlockGeneratorComponent.h"

namespace TotorisSChallengeRules
{
inline int32 AdvanceTSpinSequence(int32 Stage, const FTotorisMissionPlacementEvent& Event)
{
    if (Event.ClearedLines > 0 && !Event.bDifficultClear) return 0;
    if (Event.SpinKind == ETotorisSpinKind::None) return Stage;
    return Event.Mino == ETotorisMino::T && Event.SpinKind == ETotorisSpinKind::Full
        && Event.ClearedLines == Stage + 1 ? Stage + 1 : 0;
}
inline bool CompletesComboPC(const FTotorisMissionPlacementEvent& Event)
{
    return Event.ClearedLines > 0 && Event.ComboCount >= 10 && Event.bPerfectClear;
}
inline bool CompletesUniqueSpins(const FTotorisMissionPlacementEvent& Event)
{
    return Event.ClearedLines > 0 && Event.UniqueSpinCenterCount >= 5;
}
}
