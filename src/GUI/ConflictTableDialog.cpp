#include "GUI/ConflictTableDialog.hpp"
#include "BOSLocale.hpp"
#include "GUI/TypePriorityDialog.hpp"

#include <algorithm>
#include <set>
#include <unordered_map>
#include <wx/radiobut.h>

using namespace std;

namespace {
// Looked up fresh on every call rather than cached in a static: a language change rebuilds these
// windows in-process (see main.cpp's relaunch loop), so a value captured at first use would keep
// showing the previous language. TypePriorityDialog reads the same key, so the two always agree -
// getPriorities() is keyed by this exact string.
auto allTypesLabel() -> wxString
{
    return BOSTr("conflicts.allTypes", "All Types");
}
} // namespace

ConflictTableDialog::ConflictTableDialog(wxWindow* parent, vector<SwapKey> keys)
    : wxDialog(parent, wxID_ANY, BOSTr("conflicts.title", "Manage BOS Conflicts"), wxDefaultPosition,
               wxSize(820, 640), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_keys(std::move(keys))
{
    auto* topSizer = new wxBoxSizer(wxVERTICAL);

    auto* introText = new wxStaticText(this, wxID_ANY,
        BOSTr("conflicts.intro",
            "Only real conflicts are shown here - a key with only one line, or with chance-weighted "
            "variants (chanceR/S/L), is included as-is with nothing to decide. Pick which file wins "
            "each remaining conflict, or exclude a key entirely.")
            + "\n\n"
            + BOSTr("conflicts.introLocations",
                "A key scoped to several BOS locations (e.g. Farmhouse04 in Falkreath vs. Riverwood) is "
                "shown as ONE row: pick which file wins and every location it covers is resolved in one "
                "go. A location the chosen file doesn't cover keeps its own default until you pick "
                "something else, or exclude it.")
            + "\n\n"
            + BOSTr("conflicts.introLoadOrderTip",
                "Tip: the mod you pick as winner here should also be the one winning in your mod "
                "manager's own load order (its plugin, meshes, textures) - otherwise something else may "
                "quietly override what this swap points to. See the README for details."));
    introText->Wrap(780);
    topSizer->Add(introText, 0, wxALL, 10);

    auto* filterSizer = new wxBoxSizer(wxHORIZONTAL);
    filterSizer->Add(new wxStaticText(this, wxID_ANY, BOSTr("conflicts.filterByType", "Filter by type:")), 0,
                     wxALIGN_CENTER_VERTICAL | wxALL, 5);
    m_typeFilter = new wxChoice(this, wxID_ANY);
    filterSizer->Add(m_typeFilter, 0, wxALL, 5);
    auto* priorityButton
        = new wxButton(this, wxID_ANY, BOSTr("conflicts.setPriorityButton", "Set Priority by Type..."));
    filterSizer->Add(priorityButton, 0, wxALL, 5);
    topSizer->Add(filterSizer, 0);

    m_listCtrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 260),
                                 wxLC_REPORT | wxLC_SINGLE_SEL);
    m_listCtrl->InsertColumn(0, BOSTr("conflicts.column.type", "Type"), wxLIST_FORMAT_LEFT, 90);
    m_listCtrl->InsertColumn(1, BOSTr("conflicts.column.key", "Key"), wxLIST_FORMAT_LEFT, 170);
    m_listCtrl->InsertColumn(2, BOSTr("conflicts.column.locations", "Locations"), wxLIST_FORMAT_LEFT, 80);
    m_listCtrl->InsertColumn(3, BOSTr("conflicts.column.winner", "Winner"), wxLIST_FORMAT_LEFT, 300);
    topSizer->Add(m_listCtrl, 1, wxALL | wxEXPAND, 10);

    m_detailKeyLabel
        = new wxStaticText(this, wxID_ANY, BOSTr("conflicts.selectHint", "Select a conflict above to resolve it."));
    wxFont boldFont = m_detailKeyLabel->GetFont();
    boldFont.SetWeight(wxFONTWEIGHT_BOLD);
    m_detailKeyLabel->SetFont(boldFont);
    topSizer->Add(m_detailKeyLabel, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    m_excludeCheck = new wxCheckBox(
        this, wxID_ANY, BOSTr("conflicts.exclude", "Exclude this key from AIO_SWAP.ini (every location)"));
    m_excludeCheck->Enable(false);
    topSizer->Add(m_excludeCheck, 0, wxALL, 10);

    m_radioPanel = new wxPanel(this);
    m_radioSizer = new wxBoxSizer(wxVERTICAL);
    m_radioPanel->SetSizer(m_radioSizer);
    topSizer->Add(m_radioPanel, 0, wxLEFT | wxRIGHT | wxEXPAND, 10);

    auto* dialogButtons = CreateButtonSizer(wxOK | wxCANCEL);
    if (dialogButtons != nullptr) {
        topSizer->Add(dialogButtons, 0, wxALL | wxALIGN_RIGHT, 10);
    }

    SetSizer(topSizer);

    m_typeFilter->Bind(wxEVT_CHOICE, &ConflictTableDialog::onTypeFilterChanged, this);
    m_listCtrl->Bind(wxEVT_LIST_ITEM_SELECTED, &ConflictTableDialog::onListSelectionChanged, this);
    m_excludeCheck->Bind(wxEVT_CHECKBOX, &ConflictTableDialog::onExcludeToggled, this);
    priorityButton->Bind(wxEVT_BUTTON, &ConflictTableDialog::onSetPriorityByType, this);

    buildGroups();
    rebuildTypeFilterChoices();
    rebuildList();
}

