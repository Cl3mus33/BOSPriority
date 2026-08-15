#include "GUI/LauncherWindow.hpp"
#include "BOSIniMerger.hpp"
#include "BOSLocale.hpp"
#include "GUI/ConflictTableDialog.hpp"
#include "StringUtil.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <exception>
#include <fstream>
#include <wx/dirdlg.h>
#include <wx/notebook.h>
#include <windows.h>

using namespace std;
namespace fs = std::filesystem;

namespace {
constexpr int ID_BROWSE_GAME = wxID_HIGHEST + 10;
constexpr int ID_BROWSE_OUTPUT = wxID_HIGHEST + 11;
constexpr int ID_SCAN = wxID_HIGHEST + 12;
constexpr int ID_MANAGE_CONFLICTS = wxID_HIGHEST + 13;
constexpr int ID_GENERATE = wxID_HIGHEST + 14;
constexpr int ID_LANGUAGE = wxID_HIGHEST + 15;
constexpr int ID_THEME = wxID_HIGHEST + 16;

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
    return ranges::count_if(keys, [](const auto& k) { return k.isRealConflict(); });
}

} // namespace

LauncherWindow::LauncherWindow(const InitParams& initParams)
    : wxDialog(nullptr, wxID_ANY, "BOSPriority", wxDefaultPosition, wxSize(680, 600),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_theme(initParams.theme)
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

    auto* notebook = new wxNotebook(this, wxID_ANY);

    // "General" tab - the actual per-run controls.
    auto* generalPanel = new wxPanel(notebook);
    auto* topSizer = new wxBoxSizer(wxVERTICAL);

    auto* introText = new wxStaticText(generalPanel, wxID_ANY,
        BOSTr("launcher.intro",
            "Lets you set an explicit priority order for Base Object Swapper *_SWAP.ini files, "
            "instead of relying on BOS's own alphabetical-filename rule."));
    introText->Wrap(620);
    topSizer->Add(introText, 0, wxALL, BORDER_SIZE * 2);

    auto* warning = new wxStaticText(generalPanel, wxID_ANY,
        BOSTr("launcher.warning",
            "If you use Mod Organizer 2 or Vortex, run BOSPriority through its tool list/dashboard, "
            "not by double-clicking the exe in Explorer - otherwise it only sees your real Data "
            "folder, not your installed mods."));
    warning->SetForegroundColour(wxColour(180, 95, 0)); // amber - distinct from ACCENT, reads as caution
    warning->Wrap(620);
    topSizer->Add(warning, 0, wxLEFT | wxRIGHT | wxTOP, BORDER_SIZE * 2);

    auto* gameLocationLabel = makeSectionLabel(
        generalPanel, BOSTr("launcher.gameLocation.label", "Game Location (folder containing Data\\)"));
    topSizer->Add(gameLocationLabel, 0, wxLEFT | wxRIGHT | wxTOP, BORDER_SIZE * 2);

    auto* gameSizer = new wxBoxSizer(wxHORIZONTAL);
    m_gamePathCtrl = new wxTextCtrl(generalPanel, wxID_ANY, initParams.gameDir);
    gameSizer->Add(m_gamePathCtrl, 1, wxALL | wxEXPAND, BORDER_SIZE);
    gameSizer->Add(new wxButton(generalPanel, ID_BROWSE_GAME, BOSTr("launcher.browse", "Browse...")), 0, wxALL,
                   BORDER_SIZE);
    topSizer->Add(gameSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, BORDER_SIZE);

    auto* outputLocationLabel = makeSectionLabel(
        generalPanel, BOSTr("launcher.outputLocation.label", "Output Location (dedicated mod)"));
    topSizer->Add(outputLocationLabel, 0, wxLEFT | wxRIGHT | wxTOP, BORDER_SIZE * 2);

    auto* outSizer = new wxBoxSizer(wxHORIZONTAL);
    m_outputPathCtrl = new wxTextCtrl(generalPanel, wxID_ANY, initParams.outputDir);
    outSizer->Add(m_outputPathCtrl, 1, wxALL | wxEXPAND, BORDER_SIZE);
    outSizer->Add(new wxButton(generalPanel, ID_BROWSE_OUTPUT, BOSTr("launcher.browse", "Browse...")), 0, wxALL,
                  BORDER_SIZE);
    topSizer->Add(outSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, BORDER_SIZE);

    m_dryRunCheck = new wxCheckBox(generalPanel, wxID_ANY, BOSTr("launcher.dryRun", "Preview only (dry run)"));
    topSizer->Add(m_dryRunCheck, 0, wxALL, BORDER_SIZE * 2);

    auto* actionSizer = new wxBoxSizer(wxHORIZONTAL);
    actionSizer->Add(new wxButton(generalPanel, ID_SCAN, BOSTr("launcher.scanButton", "Scan Mods")), 0, wxALL,
                     BORDER_SIZE);
    m_manageConflictsButton
        = new wxButton(generalPanel, ID_MANAGE_CONFLICTS, BOSTr("launcher.manageConflictsButton", "Manage Conflicts..."));
    m_manageConflictsButton->Disable();
    actionSizer->Add(m_manageConflictsButton, 0, wxALL, BORDER_SIZE);
    m_generateButton = new wxButton(generalPanel, ID_GENERATE, BOSTr("launcher.generateButton", "Generate"));
    m_generateButton->Disable();
    actionSizer->Add(m_generateButton, 0, wxALL, BORDER_SIZE);
    topSizer->Add(actionSizer, 0, wxLEFT, BORDER_SIZE);

    auto* logLabel = makeSectionLabel(generalPanel, BOSTr("launcher.logLabel", "Log"));
    topSizer->Add(logLabel, 0, wxLEFT | wxRIGHT | wxTOP, BORDER_SIZE * 2);

    m_logCtrl = new wxTextCtrl(generalPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                wxTE_MULTILINE | wxTE_READONLY);
    topSizer->Add(m_logCtrl, 1, wxALL | wxEXPAND, BORDER_SIZE);

    generalPanel->SetSizer(topSizer);
    notebook->AddPage(generalPanel, BOSTr("launcher.tab.general", "General"));

    // "Options" tab - app-wide preferences (language, theme) rather than per-run settings.
    auto* optionsPanel = new wxPanel(notebook);
    auto* optionsSizer = new wxBoxSizer(wxVERTICAL);

    auto* languageLabel = makeSectionLabel(optionsPanel, BOSTr("launcher.language.label", "Language"));
    optionsSizer->Add(languageLabel, 0, wxLEFT | wxRIGHT | wxTOP, BORDER_SIZE * 2);

    m_languages = BOSLocale::getAvailableLanguages();
    wxArrayString languageChoices;
    int selectedLanguageIndex = 0;
    for (size_t i = 0; i < m_languages.size(); ++i) {
        languageChoices.Add(m_languages[i].displayName);
        if (m_languages[i].code == BOSLocale::getCurrentLanguage()) {
            selectedLanguageIndex = static_cast<int>(i);
        }
    }
    m_languageChoice = new wxChoice(optionsPanel, ID_LANGUAGE, wxDefaultPosition, wxDefaultSize, languageChoices);
    if (!m_languages.empty()) {
        m_languageChoice->SetSelection(selectedLanguageIndex);
    }
    optionsSizer->Add(m_languageChoice, 0, wxEXPAND | wxALL, BORDER_SIZE);

    // Theme selector - same immediate-relaunch pattern as the language selector, except this one
    // needs a full process restart rather than an in-process rebuild - see main.cpp.
    auto* themeLabel = makeSectionLabel(optionsPanel, BOSTr("launcher.theme.label", "Theme"));
    optionsSizer->Add(themeLabel, 0, wxLEFT | wxRIGHT | wxTOP, BORDER_SIZE * 2);

    wxArrayString themeChoices;
    themeChoices.Add(BOSTr("launcher.theme.system", "System"));
    themeChoices.Add(BOSTr("launcher.theme.light", "Light"));
    themeChoices.Add(BOSTr("launcher.theme.dark", "Dark"));
    m_themeChoice = new wxChoice(optionsPanel, ID_THEME, wxDefaultPosition, wxDefaultSize, themeChoices);
    int selectedThemeIndex = 0;
    if (initParams.theme == "light") {
        selectedThemeIndex = 1;
    } else if (initParams.theme == "dark") {
        selectedThemeIndex = 2;
    }
    m_themeChoice->SetSelection(selectedThemeIndex);
    optionsSizer->Add(m_themeChoice, 0, wxEXPAND | wxALL, BORDER_SIZE);

    optionsPanel->SetSizer(optionsSizer);
    notebook->AddPage(optionsPanel, BOSTr("launcher.tab.options", "Options"));

    mainSizer->Add(notebook, 1, wxEXPAND | wxALL, BORDER_SIZE);
    SetSizer(mainSizer);

    Bind(wxEVT_BUTTON, &LauncherWindow::onBrowseGame, this, ID_BROWSE_GAME);
    Bind(wxEVT_BUTTON, &LauncherWindow::onBrowseOutput, this, ID_BROWSE_OUTPUT);
    Bind(wxEVT_BUTTON, &LauncherWindow::onScan, this, ID_SCAN);
    Bind(wxEVT_BUTTON, &LauncherWindow::onManageConflicts, this, ID_MANAGE_CONFLICTS);
    Bind(wxEVT_BUTTON, &LauncherWindow::onGenerate, this, ID_GENERATE);
    Bind(wxEVT_CHOICE, &LauncherWindow::onLanguageChanged, this, ID_LANGUAGE);
    Bind(wxEVT_CHOICE, &LauncherWindow::onThemeChanged, this, ID_THEME);

    log(BOSTr("log.gameLocationHint",
        "Point \"Game Location\" at the folder that contains Data\\ (your Skyrim install, or "
        "wherever your mod manager launches the game from) - not the MO2 instance folder."));
    log(BOSTr("log.usvfsHint",
        "BOSPriority reads it exactly like the game would: if launched through MO2/Vortex, that "
        "path transparently shows your merged mod view; there is no need to separately resolve "
        "where your instance or mods folders live."));

    // Deferred: ShowModal() hasn't started pumping events yet at this point in construction -
    // calling EndModal() from here directly would have nothing to unwind. CallAfter runs once
    // the event loop is actually running.
    CallAfter([this] { autoScanOnLaunch(); });
}

