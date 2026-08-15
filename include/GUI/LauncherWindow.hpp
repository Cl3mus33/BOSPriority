#pragma once

#include "ModManager.hpp"

#include <memory>
#include <vector>
#include <wx/wx.h>

class LauncherWindow : public wxFrame {
public:
    LauncherWindow();

private:
    void onBrowseGame(wxCommandEvent& event);
    void onBrowseOutput(wxCommandEvent& event);
    void onScan(wxCommandEvent& event);
    void onManagePriority(wxCommandEvent& event);
    void onGenerate(wxCommandEvent& event);

    void log(const wxString& msg);
    void loadPriorityFile() const;
    void savePriorityFile() const;
    void updateButtonStates();

    wxTextCtrl* m_gamePathCtrl = nullptr;
    wxTextCtrl* m_outputPathCtrl = nullptr;
    wxCheckBox* m_dryRunCheck = nullptr;
    wxButton* m_managePriorityButton = nullptr;
    wxButton* m_generateButton = nullptr;
    wxTextCtrl* m_logCtrl = nullptr;

    std::unique_ptr<ModManager> m_modManager;
    /// Result of the last scan, current chosen order (lowest applied priority first).
    std::vector<std::shared_ptr<ModManager::Mod>> m_bosMods;
};
