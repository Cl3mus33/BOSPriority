#include "GUI/ConflictTableDialog.hpp"

#include <set>
#include <wx/radiobut.h>

using namespace std;

ConflictTableDialog::ConflictTableDialog(wxWindow* parent, vector<SwapKey> keys)
    : wxDialog(parent, wxID_ANY, "Manage BOS Conflicts", wxDefaultPosition, wxSize(760, 620),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_keys(std::move(keys))
{
    auto* topSizer = new wxBoxSizer(wxVERTICAL);

    auto* introText = new wxStaticText(this, wxID_ANY,
        "Only real conflicts are shown here - a key with only one line, or with chance-weighted "
        "variants (chanceR/S/L), is included as-is with nothing to decide. Pick which file wins "
        "each remaining conflict, or exclude a key entirely.");
    introText->Wrap(720);
    topSizer->Add(introText, 0, wxALL, 10);

    auto* filterSizer = new wxBoxSizer(wxHORIZONTAL);
    filterSizer->Add(new wxStaticText(this, wxID_ANY, "Filter by type:"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    m_typeFilter = new wxChoice(this, wxID_ANY);
    filterSizer->Add(m_typeFilter, 0, wxALL, 5);
    topSizer->Add(filterSizer, 0);

    m_listCtrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 260),
                                 wxLC_REPORT | wxLC_SINGLE_SEL);
    m_listCtrl->InsertColumn(0, "Type", wxLIST_FORMAT_LEFT, 90);
    m_listCtrl->InsertColumn(1, "Key", wxLIST_FORMAT_LEFT, 150);
    // Two rows can show the same Key text but be entirely unrelated conflicts - e.g. a
    // location-filtered BOS section like "[Forms|FalkreathLocation]" vs.
    // "[Forms|RiverwoodLocation]" for the same key name. Shown as its own column (rather than
    // folded into Key) so it's obvious at a glance which rows are actually the same decision and
    // which just happen to share a key name.
    m_listCtrl->InsertColumn(2, "Section", wxLIST_FORMAT_LEFT, 170);
    m_listCtrl->InsertColumn(3, "Winner", wxLIST_FORMAT_LEFT, 200);
    m_listCtrl->InsertColumn(4, "Candidates", wxLIST_FORMAT_LEFT, 90);
    topSizer->Add(m_listCtrl, 1, wxALL | wxEXPAND, 10);

    m_detailKeyLabel = new wxStaticText(this, wxID_ANY, "Select a conflict above to resolve it.");
    wxFont boldFont = m_detailKeyLabel->GetFont();
    boldFont.SetWeight(wxFONTWEIGHT_BOLD);
    m_detailKeyLabel->SetFont(boldFont);
    topSizer->Add(m_detailKeyLabel, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    m_excludeCheck = new wxCheckBox(this, wxID_ANY, "Exclude this key from AIO_SWAP.ini");
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

    rebuildTypeFilterChoices();
    rebuildList();
}

void ConflictTableDialog::rebuildTypeFilterChoices()
{
    set<wxString> types;
    for (const auto& swapKey : m_keys) {
        if (!swapKey.isRealConflict()) {
            continue;
        }
        types.insert(swapKey.recordType ? wxString(*swapKey.recordType) : wxString("Unknown"));
    }

    m_typeFilter->Clear();
    m_typeFilter->Append("All Types");
    for (const auto& type : types) {
        m_typeFilter->Append(type);
    }
    m_typeFilter->SetSelection(0);
}

void ConflictTableDialog::rebuildList()
{
    m_listCtrl->DeleteAllItems();
    m_conflictRowToKeyIndex.clear();
    clearDetail();

    const wxString filter = m_typeFilter->GetStringSelection();

    for (size_t i = 0; i < m_keys.size(); ++i) {
        const auto& swapKey = m_keys[i];
        if (!swapKey.isRealConflict()) {
            continue;
        }

        const wxString typeStr = swapKey.recordType ? wxString(*swapKey.recordType) : wxString("Unknown");
        if (filter != "All Types" && filter != typeStr) {
            continue;
        }

        const long row = m_listCtrl->InsertItem(m_listCtrl->GetItemCount(), typeStr);
        m_listCtrl->SetItem(row, 1, swapKey.key);
        m_listCtrl->SetItem(row, 2, swapKey.section);
        const wxString winner = swapKey.excluded
            ? wxString("(excluded)")
            : wxString(swapKey.candidates[static_cast<size_t>(swapKey.selectedCandidate)].sourceFile);
        m_listCtrl->SetItem(row, 3, winner);
        m_listCtrl->SetItem(row, 4, wxString::Format("%zu", swapKey.candidates.size()));

        m_conflictRowToKeyIndex.push_back(i);
    }
}

void ConflictTableDialog::clearDetail()
{
    m_detailKeyLabel->SetLabel("Select a conflict above to resolve it.");
    m_excludeCheck->SetValue(false);
    m_excludeCheck->Enable(false);
    m_radioSizer->Clear(true); // destroys the previous conflict's radio buttons too
    m_radioButtons.clear();
    m_radioPanel->Layout();
}

void ConflictTableDialog::showDetailFor(int listRow)
{
    if (listRow < 0 || static_cast<size_t>(listRow) >= m_conflictRowToKeyIndex.size()) {
        clearDetail();
        return;
    }

    const size_t keyIdx = m_conflictRowToKeyIndex[static_cast<size_t>(listRow)];
    const auto& swapKey = m_keys[keyIdx];

    m_detailKeyLabel->SetLabel(wxString(swapKey.section) + "  " + wxString(swapKey.key));

    m_excludeCheck->Enable(true);
    m_excludeCheck->SetValue(swapKey.excluded);

    m_radioSizer->Clear(true);
    m_radioButtons.clear();

    for (size_t i = 0; i < swapKey.candidates.size(); ++i) {
        const auto& candidate = swapKey.candidates[i];
        const wxString label = wxString(candidate.sourceFile) + ":  " + wxString(candidate.line);
        auto* radio = new wxRadioButton(m_radioPanel, wxID_ANY, label, wxDefaultPosition, wxDefaultSize,
                                         i == 0 ? wxRB_GROUP : 0);
        radio->SetValue(static_cast<int>(i) == swapKey.selectedCandidate);
        radio->Enable(!swapKey.excluded);
        radio->Bind(wxEVT_RADIOBUTTON, &ConflictTableDialog::onWinnerChosen, this);
        m_radioSizer->Add(radio, 0, wxALL, 3);
        m_radioButtons.push_back(radio);
    }

    m_radioPanel->Layout();
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
    const long row = m_listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0 || static_cast<size_t>(row) >= m_conflictRowToKeyIndex.size()) {
        return;
    }

    const size_t keyIdx = m_conflictRowToKeyIndex[static_cast<size_t>(row)];
    m_keys[keyIdx].excluded = m_excludeCheck->GetValue();

    for (auto* radio : m_radioButtons) {
        radio->Enable(!m_keys[keyIdx].excluded);
    }

    const wxString winner = m_keys[keyIdx].excluded
        ? wxString("(excluded)")
        : wxString(m_keys[keyIdx].candidates[static_cast<size_t>(m_keys[keyIdx].selectedCandidate)].sourceFile);
    m_listCtrl->SetItem(row, 3, winner);
}

void ConflictTableDialog::onWinnerChosen(wxCommandEvent& /*event*/)
{
    const long row = m_listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0 || static_cast<size_t>(row) >= m_conflictRowToKeyIndex.size()) {
        return;
    }

    const size_t keyIdx = m_conflictRowToKeyIndex[static_cast<size_t>(row)];

    for (size_t i = 0; i < m_radioButtons.size(); ++i) {
        if (m_radioButtons[i]->GetValue()) {
            m_keys[keyIdx].selectedCandidate = static_cast<int>(i);
            break;
        }
    }

    const auto& swapKey = m_keys[keyIdx];
    const wxString winner = swapKey.candidates[static_cast<size_t>(swapKey.selectedCandidate)].sourceFile;
    m_listCtrl->SetItem(row, 3, winner);
}

auto ConflictTableDialog::getKeys() const -> const vector<SwapKey>&
{
    return m_keys;
}
