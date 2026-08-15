#include "PluginTypeResolver.hpp"
#include "StringUtil.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

namespace {

constexpr size_t HEADER_SIZE = 24;
constexpr uint32_t LOCAL_FORM_ID_MASK = 0x00FFFFFFU;

auto readU32LE(const uint8_t* p) -> uint32_t
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8U)
        | (static_cast<uint32_t>(p[2]) << 16U) | (static_cast<uint32_t>(p[3]) << 24U);
}

// Recursively walks a byte range containing top-level records/groups, filling formIdToType.
// Record header (24 bytes): Signature(4) DataSize:u32LE(4) Flags:u32LE(4) FormID:u32LE(4)
// VersionControlInfo(4, skipped) FormVersion+Unknown(4, skipped).
// Group header (24 bytes): "GRUP"(4) GroupSize:u32LE(4, INCLUDES this header)
// label/type/stamp(16, skipped) - payload size to recurse into = GroupSize - 24.
void walk(const uint8_t* data, size_t size, unordered_map<uint32_t, string>& out)
{
    size_t offset = 0;
    while (offset + HEADER_SIZE <= size) {
        const uint8_t* header = data + offset;
        string tag(reinterpret_cast<const char*>(header), 4);

        if (tag == "GRUP") {
            const uint32_t groupSize = readU32LE(header + 4);
            if (groupSize < HEADER_SIZE || offset + groupSize > size) {
                break; // malformed/truncated - stop rather than misread the rest of the file
            }
            walk(data + offset + HEADER_SIZE, groupSize - HEADER_SIZE, out);
            offset += groupSize;
        } else {
            const uint32_t dataSize = readU32LE(header + 4);
            const uint32_t formId = readU32LE(header + 12);
            if (offset + HEADER_SIZE + dataSize > size) {
                break; // malformed/truncated
            }
            if (formId != 0) {
                out[formId & LOCAL_FORM_ID_MASK] = tag;
            }
            offset += HEADER_SIZE + dataSize;
        }
    }
}

auto toLowerAscii(string s) -> string
{
    ranges::transform(s, s.begin(), [](char c) { return static_cast<char>(tolower(static_cast<unsigned char>(c))); });
    return s;
}

} // namespace

auto PluginTypeResolver::buildIndex(const fs::path& pluginFile) -> PluginIndex
{
    PluginIndex index;

    ifstream f(pluginFile, ios::binary);
    if (!f.is_open()) {
        return index;
    }

    f.seekg(0, ios::end);
    const auto fileSize = f.tellg();
    if (fileSize <= 0) {
        return index;
    }
    f.seekg(0, ios::beg);

    vector<uint8_t> data(static_cast<size_t>(fileSize));
    f.read(reinterpret_cast<char*>(data.data()), fileSize);

    walk(data.data(), data.size(), index.formIdToType);
    return index;
}

auto PluginTypeResolver::getOrBuildIndex(const fs::path& dataDir, const string& pluginName) -> const PluginIndex*
{
    const string key = toLowerAscii(pluginName);

    if (const auto it = m_cache.find(key); it != m_cache.end()) {
        return &it->second;
    }

    const auto pluginFile = dataDir / StringUtil::utf8ToUtf16(pluginName);
    if (!fs::exists(pluginFile)) {
        return nullptr;
    }

    const auto [it, inserted] = m_cache.emplace(key, buildIndex(pluginFile));
    return &it->second;
}

auto PluginTypeResolver::resolveType(const fs::path& dataDir, const string& pluginName, uint32_t localFormId)
    -> optional<string>
{
    const auto* index = getOrBuildIndex(dataDir, pluginName);
    if (index == nullptr) {
        return nullopt;
    }

    const auto it = index->formIdToType.find(localFormId & LOCAL_FORM_ID_MASK);
    if (it == index->formIdToType.end()) {
        return nullopt;
    }

    return it->second;
}
