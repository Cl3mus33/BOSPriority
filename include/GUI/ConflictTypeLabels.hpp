#pragma once

#include "BOSLocale.hpp"

#include <wx/string.h>

/// Locale-independent sentinel keys used as internal lookup keys (ConflictTableDialog's groups,
/// TypePriorityDialog's per-type lists) AND as the on-disk keys BOSIniMerger::applyTypePriorities/
/// saveTypePriorities use (SwapKey::recordType.value_or("Unknown"), "All Types" as the ranking
/// fallback) - kept as fixed English text rather than a translated string so a saved ranking, and
/// matching a group's type against one, survive a language change instead of silently breaking.
inline constexpr const char* CONFLICT_ALL_TYPES_KEY = "All Types";
inline constexpr const char* CONFLICT_UNKNOWN_TYPE_KEY = "Unknown";

/// Translates a raw type key to what's actually shown in the UI. A real record signature ("STAT",
/// "TREE", ...) passes through unchanged - it's never translated, only the two sentinels are.
inline auto conflictDisplayTypeLabel(const wxString& rawType) -> wxString
{
    if (rawType == CONFLICT_ALL_TYPES_KEY) {
        return BOSTr("conflicts.allTypes", "All Types");
    }
    if (rawType == CONFLICT_UNKNOWN_TYPE_KEY) {
        return BOSTr("conflicts.unknownType", "Unknown");
    }
    return rawType;
}
