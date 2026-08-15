#include "GUI/LauncherWindow.hpp"
#include "BOSIniMerger.hpp"
#include "GUI/ConflictTableDialog.hpp"
#include "StringUtil.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <exception>
#include <fstream>
#include <wx/dirdlg.h>
#include <windows.h>

using namespace std;
namespace fs = std::filesystem;

namespace {
constexpr int ID_BROWSE_GAME = wxID_HIGHEST + 10;
constexpr int ID_BROWSE_OUTPUT = wxID_HIGHEST + 11;
constexpr int ID_SCAN = wxID_HIGHEST + 12;
constexpr int ID_MANAGE_CONFLICTS = wxID_HIGHEST + 13;
constexpr int ID_GENERATE = wxID_HIGHEST + 14;

constexpr const wchar_t* SETTINGS_FILE_NAME = L"BOSPriority_settings.json";
constexpr int BORDER_SIZE = 5;

const wxColour ACCENT_DARK(27, 94, 32); // header banner background
const wxColour ACCENT(56, 142, 60); // section label text
const wxColour ACCENT_TEXT(255, 255, 255); // text on top of ACCENT_DARK

auto makeSectionLabel(wxWindow* parent, const wxString& text) -> wxStaticText*
{
    auto* label = new wxStaticText(parent, wxID_ANY, text);
    wxFont font = label->GetFont();
    font.SetWeight(wxFONTWEIGHT_BOLD);
    label->SetFont(font);
    label->SetForegroundColour(ACCENT);
    return label;
}

// Same "settings file next to the exe" convention as AutoSeasons' AutoSeasons_config.json.
auto getExecutableDir() -> fs::path
{
    array<wchar_t, MAX_PATH> buffer {};
    if (GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size())) == 0) {
        return {};
    }
    return fs::path(buffer.data()).parent_path();
}

auto countConflicts(const vector<SwapKey>& keys) -> ptrdiff_t
{
    return ranges::count_if(keys, [](const auto& k) { return k.candidates.size() > 1 && !k.isChancePool; });
}

} // namespace

