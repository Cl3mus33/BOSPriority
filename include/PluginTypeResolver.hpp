#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

/**
 * Resolves a Base Object Swapper FormID reference (local FormID + explicit plugin filename -
 * exactly the "0xHHHHHH~PluginName.esp" format BOS itself uses) to the 4-character record
 * signature (STAT, TREE, MSTT, FLOR, ...) of the record it points to.
 *
 * Reads the plugin's own record/group headers directly - no game-wide load order or
 * master-chain resolution needed, since BOS ini lines already name the exact defining plugin,
 * and a record's signature never changes between its definition and any override.
 *
 * Byte layout verified against Ortham/esplugin (the Rust library LOOT uses), not guessed:
 * https://github.com/Ortham/esplugin/blob/master/src/record.rs and src/group.rs
 */
class PluginTypeResolver {
public:
    /// dataDir: the folder containing the plugin (normally <gameDir>/Data).
    /// pluginName: exact filename as it appears in the BOS ini's "~Plugin" suffix.
    /// localFormId: the FormID as read from the ini (only the low 24 bits are used, matching
    /// the "& 0xFFFFFF" convention BOS's own export scripts use).
    /// Returns nullopt if the plugin can't be found/read, or the FormID isn't a record header
    /// this resolver recognised (e.g. it names an EditorID instead of a hex FormID).
    [[nodiscard]] auto resolveType(const std::filesystem::path& dataDir,
                                    const std::string& pluginName,
                                    uint32_t localFormId) -> std::optional<std::string>;

private:
    struct PluginIndex {
        std::unordered_map<uint32_t, std::string> formIdToType;
    };

    [[nodiscard]] auto getOrBuildIndex(const std::filesystem::path& dataDir, const std::string& pluginName)
        -> const PluginIndex*;

    static auto buildIndex(const std::filesystem::path& pluginFile) -> PluginIndex;

    std::unordered_map<std::string, PluginIndex> m_cache; // keyed by lowercased plugin filename
};
