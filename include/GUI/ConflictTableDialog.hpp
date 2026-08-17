#pragma once

#include "BOSIniMerger.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <wx/listctrl.h>
#include <wx/wx.h>

/// Shows only the keys that actually conflict (2+ source files disagree), filterable by resolved
/// record type. Conflicts sharing the same key but scoped to different BOS location filters (e.g.
/// Farmhouse04 under [Forms|FalkreathLocation] vs. [Forms|RiverwoodLocation]) are grouped into a
/// single row.
///
/// Resolving a group means ranking its candidate files (Move Up/Down), not just picking one
/// winner: a single file rarely covers every location a key touches (one mod might only patch
/// Falkreath, another only Riverwood), so each location independently takes the highest-ranked
/// file that actually has a line for it - visible live in the resolution preview below the list.
/// This is the same "walk a ranking per location" idea as the global "Set Priority by Type"
/// button, just scoped to one key instead of every conflict of a type - the two compose: a
/// per-key order set here always wins over the global ranking for that key's locations.
class ConflictTableDialog : public wxDialog {
public:
    /// keys: the full scan() result (conflicts and non-conflicts alike). Non-conflicting entries
    /// pass through untouched; only conflicting ones can be edited here.
    /// outputDir: where a "Set Priority by Type" ranking is loaded from and saved to
    /// (BOSPriority_priorities.json, alongside BOSPriority_decisions.json).
    ConflictTableDialog(wxWindow* parent, std::vector<SwapKey> keys, std::filesystem::path outputDir);

    /// The full key set, with any winner/exclude edits applied.
    [[nodiscard]] auto getKeys() const -> const std::vector<SwapKey>&;

private:
    /// One row in the table: every isRealConflict() SwapKey sharing the same `key` text,
    /// regardless of which BOS location section scopes it.
    struct KeyGroup {
        std::string key;
        wxString typeKey; // raw/stable - see ConflictTypeLabels.hpp, translated only for display
        std::vector<size_t> memberIndices; // into m_keys
        std::vector<std::string> distinctFiles; // union of candidate source files, first-seen order
    };

    void buildGroups();
    void rebuildTypeFilterChoices();
    void rebuildList();
    void showDetailFor(int listRow);
    void clearDetail();
    /// Rebuilds m_orderList to show exactly `order` (top-of-list-first file names), numbered so
    /// the BOTTOM entry reads #1 (it's the one that wins - same convention as a mod manager's own
    /// load order) - always rebuilt rather than mutated in place so the numbers shown are never
    /// stale after a move.
    void populateOrderList(const KeyGroup& group, const std::vector<std::string>& order);
    /// Seeds m_orderList for `group`: files ordered by how many of its non-excluded locations
    /// they currently win, LEAST first (so the current winner ends up at the bottom) - a sensible
    /// reconstruction of "what order got us here" rather than an arbitrary fixed order, since the
    /// exact order a user set in an earlier session isn't itself persisted (only its per-location
    /// results are - see saveDecisions).
    void refreshOrderList(const KeyGroup& group);
    /// Applies m_orderList's current order to every non-excluded member of `group`: each location
    /// independently takes the highest-ranked file that has a candidate there. Marks every member
    /// it resolves as userDecided.
    void applyCurrentOrder(const KeyGroup& group);
    /// Refreshes the "Location -> resolved file" preview beneath the order list for `group`.
    void refreshResolutionSummary(const KeyGroup& group);
    [[nodiscard]] auto winnerLabelFor(const KeyGroup& group) const -> wxString;
    /// Returns the group currently shown in the detail panel, or nullptr if none/selection is
    /// stale - shared by the exclude checkbox and order-list handlers, which both need to know
    /// which group they're acting on.
    [[nodiscard]] auto selectedGroup() -> KeyGroup*;
    /// Swaps the selected m_orderList item with its delta neighbor (-1 up, +1 down), re-resolves,
    /// and refreshes both the resolution preview and the main list's Winner column.
    void moveOrderSelection(int delta);

    void onTypeFilterChanged(wxCommandEvent& event);
    void onListSelectionChanged(wxListEvent& event);
    void onExcludeToggled(wxCommandEvent& event);
    void onOrderMoveUp(wxCommandEvent& event);
    void onOrderMoveDown(wxCommandEvent& event);
    /// Updates m_orderList's tooltip to name the locations the hovered file covers within the
    /// selected group - the item label only has room for a count.
    void onOrderListMotion(wxMouseEvent& event);
    void onSetPriorityByType(wxCommandEvent& event);

    std::vector<SwapKey> m_keys; // full set, edited in place
    std::filesystem::path m_outputDir;
    std::vector<KeyGroup> m_groups; // every real conflict, grouped by key
    std::vector<size_t> m_rowToGroupIndex; // list row -> index into m_groups, current filter

    wxChoice* m_typeFilter = nullptr;
    std::vector<wxString> m_typeFilterRawValues; // parallel to m_typeFilter's items
    wxListCtrl* m_listCtrl = nullptr;
    wxStaticText* m_detailKeyLabel = nullptr;
    wxStaticText* m_detailLocationsLabel = nullptr; // lists every location name in the selected group
    wxCheckBox* m_excludeCheck = nullptr;
    wxListBox* m_orderList = nullptr; // the selected group's candidate files, in resolution order
    int m_hoveredOrderItem = wxNOT_FOUND; // last item onOrderListMotion set a tooltip for
    wxButton* m_moveUpButton = nullptr;
    wxButton* m_moveDownButton = nullptr;
    wxStaticText* m_resolutionSummaryLabel = nullptr; // live "Location -> resolved file" preview
};
