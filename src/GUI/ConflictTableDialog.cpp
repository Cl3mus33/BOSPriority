#include "GUI/ConflictTableDialog.hpp"
#include "BOSLocale.hpp"
#include "GUI/ConflictTypeLabels.hpp"
#include "GUI/TypePriorityDialog.hpp"

#include <algorithm>
#include <set>
#include <unordered_map>

using namespace std;

namespace {
constexpr int ID_MOVE_UP = wxID_HIGHEST + 40;
constexpr int ID_MOVE_DOWN = wxID_HIGHEST + 41;

// scan() explodes a multi-location BOS section into one display section per location (see
// BOSIniMerger.hpp's doc comment), so a member's own .section is already a single-location string
// like "[Forms|FalkreathLocation]" - this just strips the "[Prefix|" / "]" wrapper for display,
// leaving "FalkreathLocation". An unfiltered section ("[Forms]", no '|') is shown as-is - it isn't
// a location at all, but naming it beats silently omitting that member from the list.
auto locationLabel(const string& section) -> string
{
    const auto pipePos = section.find('|');
    if (pipePos == string::npos) {
        return section;
    }
    string inner = section.substr(pipePos + 1);
    if (!inner.empty() && inner.back() == ']') {
        inner.pop_back();
    }
    return inner;
}
} // namespace

ConflictTableDialog::ConflictTableDialog(wxWindow* parent, vector<SwapKey> keys, filesystem::path outputDir)
    : wxDialog(parent, wxID_ANY, BOSTr("conflicts.title", "Manage BOS Conflicts"), wxDefaultPosition,
               wxSize(820, 680), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_keys(std::move(keys))
    , m_outputDir(std::move(outputDir))
{
    auto* topSizer = new wxBoxSizer(wxVERTICAL);

    auto* introText = new wxStaticText(this, wxID_ANY,
        BOSTr("conflicts.intro",
            "Only real conflicts are shown here - a key with only one line, or with chance-weighted "
            "variants (chanceR/S/L), is included as-is with nothing to decide. Select a conflict "
            "below to rank its candidate files, or exclude a key entirely.")
            + "\n\n"
            + BOSTr("conflicts.introLocations",
                "A key scoped to several BOS locations (e.g. Farmhouse04 in Falkreath vs. Riverwood) is "
                "shown as ONE row, but rarely has one file covering every location. Rank the candidates "
                "with Move Up/Down: each location independently takes the highest-ranked file that "
                "actually has a line for it, shown live in the resolution preview below the list.")
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

    m_listCtrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 220),
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

    m_detailLocationsLabel = new wxStaticText(this, wxID_ANY, wxString());
    topSizer->Add(m_detailLocationsLabel, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    m_excludeCheck = new wxCheckBox(
        this, wxID_ANY, BOSTr("conflicts.exclude", "Exclude this key from AIO_SWAP.ini (every location)"));
    m_excludeCheck->Enable(false);
    topSizer->Add(m_excludeCheck, 0, wxALL, 10);

    auto* orderSizer = new wxBoxSizer(wxHORIZONTAL);
    m_orderList = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 110), 0, nullptr, wxLB_SINGLE);
    orderSizer->Add(m_orderList, 1, wxALL | wxEXPAND, 5);
    auto* moveButtonSizer = new wxBoxSizer(wxVERTICAL);
    m_moveUpButton = new wxButton(this, ID_MOVE_UP, BOSTr("typePriority.moveUp", "Move Up"));
    m_moveDownButton = new wxButton(this, ID_MOVE_DOWN, BOSTr("typePriority.moveDown", "Move Down"));
    moveButtonSizer->Add(m_moveUpButton, 0, wxALL, 5);
    moveButtonSizer->Add(m_moveDownButton, 0, wxALL, 5);
    orderSizer->Add(moveButtonSizer, 0, wxALIGN_CENTER_VERTICAL);
    topSizer->Add(orderSizer, 0, wxLEFT | wxRIGHT | wxEXPAND, 5);

    m_resolutionSummaryLabel = new wxStaticText(this, wxID_ANY, wxString());
    topSizer->Add(m_resolutionSummaryLabel, 0, wxALL | wxEXPAND, 10);

    auto* dialogButtons = CreateButtonSizer(wxOK | wxCANCEL);
    if (dialogButtons != nullptr) {
        topSizer->Add(dialogButtons, 0, wxALL | wxALIGN_RIGHT, 10);
    }

    SetSizer(topSizer);

    m_typeFilter->Bind(wxEVT_CHOICE, &ConflictTableDialog::onTypeFilterChanged, this);
    m_listCtrl->Bind(wxEVT_LIST_ITEM_SELECTED, &ConflictTableDialog::onListSelectionChanged, this);
    m_excludeCheck->Bind(wxEVT_CHECKBOX, &ConflictTableDialog::onExcludeToggled, this);
    m_moveUpButton->Bind(wxEVT_BUTTON, &ConflictTableDialog::onOrderMoveUp, this);
    m_moveDownButton->Bind(wxEVT_BUTTON, &ConflictTableDialog::onOrderMoveDown, this);
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
        if (group.typeKey.IsEmpty() && swapKey.recordType) {
            group.typeKey = *swapKey.recordType;
        }
        for (const auto& candidate : swapKey.candidates) {
            if (ranges::find(group.distinctFiles, candidate.sourceFile) == group.distinctFiles.end()) {
                group.distinctFiles.push_back(candidate.sourceFile);
            }
        }
    }

    for (auto& group : m_groups) {
        if (group.typeKey.IsEmpty()) {
            group.typeKey = CONFLICT_UNKNOWN_TYPE_KEY;
        }
    }
}

