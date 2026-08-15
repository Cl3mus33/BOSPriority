#include "GUI/PriorityListDialog.hpp"

using namespace std;

namespace {
constexpr int ID_MOVE_UP = wxID_HIGHEST + 1;
constexpr int ID_MOVE_DOWN = wxID_HIGHEST + 2;
} // namespace

PriorityListDialog::PriorityListDialog(wxWindow* parent, vector<std::filesystem::path> filesInApplyOrder)
    : wxDialog(parent, wxID_ANY, "Manage BOS Ini Priority", wxDefaultPosition, wxSize(480, 420),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_files(std::move(filesInApplyOrder))
{
    auto* topSizer = new wxBoxSizer(wxVERTICAL);

    auto* label = new wxStaticText(this, wxID_ANY,
        "Top = applied first (loses conflicts). Bottom = applied last (wins).\n"
        "This overrides BOS's own alphabetical-filename rule.");
    topSizer->Add(label, 0, wxALL | wxEXPAND, 10);

    auto* listSizer = new wxBoxSizer(wxHORIZONTAL);

    m_listBox = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr,
                               wxLB_SINGLE | wxLB_NEEDED_SB);
    listSizer->Add(m_listBox, 1, wxALL | wxEXPAND, 10);

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

    Bind(wxEVT_BUTTON, &PriorityListDialog::onMoveUp, this, ID_MOVE_UP);
    Bind(wxEVT_BUTTON, &PriorityListDialog::onMoveDown, this, ID_MOVE_DOWN);

    refreshListBox();
}

void PriorityListDialog::refreshListBox()
{
    const auto prevSelection = m_listBox->GetSelection();

    m_listBox->Clear();
    for (const auto& file : m_files) {
        m_listBox->Append(wxString(file.filename().wstring()));
    }

    if (prevSelection != wxNOT_FOUND && prevSelection < static_cast<int>(m_files.size())) {
        m_listBox->SetSelection(prevSelection);
    }
}

void PriorityListDialog::onMoveUp(wxCommandEvent& /*event*/)
{
    const auto sel = m_listBox->GetSelection();
    if (sel == wxNOT_FOUND || sel == 0) {
        return;
    }

    std::swap(m_files[static_cast<size_t>(sel)], m_files[static_cast<size_t>(sel) - 1]);
    refreshListBox();
    m_listBox->SetSelection(sel - 1);
}

void PriorityListDialog::onMoveDown(wxCommandEvent& /*event*/)
{
    const auto sel = m_listBox->GetSelection();
    if (sel == wxNOT_FOUND || sel + 1 >= static_cast<int>(m_files.size())) {
        return;
    }

    std::swap(m_files[static_cast<size_t>(sel)], m_files[static_cast<size_t>(sel) + 1]);
    refreshListBox();
    m_listBox->SetSelection(sel + 1);
}

auto PriorityListDialog::getOrderedFiles() const -> const vector<std::filesystem::path>&
{
    return m_files;
}
