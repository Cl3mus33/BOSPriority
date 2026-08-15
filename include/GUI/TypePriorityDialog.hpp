#pragma once

#include <map>
#include <string>
#include <vector>
#include <wx/wx.h>

/// Lets the user rank every source file involved in any real conflict, once, per record type (or
/// "All Types" as the fallback for a type with no specific order) - mirrors AutoSeasons' own
/// "priority order by type" resolution. Applying it (ConflictTableDialog::applyTypePriorities)
/// resolves every matching conflict at once instead of clicking through them individually; a
/// group can still be fine-tuned by hand afterward the same way as before.
class TypePriorityDialog : public wxDialog {
public:
    /// allFiles: every distinct source file across all real conflicts, first-seen order (the
    /// starting order for "All Types" and any type not yet customized).
    /// types: every distinct type label seen among real conflicts ("Unknown" included as-is);
    /// "All Types" is always added as the first entry regardless.
    TypePriorityDialog(wxWindow* parent, std::vector<std::string> allFiles, std::vector<wxString> types);

    /// Type label ("All Types" for the fallback) -> ordered file list, most-preferred first. Only
    /// includes types the user actually visited in this dialog.
    [[nodiscard]] auto getPriorities() const -> const std::map<wxString, std::vector<std::string>>&;

private:
    void switchToType(const wxString& type);
    void saveCurrentListToPriorities();
    void refreshListBox();
    void moveSelected(int delta);

    void onTypeChanged(wxCommandEvent& event);
    void onMoveUp(wxCommandEvent& event);
    void onMoveDown(wxCommandEvent& event);

    std::map<wxString, std::vector<std::string>> m_priorities;
    wxString m_currentType;

    wxChoice* m_typeChoice = nullptr;
    wxListBox* m_orderList = nullptr;
};
