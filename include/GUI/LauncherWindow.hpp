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

    wxTextCtrl* m_gamePathCtrl = nullptr;
    wxTextCtrl* m_outputPathCtrl = nullptr;
    wxCheckBox* m_dryRunCheck = nullptr;
    wxButton* m_manageConflictsButton = nullptr;
    wxButton* m_generateButton = nullptr;
    wxTextCtrl* m_logCtrl = nullptr;

    /// Result of the last scan, with any saved/edited winner+exclude decisions applied.
    std::vector<SwapKey> m_keys;
};