LauncherWindow::LauncherWindow()
    : wxFrame(nullptr, wxID_ANY, "BOSPriority", wxDefaultPosition, wxSize(680, 560))
{
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    auto* headerPanel = new wxPanel(this);
    headerPanel->SetBackgroundColour(ACCENT_DARK);
    auto* headerSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* headerTitle = new wxStaticText(headerPanel, wxID_ANY, "BOSPriority");
    wxFont headerFont = headerTitle->GetFont();
    headerFont.SetPointSize(headerFont.GetPointSize() + 4);
    headerFont.SetWeight(wxFONTWEIGHT_BOLD);
    headerTitle->SetFont(headerFont);
    headerTitle->SetForegroundColour(ACCENT_TEXT);
    headerSizer->Add(headerTitle, 0, wxALL | wxALIGN_CENTER_VERTICAL, BORDER_SIZE * 2);
    headerPanel->SetSizerAndFit(headerSizer);
    mainSizer->Add(headerPanel, 0, wxEXPAND);

    auto* panel = new wxPanel(this);
    auto* topSizer = new wxBoxSizer(wxVERTICAL);

    auto* introText = new wxStaticText(panel, wxID_ANY,
        "Lets you set an explicit priority order for Base Object Swapper *_SWAP.ini files, "
        "instead of relying on BOS's own alphabetical-filename rule.");
    introText->Wrap(640);
    topSizer->Add(introText, 0, wxALL, BORDER_SIZE * 2);

    auto* warning = new wxStaticText(panel, wxID_ANY,
        "If you use Mod Organizer 2 or Vortex, run BOSPriority through its tool list/dashboard, "
        "not by double-clicking the exe in Explorer - otherwise it only sees your real Data "
        "folder, not your installed mods.");
    warning->SetForegroundColour(wxColour(180, 95, 0)); // amber - distinct from ACCENT, reads as caution
    warning->Wrap(640);
    topSizer->Add(warning, 0, wxLEFT | wxRIGHT | wxTOP, BORDER_SIZE * 2);

    auto* gameLocationLabel = makeSectionLabel(panel, "Game Location (folder containing Data\\)");
    topSizer->Add(gameLocationLabel, 0, wxLEFT | wxRIGHT | wxTOP, BORDER_SIZE * 2);

    auto* gameSizer = new wxBoxSizer(wxHORIZONTAL);
    m_gamePathCtrl = new wxTextCtrl(panel, wxID_ANY);
    gameSizer->Add(m_gamePathCtrl, 1, wxALL | wxEXPAND, BORDER_SIZE);
    gameSizer->Add(new wxButton(panel, ID_BROWSE_GAME, "Browse..."), 0, wxALL, BORDER_SIZE);
    topSizer->Add(gameSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, BORDER_SIZE);

    auto* outputLocationLabel = makeSectionLabel(panel, "Output Location (dedicated mod)");
    topSizer->Add(outputLocationLabel, 0, wxLEFT | wxRIGHT | wxTOP, BORDER_SIZE * 2);

    auto* outSizer = new wxBoxSizer(wxHORIZONTAL);
    m_outputPathCtrl = new wxTextCtrl(panel, wxID_ANY);
    outSizer->Add(m_outputPathCtrl, 1, wxALL | wxEXPAND, BORDER_SIZE);
    outSizer->Add(new wxButton(panel, ID_BROWSE_OUTPUT, "Browse..."), 0, wxALL, BORDER_SIZE);
    topSizer->Add(outSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, BORDER_SIZE);

    m_dryRunCheck = new wxCheckBox(panel, wxID_ANY, "Preview only (dry run)");
    topSizer->Add(m_dryRunCheck, 0, wxALL, BORDER_SIZE * 2);

    auto* actionSizer = new wxBoxSizer(wxHORIZONTAL);
    actionSizer->Add(new wxButton(panel, ID_SCAN, "Scan Mods"), 0, wxALL, BORDER_SIZE);
    m_manageConflictsButton = new wxButton(panel, ID_MANAGE_CONFLICTS, "Manage Conflicts...");
    m_manageConflictsButton->Disable();
    actionSizer->Add(m_manageConflictsButton, 0, wxALL, BORDER_SIZE);
    m_generateButton = new wxButton(panel, ID_GENERATE, "Generate");
    m_generateButton->Disable();
    actionSizer->Add(m_generateButton, 0, wxALL, BORDER_SIZE);
    topSizer->Add(actionSizer, 0, wxLEFT, BORDER_SIZE);

    auto* logLabel = makeSectionLabel(panel, "Log");
    topSizer->Add(logLabel, 0, wxLEFT | wxRIGHT | wxTOP, BORDER_SIZE * 2);

    m_logCtrl = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                wxTE_MULTILINE | wxTE_READONLY);
    topSizer->Add(m_logCtrl, 1, wxALL | wxEXPAND, BORDER_SIZE);

    panel->SetSizer(topSizer);
    mainSizer->Add(panel, 1, wxEXPAND);
    SetSizer(mainSizer);

    Bind(wxEVT_BUTTON, &LauncherWindow::onBrowseGame, this, ID_BROWSE_GAME);
    Bind(wxEVT_BUTTON, &LauncherWindow::onBrowseOutput, this, ID_BROWSE_OUTPUT);
    Bind(wxEVT_BUTTON, &LauncherWindow::onScan, this, ID_SCAN);
    Bind(wxEVT_BUTTON, &LauncherWindow::onManageConflicts, this, ID_MANAGE_CONFLICTS);
    Bind(wxEVT_BUTTON, &LauncherWindow::onGenerate, this, ID_GENERATE);

    log("Point \"Game Location\" at the folder that contains Data\\ (your Skyrim install, or "
        "wherever your mod manager launches the game from) - not the MO2 instance folder.");
    log("BOSPriority reads it exactly like the game would: if launched through MO2/Vortex, that "
        "path transparently shows your merged mod view; there is no need to separately resolve "
        "where your instance or mods folders live.");

    loadSettings();

    // Deferred: the frame isn't shown yet at this point (Show(true) happens right after this
    // constructor returns, in main.cpp's OnInit) - calling Close() from here directly would race
    // with that Show() and get silently overridden. CallAfter runs once the event loop is
    // actually pumping, after the window is up.
    CallAfter([this] { autoScanOnLaunch(); });
}

void LauncherWindow::log(const wxString& msg)
{
    m_logCtrl->AppendText(msg + "\n");
}

void LauncherWindow::updateButtonStates()
{
    const bool hasKeys = !m_keys.empty();
    const bool hasConflicts = ranges::any_of(m_keys, [](const auto& k) { return k.candidates.size() > 1 && !k.isChancePool; });
    m_manageConflictsButton->Enable(hasConflicts);
    m_generateButton->Enable(hasKeys);
}

void LauncherWindow::onBrowseGame(wxCommandEvent& /*event*/)
{
    wxDirDialog dlg(this, "Select Game Location (folder containing Data)", m_gamePathCtrl->GetValue());
    if (dlg.ShowModal() == wxID_OK) {
        m_gamePathCtrl->SetValue(dlg.GetPath());
    }
}

void LauncherWindow::onBrowseOutput(wxCommandEvent& /*event*/)
{
    wxDirDialog dlg(this, "Select output folder", m_outputPathCtrl->GetValue());
    if (dlg.ShowModal() == wxID_OK) {
        m_outputPathCtrl->SetValue(dlg.GetPath());
    }
}

void LauncherWindow::applySavedDecisions(vector<SwapKey>& keys) const
{
    const fs::path outputDir(m_outputPathCtrl->GetValue().ToStdWstring());
    BOSIniMerger::applyDecisions(keys, outputDir / BOS_PRIORITY_DECISIONS_FILE_NAME);
}

void LauncherWindow::saveDecisions() const
{
    const fs::path outputDir(m_outputPathCtrl->GetValue().ToStdWstring());
    if (outputDir.empty()) {
        return;
    }
    BOSIniMerger::saveDecisions(m_keys, outputDir / BOS_PRIORITY_DECISIONS_FILE_NAME);
}

