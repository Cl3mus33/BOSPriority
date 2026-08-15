#pragma once

#include "BOSIniMerger.hpp"

#include <optional>
#include <string>
#include <vector>
#include <wx/listctrl.h>
#include <wx/wx.h>

/// Shows only the keys that actually conflict (2+ source files disagree), filterable by resolved
/// record type. Conflicts sharing the same key but scoped to different BOS location filters (e.g.
/// Farmhouse04 under [Forms|FalkreathLocation] vs. [Forms|RiverwoodLocation]) are grouped into a
/// single row: picking a winning FILE resolves every location that file covers in one decision,
/// rather than forcing a separate pick per location. A location the chosen file doesn't cover
/// keeps its own independent default (BOS's own last-declared-wins rule) until picked explicitly.
class ConflictTableDialog : public wxDialog {
public:
    /// keys: the full scan() result (conflicts and non-conflicts alike). Non-conflicting entries
    /// pass through untouched; only conflicting ones can be edited here.
    ConflictTableDialog(wxWindow* parent, std::vector<SwapKey> keys);

    /// The full key set, with any winner/exclude edits applied.
    [[nodiscard]] auto getKeys() const -> const std::vector<SwapKey>&;

private:
    /// One row in the table: every isRealConflict() SwapKey sharing the same `key` text,
    /// regardless of which BOS location section scopes it.
    struct KeyGroup {
        std::string key;
        wxString typeLabel;
        std::vector<size_t> memberIndices; // into m_keys
        std::vector<std::string> distinctFiles; // union of candidate source files, first-seen order
    };

    void buildGroups();
    void rebuildTypeFilterChoices();
    void rebuildList();
    void showDetailFor(int listRow);
    void clearDetail();
    void applyWinnerFile(const KeyGroup& group, const std::string& winnerFile);
    [[nodiscard]] auto winnerLabelFor(const KeyGroup& group) const -> wxString;

    void onTypeFilterChanged(wxCommandEvent& event);
    void onListSelectionChanged(wxListEvent& event);
    void onExcludeToggled(wxCommandEvent& event);
    void onWinnerChosen(wxCommandEvent& event);
    void onSetPriorityByType(wxCommandEvent& event);

    std::vector<SwapKey> m_keys; // full set, edited in place
    std::vector<KeyGroup> m_groups; // every real conflict, grouped by key
    std::vector<size_t> m_rowToGroupIndex; // list row -> index into m_groups, current filter

    wxChoice* m_typeFilter = nullptr;
    wxListCtrl* m_listCtrl = nullptr;
    wxStaticText* m_detailKeyLabel = nullptr;
    wxCheckBox* m_excludeCheck = nullptr;
    wxPanel* m_radioPanel = nullptr;
    wxBoxSizer* m_radioSizer = nullptr;
    std::vector<wxRadioButton*> m_radioButtons; // one per distinct file, for whichever group is selected
    std::vector<std::string> m_radioButtonFiles; // parallel to m_radioButtons
};
