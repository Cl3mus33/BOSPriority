#pragma once

#include "LoadOrderFile.hpp"

#include <filesystem>
#include <string>
#include <vector>

/// One [Section]/key=value line read from an active plugin's own <PluginFileName>.ini.
struct PluginIniValue {
    std::string sourcePlugin; // e.g. "SomeMod.esp" - for display
    std::string section; // original-case text between [ ]
    std::string key; // original-case key text
    std::string value; // trimmed; if a file repeats a key under the same section, last wins
    std::string rawLine; // for display
};

/// Every PluginIniValue sharing the same (section, key), normalized (trimmed, case-folded).
/// candidates are in load-order scan order, so candidates.back() is always the value from the
/// last-loaded active plugin among them - the one actually applied in-game today, per Bethesda's
/// own record-conflict rule (last loaded wins).
struct PluginIniConflictGroup {
    std::string section;
    std::string key;
    std::vector<PluginIniValue> candidates;

    /// Unlike SwapKey/SpidConflictGroup's isRealConflict() (2+ candidates from 2+ distinct files,
    /// regardless of value), this one additionally requires the values to actually DIFFER. For
    /// BOS/SPID even an equal-looking duplicate is still part of the real resolution math (BOS
    /// needs every candidate to pick a winner regardless of content; SPID needs every candidate
    /// for its chance-rebalancing math). Here, if two plugins set the same [Section]key to the
    /// same value, the in-game result is correct no matter which one wins - flagging that would be
    /// pure noise with nothing to act on. Comparison is exact trimmed-string (not case-insensitive,
    /// not numeric-aware) - a known, accepted simplification: e.g. "1" vs "1.0" would still surface
    /// as a (false-positive) difference.
    [[nodiscard]] auto isRealConflict() const -> bool;
};

/**
 * Detects cross-plugin conflicts in Skyrim's per-plugin ini override system: the engine
 * auto-loads a file named exactly <PluginFileName>.ini (e.g. "SomeMod.esp.ini" for a plugin
 * "SomeMod.esp") from Data\, but only if that exact plugin is active in the load order. Real
 * Windows-ini format ([Section] headers, key=value pairs) - unlike BOS/SPID's headerless custom
 * formats. When two active plugins' inis set the same [Section]+key to different values, the
 * plugin that loads LAST wins (same rule as Bethesda record-conflict resolution) - not alphabetical
 * filename order like BOS or SPID.
 *
 * Verified against community documentation (modding.wiki, STEP wiki, multiple corroborating
 * threads) rather than Bethesda source or official docs - the engine is closed-source, so this is
 * a lower certainty class than the BOS/SPID precedents, though the sources agree on the mechanism.
 *
 * Read-only: scan() only detects and reports candidate conflicts. It does not pick a winner, does
 * not edit any ini, and does not generate a file - the write side is structurally harder here than
 * for BOS/SPID (there's no simple trick to make an earlier-loading plugin's ini win without
 * generating a real ESP plugin to load after everything), left for a possible later slice.
 */
class PluginIniMerger {
public:
    /// loadOrder: from LoadOrderFile::load(), in file order (= load order). Only active entries
    /// with a matching Data\<pluginFileName>.ini are read; most plugins won't have one.
    [[nodiscard]] static auto scan(const std::filesystem::path& gameDir,
        const std::vector<LoadOrderEntry>& loadOrder) -> std::vector<PluginIniConflictGroup>;
};
