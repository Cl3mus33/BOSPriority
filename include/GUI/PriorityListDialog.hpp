#pragma once

#include "ModManager.hpp"

#include <memory>
#include <vector>
#include <wx/wx.h>

/// Lets the user reorder the mods that ship a BOS ini. Mirrors the "Manage Season Mod Conflicts"
/// dialog in AutoSeasons - same idea (explicit user priority instead of an implicit rule),
/// applied to Base Object Swapper ini files instead of Data/Seasons declarations.
class PriorityListDialog : public wxDialog {
public:
    PriorityListDialog(wxWindow* parent, std::vector<std::shared_ptr<ModManager::Mod>> mods);

    /// Mods in the final chosen order: index 0 is applied first (loses conflicts), the last
    /// entry is applied last (wins). Caller is responsible for turning this into `priority`
    /// values on the underlying Mod objects.
    [[nodiscard]] auto getOrderedMods() const -> const std::vector<std::shared_ptr<ModManager::Mod>>&;

private:
    void refreshListBox();
    void onMoveUp(wxCommandEvent& event);
    void onMoveDown(wxCommandEvent& event);

    wxListBox* m_listBox = nullptr;
    std::vector<std::shared_ptr<ModManager::Mod>> m_mods;
};
