#include "SpidDistrMerger.hpp"
#include "IniDiscovery.hpp"
#include "StringUtil.hpp"

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

// Returns the Nth (0-indexed) pipe-delimited field of a SPID distributable-form value
// (FormOrEditorID|StringFilters|FormFilters|LevelFilters|TraitFilters|CountOrIndex|Chance).
// A field left blank (or entirely absent, if it's a trailing one) returns an empty string -
// SPID's own documented default for every optional field.
auto nthField(const string& value, size_t n) -> string
{
    size_t start = 0;
    for (size_t i = 0; i < n; ++i) {
        const auto p = value.find('|', start);
        if (p == string::npos) {
            return {};
        }
        start = p + 1;
    }
    const auto end = value.find('|', start);
    return trim(end == string::npos ? value.substr(start) : value.substr(start, end - start));
}

auto toLowerAscii(string s) -> string
{
    ranges::transform(s, s.begin(), [](char c) { return static_cast<char>(tolower(static_cast<unsigned char>(c))); });
    return s;
}

// A normalized filter field is used only as a grouping key, never for display - strips all
// whitespace and lowercases, since real-world inis vary in spacing ("A, B" vs "A,B") and casing
// without changing what the filter actually means to SPID.
auto normalizeForGrouping(const string& field) -> string
{
    string result;
    result.reserve(field.size());
    for (const char c : field) {
        if (isspace(static_cast<unsigned char>(c)) != 0) {
            continue;
        }
        result += static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

// Chance field: a plain number, optionally suffixed with "!" (deterministic per-actor/save roll -
// same numeric value either way), "NONE", or absent - all meaning the SPID-documented default of
// 100 except a genuine number.
auto parseSpidChance(const string& rawField) -> float
{
    string field = trim(rawField);
    if (!field.empty() && field.back() == '!') {
        field.pop_back();
        field = trim(field);
    }
    if (field.empty() || toLowerAscii(field) == "none") {
        return 100.0F;
    }
    try {
        return stof(field);
    } catch (const exception&) {
        return 100.0F;
    }
}

// Recognizes only the 3 "single slot" distributable types this tool tracks (see
// SpidDistrMerger.hpp's class doc comment for why the rest are skipped) - returns the canonical
// display name, or empty if `keyword` isn't one of them.
auto recognizedRecordType(const string& keyword) -> string
{
    const string lower = toLowerAscii(trim(keyword));
    if (lower == "outfit") {
        return "Outfit";
    }
    if (lower == "sleepoutfit") {
        return "SleepOutfit";
    }
    if (lower == "skin") {
        return "Skin";
    }
    return {};
}

} // namespace

auto SpidConflictGroup::isRealConflict() const -> bool
{
    if (candidates.size() < 2) {
        return false;
    }
    return ranges::any_of(
        candidates, [&](const SpidEntry& e) { return e.sourceFile != candidates.front().sourceFile; });
}

auto SpidDistrMerger::scan(const fs::path& gameDir) -> vector<SpidConflictGroup>
{
    const auto files = IniDiscovery::findFiles(gameDir, L"_distr");

    vector<SpidConflictGroup> result;
    unordered_map<string, size_t> groupIndex; // "type\x1Fsignature" -> index into result

    for (const auto& file : files) {
        ifstream f(file);
        if (!f.is_open()) {
            continue;
        }
        IniDiscovery::skipUtf8Bom(f);

        const string fileName = StringUtil::utf16ToUtf8(file.filename().wstring());
        string line;
        while (getline(f, line)) {
            line = trim(line);
            if (line.empty() || line.front() == ';') {
                continue;
            }

            // SPID entries have no section headers at all (unlike BOS) - every line is its own
            // "Keyword = value" pair, read directly from the ini's unnamed default section.
            const auto eq = line.find('=');
            if (eq == string::npos) {
                continue;
            }
            const string recordType = recognizedRecordType(line.substr(0, eq));
            if (recordType.empty()) {
                continue; // not Outfit/SleepOutfit/Skin - additive type, nothing to conflict-check
            }
            const string value = trim(line.substr(eq + 1));

            SpidEntry entry;
            entry.sourceFile = fileName;
            entry.line = line;
            entry.formOrEditorId = nthField(value, 0);
            entry.stringFilters = nthField(value, 1);
            entry.formFilters = nthField(value, 2);
            entry.levelFilters = nthField(value, 3);
            entry.traitFilters = nthField(value, 4);
            // field 5 = Count/Index, not relevant to conflict grouping
            entry.chance = parseSpidChance(nthField(value, 6));

            const string signature = normalizeForGrouping(entry.stringFilters) + "\x1F"
                + normalizeForGrouping(entry.formFilters) + "\x1F" + normalizeForGrouping(entry.levelFilters)
                + "\x1F" + normalizeForGrouping(entry.traitFilters);
            const string mapKey = recordType + "\x1F" + signature;

            size_t idx = 0;
            if (const auto it = groupIndex.find(mapKey); it != groupIndex.end()) {
                idx = it->second;
            } else {
                idx = result.size();
                groupIndex[mapKey] = idx;
                result.push_back(SpidConflictGroup {recordType, {}});
            }
            result[idx].candidates.push_back(std::move(entry));
        }
    }

    return result;
}