void ConflictTableDialog::buildGroups()
{
    unordered_map<string, size_t> keyToGroupIndex;

    for (size_t i = 0; i < m_keys.size(); ++i) {
        const auto& swapKey = m_keys[i];
        if (!swapKey.isRealConflict()) {
            continue;
        }

        size_t groupIdx = 0;
        if (const auto it = keyToGroupIndex.find(swapKey.key); it != keyToGroupIndex.end()) {
            groupIdx = it->second;
        } else {
            groupIdx = m_groups.size();
            keyToGroupIndex[swapKey.key] = groupIdx;
            m_groups.push_back(KeyGroup {swapKey.key, wxString(), {}, {}});
        }

        auto& group = m_groups[groupIdx];
        group.memberIndices.push_back(i);
        if (group.typeLabel.IsEmpty() && swapKey.recordType) {
            group.typeLabel = *swapKey.recordType;
        }
        for (const auto& candidate : swapKey.candidates) {
            if (ranges::find(group.distinctFiles, candidate.sourceFile) == group.distinctFiles.end()) {
                group.distinctFiles.push_back(candidate.sourceFile);
            }
        }
    }

    for (auto& group : m_groups) {
        if (group.typeLabel.IsEmpty()) {
            group.typeLabel = BOSTr("conflicts.unknownType", "Unknown");
        }
    }
}

void ConflictTableDialog::rebuildTypeFilterChoices()
{
    set<wxString> types;
    for (const auto& group : m_groups) {
        types.insert(group.typeLabel);
    }

    m_typeFilter->Clear();
    m_typeFilter->Append(allTypesLabel());
    for (const auto& type : types) {
        m_typeFilter->Append(type);
    }
    m_typeFilter->SetSelection(0);
}

auto ConflictTableDialog::winnerLabelFor(const KeyGroup& group) const -> wxString
{
    bool anyIncluded = false;
    set<string> winners;
    for (const size_t idx : group.memberIndices) {
        const auto& swapKey = m_keys[idx];
        if (swapKey.excluded) {
            continue;
        }
        anyIncluded = true;
        winners.insert(swapKey.candidates[static_cast<size_t>(swapKey.selectedCandidate)].sourceFile);
    }

    if (!anyIncluded) {
        return BOSTr("conflicts.excludedLabel", "(excluded)");
    }
    if (winners.size() == 1) {
        return wxString(*winners.begin());
    }
    return wxString::Format(BOSTr("conflicts.winnerVaries", "(varies by location - %zu file(s))"), winners.size());
}

