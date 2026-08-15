#include "BOSIniMerger.hpp"
#include "GUI/LauncherWindow.hpp"
#include "StringUtil.hpp"

#include <CLI/CLI.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <wx/wx.h>

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>

using namespace std;
namespace fs = std::filesystem;

namespace {

// True when this process owns its console alone (double-clicked from Explorer, or launched by
// MO2/a similar tool that spawns a fresh console) rather than run from an existing terminal. Used
// to decide whether to pause before exiting, so result/error output isn't lost to a console
// window that closes the instant the process ends. Same pattern as AutoSeasons' main.cpp.
auto isSoleConsoleOwner() -> bool
{
    array<DWORD, 2> processList {};
    const auto count = GetConsoleProcessList(processList.data(), static_cast<DWORD>(processList.size()));
    return count <= 1;
}

void pauseBeforeExit()
{
    cout << "\nPress ENTER to exit...";
    cin.get();
}

struct BOSPriorityCLIArgs {
    string gameDir;
    string outputDir;
    vector<string> filePriority; // filenames, lowest applied priority first - fallback only,
                                  // used for conflicting keys with no saved per-key decision
    bool dryRun = false;
    int verbosity = 0;
};

void addArguments(CLI::App& app, BOSPriorityCLIArgs& args)
{
    app.add_flag("-v", args.verbosity, "Verbosity level -v for DEBUG data or -vv for TRACE data");
    app.add_option("game-dir", args.gameDir,
                   "Game location: the folder containing Data\\ (not an MO2 instance folder). "
                   "When launched through MO2/Vortex's tool list, this transparently shows your "
                   "merged mod view.")
        ->required();
    app.add_option("output", args.outputDir, "Output directory")->default_str("BOSPriority_Output");
    app.add_option("--file-priority", args.filePriority,
                   "*_SWAP.ini filenames, comma-separated, lowest priority first - fallback used "
                   "only for conflicting keys that have no saved decision (see the GUI's Manage "
                   "Conflicts table, saved as BOSPriority_decisions.json in the output folder). "
                   "Per-key winner/exclude edits are GUI-only, same as AutoSeasons' own "
                   "config-file-only per-type overrides.")
        ->delimiter(',');
    app.add_flag("--dry-run", args.dryRun,
                 "Scan and log what would be merged, but write nothing to the output directory")
        ->default_val(false);
}

// Applied only to keys with no saved per-key decision (BOSIniMerger::applyDecisions already ran).
void applyFilePriorityFallback(vector<SwapKey>& keys, const vector<string>& filePriority)
{
    if (filePriority.empty()) {
        return;
    }

    unordered_map<string, int> priorityMap;
    for (size_t i = 0; i < filePriority.size(); ++i) {
        priorityMap[filePriority[i]] = static_cast<int>(i);
    }

    for (auto& swapKey : keys) {
        if (swapKey.candidates.size() < 2 || swapKey.isChancePool) {
            continue;
        }

        int bestPriority = -1;
        int bestIndex = swapKey.selectedCandidate;
        for (size_t i = 0; i < swapKey.candidates.size(); ++i) {
            const auto it = priorityMap.find(swapKey.candidates[i].sourceFile);
            if (it != priorityMap.end() && it->second > bestPriority) {
                bestPriority = it->second;
                bestIndex = static_cast<int>(i);
            }
        }
        if (bestPriority >= 0) {
            swapKey.selectedCandidate = bestIndex;
        }
    }
}

auto runCLI(int argC, char** argV) -> int
{
    BOSPriorityCLIArgs args;
    CLI::App app {"BOSPriority: lets you set explicit priority for Base Object Swapper ini files"};
    addArguments(app, args);

    try {
        app.parse(argC, argV);
    } catch (const CLI::ParseError& e) {
        const auto code = app.exit(e);
        if (code != 0 && isSoleConsoleOwner()) {
            pauseBeforeExit();
        }
        return code;
    }

    const vector<spdlog::sink_ptr> sinks {make_shared<spdlog::sinks::stdout_color_sink_mt>()};
    auto logger = make_shared<spdlog::logger>("bospriority", sinks.begin(), sinks.end());
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    if (args.verbosity >= 1) {
        spdlog::set_level(spdlog::level::debug);
    }
    if (args.verbosity >= 2) {
        spdlog::set_level(spdlog::level::trace);
    }

    const fs::path gameDir = StringUtil::utf8ToUtf16(args.gameDir);
    const fs::path outputDir = StringUtil::utf8ToUtf16(args.outputDir);

    try {
        auto keys = BOSIniMerger::scan(gameDir);
        const auto conflictCount = ranges::count_if(keys, [](const auto& k) { return k.candidates.size() > 1 && !k.isChancePool; });
        spdlog::info("Found {} key(s), {} in conflict.", keys.size(), conflictCount);

        BOSIniMerger::applyDecisions(keys, outputDir / BOS_PRIORITY_DECISIONS_FILE_NAME);
        applyFilePriorityFallback(keys, args.filePriority);

        const auto stats = BOSIniMerger::merge(keys, outputDir, args.dryRun);
        spdlog::info("{}: {} file(s) read, {} line(s) read, {} key(s) overridden by priority, {} line(s) {}.",
                     args.dryRun ? "Preview" : "Done", stats.filesRead, stats.linesRead, stats.keysOverridden,
                     stats.linesWritten, args.dryRun ? "would be written" : "written");
    } catch (const exception& e) {
        spdlog::error("{}", e.what());
        if (isSoleConsoleOwner()) {
            pauseBeforeExit();
        }
        return 1;
    }

    if (isSoleConsoleOwner()) {
        pauseBeforeExit();
    }
    return 0;
}

class BOSPriorityApp : public wxApp {
public:
    auto OnInit() -> bool override
    {
        auto* window = new LauncherWindow();
        window->Show(true);
        return true;
    }
};

} // namespace

wxIMPLEMENT_APP_NO_MAIN(BOSPriorityApp);

auto main(int argc, char** argv) -> int
{
    // No arguments: launch the GUI. Any argument: CLI11 automation mode (see addArguments above).
    if (argc > 1) {
        return runCLI(argc, argv);
    }

    // GUI launch: the exe is a console-subsystem build (so CLI/automation output and the
    // pause-before-exit prompt work), but that means a console window pops up alongside the GUI
    // window every time - minimize it instead of leaving it sitting in front. Still reachable
    // from the taskbar if ever needed; CLI mode above never touches this, console stays visible.
    if (HWND consoleWindow = GetConsoleWindow(); consoleWindow != nullptr) {
        ShowWindow(consoleWindow, SW_MINIMIZE);
    }

    return wxEntry(argc, argv);
}
