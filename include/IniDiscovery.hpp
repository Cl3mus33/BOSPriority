#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

/// Shared by BOSIniMerger (BOS's own *_SWAP.ini) and SpidDistrMerger (SPID's *_DISTR.ini): both
/// SKSE plugins discover and order their own config files the exact same way, so this is one
/// real, verified algorithm reused by two independent scanners - not a speculative abstraction.
namespace IniDiscovery {

/// Every file directly under <gameDir>/Data whose name ends in suffixLower (case-insensitive,
/// e.g. L"_swap" or L"_distr") before the .ini extension, sorted alphabetically - the same
/// discovery-and-ordering rule both BOS and SPID use at runtime (verified against BOS's real
/// Manager.cpp and SPID's own "SPID: The Complete Reference" documentation).
[[nodiscard]] auto findFiles(const std::filesystem::path& gameDir, const std::wstring& suffixLower)
    -> std::vector<std::filesystem::path>;

/// Several real-world ini exports start with a UTF-8 BOM (EF BB BF). Left unhandled, it glues
/// itself to the first line's first byte, silently breaking any check like "line.front() == '['"
/// or "line.front() == ';'" on the file's first real line. Must be called right after opening the
/// stream, before any getline().
void skipUtf8Bom(std::ifstream& f);

} // namespace IniDiscovery
