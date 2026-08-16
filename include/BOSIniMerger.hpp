#pragma once

#include <algorithm>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct SwapEntry {
    std::string sourceFile; // filename only, for display
    std::string line; // full raw ini line
    /// True when this candidate's swap TARGET couldn't be found in any plugin under Data at all -
    /// a hex reference naming a plugin that isn't there, or an EditorID nothing defines. BOS
    /// itself would just silently skip a swap like this at runtime (nothing to swap TO), so this
    /// candidate can never actually win even if picked - surfaced so the user isn't left guessing
    /// why a mod they expected to apply doesn't seem to.
    bool targetMissing = false;
};

/// One BOS ini key ("[section]" + the first pipe-delimited field). candidates.size() > 1 can mean
/// two different things, matching BOS's own Manager.cpp::log_conflicts distinction:
///  - a real conflict: several files disagree and the winning (last-loaded) line always applies
///    (chance field absent/"NONE" or a chance of 100) - only one should end up in the output.
///  - a chance pool: BOS itself keeps every entry with the same key in a vector and rolls between
///    them at runtime (chanceR/chanceS/chanceL) - ALL of them belong in the output unchanged, and
///    there is nothing to pick between (isChancePool is true, see below).
/// candidates.size() == 1 means only one file defines it - nothing to decide either way.
struct SwapKey {
    std::string section; // e.g. "[Forms]", "[References]", or a filtered variant
    std::string key;
    /// 4-character record signature (STAT, TREE, MSTT, FLOR, ...) of the swap TARGET (resolved
    /// via PluginTypeResolver from the second pipe field, which is always a base-object FormID -
    /// unlike the key itself, which for [References] entries names a REFR, not a useful type to
    /// filter by). nullopt if that field doesn't reference a hex FormID~Plugin pair (e.g. it
    /// names an EditorID instead) or the plugin/FormID couldn't be found.
    std::optional<std::string> recordType;
    std::vector<SwapEntry> candidates;
    /// True when candidates.size() > 1 but they're chance-weighted variants of the same key
    /// (see above) rather than a real conflict - BOS's own rule: the winning (last) candidate has
    /// a chance value below 100. Never shown in the conflict table and never reduced to a single
    /// winner; every candidate is written to AIO_SWAP.ini.
    bool isChancePool = false;
    /// Index into candidates chosen as the winner (ignored when isChancePool is true). Defaults
    /// to the last candidate (BOS's own alphabetical-last-file-wins rule, since scan() discovers
    /// files in that same order).
    int selectedCandidate = 0;
    /// If true, this key is dropped entirely - no line for it ends up in AIO_SWAP.ini. Not
    /// offered for chance pools.
    bool excluded = false;
    /// True once a real user action has settled this key: a radio pick or exclude toggle in the
    /// conflict table, or a saved decision loaded by applyDecisions(). Left false for a key that's
    /// just sitting at its default (BOS's own alphabetical-last-file-wins) because nobody has
    /// looked at it yet. saveDecisions() only persists keys where this is true - without it, every
    /// untouched conflict's arbitrary default would get written to disk as if it were a deliberate
    /// choice, and a mod installed later that also defines that key would silently lose to a
    /// "decision" nobody actually made. Not set by applyTypePriorities(): that ranking is meant to
    /// keep resolving whatever conflicts exist on a future scan, including ones that don't exist
    /// yet, so there's nothing here that needs freezing for it either.
    bool userDecided = false;

    /// True only for an actual cross-mod disagreement: 2+ non-chance candidates that come from
    /// more than one distinct source file AND don't all say the same thing. Both halves are
    /// needed, for different real-world reasons:
    ///  - same file, different lines (a mod author declaring the same key twice - happens in the
    ///    wild, e.g. a rolled vs. unrolled rug variant left in by mistake): nothing to decide,
    ///    BOS's own line-by-line parsing already deterministically keeps whichever is declared
    ///    last in that one file - exactly what selectedCandidate already defaults to.
    ///  - different files, identical lines (two mods shipping the same swap, or the same ini
    ///    redistributed under two names): also nothing to decide, every candidate produces the
    ///    same output. They're still kept as separate candidates rather than deduplicated away,
    ///    because merge() derives the set of files to blank from them - a file dropped here would
    ///    keep its original ini live in the load order alongside AIO_SWAP.ini.
    /// Either way it's excluded from the conflict table just like a chance pool, just resolved
    /// differently under the hood (only the winning line, not every candidate, reaches the output).
    [[nodiscard]] auto isRealConflict() const -> bool
    {
        if (candidates.size() < 2 || isChancePool) {
            return false;
        }
        const bool acrossFiles = std::ranges::any_of(
            candidates, [&](const SwapEntry& e) { return e.sourceFile != candidates.front().sourceFile; });
        const bool linesDisagree
            = std::ranges::any_of(candidates, [&](const SwapEntry& e) { return e.line != candidates.front().line; });
        return acrossFiles && linesDisagree;
    }
};

/// Filename used to persist winner/exclude decisions in an output folder - shared between the GUI
/// and the CLI so both read/write the exact same file.
inline constexpr const wchar_t* BOS_PRIORITY_DECISIONS_FILE_NAME = L"BOSPriority_decisions.json";

/// Filename used to persist the "Set Priority by Type" file ranking - kept separate from
/// BOSPriority_decisions.json because it isn't an answer to a specific key: it's reapplied fresh
/// after every scan (see applyTypePriorities()), so it naturally keeps resolving conflicts that
/// didn't exist when it was set, rather than needing its own touched/untouched tracking.
inline constexpr const wchar_t* BOS_PRIORITY_TYPE_RANKING_FILE_NAME = L"BOSPriority_priorities.json";

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
 * Chance-weighted duplicate keys (see SwapKey::isChancePool) are recognised and passed through in
 * full rather than treated as conflicts, matching BOS's own Manager.cpp behaviour exactly.
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
 *
 * A BOS section can restrict a rule to specific locations ("[Forms|A,B]" applies if the ref is in
 * A or B). scan() explodes a multi-location section into one group per listed location rather
 * than grouping by the raw section text, so a rule scoped to "[Forms|A,B]" in one file correctly
 * conflicts with another file's "[Forms|A]" for the same key - comparing the raw strings verbatim
 * would miss that real overlap entirely.
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

    /// Ranks source files - globally ("All Types") and/or per record type ("STAT", "Unknown" for
    /// an unresolved type, ...) - and applies that ranking to every real, non-excluded conflict:
    /// for each one independently (already at (section,key)/single-location granularity, not
    /// grouped), walks its type's list (falling back to "All Types") until it finds a file that
    /// actually has a candidate there, and picks it. A conflict whose type has no matching entry
    /// in either list, or that's already excluded, is left untouched. Does NOT set
    /// SwapKey::userDecided - see that field's doc comment for why.
    static void applyTypePriorities(std::vector<SwapKey>& keys,
                                     const std::map<std::string, std::vector<std::string>>& priorities);

    /// Persists a type-ranking (see applyTypePriorities) as JSON to rankingFile.
    static void saveTypePriorities(const std::map<std::string, std::vector<std::string>>& priorities,
                                    const std::filesystem::path& rankingFile);

    /// Loads a previously-saved ranking, or an empty map if rankingFile doesn't exist or can't be
    /// parsed.
    [[nodiscard]] static auto loadTypePriorities(const std::filesystem::path& rankingFile)
        -> std::map<std::string, std::vector<std::string>>;
};
