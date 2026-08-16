#pragma once

#include "BOSIniMerger.hpp"
#include "BOSLocale.hpp"

#include <filesystem>
#include <vector>
#include <wx/wx.h>

/// Main window. A wxDialog (not a wxFrame) shown modally in a loop from main(), same pattern as
/// AutoSeasons: a language change only needs the dialog rebuilt in-process (EndModal with
/// RESULT_RELAUNCH), but a theme change needs a full process restart (RESULT_RESTART) - see
/// main.cpp for why (wx's MSW dark mode support is a one-way switch per process).
class LauncherWindow : public wxDialog {
public:
    static constexpr int RESULT_RELAUNCH = wxID_HIGHEST + 100;
    static constexpr int RESULT_RESTART = wxID_HIGHEST + 101;

    /// Carries state across a relaunch/restart so in-progress (possibly unsaved) field values
    /// survive a language or theme change instead of reverting to whatever was last saved.
    struct InitParams {
        wxString gameDir;
        wxString outputDir;
        wxString theme = "system"; // "system" | "light" | "dark"
    };

    explicit LauncherWindow(const InitParams& initParams);

    /// Current field values, including any in-progress edits - read by main.cpp's relaunch loop
    /// right before destroying this instance.
    [[nodiscard]] auto getParams() const -> InitParams;

private:
    void onBrowseGame(wxCommandEvent& event);
    void onBrowseOutput(wxCommandEvent& event);
    void onScan(wxCommandEvent& event);
    void onManageConflicts(wxCommandEvent& event);
    void onGenerate(wxCommandEvent& event);
    void onLanguageChanged(wxCommandEvent& event);
    void onThemeChanged(wxCommandEvent& event);

    void log(const wxString& msg);
    void applySavedDecisions(std::vector<SwapKey>& keys) const;
    void saveDecisions() const;
    /// Reapplies a previously-saved "Set Priority by Type" ranking (if any) after every scan -
    /// unlike per-key decisions, a ranking is meant to keep resolving whatever conflicts exist on
    /// this scan, including ones that didn't exist when it was set, so there's no separate
    /// touched/untouched state to track for it.
    void applySavedTypePriorities(std::vector<SwapKey>& keys) const;
    void updateButtonStates();

    /// Runs a scan against the current Game Location and stores the result in m_keys - shared by
    /// the "Scan Mods" button and the auto-scan-on-launch path below.
    void performScan();

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
    wxChoice* m_languageChoice = nullptr;
    wxChoice* m_themeChoice = nullptr;

    std::vector<BOSLocale::Language> m_languages;
    wxString m_theme;

    /// Result of the last scan, with any saved/edited winner+exclude decisions applied.
    std::vector<SwapKey> m_keys;
};
