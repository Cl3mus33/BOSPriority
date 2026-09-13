#include "IniDiscovery.hpp"
#include "StringUtil.hpp"

#include <algorithm>
#include <array>

using namespace std;
namespace fs = std::filesystem;

namespace IniDiscovery {

auto findFiles(const fs::path& gameDir, const wstring& suffixLower) -> vector<fs::path>
{
    const auto dataDir = gameDir / L"Data";

    vector<fs::path> found;
    if (!fs::exists(dataDir) || !fs::is_directory(dataDir)) {
        return found;
    }

    for (const auto& entry : fs::directory_iterator(dataDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (StringUtil::toLowerW(entry.path().extension().wstring()) != L".ini") {
            continue;
        }
        if (!StringUtil::toLowerW(entry.path().stem().wstring()).ends_with(suffixLower)) {
            continue;
        }
        found.push_back(entry.path());
    }

    ranges::sort(found); // alphabetical - matches both BOS's and SPID's own runtime discovery order
    return found;
}

void skipUtf8Bom(ifstream& f)
{
    constexpr array<unsigned char, 3> bom {0xEF, 0xBB, 0xBF};
    array<char, 3> buf {};
    const auto start = f.tellg();
    f.read(buf.data(), static_cast<streamsize>(buf.size()));
    if (f.gcount() == static_cast<streamsize>(buf.size())
        && static_cast<unsigned char>(buf[0]) == bom[0] && static_cast<unsigned char>(buf[1]) == bom[1]
        && static_cast<unsigned char>(buf[2]) == bom[2]) {
        return; // BOM consumed - stream stays positioned right after it
    }
    f.clear();
    f.seekg(start);
}

} // namespace IniDiscovery