void ConflictTableDialog::rebuildList()
{
    m_listCtrl->DeleteAllItems();
    m_rowToGroupIndex.clear();
    clearDetail();

    const wxString filter = m_typeFilter->GetStringSelection();

    for (size_t i = 0; i < m_groups.size(); ++i) {
        const auto& group = m_groups[i];
        if (filter != allTypesLabel() && filter != group.typeLabel) {
            continue;
        }

        const long row = m_listCtrl->InsertItem(m_listCtrl->GetItemCount(), group.typeLabel);
        m_listCtrl->SetItem(row, 1, group.key);
        m_listCtrl->SetItem(row, 2, wxString::Format("%zu", group.memberIndices.size()));
        m_listCtrl->SetItem(row, 3, winnerLabelFor(group));

        m_rowToGroupIndex.push_back(i);
    }
}

void ConflictTableDialog::clearDetail()
{
    m_detailKeyLabel->SetLabel(BOSTr("conflicts.selectHint", "Select a conflict above to resolve it."));
    m_excludeCheck->SetValue(false);
    m_excludeCheck->Enable(false);
    m_radioSizer->Clear(true); // destroys the previous group's radio buttons too
    m_radioButtons.clear();
    m_radioButtonFiles.clear();
    m_radioPanel->Layout();
}

void ConflictTableDialog::showDetailFor(int listRow)
{
    if (listRow < 0 || static_cast<size_t>(listRow) >= m_rowToGroupIndex.size()) {
        clearDetail();
        return;
    }

    const size_t groupIdx = m_rowToGroupIndex[static_cast<size_t>(listRow)];
    const auto& group = m_groups[groupIdx];

    m_detailKeyLabel->SetLabel(wxString(group.key)
        + wxString::Format(BOSTr("conflicts.locationsAffected", "  (%zu location(s) affected)"),
            group.memberIndices.size()));

    const bool allExcluded = ranges::all_of(group.memberIndices, [&](size_t idx) { return m_keys[idx].excluded; });
    m_excludeCheck->Enable(true);
    m_excludeCheck->SetValue(allExcluded);

    m_radioSizer->Clear(true);
    m_radioButtons.clear();
    m_radioButtonFiles.clear();

    // A radio pre-selects only if every non-excluded location currently agrees on the same
    // winning file - otherwise no radio is pre-selected, since there isn't yet one answer to show.
    optional<string> unanimousWinner;
    bool disagreement = false;
    for (const size_t idx : group.memberIndices) {
        const auto& swapKey = m_keys[idx];
        if (swapKey.excluded) {
            continue;
        }
        const string winner = swapKey.candidates[static_cast<size_t>(swapKey.selectedCandidate)].sourceFile;
        if (!unanimousWinner) {
            unanimousWinner = winner;
        } else if (*unanimousWinner != winner) {
            disagreement = true;
        }
    }
    if (disagreement) {
        unanimousWinner.reset();
    }

    for (size_t i = 0; i < group.distinctFiles.size(); ++i) {
        const auto& file = group.distinctFiles[i];
        size_t coversCount = 0;
        bool anyTargetMissing = false;
        for (const size_t idx : group.memberIndices) {
            const auto& candidates = m_keys[idx].candidates;
            const auto it = ranges::find_if(candidates, [&](const SwapEntry& e) { return e.sourceFile == file; });
            if (it == candidates.end()) {
                continue;
            }
            ++coversCount;
            if (it->targetMissing) {
                anyTargetMissing = true;
            }
        }

        wxString label = wxString::Format(BOSTr("conflicts.candidateWins", "%s  -  wins %zu of %zu location(s)"),
            wxString(file), coversCount, group.memberIndices.size());
        if (anyTargetMissing) {
            // BOS silently skips a swap whose target isn't defined by any active plugin - picking
            // this file as winner wouldn't actually change anything in-game for that location.
            label += BOSTr("conflicts.candidateTargetMissing",
                "  [warning: this file's swap target isn't defined by any plugin in Data - BOS would skip it]");
        }
        auto* radio = new wxRadioButton(m_radioPanel, wxID_ANY, label, wxDefaultPosition, wxDefaultSize,
                                         i == 0 ? wxRB_GROUP : 0);
        radio->SetValue(unanimousWinner.has_value() && *unanimousWinner == file);
        radio->Enable(!allExcluded);
        radio->Bind(wxEVT_RADIOBUTTON, &ConflictTableDialog::onWinnerChosen, this);
        m_radioSizer->Add(radio, 0, wxALL, 3);
        m_radioButtons.push_back(radio);
        m_radioButtonFiles.push_back(file);
    }

    m_radioPanel->Layout();
    Layout();
}