auto LauncherWindow::getParams() const -> InitParams
{
    InitParams params;
    params.gameDir = m_gamePathCtrl->GetValue();
    params.outputDir = m_outputPathCtrl->GetValue();
    params.theme = m_theme;
    return params;
}

void LauncherWindow::log(const wxString& msg)
{
    m_logCtrl->AppendText(msg + "\n");
}

void LauncherWindow::updateButtonStates()
{
    const bool hasKeys = !m_keys.empty();
    const bool hasConflicts = countConflicts(m_keys) > 0;
    m_manageConflictsButton->Enable(hasConflicts);
    m_generateButton->Enable(hasKeys);
}

void LauncherWindow::onBrowseGame(wxCommandEvent& /*event*/)
{
    wxDirDialog dlg(this, BOSTr("launcher.gameLocation.dialogTitle", "Select Game Location (folder containing Data)"),
                     m_gamePathCtrl->GetValue());
    if (dlg.ShowModal() == wxID_OK) {
        m_gamePathCtrl->SetValue(dlg.GetPath());
    }
}

void LauncherWindow::onBrowseOutput(wxCommandEvent& /*event*/)
{
    wxDirDialog dlg(this, BOSTr("launcher.outputLocation.dialogTitle", "Select output folder"),
                     m_outputPathCtrl->GetValue());
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
    log(wxString::Format(BOSTr("log.foundKeys", "Found %zu key(s), %lld in conflict."), m_keys.size(), conflictCount));
    if (!m_keys.empty() && conflictCount == 0) {
        log(BOSTr("log.noConflictsHint",
            "No conflicts - BOS will already produce the correct result directly from these "
            "files, nothing needs generating. Generate is still there if you just want a single "
            "consolidated ini (e.g. to tidy up a modlist), but it's optional."));
    }
    updateButtonStates();
    saveSettings();
}

