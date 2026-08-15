#include "BOSIniMerger.hpp"
#include "BOSLocale.hpp"
#include "GUI/LauncherWindow.hpp"
#include "StringUtil.hpp"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <wx/wx.h>

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>

using namespace std;
namespace fs = std::filesystem;

namespace {

constexpr const wchar_t* SETTINGS_FILE_NAME = L"BOSPriority_settings.json";

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

auto getExecutableDir() -> fs::path
{
    array<wchar_t, MAX_PATH> buffer {};
    if (GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size())) == 0) {
        return {};
    }
    return fs::path(buffer.data()).parent_path();
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

// Only what's needed before the first window exists: uiLanguage/uiTheme must be known to
// initialize BOSLocale and apply the requested appearance before any wxWindow is created (see
// runGUI below) - LauncherWindow's own saveSettings()/InitParams plumbing takes over for
// everything after that.
auto loadInitialParams(const fs::path& exeDir) -> LauncherWindow::InitParams
{
    LauncherWindow::InitParams params;
    string uiLanguage = "en";

    const auto file = exeDir / SETTINGS_FILE_NAME;
    if (fs::exists(file)) {
        try {
            ifstream f(file);
            const auto json = nlohmann::json::parse(f);
            if (json.contains("gameDir") && json["gameDir"].is_string()) {
                params.gameDir = StringUtil::utf8ToUtf16(json["gameDir"].get<string>());
            }
            if (json.contains("outputDir") && json["outputDir"].is_string()) {
                params.outputDir = StringUtil::utf8ToUtf16(json["outputDir"].get<string>());
            }
            if (json.contains("uiLanguage") && json["uiLanguage"].is_string()) {
                uiLanguage = json["uiLanguage"].get<string>();
            }
            if (json.contains("uiTheme") && json["uiTheme"].is_string()) {
                params.theme = StringUtil::utf8ToUtf16(json["uiTheme"].get<string>());
            }
        } catch (const exception&) {
            // corrupt/unreadable - start with defaults
        }
    }

    BOSLocale::init(exeDir / "BOSPriority_translations", uiLanguage);
    return params;
}

auto runGUI() -> int
{
    // Console-subsystem build (so CLI/automation output and the pause-before-exit prompt work),
    // but that means a console window pops up alongside the GUI window every time - minimize it
    // instead of leaving it sitting in front. Still reachable from the taskbar if ever needed;
    // the CLI path above never touches this, console stays visible there.
    if (HWND consoleWindow = GetConsoleWindow(); consoleWindow != nullptr) {
        ShowWindow(consoleWindow, SW_MINIMIZE);
    }

    const auto exeDir = getExecutableDir();

    // Bootstrapped manually (not via wxIMPLEMENT_APP/wxEntry(argc,argv)) so there's a point to
    // call wxTheApp->SetAppearance() at, before any window exists - it's a wxApp-level setting
    // that doesn't live-update already-shown windows. Same approach as AutoSeasons' main.cpp,
    // required by the same constraint, not an arbitrary style choice.
    wxApp::SetInstance(new wxApp()); // NOLINT(cppcoreguidelines-owning-memory)
    if (!wxEntryStart(nullptr, nullptr)) {
        cerr << "Failed to initialize wxWidgets.\n";
        return 1;
    }

    auto params = loadInitialParams(exeDir);

    int result = 0;
    do {
        // Re-applied every loop iteration so a theme change made in the Options tab takes effect
        // on the relaunch/restart that follows it.
        wxApp::Appearance appearance = wxApp::Appearance::System;
        if (params.theme == "light") {
            appearance = wxApp::Appearance::Light;
        } else if (params.theme == "dark") {
            appearance = wxApp::Appearance::Dark;
        }
        if (wxTheApp->SetAppearance(appearance) != wxApp::AppearanceResult::Ok) {
            // Not fatal - the window still opens, just without the requested forced appearance
            // (can happen if the OS/wxWidgets combination doesn't support overriding the
            // system-wide light/dark preference per-app).
            cerr << "Could not apply the requested theme; falling back to system appearance.\n";
        }
        // SetAppearance() alone only covers a few high-level things (e.g. the title bar) -
        // painting individual controls in dark colors needs wx's separate MSW-specific dark mode
        // support, turned on via MSWEnableDarkMode(). Only called for "dark" specifically: calling
        // it for "system" turned out to make dark rendering win regardless of SetAppearance()
        // whenever the OS itself is in dark mode, which broke explicit "Light". Same reasoning and
        // sequence as AutoSeasons' main.cpp, which found this the hard way.
        if (params.theme == "dark") {
            wxTheApp->MSWEnableDarkMode(wxApp::DarkMode_Always);
        }

        auto* launcher = new LauncherWindow(params); // NOLINT(cppcoreguidelines-owning-memory)
        result = launcher->ShowModal();
        if (result == LauncherWindow::RESULT_RELAUNCH || result == LauncherWindow::RESULT_RESTART) {
            // Preserve current (possibly unsaved) field values across the rebuild.
            params = launcher->getParams();
        }
        launcher->Destroy();
    } while (result == LauncherWindow::RESULT_RELAUNCH);

    if (result == LauncherWindow::RESULT_RESTART) {
        // Settings (including the new theme) were already saved by LauncherWindow::onThemeChanged
        // before EndModal() - respawn a fresh process so wx's MSW dark mode support starts clean.
        // wxEntryCleanup() is deliberately deferred until after this attempt - calling it first
        // would tear down wx before a failure could be reported through it.
        STARTUPINFOW startupInfo {};
        startupInfo.cb = sizeof(startupInfo);
        PROCESS_INFORMATION processInfo {};
        const auto exeFullPath = (exeDir / "BOSPriority.exe").wstring();
        const auto workingDir = exeDir.wstring();
        if (CreateProcessW(exeFullPath.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, workingDir.c_str(),
                &startupInfo, &processInfo)
            != 0) {
            CloseHandle(processInfo.hProcess);
            CloseHandle(processInfo.hThread);
        } else {
            cerr << "Failed to restart BOSPriority for the new theme (error " << GetLastError() << ").\n";
        }
    }

    wxEntryCleanup();
    return 0;
}

} // namespace

auto main(int argc, char** argv) -> int
{
    // No arguments: launch the GUI. Any argument: CLI11 automation mode (see addArguments above).
    if (argc > 1) {
        return runCLI(argc, argv);
    }

    return runGUI();
}
