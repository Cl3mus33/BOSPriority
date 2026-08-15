#include "PluginTypeResolver.hpp"
#include "StringUtil.hpp"

#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

namespace {

constexpr size_t HEADER_SIZE = 24;
constexpr uint32_t LOCAL_FORM_ID_MASK = 0x00FFFFFFU;
constexpr uint32_t COMPRESSED_RECORD_FLAG = 0x00040000U;
// Real Bethesda records are at most a few MB decompressed - this just bounds a corrupt/crafted
// "decompressed size" field from driving an enormous allocation before uncompress() ever runs.
constexpr uint32_t MAX_SANE_DECOMPRESSED_SIZE = 64U * 1024U * 1024U;

auto readU32LE(const uint8_t* p) -> uint32_t
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8U)
        | (static_cast<uint32_t>(p[2]) << 16U) | (static_cast<uint32_t>(p[3]) << 24U);
}

auto readU16LE(const uint8_t* p) -> uint16_t
{
    return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8U));
}

// Walks a byte range containing top-level records/groups, calling visit(tag, formId, flags,
// payload, payloadSize) once per non-GRUP record. Iterative (an explicit stack of ranges still to
// process) rather than recursive: a GRUP only needs its 24-byte header to queue up another one, so
// a crafted or merely corrupted plugin file could otherwise nest deep enough to overflow the call
// stack - this parses arbitrary binary files from the user's mod folder, not trusted input.
// Record header (24 bytes): Signature(4) DataSize:u32LE(4) Flags:u32LE(4) FormID:u32LE(4)
// VersionControlInfo(4, skipped) FormVersion+Unknown(4, skipped).
// Group header (24 bytes): "GRUP"(4) GroupSize:u32LE(4, INCLUDES this header)
// label/type/stamp(16, skipped) - payload size to recurse into = GroupSize - 24.
template <typename Visitor>
void walkRecords(const uint8_t* data, size_t size, Visitor&& visit)
{
    struct Range {
        const uint8_t* data;
        size_t size;
    };
    vector<Range> pending {{data, size}};

    while (!pending.empty()) {
        const auto [rangeData, rangeSize] = pending.back();
        pending.pop_back();

        size_t offset = 0;
        while (offset + HEADER_SIZE <= rangeSize) {
            const uint8_t* header = rangeData + offset;
            string tag(reinterpret_cast<const char*>(header), 4);

            if (tag == "GRUP") {
                const uint32_t groupSize = readU32LE(header + 4);
                if (groupSize < HEADER_SIZE || offset + groupSize > rangeSize) {
                    break; // malformed/truncated - stop rather than misread the rest of the file
                }
                pending.push_back({rangeData + offset + HEADER_SIZE, groupSize - HEADER_SIZE});
                offset += groupSize;
            } else {
                const uint32_t dataSize = readU32LE(header + 4);
                const uint32_t flags = readU32LE(header + 8);
                const uint32_t formId = readU32LE(header + 12);
                if (offset + HEADER_SIZE + dataSize > rangeSize) {
                    break; // malformed/truncated
                }
                visit(tag, formId, flags, rangeData + offset + HEADER_SIZE, static_cast<size_t>(dataSize));
                offset += HEADER_SIZE + dataSize;
            }
        }
    }
}

auto toLowerAscii(string s) -> string
{
    ranges::transform(s, s.begin(), [](char c) { return static_cast<char>(tolower(static_cast<unsigned char>(c))); });
    return s;
}

auto readWholeFile(const fs::path& file) -> vector<uint8_t>
{
    ifstream f(file, ios::binary);
    if (!f.is_open()) {
        return {};
    }

    f.seekg(0, ios::end);
    const auto fileSize = f.tellg();
    if (fileSize <= 0) {
        return {};
    }
    f.seekg(0, ios::beg);

    vector<uint8_t> data(static_cast<size_t>(fileSize));
    f.read(reinterpret_cast<char*>(data.data()), fileSize);
    return data;
}

