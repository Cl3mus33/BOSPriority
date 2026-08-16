#pragma once

#include <map>
#include <string>
#include <vector>
#include <wx/wx.h>

/// Lets the user rank every source file involved in any real conflict, once, per record type (or
/// CONFLICT_ALL_TYPES_KEY as the fallback for a type with no specific order) - mirrors AutoSeasons'
/// own "priority order by type" resolution. Applying it (BOSIniMerger::applyTypePriorities)
/// resolves every matching conflict at once instead of clicking through them individually; a
/// group can still be fine-tuned by hand afterward the same way as before.
///
/// Internally keyed by the same locale-independent type strings BOSIniMerger and its persisted
/// ranking file use (see ConflictTypeLabels.hpp) - never a translated display string - so a saved
/// ranking survives a language change instead of silently no longer matching anything.
class TypePriorityDialog : public wxDialog {
public:
    /// allFiles: every distinct source file across all real conflicts, first-seen order (the
    /// starting order for CONFLICT_ALL_TYPES_KEY and any type not covered by initialPriorities).
    /// types: every distinct raw type key seen among real conflicts (CONFLICT_UNKNOWN_TYPE_KEY
    /// included as one of them where it applies) - CONFLICT_ALL_TYPES_KEY is always added as the
    /// first entry regardless.
    /// initialPriorities: a previously-saved ranking to start from (e.g. loaded from
    /// BOSPriority_priorities.json) - a type present here shows its saved order (any file no
    /// longer in allFiles is dropped, any file in allFiles not yet ranked is appended); a type
    /// absent from it behaves as if never customized, same as before this parameter existed.
    TypePriorityDialog(wxWindow* parent, std::vector<std::string> allFiles, std::vector<std::string> types,
                        std::map<std::string, std::vector<std::string>> initialPriorities);

    /// Type key (CONFLICT_ALL_TYPES_KEY for the fallback) -> ordered file list, most-preferred
    /// first. Only includes types the user actually visited in this dialog.
    [[nodiscard]] auto getPriorities() const -> const std::map<std::string, std::vector<std::string>>&;

private:
    void switchToType(const std::string& type);
    void saveCurrentListToPriorities();
    void refreshListBox();
    void moveSelected(int delta);

    void onTypeChanged(wxCommandEvent& event);
    void onMoveUp(wxCommandEvent& event);
    void onMoveDown(wxCommandEvent& event);

    std::map<std::string, std::vector<std::string>> m_priorities;
    std::string m_currentType;

    wxChoice* m_typeChoice = nullptr;
    std::vector<std::string> m_typeChoiceRawValues; // parallel to m_typeChoice's items
    wxListBox* m_orderList = nullptr;
};
