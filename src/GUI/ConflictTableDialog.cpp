#include "GUI/ConflictTableDialog.hpp"

#include <algorithm>
#include <set>
#include <unordered_map>
#include <wx/radiobut.h>

using namespace std;

ConflictTableDialog::ConflictTableDialog(wxWindow* parent, vector<SwapKey> keys)
    : wxDialog(parent, wxID_ANY, "Manage BOS Conflicts", wxDefaultPosition, wxSize(820, 640),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_keys(std::move(keys))
{
    auto* topSizer = new wxBoxSizer(wxVERTICAL);

    auto* introText = new wxStaticText(this, wxID_ANY,
        "Only real conflicts are shown here - a key with only one line, or with chance-weighted "
        "variants (chanceR/S/L), is included as-is with nothing to decide. A key scoped to several "
        "BOS locations (e.g. Farmhouse04 in Falkreath vs. Riverwood) is shown as ONE row: pick "
        "which file wins and every location it covers is resolved in one go. A location the chosen "
        "file doesn't cover keeps its own default until you pick something else, or exclude it.");
    introText->Wrap(780);
    topSizer->Add(introText, 0, wxALL, 10);

    auto* filterSizer = new wxBoxSizer(wxHORIZONTAL);
    filterSizer->Add(new wxStaticText(this, wxID_ANY, "Filter by type:"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    m_typeFilter = new wxChoice(this, wxID_ANY);
    filterSizer->Add(m_typeFilter, 0, wxALL, 5);
    topSizer->Add(filterSizer, 0);

    m_listCtrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 260),
                                 wxLC_REPORT | wxLC_SINGLE_SEL);
    m_listCtrl->InsertColumn(0, "Type", wxLIST_FORMAT_LEFT, 90);
    m_listCtrl->InsertColumn(1, "Key", wxLIST_FORMAT_LEFT, 170);
    m_listCtrl->InsertColumn(2, "Locations", wxLIST_FORMAT_LEFT, 80);
    m_listCtrl->InsertColumn(3, "Winner", wxLIST_FORMAT_LEFT, 300);
    topSizer->Add(m_listCtrl, 1, wxALL | wxEXPAND, 10);

    m_detailKeyLabel = new wxStaticText(this, wxID_ANY, "Select a conflict above to resolve it.");
    wxFont boldFont = m_detailKeyLabel->GetFont();
    boldFont.SetWeight(wxFONTWEIGHT_BOLD);
    m_detailKeyLabel->SetFont(boldFont);
    topSizer->Add(m_detailKeyLabel, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    m_excludeCheck = new wxCheckBox(this, wxID_ANY, "Exclude this key from AIO_SWAP.ini (every location)");
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
            group.typeLabel = "Unknown";
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
    m_typeFilter->Append("All Types");
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
        return "(excluded)";
    }
    if (winners.size() == 1) {
        return wxString(*winners.begin());
    }
    return wxString::Format("(varies by location - %zu file(s))", winners.size());
}

void ConflictTableDialog::rebuildList()
{
    m_listCtrl->DeleteAllItems();
    m_rowToGroupIndex.clear();
    clearDetail();

    const wxString filter = m_typeFilter->GetStringSelection();

    for (size_t i = 0; i < m_groups.size(); ++i) {
        const auto& group = m_groups[i];
        if (filter != "All Types" && filter != group.typeLabel) {
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
    m_detailKeyLabel->SetLabel("Select a conflict above to resolve it.");
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

    m_detailKeyLabel->SetLabel(
        wxString(group.key) + wxString::Format("  (%zu location(s) affected)", group.memberIndices.size()));

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
        for (const size_t idx : group.memberIndices) {
            if (ranges::any_of(
                    m_keys[idx].candidates, [&](const SwapEntry& e) { return e.sourceFile == file; })) {
                ++coversCount;
            }
        }

        const wxString label = wxString::Format(
            "%s  -  wins %zu/%zu location(s) if chosen", wxString(file), coversCount, group.memberIndices.size());
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

auto ConflictTableDialog::getKeys() const -> const vector<SwapKey>&
{
    return m_keys;
}
