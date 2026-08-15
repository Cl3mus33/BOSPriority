#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

/**
 * Resolves a Base Object Swapper reference to the 4-character record signature (STAT, TREE,
 * MSTT, FLOR, ...) of the record it points to. BOS lines reference a record either way:
 *  - a hex FormID + explicit plugin filename ("0xHHHHHH~PluginName.esp") - resolveType() reads
 *    that one plugin's own record/group headers directly, no load-order/master-chain resolution
 *    needed, since the ini already names the exact defining plugin and a record's signature never
 *    changes between its definition and any override.
 *  - a bare EditorID ("Farmhouse04") - the far more common case in real-world BOS ini exports,
 *    despite this resolver's own comments once assuming otherwise. The line doesn't say which
 *    plugin defines it, so resolveTypeByEditorId() builds a global EditorID -> type index across
 *    every plugin in Data on first use (lazily, cached after) and looks it up there. Building that
 *    index needs to look inside each record's subrecord data (for its "EDID" subrecord), including
 *    decompressing compressed records - resolveType()'s header-only walk doesn't need to.
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
    /// this resolver recognised.
    [[nodiscard]] auto resolveType(const std::filesystem::path& dataDir,
                                    const std::string& pluginName,
                                    uint32_t localFormId) -> std::optional<std::string>;

    /// editorId: as it appears verbatim in the BOS ini line (case-insensitive lookup).
    /// Builds a global index across every .esp/.esm/.esl directly under dataDir the first time
    /// it's called, cached for the lifetime of this resolver. Returns nullopt if no plugin in
    /// dataDir defines a record with that EditorID, or on a collision this resolver can't tell
    /// apart it just returns whichever plugin was indexed first - real load orders are expected
    /// to keep EditorIDs unique, so this is a rare, accepted edge case rather than a real
    /// ambiguity.
    [[nodiscard]] auto resolveTypeByEditorId(const std::filesystem::path& dataDir, const std::string& editorId)
        -> std::optional<std::string>;

private:
    struct PluginIndex {
        std::unordered_map<uint32_t, std::string> formIdToType;
    };

    [[nodiscard]] auto getOrBuildIndex(const std::filesystem::path& dataDir, const std::string& pluginName)
        -> const PluginIndex*;

    static auto buildIndex(const std::filesystem::path& pluginFile) -> PluginIndex;

    void ensureEdidIndexBuilt(const std::filesystem::path& dataDir);
    void indexPluginEditorIds(const std::filesystem::path& pluginFile);

    std::unordered_map<std::string, PluginIndex> m_cache; // keyed by lowercased plugin filename

    std::unordered_map<std::string, std::string> m_edidToType; // lowercased EditorID -> type
    bool m_edidIndexBuilt = false;
};
