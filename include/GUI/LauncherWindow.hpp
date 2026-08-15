#pragma once

#include <filesystem>
#include <unordered_map>
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
    [[nodiscard]] auto loadPriorityMap() const -> std::unordered_map<std::wstring, int>;
    void savePriorityMap() const;
    void applySavedPriority(std::vector<std::filesystem::path>& files) const;
    void updateButtonStates();

    wxTextCtrl* m_gamePathCtrl = nullptr;
    wxTextCtrl* m_outputPathCtrl = nullptr;
    wxCheckBox* m_dryRunCheck = nullptr;
    wxButton* m_managePriorityButton = nullptr;
    wxButton* m_generateButton = nullptr;
    wxTextCtrl* m_logCtrl = nullptr;

    /// Result of the last scan, current chosen order (lowest applied priority first).
    std::vector<std::filesystem::path> m_iniFiles;
};
