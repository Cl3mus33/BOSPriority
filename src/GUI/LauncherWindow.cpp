#include "GUI/LauncherWindow.hpp"
#include "BOSIniMerger.hpp"
#include "GUI/PriorityListDialog.hpp"
#include "StringUtil.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <exception>
#include <fstream>
#include <wx/dirdlg.h>

using namespace std;
namespace fs = std::filesystem;

namespace {
constexpr int ID_BROWSE_GAME = wxID_HIGHEST + 10;
constexpr int ID_BROWSE_OUTPUT = wxID_HIGHEST + 11;
constexpr int ID_SCAN = wxID_HIGHEST + 12;
constexpr int ID_MANAGE_PRIORITY = wxID_HIGHEST + 13;
constexpr int ID_GENERATE = wxID_HIGHEST + 14;

constexpr const wchar_t* PRIORITY_FILE_NAME = L"BOSPriority_priority.json";
constexpr int BORDER_SIZE = 5;

// Same accent palette/treatment as AutoSeasons (github.com/Cl3mus33/AutoSeasons), so this reads
// as the same tool family instead of a generic, undifferentiated wx dialog.
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
} // namespace

LauncherWindow::LauncherWindow()
    : wxFrame(nullptr, wxID_ANY, "BOSPriority", wxDefaultPosition, wxSize(680, 560))
{
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Header banner: same identity treatment as AutoSeasons' launcher, so the two feel like part
    // of the same toolset at a glance instead of a plain, undifferentiated settings form.
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
    m_managePriorityButton = new wxButton(panel, ID_MANAGE_PRIORITY, "Manage Priority...");
    m_managePriorityButton->Disable();
    actionSizer->Add(m_managePriorityButton, 0, wxALL, BORDER_SIZE);
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
    Bind(wxEVT_BUTTON, &LauncherWindow::onManagePriority, this, ID_MANAGE_PRIORITY);
    Bind(wxEVT_BUTTON, &LauncherWindow::onGenerate, this, ID_GENERATE);

    log("Point \"Game Location\" at the folder that contains Data\\ (your Skyrim install, or "
        "wherever your mod manager launches the game from) - not the MO2 instance folder.");
    log("BOSPriority reads it exactly like the game would: if launched through MO2/Vortex, that "
        "path transparently shows your merged mod view; there is no need to separately resolve "
        "where your instance or mods folders live.");
}

void LauncherWindow::log(const wxString& msg)
{
    m_logCtrl->AppendText(msg + "\n");
}

void LauncherWindow::updateButtonStates()
{
    const bool hasFiles = !m_iniFiles.empty();
    m_managePriorityButton->Enable(m_iniFiles.size() > 1);
    m_generateButton->Enable(hasFiles);
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

auto LauncherWindow::loadPriorityMap() const -> unordered_map<wstring, int>
{
    unordered_map<wstring, int> map;

    const fs::path outputDir(m_outputPathCtrl->GetValue().ToStdWstring());
    const auto priorityFile = outputDir / PRIORITY_FILE_NAME;
    if (!fs::exists(priorityFile)) {
        return map;
    }

    try {
        ifstream f(priorityFile);
        const auto json = nlohmann::json::parse(f);
        for (const auto& [fileName, priority] : json.items()) {
            if (priority.is_number_integer()) {
                map[StringUtil::utf8ToUtf16(fileName)] = priority.get<int>();
            }
        }
    } catch (const exception& e) {
        wxLogWarning("Could not read saved priority file: %s", e.what());
    }

    return map;
}

void LauncherWindow::savePriorityMap() const
{
    const fs::path outputDir(m_outputPathCtrl->GetValue().ToStdWstring());
    if (outputDir.empty()) {
        return;
    }

    auto json = nlohmann::json::object();
    for (size_t i = 0; i < m_iniFiles.size(); ++i) {
        json[StringUtil::utf16ToUtf8(m_iniFiles[i].filename().wstring())] = static_cast<int>(i);
    }

    fs::create_directories(outputDir);
    ofstream out(outputDir / PRIORITY_FILE_NAME);
    out << json.dump(2);
}

void LauncherWindow::applySavedPriority(vector<fs::path>& files) const
{
    const auto priorityMap = loadPriorityMap();
    if (priorityMap.empty()) {
        return;
    }

    ranges::stable_sort(files, [&](const fs::path& a, const fs::path& b) {
        const auto aIt = priorityMap.find(a.filename().wstring());
        const auto bIt = priorityMap.find(b.filename().wstring());
        const bool aHas = aIt != priorityMap.end();
        const bool bHas = bIt != priorityMap.end();
        if (aHas != bHas) {
            return bHas; // files without a saved priority default to the bottom (lowest applied)
        }
        if (aHas) {
            return aIt->second < bIt->second;
        }
        return false; // keep relative (alphabetical) order among files with no saved priority
    });
}

void LauncherWindow::onScan(wxCommandEvent& /*event*/)
{
    const fs::path gameDir(m_gamePathCtrl->GetValue().ToStdWstring());
    if (gameDir.empty()) {
        log("Pick a Game Location first.");
        return;
    }

    m_iniFiles = BOSIniMerger::discoverIniFiles(gameDir);
    applySavedPriority(m_iniFiles);

    log(wxString::Format("Found %zu BaseObjectSwapper ini file(s).", m_iniFiles.size()));
    for (const auto& file : m_iniFiles) {
        log(wxString("  - ") + file.filename().wstring());
    }
    updateButtonStates();
}

void LauncherWindow::onManagePriority(wxCommandEvent& /*event*/)
{
    if (m_iniFiles.size() < 2) {
        return;
    }

    PriorityListDialog dlg(this, m_iniFiles);
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    m_iniFiles = dlg.getOrderedFiles();
    savePriorityMap();
    log("Priority order updated and saved - it will be remembered next time you scan.");
}

void LauncherWindow::onGenerate(wxCommandEvent& /*event*/)
{
    if (m_iniFiles.empty()) {
        log("Nothing to merge - Scan Mods first.");
        return;
    }

    const fs::path outputDir(m_outputPathCtrl->GetValue().ToStdWstring());
    if (outputDir.empty()) {
        log("Pick an output folder first.");
        return;
    }

    const bool dryRun = m_dryRunCheck->GetValue();
    const auto stats = BOSIniMerger::merge(m_iniFiles, outputDir, dryRun);

    log(wxString::Format(
        "%s: %d file(s) read, %d line(s) read, %d key(s) overridden by priority, %d line(s) %s.",
        dryRun ? "Preview" : "Done", stats.filesRead, stats.linesRead, stats.keysOverridden,
        stats.linesWritten, dryRun ? "would be written" : "written"));

    if (!dryRun) {
        log(wxString("Wrote ") + (outputDir / L"AIO_SWAP.ini").wstring());
    }
}
