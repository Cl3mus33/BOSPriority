#include "GUI/TypePriorityDialog.hpp"
#include "BOSLocale.hpp"
#include "GUI/ConflictTypeLabels.hpp"

#include <algorithm>

using namespace std;

namespace {
constexpr int ID_MOVE_UP = wxID_HIGHEST + 30;
constexpr int ID_MOVE_DOWN = wxID_HIGHEST + 31;

// Fills `order` with `saved` (any file no longer in allFiles dropped), then appends whatever's in
// allFiles that isn't in there yet - so a type customized before a rescan still shows every
// currently-relevant file, newly-relevant ones included, rather than silently hiding them.
auto completeOrder(const vector<string>& saved, const vector<string>& allFiles) -> vector<string>
{
    vector<string> order;
    for (const auto& file : saved) {
        if (ranges::find(allFiles, file) != allFiles.end() && ranges::find(order, file) == order.end()) {
            order.push_back(file);
        }
    }
    for (const auto& file : allFiles) {
        if (ranges::find(order, file) == order.end()) {
            order.push_back(file);
        }
    }
    return order;
}
} // namespace

TypePriorityDialog::TypePriorityDialog(wxWindow* parent, vector<string> allFiles, vector<string> types,
    map<string, vector<string>> initialPriorities)
    : wxDialog(parent, wxID_ANY, BOSTr("typePriority.title", "Set Priority by Type"), wxDefaultPosition,
               wxSize(560, 480), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    const auto savedAllTypes = initialPriorities.find(CONFLICT_ALL_TYPES_KEY);
    m_priorities[CONFLICT_ALL_TYPES_KEY] = savedAllTypes != initialPriorities.end()
        ? completeOrder(savedAllTypes->second, allFiles)
        : std::move(allFiles);
    m_currentType = CONFLICT_ALL_TYPES_KEY;

    for (const auto& [type, order] : initialPriorities) {
        if (type == CONFLICT_ALL_TYPES_KEY) {
            continue; // already seeded above
        }
        m_priorities[type] = completeOrder(order, m_priorities[CONFLICT_ALL_TYPES_KEY]);
    }

    auto* topSizer = new wxBoxSizer(wxVERTICAL);

    auto* introText = new wxStaticText(this, wxID_ANY,
        wxString::Format(BOSTr("typePriority.intro",
                             "Rank every source file that appears in a real conflict, most trusted at the "
                             "top. Applying this resolves every conflict at once: for each one, the "
                             "highest-ranked file that actually has a line for it wins. Pick a type below "
                             "to give just that type a different order - anything you don't customise "
                             "falls back to \"%s\". Your ranking is remembered for this output folder and "
                             "reapplied automatically on your next scan."),
            conflictDisplayTypeLabel(CONFLICT_ALL_TYPES_KEY)));
    introText->Wrap(520);
    topSizer->Add(introText, 0, wxALL, 10);

    auto* typeSizer = new wxBoxSizer(wxHORIZONTAL);
    typeSizer->Add(new wxStaticText(this, wxID_ANY, BOSTr("typePriority.typeLabel", "Type:")), 0,
                   wxALIGN_CENTER_VERTICAL | wxALL, 5);
    m_typeChoice = new wxChoice(this, wxID_ANY);
    m_typeChoice->Append(conflictDisplayTypeLabel(CONFLICT_ALL_TYPES_KEY));
    m_typeChoiceRawValues.emplace_back(CONFLICT_ALL_TYPES_KEY);
    for (const auto& type : types) {
        if (type != CONFLICT_ALL_TYPES_KEY) {
            m_typeChoice->Append(conflictDisplayTypeLabel(type));
            m_typeChoiceRawValues.push_back(type);
        }
    }
    m_typeChoice->SetSelection(0);
    typeSizer->Add(m_typeChoice, 0, wxALL, 5);
    topSizer->Add(typeSizer, 0);

    auto* listSizer = new wxBoxSizer(wxHORIZONTAL);
    m_orderList = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SINGLE);
    listSizer->Add(m_orderList, 1, wxALL | wxEXPAND, 10);

    auto* buttonSizer = new wxBoxSizer(wxVERTICAL);
    auto* upButton = new wxButton(this, ID_MOVE_UP, BOSTr("typePriority.moveUp", "Move Up"));
    auto* downButton = new wxButton(this, ID_MOVE_DOWN, BOSTr("typePriority.moveDown", "Move Down"));
    buttonSizer->Add(upButton, 0, wxALL, 5);
    buttonSizer->Add(downButton, 0, wxALL, 5);
    listSizer->Add(buttonSizer, 0, wxALIGN_CENTER_VERTICAL);

    topSizer->Add(listSizer, 1, wxEXPAND);

    auto* dialogButtons = CreateButtonSizer(wxOK | wxCANCEL);
    if (dialogButtons != nullptr) {
        topSizer->Add(dialogButtons, 0, wxALL | wxALIGN_RIGHT, 10);
    }

    SetSizer(topSizer);

    m_typeChoice->Bind(wxEVT_CHOICE, &TypePriorityDialog::onTypeChanged, this);
    upButton->Bind(wxEVT_BUTTON, &TypePriorityDialog::onMoveUp, this);
    downButton->Bind(wxEVT_BUTTON, &TypePriorityDialog::onMoveDown, this);

    refreshListBox();
}

void TypePriorityDialog::refreshListBox()
{
    m_orderList->Clear();
    for (const auto& file : m_priorities[m_currentType]) {
        m_orderList->Append(file);
    }
}

void TypePriorityDialog::saveCurrentListToPriorities()
{
    vector<string> order;
    for (unsigned i = 0; i < m_orderList->GetCount(); ++i) {
        order.push_back(m_orderList->GetString(i).ToStdString());
    }
    m_priorities[m_currentType] = std::move(order);
}

void TypePriorityDialog::switchToType(const string& type)
{
    saveCurrentListToPriorities();
    m_currentType = type;
    if (!m_priorities.contains(type)) {
        // Not customized yet - start from the current "All Types" order.
        m_priorities[type] = m_priorities[CONFLICT_ALL_TYPES_KEY];
    }
    refreshListBox();
}

void TypePriorityDialog::onTypeChanged(wxCommandEvent& /*event*/)
{
    const int selection = m_typeChoice->GetSelection();
    if (selection == wxNOT_FOUND || static_cast<size_t>(selection) >= m_typeChoiceRawValues.size()) {
        return;
    }
    switchToType(m_typeChoiceRawValues[static_cast<size_t>(selection)]);
}

void TypePriorityDialog::moveSelected(int delta)
{
    const int selection = m_orderList->GetSelection();
    if (selection == wxNOT_FOUND) {
        return;
    }
    const int target = selection + delta;
    if (target < 0 || target >= static_cast<int>(m_orderList->GetCount())) {
        return;
    }

    saveCurrentListToPriorities();
    auto& order = m_priorities[m_currentType];
    std::swap(order[static_cast<size_t>(selection)], order[static_cast<size_t>(target)]);
    refreshListBox();
    m_orderList->SetSelection(target);
}

void TypePriorityDialog::onMoveUp(wxCommandEvent& /*event*/)
{
    moveSelected(-1);
}

void TypePriorityDialog::onMoveDown(wxCommandEvent& /*event*/)
{
    moveSelected(1);
}

auto TypePriorityDialog::getPriorities() const -> const map<string, vector<string>>&
{
    return m_priorities;
}