void LauncherWindow::onScan(wxCommandEvent& /*event*/)
{
    const fs::path gameDir(m_gamePathCtrl->GetValue().ToStdWstring());
    if (gameDir.empty()) {
        log(BOSTr("log.pickGameLocation", "Pick a Game Location first."));
        return;
    }

    performScan();
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
    json["uiLanguage"] = BOSLocale::getCurrentLanguage();
    json["uiTheme"] = StringUtil::utf16ToUtf8(m_theme.ToStdWstring());

    try {
        ofstream out(dir / SETTINGS_FILE_NAME);
        out.exceptions(ios::failbit | ios::badbit);
        out << json.dump(2);
    } catch (const exception&) {
        // Best-effort only - losing this file just means Game/Output/language/theme won't be
        // remembered next launch, not worth surfacing to the user over.
    }
}

void LauncherWindow::autoScanOnLaunch()
{
    const fs::path gameDir(m_gamePathCtrl->GetValue().ToStdWstring());
    const fs::path outputDir(m_outputPathCtrl->GetValue().ToStdWstring());
    if (gameDir.empty() || outputDir.empty()) {
        return; // nothing remembered from a previous run - wait for the user
    }

    log(BOSTr("log.autoScanning", "Remembered Game/Output locations from last time - scanning automatically..."));
    performScan();

    if (m_keys.empty()) {
        return; // likely a stale/misconfigured path - let the user notice and fix it themselves
    }

    if (countConflicts(m_keys) > 0) {
        return; // there's real work to do, leave the window open on it
    }

    wxMessageDialog dlg(this,
        BOSTr("dialog.nothingToManage.message",
            "Scanned automatically and found no conflicts - BOS will already produce the correct "
            "result directly from these files, nothing needs generating. Generating a single "
            "consolidated ini is still there if you want one (e.g. to tidy up a modlist), but it's "
            "entirely optional. Close BOSPriority?"),
        BOSTr("dialog.nothingToManage.title", "Nothing to manage"), wxYES_NO | wxICON_INFORMATION);
    dlg.SetYesNoLabels(BOSTr("dialog.nothingToManage.close", "Close BOSPriority"),
                        BOSTr("dialog.nothingToManage.keepOpen", "Keep Open"));
    if (dlg.ShowModal() == wxID_YES) {
        EndModal(wxID_CANCEL);
    }
}

