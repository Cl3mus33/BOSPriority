#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct SwapEntry {
    std::string sourceFile; // filename only, for display
    std::string line; // full raw ini line
};

/// One BOS ini key ("[section]" + the first pipe-delimited field). candidates.size() > 1 means
/// two or more source files disagree on this key - a real conflict. candidates.size() == 1 means
/// only one file defines it - nothing to decide, it is always included.
struct SwapKey {
    std::string section; // e.g. "[Forms]", "[References]", or a filtered variant
    std::string key;
    /// 4-character record signature (STAT, TREE, MSTT, FLOR, ...) resolved via
    /// PluginTypeResolver, or nullopt if the key doesn't reference a hex FormID~Plugin pair
    /// (e.g. it names an EditorID instead) or the plugin/FormID couldn't be found.
    std::optional<std::string> recordType;
    std::vector<SwapEntry> candidates;
    /// Index into candidates chosen as the winner. Defaults to the last candidate (BOS's own
    /// alphabetical-last-file-wins rule, since scan() discovers files in that same order).
    int selectedCandidate = 0;
    /// If true, this key is dropped entirely - no line for it ends up in AIO_SWAP.ini.
    bool excluded = false;
};

/// Filename used to persist winner/exclude decisions in an output folder - shared between the GUI
/// and the CLI so both read/write the exact same file.
inline constexpr const wchar_t* BOS_PRIORITY_DECISIONS_FILE_NAME = L"BOSPriority_decisions.json";

struct BosMergeStats {
    int filesRead = 0;
    int linesRead = 0;
    int linesWritten = 0;
    int keysOverridden = 0;
};

/**
 * Parses Base Object Swapper `*_SWAP.ini` files directly under `<gameDir>/Data` (BOS reads them
 * from Data\ itself, not a subfolder) and exposes every discovered key, grouped with all of its
 * candidate lines so a caller (the GUI's conflict table, or the CLI applying saved decisions) can
 * pick a winner or exclude it before generating output.
 *
 * merge() then writes a single AIO_SWAP.ini AND, matching AutoSeasons' own generation pattern,
 * an emptied stand-in for every original source file - once the output mod sits after every
 * source mod in the load order, those emptied files overwrite the originals in the merged view,
 * leaving only AIO_SWAP.ini active. Before doing that it verifies every non-excluded key
 * contributed exactly one line to the output; on any mismatch it aborts without touching
 * anything, rather than risk silently dropping a swap rule.
 *
 * scan() ignores any file that is itself a previous BOSPriority output (empty, or starting with
 * one of this tool's own marker comments) so re-scanning after a generate doesn't pick up its own
 * blanked stand-ins or AIO_SWAP.ini as if they were new source mods.
 */
class BOSIniMerger {
public:
    [[nodiscard]] static auto scan(const std::filesystem::path& gameDir) -> std::vector<SwapKey>;

    /// dryRun: computes stats and runs the verification pass, but writes nothing.
    [[nodiscard]] static auto merge(const std::vector<SwapKey>& keys,
                                     const std::filesystem::path& outputFolder,
                                     bool dryRun) -> BosMergeStats;

    /// Applies previously-saved winner/exclude decisions (see saveDecisions) to matching keys, by
    /// "section||key". Keys with no saved decision are left untouched. Does nothing (including on
    /// a corrupt file) if decisionsFile doesn't exist or can't be parsed - shared by the GUI and
    /// the CLI so both apply saved state identically.
    static void applyDecisions(std::vector<SwapKey>& keys, const std::filesystem::path& decisionsFile);

    /// Persists winner/exclude decisions for every conflicting key (candidates.size() > 1) to
    /// decisionsFile as JSON, keyed by "section||key". Non-conflicting keys have nothing to
    /// persist and are skipped.
    static void saveDecisions(const std::vector<SwapKey>& keys, const std::filesystem::path& decisionsFile);
};