// Finds the "EDID" subrecord in one record's (already-decompressed) data and returns its string
// value, or nullopt if this record has none / it's malformed. Subrecord header: 4-byte
// signature + 2-byte size:u16LE (6 bytes), followed by that many bytes of data.
// The "XXXX" large-subrecord extension (a real EDID never needs it - it's for subrecords over
// 65535 bytes) isn't handled: encountering one means every subsequent subrecord offset in this
// record is unknowable without decoding it, so parsing simply stops for this record rather than
// risk reading garbage as if it were still subrecord-aligned.
auto findEdid(const uint8_t* data, size_t size) -> optional<string>
{
    size_t offset = 0;
    while (offset + 6 <= size) {
        const string subTag(reinterpret_cast<const char*>(data + offset), 4);
        const uint16_t subLen = readU16LE(data + offset + 4);
        if (subTag == "XXXX") {
            return nullopt;
        }
        if (offset + 6 + subLen > size) {
            return nullopt; // malformed
        }
        if (subTag == "EDID") {
            string edid(reinterpret_cast<const char*>(data + offset + 6), subLen);
            while (!edid.empty() && edid.back() == '\0') {
                edid.pop_back();
            }
            return edid.empty() ? nullopt : optional<string>(edid);
        }
        offset += 6 + subLen;
    }
    return nullopt;
}

} // namespace

auto PluginTypeResolver::buildIndex(const fs::path& pluginFile) -> PluginIndex
{
    PluginIndex index;
    const auto data = readWholeFile(pluginFile);
    if (data.empty()) {
        return index;
    }

    walkRecords(data.data(), data.size(), [&](const string& tag, uint32_t formId, uint32_t /*flags*/,
                                                const uint8_t* /*payload*/, size_t /*payloadSize*/) {
        if (formId != 0) {
            index.formIdToType[formId & LOCAL_FORM_ID_MASK] = tag;
        }
    });
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

void PluginTypeResolver::indexPluginEditorIds(const fs::path& pluginFile)
{
    const auto data = readWholeFile(pluginFile);
    if (data.empty()) {
        return;
    }

    walkRecords(data.data(), data.size(),
        [&](const string& tag, uint32_t /*formId*/, uint32_t flags, const uint8_t* payload, size_t payloadSize) {
            const uint8_t* subData = payload;
            size_t subSize = payloadSize;
            vector<uint8_t> decompressed;

            if ((flags & COMPRESSED_RECORD_FLAG) != 0) {
                if (payloadSize < 4) {
                    return; // malformed - no room for the decompressed-size prefix
                }
                const uint32_t decompressedSize = readU32LE(payload);
                if (decompressedSize == 0 || decompressedSize > MAX_SANE_DECOMPRESSED_SIZE) {
                    return; // corrupt or absurd - refuse rather than allocate blindly
                }
                decompressed.resize(decompressedSize);
                auto destLen = static_cast<uLongf>(decompressedSize);
                const int result = uncompress(decompressed.data(), &destLen,
                    payload + 4, static_cast<uLong>(payloadSize - 4));
                if (result != Z_OK || destLen != decompressedSize) {
                    return; // corrupt compressed data - skip this record
                }
                subData = decompressed.data();
                subSize = decompressed.size();
            }

            if (const auto edid = findEdid(subData, subSize)) {
                // First plugin indexed wins a collision - real load orders keep EditorIDs unique,
                // so this is a rare, accepted tie-break rather than a real ambiguity to resolve.
                m_edidToType.try_emplace(toLowerAscii(*edid), tag);
            }
        });
}

void PluginTypeResolver::ensureEdidIndexBuilt(const fs::path& dataDir)
{
    if (m_edidIndexBuilt) {
        return;
    }
    m_edidIndexBuilt = true; // set first - a partial/failed build this run should not retry per-key

    if (!fs::exists(dataDir) || !fs::is_directory(dataDir)) {
        return;
    }

    for (const auto& entry : fs::directory_iterator(dataDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto ext = StringUtil::toLowerW(entry.path().extension().wstring());
        if (ext != L".esp" && ext != L".esm" && ext != L".esl") {
            continue;
        }
        indexPluginEditorIds(entry.path());
    }
}

auto PluginTypeResolver::resolveTypeByEditorId(const fs::path& dataDir, const string& editorId) -> optional<string>
{
    ensureEdidIndexBuilt(dataDir);

    const auto it = m_edidToType.find(toLowerAscii(editorId));
    if (it == m_edidToType.end()) {
        return nullopt;
    }
    return it->second;
}