void LauncherWindow::onManageConflicts(wxCommandEvent& /*event*/)
{
    ConflictTableDialog dlg(this, m_keys);
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    m_keys = dlg.getKeys();
    try {
        saveDecisions();
        log(BOSTr("log.decisionsSaved",
            "Conflict decisions updated and saved - remembered next time you scan this output folder."));
    } catch (const exception& e) {
        log(wxString::Format(
            BOSTr("log.decisionsSaveFailed", "Could not save conflict decisions: %s"), e.what()));
    }
    updateButtonStates();
}

void LauncherWindow::onGenerate(wxCommandEvent& /*event*/)
{
    if (m_keys.empty()) {
        log(BOSTr("log.nothingToMerge", "Nothing to merge - Scan Mods first."));
        return;
    }

    const fs::path outputDir(m_outputPathCtrl->GetValue().ToStdWstring());
    if (outputDir.empty()) {
        log(BOSTr("log.pickOutputLocation", "Pick an output folder first."));
        return;
    }

    const bool dryRun = m_dryRunCheck->GetValue();

    try {
        const auto stats = BOSIniMerger::merge(m_keys, outputDir, dryRun);

        const wxString status = dryRun ? BOSTr("log.preview", "Preview") : BOSTr("log.done", "Done");
        const wxString writtenWord
            = dryRun ? BOSTr("log.wouldBeWritten", "would be written") : BOSTr("log.written", "written");
        log(wxString::Format(
            BOSTr("log.resultLine", "%s: %d file(s) read, %d line(s) read, %d key(s) overridden by priority, %d line(s) %s."),
            status, stats.filesRead, stats.linesRead, stats.keysOverridden, stats.linesWritten, writtenWord));

        if (!dryRun) {
            log(wxString::Format(BOSTr("log.wrote", "Wrote %s"), (outputDir / L"AIO_SWAP.ini").wstring()));
            log(BOSTr("log.blankedHint",
                "Original source ini files were replaced with empty stand-ins in this output "
                "folder - only AIO_SWAP.ini is active now."));
        }
    } catch (const exception& e) {
        log(wxString::Format(BOSTr("log.generationAborted", "Generation aborted: %s"), e.what()));
    }
}

void LauncherWindow::onLanguageChanged(wxCommandEvent& /*event*/)
{
    const int selection = m_languageChoice->GetSelection();
    if (selection == wxNOT_FOUND) {
        return;
    }

    const auto& selectedLang = m_languages.at(static_cast<size_t>(selection));
    if (selectedLang.code == BOSLocale::getCurrentLanguage()) {
        return;
    }

    BOSLocale::init(getExecutableDir() / "BOSPriority_translations", selectedLang.code);
    saveSettings();
    EndModal(RESULT_RELAUNCH);
}

void LauncherWindow::onThemeChanged(wxCommandEvent& /*event*/)
{
    switch (m_themeChoice->GetSelection()) {
    case 1:
        m_theme = "light";
        break;
    case 2:
        m_theme = "dark";
        break;
    default:
        m_theme = "system";
        break;
    }

    // Unlike a language change, this needs a full process restart, not just an internal rebuild -
    // wx's MSW dark mode support turned out to be a one-way switch once enabled for a process, so
    // an in-process relaunch can't reliably get back to a lighter appearance after a darker one.
    // See main.cpp's handling of RESULT_RESTART.
    saveSettings();
    EndModal(RESULT_RESTART);
}
