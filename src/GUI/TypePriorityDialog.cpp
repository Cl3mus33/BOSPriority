#include "GUI/TypePriorityDialog.hpp"

#include <algorithm>

using namespace std;

namespace {
constexpr int ID_MOVE_UP = wxID_HIGHEST + 30;
constexpr int ID_MOVE_DOWN = wxID_HIGHEST + 31;
const wxString ALL_TYPES = "All Types";
} // namespace

TypePriorityDialog::TypePriorityDialog(wxWindow* parent, vector<string> allFiles, vector<wxString> types)
    : wxDialog(parent, wxID_ANY, "Set Priority by Type", wxDefaultPosition, wxSize(560, 480),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    m_priorities[ALL_TYPES] = std::move(allFiles);
    m_currentType = ALL_TYPES;

    auto* topSizer = new wxBoxSizer(wxVERTICAL);

    auto* introText = new wxStaticText(this, wxID_ANY,
        "Rank every source file that appears in a real conflict. Applying this resolves every "
        "conflict at once: whichever file is highest here that actually has a candidate for a "
        "given conflict wins it. Pick a type below to set a different order just for that type - "
        "otherwise \"All Types\" is used as the fallback for it.");
    introText->Wrap(520);
    topSizer->Add(introText, 0, wxALL, 10);

    auto* typeSizer = new wxBoxSizer(wxHORIZONTAL);
    typeSizer->Add(new wxStaticText(this, wxID_ANY, "Type:"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    m_typeChoice = new wxChoice(this, wxID_ANY);
    m_typeChoice->Append(ALL_TYPES);
    for (const auto& type : types) {
        if (type != ALL_TYPES) {
            m_typeChoice->Append(type);
        }
    }
    m_typeChoice->SetSelection(0);
    typeSizer->Add(m_typeChoice, 0, wxALL, 5);
    topSizer->Add(typeSizer, 0);

    auto* listSizer = new wxBoxSizer(wxHORIZONTAL);
    m_orderList = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SINGLE);
    listSizer->Add(m_orderList, 1, wxALL | wxEXPAND, 10);

    auto* buttonSizer = new wxBoxSizer(wxVERTICAL);
    auto* upButton = new wxButton(this, ID_MOVE_UP, "Move Up");
    auto* downButton = new wxButton(this, ID_MOVE_DOWN, "Move Down");
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

void TypePriorityDialog::switchToType(const wxString& type)
{
    saveCurrentListToPriorities();
    m_currentType = type;
    if (!m_priorities.contains(type)) {
        // Not customized yet - start from the current "All Types" order.
        m_priorities[type] = m_priorities[ALL_TYPES];
    }
    refreshListBox();
}

void TypePriorityDialog::onTypeChanged(wxCommandEvent& /*event*/)
{
    switchToType(m_typeChoice->GetStringSelection());
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

auto TypePriorityDialog::getPriorities() const -> const map<wxString, vector<string>>&
{
    return m_priorities;
}
