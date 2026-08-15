#pragma once

#include "BOSIniMerger.hpp"

#include <vector>
#include <wx/listctrl.h>
#include <wx/wx.h>

/// Shows only the keys that actually conflict (2+ source files disagree), filterable by resolved
/// record type, with a master-detail layout: select a conflict in the list, then either pick
/// which candidate line wins or exclude the key entirely in the detail panel below.
/// Non-conflicting keys are never shown - there's nothing to decide about them, they're always
/// included as-is.
class ConflictTableDialog : public wxDialog {
public:
    /// keys: the full scan() result (conflicts and non-conflicts alike). Non-conflicting entries
    /// pass through untouched; only conflicting ones can be edited here.
    ConflictTableDialog(wxWindow* parent, std::vector<SwapKey> keys);

    /// The full key set, with any winner/exclude edits applied.
    [[nodiscard]] auto getKeys() const -> const std::vector<SwapKey>&;

private:
    void rebuildTypeFilterChoices();
    void rebuildList();
    void showDetailFor(int listRow);
    void clearDetail();

    void onTypeFilterChanged(wxCommandEvent& event);
    void onListSelectionChanged(wxListEvent& event);
    void onExcludeToggled(wxCommandEvent& event);
    void onWinnerChosen(wxCommandEvent& event);

    std::vector<SwapKey> m_keys; // full set, edited in place
    std::vector<size_t> m_conflictRowToKeyIndex; // list row -> index into m_keys, current filter

    wxChoice* m_typeFilter = nullptr;
    wxListCtrl* m_listCtrl = nullptr;
    wxStaticText* m_detailKeyLabel = nullptr;
    wxCheckBox* m_excludeCheck = nullptr;
    wxPanel* m_radioPanel = nullptr;
    wxBoxSizer* m_radioSizer = nullptr;
    std::vector<wxRadioButton*> m_radioButtons; // rebuilt for whichever conflict is selected
};
