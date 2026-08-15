#pragma once

#include <filesystem>
#include <vector>
#include <wx/wx.h>

/// Lets the user reorder the discovered *_SWAP.ini files. Same idea as AutoSeasons' "Manage
/// Season Mod Conflicts" dialog - explicit user priority instead of an implicit rule - applied
/// per ini file (BOS's own unit of conflict resolution) rather than per mod.
class PriorityListDialog : public wxDialog {
public:
    PriorityListDialog(wxWindow* parent, std::vector<std::filesystem::path> filesInApplyOrder);

    /// Files in the final chosen order: index 0 is applied first (loses conflicts), the last
    /// entry is applied last (wins).
    [[nodiscard]] auto getOrderedFiles() const -> const std::vector<std::filesystem::path>&;

private:
    void refreshListBox();
    void onMoveUp(wxCommandEvent& event);
    void onMoveDown(wxCommandEvent& event);

    wxListBox* m_listBox = nullptr;
    std::vector<std::filesystem::path> m_files;
};