void ConflictTableDialog::rebuildTypeFilterChoices()
{
    set<wxString> types;
    for (const auto& group : m_groups) {
        types.insert(group.typeKey);
    }

    m_typeFilter->Clear();
    m_typeFilterRawValues.clear();

    m_typeFilter->Append(conflictDisplayTypeLabel(CONFLICT_ALL_TYPES_KEY));
    m_typeFilterRawValues.emplace_back(CONFLICT_ALL_TYPES_KEY);
    for (const auto& type : types) {
        m_typeFilter->Append(conflictDisplayTypeLabel(type));
        m_typeFilterRawValues.push_back(type);
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

    const int filterSelection = m_typeFilter->GetSelection();
    const wxString filterRaw
        = (filterSelection != wxNOT_FOUND && static_cast<size_t>(filterSelection) < m_typeFilterRawValues.size())
        ? m_typeFilterRawValues[static_cast<size_t>(filterSelection)]
        : wxString(CONFLICT_ALL_TYPES_KEY);

    for (size_t i = 0; i < m_groups.size(); ++i) {
        const auto& group = m_groups[i];
        if (filterRaw != CONFLICT_ALL_TYPES_KEY && filterRaw != group.typeKey) {
            continue;
        }

        const long row = m_listCtrl->InsertItem(m_listCtrl->GetItemCount(), conflictDisplayTypeLabel(group.typeKey));
        m_listCtrl->SetItem(row, 1, group.key);
        m_listCtrl->SetItem(row, 2, wxString::Format("%zu", group.memberIndices.size()));
        m_listCtrl->SetItem(row, 3, winnerLabelFor(group));

        m_rowToGroupIndex.push_back(i);
    }
}

void ConflictTableDialog::clearDetail()
{
    m_detailKeyLabel->SetLabel(BOSTr("conflicts.selectHint", "Select a conflict above to resolve it."));
    m_detailLocationsLabel->SetLabel(wxString());
    m_excludeCheck->SetValue(false);
    m_excludeCheck->Enable(false);
    m_orderList->Clear();
    m_orderList->Enable(false);
    m_moveUpButton->Enable(false);
    m_moveDownButton->Enable(false);
    m_resolutionSummaryLabel->SetLabel(wxString());
}

auto ConflictTableDialog::selectedGroup() -> KeyGroup*
{
    const long row = m_listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0 || static_cast<size_t>(row) >= m_rowToGroupIndex.size()) {
        return nullptr;
    }
    return &m_groups[m_rowToGroupIndex[static_cast<size_t>(row)]];
}

