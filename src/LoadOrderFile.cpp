#include "LoadOrderFile.hpp"
#include "IniDiscovery.hpp"

#include <algorithm>
#include <fstream>

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

} // namespace

auto LoadOrderFile::load(const fs::path& path) -> vector<LoadOrderEntry>
{
    ifstream f(path);
    if (!f.is_open()) {
        return {};
    }
    IniDiscovery::skipUtf8Bom(f);

    vector<string> lines;
    string line;
    while (getline(f, line)) {
        line = trim(line);
        if (!line.empty()) {
            lines.push_back(std::move(line));
        }
    }

    const bool anyStarMarker = ranges::any_of(lines, [](const string& l) { return l.front() == '*'; });

    vector<LoadOrderEntry> result;
    result.reserve(lines.size());
    for (const auto& l : lines) {
        const bool starred = l.front() == '*';
        result.push_back(LoadOrderEntry {
            .pluginFileName = starred ? trim(l.substr(1)) : l,
            .active = anyStarMarker ? starred : true,
        });
    }
    return result;
}
