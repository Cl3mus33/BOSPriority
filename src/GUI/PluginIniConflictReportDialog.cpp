#include "GUI/PluginIniConflictReportDialog.hpp"

#include <algorithm>

using namespace std;

namespace {

auto currentWinner(const PluginIniConflictGroup& group) -> wxString
{
    if (group.candidates.empty()) {
        return wxString();
    }
    const auto& winner = group.candidates.back();
    return wxString::Format("%s: %s", wxString(winner.sourcePlugin), wxString(winner.value));
}

auto allCandidates(const PluginIniConflictGroup& group) -> wxString
{
    wxString result;
    for (size_t i = 0; i < group.candidates.size(); ++i) {
        if (i > 0) {
            result += ",  ";
        }
        result += wxString::Format(
            "%s: %s", wxString(group.candidates[i].sourcePlugin), wxString(group.candidates[i].value));
    }
    return result;
}

} // namespace

PluginIniConflictReportDialog::PluginIniConflictReportDialog(wxWindow* parent, vector<PluginIniConflictGroup> groups)
    : wxDialog(parent, wxID_ANY, "Plugin INI Conflicts (beta)", wxDefaultPosition, wxSize(900, 560),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_groups(std::move(groups))
{
    auto* topSizer = new wxBoxSizer(wxVERTICAL);

    auto* introText = new wxStaticText(this, wxID_ANY,
        "Read-only report: candidate conflicts between active plugins' own <PluginName>.ini "
        "override files. Skyrim loads a plugin's <PluginName>.ini automatically only while that "
        "plugin is active, and when two active plugins set the same [Section]+key to different "
        "values, the plugin that loads LAST wins - same rule as ordinary record conflicts. This "
        "is based on community-documented engine behavior, not primary Bethesda documentation, "
        "and is only as accurate as the Load Order File you provided. Only shown when two active "
        "plugins set the exact same setting to DIFFERENT values - identical values aren't "
        "flagged, since nothing would actually be wrong in that case. Nothing on this screen is "
        "editable yet.");
    introText->Wrap(860);
    topSizer->Add(introText, 0, wxALL, 10);

    auto* listCtrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    listCtrl->InsertColumn(0, "Section", wxLIST_FORMAT_LEFT, 120);
    listCtrl->InsertColumn(1, "Key", wxLIST_FORMAT_LEFT, 150);
    listCtrl->InsertColumn(2, "Current winner (plugin: value)", wxLIST_FORMAT_LEFT, 220);
    listCtrl->InsertColumn(3, "All candidates (plugin: value)", wxLIST_FORMAT_LEFT, 280);
    topSizer->Add(listCtrl, 1, wxALL | wxEXPAND, 10);

    long row = 0;
    for (const auto& group : m_groups) {
        if (!group.isRealConflict()) {
            continue;
        }
        const long index = listCtrl->InsertItem(row, group.section);
        listCtrl->SetItem(index, 1, group.key);
        listCtrl->SetItem(index, 2, currentWinner(group));
        listCtrl->SetItem(index, 3, allCandidates(group));
        ++row;
    }

    if (row == 0) {
        auto* noneLabel = new wxStaticText(this, wxID_ANY, "No candidate conflicts detected.");
        topSizer->Add(noneLabel, 0, wxALL, 10);
    }

    auto* dialogButtons = CreateButtonSizer(wxOK);
    if (dialogButtons != nullptr) {
        topSizer->Add(dialogButtons, 0, wxALL | wxALIGN_RIGHT, 10);
    }

    SetSizer(topSizer);
    CentreOnParent();
}
