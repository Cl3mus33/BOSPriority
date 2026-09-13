#include "GUI/SpidConflictReportDialog.hpp"

#include <algorithm>

using namespace std;

namespace {
// A group's 4 filter fields, joined for display - truncated since real filters can be long and
// this is a report column, not an editable field.
auto filterSummary(const SpidConflictGroup& group) -> wxString
{
    if (group.candidates.empty()) {
        return wxString();
    }
    const auto& first = group.candidates.front();
    wxString parts;
    bool any = false;
    for (const auto& field : {first.stringFilters, first.formFilters, first.levelFilters, first.traitFilters}) {
        if (field.empty()) {
            continue;
        }
        if (any) {
            parts += " | ";
        }
        parts += field;
        any = true;
    }
    if (!any) {
        parts = "(no filters - matches every NPC)";
    }
    if (parts.length() > 100) {
        parts = parts.Left(97) + "...";
    }
    return parts;
}

auto filesAndChances(const SpidConflictGroup& group) -> wxString
{
    wxString result;
    for (size_t i = 0; i < group.candidates.size(); ++i) {
        if (i > 0) {
            result += ",  ";
        }
        result += wxString::Format("%s (%.0f%%)", wxString(group.candidates[i].sourceFile), group.candidates[i].chance);
    }
    return result;
}
} // namespace

SpidConflictReportDialog::SpidConflictReportDialog(wxWindow* parent, vector<SpidConflictGroup> groups)
    : wxDialog(parent, wxID_ANY, "SPID Conflicts (beta)", wxDefaultPosition, wxSize(900, 560),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_groups(std::move(groups))
{
    auto* topSizer = new wxBoxSizer(wxVERTICAL);

    auto* introText = new wxStaticText(this, wxID_ANY,
        "Read-only report: candidate Outfit/SleepOutfit/Skin distribution conflicts across your "
        "*_DISTR.ini files. Two entries are only flagged here when their filters are EXACTLY the "
        "same (same NPC audience) - two different filters that could still overlap in practice "
        "(e.g. a Race filter and a Keyword that Race happens to carry) are NOT detected by this "
        "version. Each candidate's raw chance is shown as written in its ini - because SPID "
        "processes *_DISTR.ini files alphabetically and a later file's Outfit/SleepOutfit/Skin "
        "roll overwrites an earlier one's, the percentage actually observed in-game for the "
        "earlier file is lower than what's written here whenever a later one also matches. Nothing "
        "on this screen is editable yet.");
    introText->Wrap(860);
    topSizer->Add(introText, 0, wxALL, 10);

    auto* listCtrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    listCtrl->InsertColumn(0, "Type", wxLIST_FORMAT_LEFT, 80);
    listCtrl->InsertColumn(1, "Filters (same = same NPC audience)", wxLIST_FORMAT_LEFT, 340);
    listCtrl->InsertColumn(2, "Candidates (file, raw chance)", wxLIST_FORMAT_LEFT, 380);
    topSizer->Add(listCtrl, 1, wxALL | wxEXPAND, 10);

    long row = 0;
    for (const auto& group : m_groups) {
        if (!group.isRealConflict()) {
            continue;
        }
        const long index = listCtrl->InsertItem(row, group.recordType);
        listCtrl->SetItem(index, 1, filterSummary(group));
        listCtrl->SetItem(index, 2, filesAndChances(group));
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
