#include "ModManager.hpp"
#include "StringUtil.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <cstring>
#include <fstream>
#include <ranges>
#include <regex>
#include <stdexcept>

using namespace std;
namespace fs = std::filesystem;

namespace {

auto trimQuotes(string s) -> string
{
    const auto first = s.find_first_not_of('"');
    if (first == string::npos) {
        return {};
    }
    const auto last = s.find_last_not_of('"');
    return s.substr(first, last - first + 1);
}

auto startsWith(const string& s, const char* prefix) -> bool
{
    return s.rfind(prefix, 0) == 0;
}

auto endsWith(const string& s, const char* suffix) -> bool
{
    const auto suffixLen = strlen(suffix);
    if (s.size() < suffixLen) {
        return false;
    }
    return s.compare(s.size() - suffixLen, suffixLen, suffix) == 0;
}

void replaceAll(wstring& s, const wstring& from, const wstring& to)
{
    if (from.empty()) {
        return;
    }
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != wstring::npos) {
        s.replace(pos, from.length(), to);
        pos += to.length();
    }
}

auto toLowerW(wstring s) -> wstring
{
    ranges::transform(s, s.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return s;
}

} // namespace

ModManager::ModManager(ModManagerType mmType)
    : m_mmType(mmType)
{
}

auto ModManager::fromHexDigit(char c) -> uint8_t
{
    if (c >= '0' && c <= '9') {
        return static_cast<uint8_t>(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return static_cast<uint8_t>(HEX_ALPHA_BASE + (c - 'a'));
    }
    if (c >= 'A' && c <= 'F') {
        return static_cast<uint8_t>(HEX_ALPHA_BASE + (c - 'A'));
    }
    return 0;
}

auto ModManager::decodeQtByteArrayValue(const string& byteArrayVal) -> wstring
{
    // Qt may serialize QByteArray values with C-style escapes like "\xC3\xA0".
    string decoded;
    decoded.reserve(byteArrayVal.size());

    for (size_t i = 0; i < byteArrayVal.size();) {
        const auto c = byteArrayVal[i];
        if (c == '\\' && i + 1 < byteArrayVal.size()) {
            const auto next = byteArrayVal[i + 1];
            if (next == 'x' && i + 3 < byteArrayVal.size()) {
                const auto hi = byteArrayVal[i + 2];
                const auto lo = byteArrayVal[i + 3];
                if (isxdigit(static_cast<unsigned char>(hi)) != 0 && isxdigit(static_cast<unsigned char>(lo)) != 0) {
                    decoded.push_back(static_cast<char>((fromHexDigit(hi) << 4U) | fromHexDigit(lo)));
                    i += 4;
                    continue;
                }
            }
            if (next == '\\') {
                decoded.push_back('\\');
                i += 2;
                continue;
            }
        }
        decoded.push_back(c);
        ++i;
    }

    return StringUtil::utf8ToUtf16(decoded);
}

auto ModManager::isValidMO2InstanceDir(const fs::path& instanceDir) -> bool
{
    return fs::exists(instanceDir / "modorganizer.ini");
}

auto ModManager::getMO2INIField(const fs::path& instanceDir, const string& fieldName, bool isByteArray) -> wstring
{
    const fs::path iniFile = instanceDir / L"modorganizer.ini";
    if (!fs::exists(iniFile)) {
        return {};
    }

    ifstream f(iniFile);
    string line;
    while (getline(f, line)) {
        if (!startsWith(line, fieldName.c_str())) {
            continue;
        }

        auto value = trimQuotes(line.substr(fieldName.size()));

        if (isByteArray && startsWith(value, MO2INI_BYTEARRAYPREFIX) && endsWith(value, MO2INI_BYTEARRAYSUFFIX)) {
            const auto prefixLen = strlen(MO2INI_BYTEARRAYPREFIX);
            const auto suffixLen = strlen(MO2INI_BYTEARRAYSUFFIX);
            const auto inner = value.substr(prefixLen, value.size() - prefixLen - suffixLen);
            return decodeQtByteArrayValue(inner);
        }

        return StringUtil::utf8ToUtf16(value);
    }

    return {};
}

auto ModManager::getGamePathFromInstanceDir(const fs::path& instanceDir) -> fs::path
{
    return getMO2INIField(instanceDir, MO2INI_GAMEDIR_KEY, true);
}

auto ModManager::getSelectedProfileFromInstanceDir(const fs::path& instanceDir) -> wstring
{
    return getMO2INIField(instanceDir, MO2INI_PROFILE_KEY, true);
}

auto ModManager::getMO2FilePaths(const fs::path& instanceDir) -> pair<fs::path, fs::path>
{
    if (!fs::exists(instanceDir / L"modorganizer.ini")) {
        return {{}, {}};
    }

    auto profileDirField = getMO2INIField(instanceDir, MO2INI_PROFILESDIR_KEY, true);
    auto modDirField = getMO2INIField(instanceDir, MO2INI_MODDIR_KEY, true);
    fs::path baseDir = getMO2INIField(instanceDir, MO2INI_BASEDIR_KEY, true);

    if (baseDir.empty()) {
        baseDir = instanceDir;
    }

    const wstring wildcard = StringUtil::utf8ToUtf16(MO2INI_BASEDIR_WILDCARD);
    replaceAll(profileDirField, wildcard, baseDir.wstring());
    replaceAll(modDirField, wildcard, baseDir.wstring());

    fs::path profileDir = profileDirField.empty() ? baseDir / "profiles" : fs::path(profileDirField);
    fs::path modDir = modDirField.empty() ? baseDir / "mods" : fs::path(modDirField);

    return {profileDir, modDir};
}

auto ModManager::hasBosIniFiles(const fs::path& modFolder) -> bool
{
    const auto bosDir = modFolder / L"SKSE" / L"Plugins" / L"BaseObjectSwapper";
    if (!fs::exists(bosDir) || !fs::is_directory(bosDir)) {
        return false;
    }

    for (const auto& entry : fs::directory_iterator(bosDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (toLowerW(entry.path().extension().wstring()) != L".ini") {
            continue;
        }
        if (toLowerW(entry.path().stem().wstring()).ends_with(L"_swap")) {
            return true;
        }
    }

    return false;
}

void ModManager::populateModsMO2(const fs::path& instanceDir, const fs::path& outputDir)
{
    const auto [profileDir, modDir] = getMO2FilePaths(instanceDir);
    const auto curProfile = getSelectedProfileFromInstanceDir(instanceDir);
    const auto modListFile = profileDir / curProfile / "modlist.txt";

    if (!fs::exists(modListFile)) {
        throw runtime_error("MO2 modlist.txt not found: " + modListFile.string());
    }

    ifstream f(modListFile);
    string line;
    int basePriority = 0;

    while (getline(f, line)) {
        wstring mod = StringUtil::utf8ToUtf16(line);
        if (mod.empty()) {
            continue;
        }
        if (mod.starts_with(L"-") || mod.starts_with(L"*")) {
            continue; // disabled / uncontrolled
        }
        if (mod.starts_with(L"#")) {
            continue; // comment
        }
        if (mod.ends_with(L"_separator")) {
            continue;
        }

        mod.erase(0, 1); // strip leading '+'
        const auto curModDir = modDir / mod;
        if (!fs::exists(curModDir)) {
            continue;
        }

        // fs::equivalent() requires both paths to exist - the output folder legitimately does
        // not exist yet on a first run, in which case it trivially cannot be this mod's folder.
        if (fs::exists(outputDir) && fs::equivalent(curModDir, outputDir)) {
            throw runtime_error("The output folder is an enabled MO2 mod - disable it first so "
                                 "BOSPriority does not scan its own previous output.");
        }

        auto modPtr = make_shared<Mod>();
        modPtr->name = mod;
        modPtr->folder = curModDir;
        modPtr->isEnabled = true;
        modPtr->modManagerOrder = basePriority++;
        modPtr->hasBosIni = hasBosIniFiles(curModDir);

        if (m_modMap.contains(mod)) {
            modPtr->priority = m_modMap[mod]->priority; // keep previously restored priority, if any
        }

        m_modMap[mod] = modPtr;
    }
}

void ModManager::populateModsVortex(const fs::path& deploymentDir)
{
    const auto deploymentFile = deploymentDir / "vortex.deployment.json";
    if (!fs::exists(deploymentFile)) {
        throw runtime_error("Vortex deployment file not found: " + deploymentFile.string());
    }

    ifstream f(deploymentFile);
    nlohmann::json deployment = nlohmann::json::parse(f);
    if (!deployment.contains("files") || !deployment.contains("stagingPath")) {
        throw runtime_error("Vortex deployment file is missing 'files' or 'stagingPath'");
    }

    const auto stagingPath = fs::path(StringUtil::utf8ToUtf16(deployment["stagingPath"].get<string>()));
    static const wregex suffixRe(L"-[0-9]+-.*");

    unordered_map<wstring, fs::path> modFolders;
    for (const auto& file : deployment["files"]) {
        const auto sourceId = StringUtil::utf8ToUtf16(file.at("source").get<string>());
        const wstring modName = regex_replace(sourceId, suffixRe, L"");

        if (modFolders.contains(modName)) {
            continue;
        }
        const auto curModDir = stagingPath / sourceId;
        if (!fs::exists(curModDir)) {
            continue;
        }
        modFolders[modName] = curModDir;
    }

    int order = 0;
    for (const auto& [modName, folder] : modFolders) {
        auto modPtr = make_shared<Mod>();
        modPtr->name = modName;
        modPtr->folder = folder;
        modPtr->isEnabled = true;
        modPtr->modManagerOrder = order++; // Vortex exposes no native priority order here
        modPtr->hasBosIni = hasBosIniFiles(folder);

        if (m_modMap.contains(modName)) {
            modPtr->priority = m_modMap[modName]->priority;
        }

        m_modMap[modName] = modPtr;
    }
}

auto ModManager::getMods() const -> vector<shared_ptr<Mod>>
{
    vector<shared_ptr<Mod>> mods;
    mods.reserve(m_modMap.size());
    for (const auto& [name, mod] : m_modMap) {
        mods.push_back(mod);
    }
    return mods;
}

auto ModManager::getBosModsInApplyOrder() const -> vector<shared_ptr<Mod>>
{
    vector<shared_ptr<Mod>> mods;
    for (const auto& [name, mod] : m_modMap) {
        if (mod->isEnabled && mod->hasBosIni) {
            mods.push_back(mod);
        }
    }

    // Ascending: the mod that should win a conflicting key must sort LAST, since BOSIniMerger
    // folds mods in this order and lets each subsequent mod overwrite matching keys.
    ranges::stable_sort(mods, [](const auto& a, const auto& b) {
        const bool aHasPriority = a->priority >= 0;
        const bool bHasPriority = b->priority >= 0;
        if (aHasPriority != bHasPriority) {
            // Mods without an explicit user priority default to the bottom (lowest applied
            // priority) so an untouched priority list behaves like plain mod-manager order.
            return bHasPriority;
        }
        if (aHasPriority) {
            return a->priority < b->priority;
        }
        return a->modManagerOrder < b->modManagerOrder;
    });

    return mods;
}

auto ModManager::getMod(const wstring& modName) const -> shared_ptr<Mod>
{
    if (m_modMap.contains(modName)) {
        return m_modMap.at(modName);
    }
    return nullptr;
}

void ModManager::loadJSON(const nlohmann::json& json)
{
    if (!json.is_object()) {
        throw runtime_error("BOSPriority priority file is not a JSON object");
    }

    for (const auto& [modName, properties] : json.items()) {
        if (!properties.is_object() || !properties.contains("priority")) {
            continue;
        }
        const auto wname = StringUtil::utf8ToUtf16(modName);
        if (m_modMap.contains(wname)) {
            m_modMap[wname]->priority = properties["priority"].get<int>();
        }
    }
}

auto ModManager::getJSON() const -> nlohmann::json
{
    auto json = nlohmann::json::object();
    for (const auto& [name, mod] : m_modMap) {
        if (mod->priority < 0) {
            continue; // only persist explicit user overrides
        }
        json[StringUtil::utf16ToUtf8(name)] = {{"priority", mod->priority}};
    }
    return json;
}
