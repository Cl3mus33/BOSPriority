#include "PluginIniMerger.hpp"
#include "IniDiscovery.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_map>

using namespace std;
namespace fs = std::filesystem;

namespace {

auto trim(string s) -> string
{
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
        ++start;
    }
    return s.substr(start);
}

auto toLowerAscii(string s) -> string
{
    ranges::transform(s, s.begin(), [](char c) { return static_cast<char>(tolower(static_cast<unsigned char>(c))); });
    return s;
}

// Grouping key only, never for display - trims and lowercases so real-world casing/spacing
// differences between plugins ("[Grass]" vs "[grass]", "iMinGrassSize" vs "IMinGrassSize") don't
// hide a real conflict.
auto normalizeForGrouping(const string& s) -> string
{
    return toLowerAscii(trim(s));
}

struct ParsedLine {
    string section;
    string key;
    string value;
    string rawLine;
};

// Hand-rolled [Section]/key=value/;-comment parser - no ini-parsing library is vendored via
// vcpkg in this project, and the format needed here is simple enough not to justify adding one.
// Within one file, a key repeated under the same section keeps only its LAST occurrence, matching
// how real ini readers behave and avoiding polluting a group with a value that plugin doesn't
// actually end up applying.
auto parsePluginIni(const fs::path& file, const string& pluginFileName) -> vector<ParsedLine>
{
    ifstream f(file);
    if (!f.is_open()) {
        return {};
    }
    IniDiscovery::skipUtf8Bom(f);

    unordered_map<string, size_t> indexByKey; // normalized "section\x1Fkey" -> index into result
    vector<ParsedLine> result;

    string currentSection;
    string line;
    while (getline(f, line)) {
        line = trim(line);
        if (line.empty() || line.front() == ';' || line.front() == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            currentSection = trim(line.substr(1, line.size() - 2));
            continue;
        }
        if (currentSection.empty()) {
            continue; // malformed/stray line before any section header - ignore
        }
        const auto eq = line.find('=');
        if (eq == string::npos) {
            continue;
        }
        const string key = trim(line.substr(0, eq));
        const string value = trim(line.substr(eq + 1));
        const string mapKey = normalizeForGrouping(currentSection) + "\x1F" + normalizeForGrouping(key);

        ParsedLine parsed {.section = currentSection, .key = key, .value = value, .rawLine = line};
        if (const auto it = indexByKey.find(mapKey); it != indexByKey.end()) {
            result[it->second] = std::move(parsed);
        } else {
            indexByKey[mapKey] = result.size();
            result.push_back(std::move(parsed));
        }
    }

    (void)pluginFileName;
    return result;
}

} // namespace

auto PluginIniConflictGroup::isRealConflict() const -> bool
{
    if (candidates.size() < 2) {
        return false;
    }
    const bool multiSource = ranges::any_of(
        candidates, [&](const PluginIniValue& v) { return v.sourcePlugin != candidates.front().sourcePlugin; });
    if (!multiSource) {
        return false;
    }
    return ranges::any_of(
        candidates, [&](const PluginIniValue& v) { return v.value != candidates.front().value; });
}

auto PluginIniMerger::scan(const fs::path& gameDir, const vector<LoadOrderEntry>& loadOrder)
    -> vector<PluginIniConflictGroup>
{
    vector<PluginIniConflictGroup> result;
    unordered_map<string, size_t> groupIndex; // "section\x1Fkey" -> index into result

    const fs::path dataDir = gameDir / "Data";
    for (const auto& entry : loadOrder) {
        if (!entry.active) {
            continue;
        }
        const fs::path iniPath = dataDir / (entry.pluginFileName + ".ini");
        if (!fs::exists(iniPath)) {
            continue;
        }

        for (auto& parsed : parsePluginIni(iniPath, entry.pluginFileName)) {
            const string mapKey
                = normalizeForGrouping(parsed.section) + "\x1F" + normalizeForGrouping(parsed.key);

            size_t idx = 0;
            if (const auto it = groupIndex.find(mapKey); it != groupIndex.end()) {
                idx = it->second;
            } else {
                idx = result.size();
                groupIndex[mapKey] = idx;
                result.push_back(PluginIniConflictGroup {.section = parsed.section, .key = parsed.key, .candidates = {}});
            }
            result[idx].candidates.push_back(PluginIniValue {
                .sourcePlugin = entry.pluginFileName,
                .section = parsed.section,
                .key = parsed.key,
                .value = parsed.value,
                .rawLine = parsed.rawLine,
            });
        }
    }

    return result;
}
