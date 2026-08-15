#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * Tracks mods relevant to Base Object Swapper: which ones ship a
 * SKSE/Plugins/BaseObjectSwapper/*_SWAP.ini, their native mod-manager order, and an optional
 * user-assigned priority override.
 *
 * Ported from AutoSeasons' PGModManager (https://github.com/Cl3mus33/AutoSeasons, GPLv3, itself
 * derived from PGPatcher by hakasapl: https://github.com/hakasapl/PGPatcher). Trimmed to drop
 * everything that isn't relevant to plain ini-file priority - no shader/mesh/conflict tracking,
 * no BSA handling, no game-type detection.
 */
class ModManager {
public:
    enum class ModManagerType : uint8_t { NONE, VORTEX, MODORGANIZER2 };

    struct Mod {
        std::wstring name;
        std::filesystem::path folder;
        bool isEnabled = false;
        bool hasBosIni = false;
        /// Index in the mod manager's native ordering: lower = applied earlier = loses conflicts
        /// (mirrors AutoSeasons: modlist.txt is read top to bottom, top gets the lowest index).
        int modManagerOrder = 0;
        /// User-assigned priority; -1 means "not set, fall back to modManagerOrder". Higher
        /// values are applied later and win conflicts - same convention as AutoSeasons.
        int priority = -1;
    };

    explicit ModManager(ModManagerType mmType);

    /// Populates the mod list from an MO2 instance directory (modorganizer.ini + the active
    /// profile's modlist.txt). Throws if outputDir is itself an enabled mod in that profile.
    void populateModsMO2(const std::filesystem::path& instanceDir, const std::filesystem::path& outputDir);

    /// Populates the mod list from a Vortex deployment manifest (vortex.deployment.json).
    void populateModsVortex(const std::filesystem::path& deploymentDir);

    [[nodiscard]] auto getMods() const -> std::vector<std::shared_ptr<Mod>>;

    /// Enabled mods that ship at least one BOS ini, sorted from lowest to highest applied
    /// priority: iterate in this order and let each subsequent mod's lines overwrite conflicting
    /// keys from the ones before it - the last mod in the returned vector wins.
    [[nodiscard]] auto getBosModsInApplyOrder() const -> std::vector<std::shared_ptr<Mod>>;

    [[nodiscard]] auto getMod(const std::wstring& modName) const -> std::shared_ptr<Mod>;

    /// Restores previously-saved user priority overrides (does nothing for mods not present).
    void loadJSON(const nlohmann::json& json);
    /// Serializes only mods with an explicit user-assigned priority (priority >= 0).
    [[nodiscard]] auto getJSON() const -> nlohmann::json;

    [[nodiscard]] static auto isValidMO2InstanceDir(const std::filesystem::path& instanceDir) -> bool;
    [[nodiscard]] static auto getGamePathFromInstanceDir(const std::filesystem::path& instanceDir)
        -> std::filesystem::path;
    [[nodiscard]] static auto getSelectedProfileFromInstanceDir(const std::filesystem::path& instanceDir)
        -> std::wstring;

private:
    [[nodiscard]] static auto getMO2INIField(const std::filesystem::path& instanceDir,
                                              const std::string& fieldName,
                                              bool isByteArray) -> std::wstring;
    [[nodiscard]] static auto getMO2FilePaths(const std::filesystem::path& instanceDir)
        -> std::pair<std::filesystem::path, std::filesystem::path>;
    [[nodiscard]] static auto decodeQtByteArrayValue(const std::string& byteArrayVal) -> std::wstring;
    [[nodiscard]] static auto fromHexDigit(char c) -> uint8_t;
    [[nodiscard]] static auto hasBosIniFiles(const std::filesystem::path& modFolder) -> bool;

    ModManagerType m_mmType;
    std::unordered_map<std::wstring, std::shared_ptr<Mod>> m_modMap;

    static constexpr const char* MO2INI_PROFILESDIR_KEY = "profiles_directory=";
    static constexpr const char* MO2INI_MODDIR_KEY = "mod_directory=";
    static constexpr const char* MO2INI_BASEDIR_KEY = "base_directory=";
    static constexpr const char* MO2INI_GAMEDIR_KEY = "gamePath=";
    static constexpr const char* MO2INI_PROFILE_KEY = "selected_profile=";
    static constexpr const char* MO2INI_BASEDIR_WILDCARD = "%BASE_DIR%";
    static constexpr const char* MO2INI_BYTEARRAYPREFIX = "@ByteArray(";
    static constexpr const char* MO2INI_BYTEARRAYSUFFIX = ")";
    static constexpr uint8_t HEX_ALPHA_BASE = 10U;
};
