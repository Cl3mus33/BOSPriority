#pragma once

#include <filesystem>
#include <string>
#include <vector>

/// One line from a plugins.txt: a plugin filename (e.g. "SomeMod.esp") and whether it's active.
struct LoadOrderEntry {
    std::string pluginFileName;
    bool active = false;
};

/**
 * Parses a Bethesda-style plugins.txt (the format MO2/Vortex/the vanilla launcher all write):
 * one plugin filename per line, in load order (top loads first/lowest priority, bottom loads
 * last/highest priority - same "bottom wins" convention this app's own BOS priority-ordering UI
 * already uses), with an optional leading '*' marking a plugin as active (unmarked lines are
 * known-but-inactive plugins).
 *
 * Verified against community documentation (modding.wiki, STEP wiki) rather than Bethesda source
 * (the engine is closed-source, unlike BOS/SPID) - a lower certainty class than those two, but
 * multiple independent sources agree on this exact format.
 */
class LoadOrderFile {
public:
    /// Returns entries in file order = load order. Some older/alternate tooling writes a
    /// plugins.txt with no '*' markers at all (meaning every listed plugin is implicitly active) -
    /// detected file-wide (not per-line, so a file that legitimately has zero active plugins isn't
    /// misread): if no line anywhere in the file starts with '*', every non-empty line is treated
    /// as active; otherwise the standard '*' = active / no '*' = inactive rule applies.
    [[nodiscard]] static auto load(const std::filesystem::path& path) -> std::vector<LoadOrderEntry>;
};
