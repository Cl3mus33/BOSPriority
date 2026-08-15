#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct BosMergeStats {
    int filesRead = 0;
    int linesRead = 0;
    int linesWritten = 0;
    int keysOverridden = 0;
};

/**
 * Parses and merges Base Object Swapper `*_SWAP.ini` files.
 *
 * Mirrors how AutoSeasons/PGPatcher actually discover files: point this at the real game
 * directory (the one containing `Data\`) and, when the process is launched through MO2 or
 * Vortex's tool list, the OS-level virtual file system (USVFS) transparently shows the merged
 * view of every installed mod - no need to independently resolve MO2's `mod_directory` or parse
 * `modlist.txt` at all, which is fragile whenever the MO2 instance folder and the mods folder
 * live in different places (a very common setup). That resolution is exactly what the previous
 * version of this tool did wrong.
 *
 * Priority here is tracked per ini FILENAME, not per mod - that is the actual unit BOS itself
 * resolves conflicts on (its own rule: alphabetically-last filename wins), and a mod can ship
 * more than one `*_SWAP.ini`.
 */
class BOSIniMerger {
public:
    /// Lists every `*_SWAP.ini` under `<gameDir>/Data/SKSE/Plugins/BaseObjectSwapper`, sorted
    /// alphabetically (BOS's own default order, used as the default priority order too).
    [[nodiscard]] static auto discoverIniFiles(const std::filesystem::path& gameDir)
        -> std::vector<std::filesystem::path>;

    /// Parses every file in `filesInApplyOrder` (already in the order they should be applied -
    /// last wins) and writes the merged result as one AIO_SWAP.ini. dryRun: parses and reports
    /// what would happen, writes nothing to outputFolder.
    [[nodiscard]] static auto merge(const std::vector<std::filesystem::path>& filesInApplyOrder,
                                     const std::filesystem::path& outputFolder,
                                     bool dryRun) -> BosMergeStats;
};