void ConflictTableDialog::applyWinnerFile(const KeyGroup& group, const string& winnerFile)
{
    for (const size_t idx : group.memberIndices) {
        auto& swapKey = m_keys[idx];
        for (size_t i = 0; i < swapKey.candidates.size(); ++i) {
            if (swapKey.candidates[i].sourceFile == winnerFile) {
                swapKey.selectedCandidate = static_cast<int>(i);
                swapKey.excluded = false;
                break;
            }
        }
        // A location this file doesn't cover is left untouched - it keeps whatever it already
        // had (its own last-declared-wins default, or an earlier explicit pick).
    }
}

void ConflictTableDialog::onTypeFilterChanged(wxCommandEvent& /*event*/)
{
    rebuildList();
}

void ConflictTableDialog::onListSelectionChanged(wxListEvent& event)
{
    showDetailFor(static_cast<int>(event.GetIndex()));
}

void ConflictTableDialog::onExcludeToggled(wxCommandEvent& /*event*/)
{
    const long row = m_listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0 || static_cast<size_t>(row) >= m_rowToGroupIndex.size()) {
        return;
    }

    const size_t groupIdx = m_rowToGroupIndex[static_cast<size_t>(row)];
    const auto& group = m_groups[groupIdx];
    const bool excluded = m_excludeCheck->GetValue();
    for (const size_t idx : group.memberIndices) {
        m_keys[idx].excluded = excluded;
    }

    for (auto* radio : m_radioButtons) {
        radio->Enable(!excluded);
    }

    m_listCtrl->SetItem(row, 3, winnerLabelFor(group));
}

void ConflictTableDialog::onWinnerChosen(wxCommandEvent& /*event*/)
{
    const long row = m_listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0 || static_cast<size_t>(row) >= m_rowToGroupIndex.size()) {
        return;
    }

    const size_t groupIdx = m_rowToGroupIndex[static_cast<size_t>(row)];
    const auto& group = m_groups[groupIdx];

    for (size_t i = 0; i < m_radioButtons.size(); ++i) {
        if (m_radioButtons[i]->GetValue()) {
            applyWinnerFile(group, m_radioButtonFiles[i]);
            break;
        }
    }

    m_listCtrl->SetItem(row, 3, winnerLabelFor(group));
}

void ConflictTableDialog::onSetPriorityByType(wxCommandEvent& /*event*/)
{
    vector<string> allFiles;
    for (const auto& group : m_groups) {
        for (const auto& file : group.distinctFiles) {
            if (ranges::find(allFiles, file) == allFiles.end()) {
                allFiles.push_back(file);
            }
        }
    }

    vector<wxString> types;
    for (const auto& group : m_groups) {
        if (ranges::find(types, group.typeLabel) == types.end()) {
            types.push_back(group.typeLabel);
        }
    }

    TypePriorityDialog dlg(this, allFiles, types);
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const auto& priorities = dlg.getPriorities();
    for (auto& group : m_groups) {
        auto it = priorities.find(group.typeLabel);
        if (it == priorities.end()) {
            it = priorities.find(allTypesLabel());
        }
        if (it == priorities.end()) {
            continue;
        }

        // Resolved per location, not once per group: the highest-ranked file often only has a
        // candidate for some of a key's locations, and the rest should fall through to the next
        // ranked file that does - applying the group's single best file and leaving the remaining
        // locations on BOS's alphabetical default would ignore the ranking exactly where it was
        // needed most. An explicitly excluded location is left excluded: that's a deliberate
        // decision the user made per key, not something a file ranking should silently undo.
        for (const size_t idx : group.memberIndices) {
            auto& swapKey = m_keys[idx];
            if (swapKey.excluded) {
                continue;
            }
            for (const auto& file : it->second) {
                const auto candidate = ranges::find_if(
                    swapKey.candidates, [&](const SwapEntry& e) { return e.sourceFile == file; });
                if (candidate != swapKey.candidates.end()) {
                    swapKey.selectedCandidate = static_cast<int>(candidate - swapKey.candidates.begin());
                    break;
                }
            }
        }
    }

    rebuildList();
}

auto ConflictTableDialog::getKeys() const -> const vector<SwapKey>&
{
    return m_keys;
}
