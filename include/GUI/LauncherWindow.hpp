#pragma once

#include "BOSIniMerger.hpp"

#include <filesystem>
#include <vector>
#include <wx/wx.h>

class LauncherWindow : public wxFrame {
public:
    LauncherWindow();

private:
    void onBrowseGame(wxCommandEvent& event);
    void onBrowseOutput(wxCommandEvent& event);
    void onScan(wxCommandEvent& event);
    void onManageConflicts(wxCommandEvent& event);
    void onGenerate(wxCommandEvent& event);

    void log(const wxString& msg);
    void applySavedDecisions(std::vector<SwapKey>& keys) const;
    void saveDecisions() const;
    void updateButtonStates();

    /// Runs a scan against the current Game Location and stores the result in m_keys - shared by
    /// the "Scan Mods" button and the auto-scan-on-launch path below.
    void performScan();

    void loadSettings();
    void saveSettings() const;
    /// Called once at startup, only if both paths were remembered from a previous run: scans
    /// automatically and, if everything the last generate needed is still fully resolved (no
    /// conflicts left to decide), offers to just close the app instead of making the user check.
    void autoScanOnLaunch();

    wxTextCtrl* m_gamePathCtrl = nullptr;
    wxTextCtrl* m_outputPathCtrl = nullptr;
    wxCheckBox* m_dryRunCheck = nullptr;
    wxButton* m_manageConflictsButton = nullptr;
    wxButton* m_generateButton = nullptr;
    wxTextCtrl* m_logCtrl = nullptr;

    /// Result of the last scan, with any saved/edited winner+exclude decisions applied.
    std::vector<SwapKey> m_keys;
};
