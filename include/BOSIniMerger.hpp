#pragma once

#include <filesystem>
#include <string>
#include <vector>

/// One mod to fold into the merge, already in the order it should be applied (last wins).
struct BosMergeSourceMod {
    std::wstring name;
    std::filesystem::path folder;
};

struct BosMergeStats {
    int filesRead = 0;
    int linesRead = 0;
    int linesWritten = 0;
    int keysOverridden = 0;
};

/**
 * Parses every `*_SWAP.ini` under `<mod folder>/SKSE/Plugins/BaseObjectSwapper` for each mod,
 * folds them into a single merged structure in the given order (a later mod in
 * `modsInApplyOrder` wins a conflicting key over an earlier one - see
 * ModManager::getBosModsInApplyOrder), and writes the result as one AIO_SWAP.ini.
 *
 * Mirrors the format and merge algorithm already validated in "BOS AIO Patcher.pas" (the xEdit
 * script written for this same tool family): sections like [Forms] / [References] / filtered
 * variants, each data line is `origBaseID|swapBaseID|propertyOverrides|chance`, key = the first
 * pipe-delimited field, last write wins per key. The only behavioural difference from BOS's own
 * native merge: here the winning order is the user-chosen mod priority instead of raw
 * alphabetical filename order.
 */
class BOSIniMerger {
public:
    /// dryRun: parses and reports what would happen, writes nothing to outputFolder.
    static auto merge(const std::vector<BosMergeSourceMod>& modsInApplyOrder,
                       const std::filesystem::path& outputFolder,
                       bool dryRun) -> BosMergeStats;
};
