#include "GUI/LauncherWindow.hpp"
#include "BOSIniMerger.hpp"
#include "GUI/PriorityListDialog.hpp"

#include <nlohmann/json.hpp>

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
} // namespace

LauncherWindow::LauncherWindow()
    : wxFrame(nullptr, wxID_ANY, "BOSPriority", wxDefaultPosition, wxSize(680, 520))
{
    auto* panel = new wxPanel(this);
    auto* topSizer = new wxBoxSizer(wxVERTICAL);

    auto* gameSizer = new wxBoxSizer(wxHORIZONTAL);
    gameSizer->Add(
        new wxStaticText(panel, wxID_ANY, "MO2 instance / Vortex staging / Data folder:"),
        0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    m_gamePathCtrl = new wxTextCtrl(panel, wxID_ANY);
    gameSizer->Add(m_gamePathCtrl, 1, wxALL | wxEXPAND, 5);
    gameSizer->Add(new wxButton(panel, ID_BROWSE_GAME, "..."), 0, wxALL, 5);
    topSizer->Add(gameSizer, 0, wxEXPAND);

    auto* outSizer = new wxBoxSizer(wxHORIZONTAL);
    outSizer->Add(new wxStaticText(panel, wxID_ANY, "Output folder (dedicated mod):"),
                  0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    m_outputPathCtrl = new wxTextCtrl(panel, wxID_ANY);
    outSizer->Add(m_outputPathCtrl, 1, wxALL | wxEXPAND, 5);
    outSizer->Add(new wxButton(panel, ID_BROWSE_OUTPUT, "..."), 0, wxALL, 5);
    topSizer->Add(outSizer, 0, wxEXPAND);

    m_dryRunCheck = new wxCheckBox(panel, wxID_ANY, "Preview only (dry run)");
    topSizer->Add(m_dryRunCheck, 0, wxALL, 5);

    auto* actionSizer = new wxBoxSizer(wxHORIZONTAL);
    actionSizer->Add(new wxButton(panel, ID_SCAN, "Scan Mods"), 0, wxALL, 5);
    m_managePriorityButton = new wxButton(panel, ID_MANAGE_PRIORITY, "Manage Priority...");
    m_managePriorityButton->Disable();
    actionSizer->Add(m_managePriorityButton, 0, wxALL, 5);
    m_generateButton = new wxButton(panel, ID_GENERATE, "Generate");
    m_generateButton->Disable();
    actionSizer->Add(m_generateButton, 0, wxALL, 5);
    topSizer->Add(actionSizer, 0);

    m_logCtrl = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                wxTE_MULTILINE | wxTE_READONLY);
    topSizer->Add(m_logCtrl, 1, wxALL | wxEXPAND, 5);

    panel->SetSizer(topSizer);

    Bind(wxEVT_BUTTON, &LauncherWindow::onBrowseGame, this, ID_BROWSE_GAME);
    Bind(wxEVT_BUTTON, &LauncherWindow::onBrowseOutput, this, ID_BROWSE_OUTPUT);
    Bind(wxEVT_BUTTON, &LauncherWindow::onScan, this, ID_SCAN);
    Bind(wxEVT_BUTTON, &LauncherWindow::onManagePriority, this, ID_MANAGE_PRIORITY);
    Bind(wxEVT_BUTTON, &LauncherWindow::onGenerate, this, ID_GENERATE);

    log("Point this at your MO2 instance folder (the one containing modorganizer.ini), your "
        "Vortex staging folder, or a plain Data folder, then Scan Mods.");
    log("Run this via your mod manager's executables list, not by double-clicking the exe - "
        "otherwise it only sees your game's real Data folder, not your installed mods.");
}

void LauncherWindow::log(const wxString& msg)
{
    m_logCtrl->AppendText(msg + "\n");
}

void LauncherWindow::updateButtonStates()
{
    const bool hasMods = !m_bosMods.empty();
    m_managePriorityButton->Enable(m_bosMods.size() > 1);
    m_generateButton->Enable(hasMods);
}

void LauncherWindow::onBrowseGame(wxCommandEvent& /*event*/)
{
    wxDirDialog dlg(this, "Select MO2 instance / Vortex staging / Data folder", m_gamePathCtrl->GetValue());
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

void LauncherWindow::loadPriorityFile() const
{
    const fs::path outputDir(m_outputPathCtrl->GetValue().ToStdWstring());
    const auto priorityFile = outputDir / PRIORITY_FILE_NAME;
    if (!fs::exists(priorityFile)) {
        return;
    }

    try {
        ifstream f(priorityFile);
        const auto json = nlohmann::json::parse(f);
        m_modManager->loadJSON(json);
    } catch (const exception& e) {
        wxLogWarning("Could not read saved priority file: %s", e.what());
    }
}

void LauncherWindow::savePriorityFile() const
{
    if (!m_modManager) {
        return;
    }

    const fs::path outputDir(m_outputPathCtrl->GetValue().ToStdWstring());
    if (outputDir.empty()) {
        return;
    }

    fs::create_directories(outputDir);
    ofstream out(outputDir / PRIORITY_FILE_NAME);
    out << m_modManager->getJSON().dump(2);
}

void LauncherWindow::onScan(wxCommandEvent& /*event*/)
{
    const fs::path gameDir(m_gamePathCtrl->GetValue().ToStdWstring());
    const fs::path outputDir(m_outputPathCtrl->GetValue().ToStdWstring());

    if (gameDir.empty()) {
        log("Pick a game/instance folder first.");
        return;
    }

    m_modManager.reset();
    m_bosMods.clear();

    try {
        if (ModManager::isValidMO2InstanceDir(gameDir)) {
            log("Detected a Mod Organizer 2 instance.");
            m_modManager = make_unique<ModManager>(ModManager::ModManagerType::MODORGANIZER2);
            m_modManager->populateModsMO2(gameDir, outputDir);
        } else if (fs::exists(gameDir / "vortex.deployment.json")) {
            log("Detected a Vortex deployment manifest.");
            m_modManager = make_unique<ModManager>(ModManager::ModManagerType::VORTEX);
            m_modManager->populateModsVortex(gameDir);
        } else {
            log("No modorganizer.ini or vortex.deployment.json found here - treating this as a "
                "plain merged Data folder. There is only one copy of each file in that case, so "
                "there is no cross-mod priority to manage.");
            m_modManager = make_unique<ModManager>(ModManager::ModManagerType::NONE);
        }
    } catch (const exception& e) {
        log(wxString("Error scanning mods: ") + e.what());
        m_modManager.reset();
        updateButtonStates();
        return;
    }

    loadPriorityFile();
    m_bosMods = m_modManager->getBosModsInApplyOrder();
    log(wxString::Format("Found %zu mod(s) shipping a BaseObjectSwapper ini.", m_bosMods.size()));
    updateButtonStates();
}

void LauncherWindow::onManagePriority(wxCommandEvent& /*event*/)
{
    if (!m_modManager || m_bosMods.size() < 2) {
        return;
    }

    PriorityListDialog dlg(this, m_bosMods);
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    m_bosMods = dlg.getOrderedMods();
    for (size_t i = 0; i < m_bosMods.size(); ++i) {
        m_bosMods[i]->priority = static_cast<int>(i);
    }

    savePriorityFile();
    log("Priority order updated and saved - it will be remembered next time you scan this output folder.");
}

void LauncherWindow::onGenerate(wxCommandEvent& /*event*/)
{
    if (m_bosMods.empty()) {
        log("Nothing to merge - Scan Mods first.");
        return;
    }

    const fs::path outputDir(m_outputPathCtrl->GetValue().ToStdWstring());
    if (outputDir.empty()) {
        log("Pick an output folder first.");
        return;
    }

    vector<BosMergeSourceMod> sources;
    sources.reserve(m_bosMods.size());
    for (const auto& mod : m_bosMods) {
        sources.push_back(BosMergeSourceMod{mod->name, mod->folder});
    }

    const bool dryRun = m_dryRunCheck->GetValue();
    const auto stats = BOSIniMerger::merge(sources, outputDir, dryRun);

    log(wxString::Format(
        "%s: %d file(s) read, %d line(s) read, %d key(s) overridden by priority, %d line(s) %s.",
        dryRun ? "Preview" : "Done", stats.filesRead, stats.linesRead, stats.keysOverridden,
        stats.linesWritten, dryRun ? "would be written" : "written"));

    if (!dryRun) {
        log(wxString("Wrote ") + (outputDir / L"AIO_SWAP.ini").wstring());
    }
}