void ConflictTableDialog::refreshOrderList(const KeyGroup& group)
{
    // Seeded by how many of the group's non-excluded locations each file currently wins, most
    // first - a reasonable reconstruction of "what got us here" (BOS's own default, a loaded
    // decision, or an applied type ranking), since the exact order behind that isn't itself
    // persisted, only its per-location results are.
    vector<pair<string, int>> fileWinCounts;
    for (const auto& file : group.distinctFiles) {
        fileWinCounts.emplace_back(file, 0);
    }
    for (const size_t idx : group.memberIndices) {
        const auto& swapKey = m_keys[idx];
        if (swapKey.excluded) {
            continue;
        }
        const string winner = swapKey.candidates[static_cast<size_t>(swapKey.selectedCandidate)].sourceFile;
        for (auto& [file, count] : fileWinCounts) {
            if (file == winner) {
                ++count;
                break;
            }
        }
    }
    ranges::stable_sort(fileWinCounts, [](const auto& a, const auto& b) { return a.second > b.second; });

    m_orderList->Clear();
    for (const auto& [file, count] : fileWinCounts) {
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

        wxString itemLabel = wxString::Format(BOSTr("conflicts.orderItemCoverage", "%s  (covers %zu of %zu location(s))"),
            wxString(file), coversCount, group.memberIndices.size());
        if (anyTargetMissing) {
            itemLabel += BOSTr("conflicts.candidateTargetMissing",
                "  [warning: this file's swap target isn't defined by any plugin in Data - BOS would skip it]");
        }
        m_orderList->Append(itemLabel, new wxStringClientData(file));
    }
}

void ConflictTableDialog::applyCurrentOrder(const KeyGroup& group)
{
    vector<string> order;
    for (unsigned i = 0; i < m_orderList->GetCount(); ++i) {
        auto* data = dynamic_cast<wxStringClientData*>(m_orderList->GetClientObject(i));
        if (data != nullptr) {
            order.push_back(data->GetData().ToStdString());
        }
    }

    for (const size_t idx : group.memberIndices) {
        auto& swapKey = m_keys[idx];
        if (swapKey.excluded) {
            continue; // stays excluded regardless of order - see onExcludeToggled
        }
        for (const auto& file : order) {
            const auto candidateIt
                = ranges::find_if(swapKey.candidates, [&](const SwapEntry& e) { return e.sourceFile == file; });
            if (candidateIt != swapKey.candidates.end()) {
                swapKey.selectedCandidate = static_cast<int>(candidateIt - swapKey.candidates.begin());
                swapKey.userDecided = true;
                break;
            }
        }
    }
}

void ConflictTableDialog::refreshResolutionSummary(const KeyGroup& group)
{
    wxString summary = BOSTr("conflicts.resolutionPreview", "Resolution preview:") + "\n";
    for (const size_t idx : group.memberIndices) {
        const auto& swapKey = m_keys[idx];
        const wxString locName = locationLabel(swapKey.section);
        const wxString resolvedTo = swapKey.excluded
            ? BOSTr("conflicts.excludedLabel", "(excluded)")
            : wxString(swapKey.candidates[static_cast<size_t>(swapKey.selectedCandidate)].sourceFile);
        summary += wxString::Format("  %s -> %s\n", locName, resolvedTo);
    }

    m_resolutionSummaryLabel->SetLabel(summary);
    m_resolutionSummaryLabel->Wrap(780);
    Layout();
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

    vector<string> allLocationNames;
    for (const size_t idx : group.memberIndices) {
        allLocationNames.push_back(locationLabel(m_keys[idx].section));
    }
    wxString locationsList = BOSTr("conflicts.locationsLabel", "Locations: ");
    for (size_t i = 0; i < allLocationNames.size(); ++i) {
        if (i > 0) {
            locationsList += ", ";
        }
        locationsList += allLocationNames[i];
    }
    m_detailLocationsLabel->SetLabel(locationsList);
    m_detailLocationsLabel->Wrap(780); // re-wrap for this group's actual text - length varies a lot

    const bool allExcluded = ranges::all_of(group.memberIndices, [&](size_t idx) { return m_keys[idx].excluded; });
    m_excludeCheck->Enable(true);
    m_excludeCheck->SetValue(allExcluded);

    m_orderList->Enable(!allExcluded);
    m_moveUpButton->Enable(!allExcluded);
    m_moveDownButton->Enable(!allExcluded);
    refreshOrderList(group);
    refreshResolutionSummary(group);

    Layout();
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
    auto* group = selectedGroup();
    if (group == nullptr) {
        return;
    }

    const bool excluded = m_excludeCheck->GetValue();
    for (const size_t idx : group->memberIndices) {
        m_keys[idx].excluded = excluded;
        m_keys[idx].userDecided = true;
    }

    m_orderList->Enable(!excluded);
    m_moveUpButton->Enable(!excluded);
    m_moveDownButton->Enable(!excluded);
    if (!excluded) {
        applyCurrentOrder(*group); // re-resolve now that it's no longer excluded
    }
    refreshResolutionSummary(*group);

    const long row = m_listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row >= 0) {
        m_listCtrl->SetItem(row, 3, winnerLabelFor(*group));
    }
}

