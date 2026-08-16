#include "GUI/TypePriorityDialog.hpp"
#include "BOSLocale.hpp"

#include <algorithm>

using namespace std;

namespace {
constexpr int ID_MOVE_UP = wxID_HIGHEST + 30;
constexpr int ID_MOVE_DOWN = wxID_HIGHEST + 31;

// Must stay byte-identical to ConflictTableDialog's own allTypesLabel(): it's both the displayed
// entry and the map key that dialog looks the fallback ranking up by. Looked up per call rather
// than stored in a static so an in-process language change is picked up (see main.cpp's loop).
auto allTypesLabel() -> wxString
{
    return BOSTr("conflicts.allTypes", "All Types");
}
} // namespace

TypePriorityDialog::TypePriorityDialog(wxWindow* parent, vector<string> allFiles, vector<wxString> types)
    : wxDialog(parent, wxID_ANY, BOSTr("typePriority.title", "Set Priority by Type"), wxDefaultPosition,
               wxSize(560, 480), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    m_priorities[allTypesLabel()] = std::move(allFiles);
    m_currentType = allTypesLabel();

    auto* topSizer = new wxBoxSizer(wxVERTICAL);

    auto* introText = new wxStaticText(this, wxID_ANY,
        wxString::Format(BOSTr("typePriority.intro",
                             "Rank every source file that appears in a real conflict, most trusted at the "
                             "top. Applying this resolves every conflict at once: for each one, the "
                             "highest-ranked file that actually has a line for it wins. Pick a type below "
                             "to give just that type a different order - anything you don't customise "
                             "falls back to \"%s\"."),
            allTypesLabel()));
    introText->Wrap(520);
    topSizer->Add(introText, 0, wxALL, 10);

    auto* typeSizer = new wxBoxSizer(wxHORIZONTAL);
    typeSizer->Add(new wxStaticText(this, wxID_ANY, BOSTr("typePriority.typeLabel", "Type:")), 0,
                   wxALIGN_CENTER_VERTICAL | wxALL, 5);
    m_typeChoice = new wxChoice(this, wxID_ANY);
    m_typeChoice->Append(allTypesLabel());
    for (const auto& type : types) {
        if (type != allTypesLabel()) {
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

void TypePriorityDialog::switchToType(const wxString& type)
{
    saveCurrentListToPriorities();
    m_currentType = type;
    if (!m_priorities.contains(type)) {
        // Not customized yet - start from the current "All Types" order.
        m_priorities[type] = m_priorities[allTypesLabel()];
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