void LauncherWindow::performScan()
{
    const fs::path gameDir(m_gamePathCtrl->GetValue().ToStdWstring());

    m_keys = BOSIniMerger::scan(gameDir);
    applySavedDecisions(m_keys);

    const auto conflictCount = countConflicts(m_keys);
    log(wxString::Format("Found %zu key(s), %lld in conflict.", m_keys.size(), conflictCount));
    if (!m_keys.empty() && conflictCount == 0) {
        log("No conflicts - BOS will already produce the correct result directly from these "
            "files, nothing needs generating. Generate is still there if you just want a single "
            "consolidated ini (e.g. to tidy up a modlist), but it's optional.");
    }
    updateButtonStates();
    saveSettings();
}

void LauncherWindow::onScan(wxCommandEvent& /*event*/)
{
    const fs::path gameDir(m_gamePathCtrl->GetValue().ToStdWstring());
    if (gameDir.empty()) {
        log("Pick a Game Location first.");
        return;
    }

    performScan();
}

void LauncherWindow::loadSettings()
{
    const auto file = getExecutableDir() / SETTINGS_FILE_NAME;
    if (!fs::exists(file)) {
        return;
    }

    try {
        ifstream f(file);
        const auto json = nlohmann::json::parse(f);
        if (json.contains("gameDir") && json["gameDir"].is_string()) {
            m_gamePathCtrl->SetValue(StringUtil::utf8ToUtf16(json["gameDir"].get<string>()));
        }
        if (json.contains("outputDir") && json["outputDir"].is_string()) {
            m_outputPathCtrl->SetValue(StringUtil::utf8ToUtf16(json["outputDir"].get<string>()));
        }
    } catch (const exception&) {
        // corrupt/unreadable settings file - just start with blank fields
    }
}

void LauncherWindow::saveSettings() const
{
    const auto dir = getExecutableDir();
    if (dir.empty()) {
        return;
    }

    nlohmann::json json;
    json["gameDir"] = StringUtil::utf16ToUtf8(m_gamePathCtrl->GetValue().ToStdWstring());
    json["outputDir"] = StringUtil::utf16ToUtf8(m_outputPathCtrl->GetValue().ToStdWstring());

    ofstream out(dir / SETTINGS_FILE_NAME);
    out << json.dump(2);
}

void LauncherWindow::autoScanOnLaunch()
{
    const fs::path gameDir(m_gamePathCtrl->GetValue().ToStdWstring());
    const fs::path outputDir(m_outputPathCtrl->GetValue().ToStdWstring());
    if (gameDir.empty() || outputDir.empty()) {
        return; // nothing remembered from a previous run - wait for the user
    }

    log("Remembered Game/Output locations from last time - scanning automatically...");
    performScan();

    if (m_keys.empty()) {
        return; // likely a stale/misconfigured path - let the user notice and fix it themselves
    }

    if (countConflicts(m_keys) > 0) {
        return; // there's real work to do, leave the window open on it
    }

    wxMessageDialog dlg(this,
        "Scanned automatically and found no conflicts - BOS will already produce the correct "
        "result directly from these files, nothing needs generating. Generating a single "
        "consolidated ini is still there if you want one (e.g. to tidy up a modlist), but it's "
        "entirely optional. Close BOSPriority?",
        "Nothing to manage", wxYES_NO | wxICON_INFORMATION);
    dlg.SetYesNoLabels("Close BOSPriority", "Keep Open");
    if (dlg.ShowModal() == wxID_YES) {
        Close(true);
    }
}

void LauncherWindow::onManageConflicts(wxCommandEvent& /*event*/)
{
    ConflictTableDialog dlg(this, m_keys);
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    m_keys = dlg.getKeys();
    saveDecisions();
    log("Conflict decisions updated and saved - remembered next time you scan this output folder.");
    updateButtonStates();
}

void LauncherWindow::onGenerate(wxCommandEvent& /*event*/)
{
    if (m_keys.empty()) {
        log("Nothing to merge - Scan Mods first.");
        return;
    }

    const fs::path outputDir(m_outputPathCtrl->GetValue().ToStdWstring());
    if (outputDir.empty()) {
        log("Pick an output folder first.");
        return;
    }

    const bool dryRun = m_dryRunCheck->GetValue();

    try {
        const auto stats = BOSIniMerger::merge(m_keys, outputDir, dryRun);

        log(wxString::Format(
            "%s: %d file(s) read, %d line(s) read, %d key(s) overridden by priority, %d line(s) %s.",
            dryRun ? "Preview" : "Done", stats.filesRead, stats.linesRead, stats.keysOverridden,
            stats.linesWritten, dryRun ? "would be written" : "written"));

        if (!dryRun) {
            log(wxString("Wrote ") + (outputDir / L"AIO_SWAP.ini").wstring());
            log("Original source ini files were replaced with empty stand-ins in this output "
                "folder - only AIO_SWAP.ini is active now.");
        }
    } catch (const exception& e) {
        log(wxString("Generation aborted: ") + e.what());
    }
}