namespace {
void moveListBoxItem(wxListBox* list, int selection, int target)
{
    const wxString label = list->GetString(static_cast<unsigned>(selection));
    auto* data = dynamic_cast<wxStringClientData*>(list->GetClientObject(static_cast<unsigned>(selection)));
    const wxString value = data != nullptr ? data->GetData() : wxString();

    list->Delete(static_cast<unsigned>(selection));
    list->Insert(label, static_cast<unsigned>(target), new wxStringClientData(value));
    list->SetSelection(target);
}
} // namespace

void ConflictTableDialog::onOrderMoveUp(wxCommandEvent& /*event*/)
{
    const int selection = m_orderList->GetSelection();
    if (selection == wxNOT_FOUND || selection == 0) {
        return;
    }
    moveListBoxItem(m_orderList, selection, selection - 1);

    auto* group = selectedGroup();
    if (group == nullptr) {
        return;
    }
    applyCurrentOrder(*group);
    refreshResolutionSummary(*group);
    const long row = m_listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row >= 0) {
        m_listCtrl->SetItem(row, 3, winnerLabelFor(*group));
    }
}

void ConflictTableDialog::onOrderMoveDown(wxCommandEvent& /*event*/)
{
    const int selection = m_orderList->GetSelection();
    if (selection == wxNOT_FOUND || static_cast<unsigned>(selection) + 1 >= m_orderList->GetCount()) {
        return;
    }
    moveListBoxItem(m_orderList, selection, selection + 1);

    auto* group = selectedGroup();
    if (group == nullptr) {
        return;
    }
    applyCurrentOrder(*group);
    refreshResolutionSummary(*group);
    const long row = m_listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row >= 0) {
        m_listCtrl->SetItem(row, 3, winnerLabelFor(*group));
    }
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

    vector<string> types;
    for (const auto& group : m_groups) {
        const string typeKey = group.typeKey.ToStdString();
        if (ranges::find(types, typeKey) == types.end()) {
            types.push_back(typeKey);
        }
    }

    const auto rankingFile = m_outputDir / BOS_PRIORITY_TYPE_RANKING_FILE_NAME;
    auto initialPriorities = BOSIniMerger::loadTypePriorities(rankingFile);

    TypePriorityDialog dlg(this, allFiles, types, std::move(initialPriorities));
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const auto& priorities = dlg.getPriorities();
    BOSIniMerger::applyTypePriorities(m_keys, priorities);
    try {
        BOSIniMerger::saveTypePriorities(priorities, rankingFile);
    } catch (const exception&) {
        // Applied in-memory regardless - losing the save just means it won't be remembered next
        // scan, not worth blocking the rest of this dialog over.
    }

    rebuildList();
}

auto ConflictTableDialog::getKeys() const -> const vector<SwapKey>&
{
    return m_keys;
}
