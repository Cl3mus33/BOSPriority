#include "BOSIniMerger.hpp"
#include "GUI/LauncherWindow.hpp"
#include "ModManager.hpp"
#include "StringUtil.hpp"

#include <CLI/CLI.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <wx/wx.h>

#include <array>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
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
    vector<string> modPriority; // lowest applied priority first
    bool dryRun = false;
    int verbosity = 0;
};

void addArguments(CLI::App& app, BOSPriorityCLIArgs& args)
{
    app.add_flag("-v", args.verbosity, "Verbosity level -v for DEBUG data or -vv for TRACE data");
    app.add_option(
           "game-dir", args.gameDir, "MO2 instance folder, Vortex staging folder, or a plain Data folder")
        ->required();
    app.add_option("output", args.outputDir, "Output directory")->default_str("BOSPriority_Output");
    app.add_option("--mod-priority", args.modPriority,
                   "Mod names, comma-separated, lowest priority first - when two mods both ship a "
                   "BOS ini touching the same key, the one listed later wins. Mods not listed keep "
                   "their mod-manager order, below every named mod.")
        ->delimiter(',');
    app.add_flag("--dry-run", args.dryRun,
                 "Scan and log what would be merged, but write nothing to the output directory")
        ->default_val(false);
}

auto buildModManager(const fs::path& gameDir, const fs::path& outputDir) -> unique_ptr<ModManager>
{
    if (ModManager::isValidMO2InstanceDir(gameDir)) {
        spdlog::info("Detected a Mod Organizer 2 instance.");
        auto mm = make_unique<ModManager>(ModManager::ModManagerType::MODORGANIZER2);
        mm->populateModsMO2(gameDir, outputDir);
        return mm;
    }
    if (fs::exists(gameDir / "vortex.deployment.json")) {
        spdlog::info("Detected a Vortex deployment manifest.");
        auto mm = make_unique<ModManager>(ModManager::ModManagerType::VORTEX);
        mm->populateModsVortex(gameDir);
        return mm;
    }
    spdlog::info("No modorganizer.ini or vortex.deployment.json found - treating this as a plain "
                 "merged Data folder.");
    return make_unique<ModManager>(ModManager::ModManagerType::NONE);
}

auto runCLI(int argC, char** argV) -> int
{
    BOSPriorityCLIArgs args;
    CLI::App app {"BOSPriority: lets you set explicit mod priority for Base Object Swapper ini files"};
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
        auto modManager = buildModManager(gameDir, outputDir);

        for (size_t i = 0; i < args.modPriority.size(); ++i) {
            const auto modName = StringUtil::utf8ToUtf16(args.modPriority[i]);
            if (auto mod = modManager->getMod(modName)) {
                mod->priority = static_cast<int>(i);
            } else {
                spdlog::warn("--mod-priority named a mod that was not found or is not enabled: {}",
                             args.modPriority[i]);
            }
        }

        const auto bosMods = modManager->getBosModsInApplyOrder();
        spdlog::info("Found {} mod(s) shipping a BaseObjectSwapper ini.", bosMods.size());

        vector<BosMergeSourceMod> sources;
        sources.reserve(bosMods.size());
        for (const auto& mod : bosMods) {
            sources.push_back(BosMergeSourceMod {mod->name, mod->folder});
        }

        const auto stats = BOSIniMerger::merge(sources, outputDir, args.dryRun);
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

    return wxEntry(argc, argv);
}
