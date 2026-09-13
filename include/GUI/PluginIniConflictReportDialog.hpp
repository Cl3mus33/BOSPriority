#pragma once

#include "PluginIniMerger.hpp"

#include <vector>
#include <wx/listctrl.h>
#include <wx/wx.h>

/// Read-only report of detected per-plugin ini conflicts (see PluginIniMerger's doc comment for
/// exactly what counts as a conflict here, and its known limits). Deliberately no winner-picking,
/// no exclude, no Generate button yet - same first-slice scope as SpidConflictReportDialog, and
/// like that dialog, its strings stay hardcoded English rather than going through BOSTr (an
/// existing inconsistency in SpidConflictReportDialog, reproduced here rather than fixed in passing).
class PluginIniConflictReportDialog : public wxDialog {
public:
    /// groups: the full PluginIniMerger::scan() result - only isRealConflict() ones are shown.
    PluginIniConflictReportDialog(wxWindow* parent, std::vector<PluginIniConflictGroup> groups);

private:
    std::vector<PluginIniConflictGroup> m_groups;
};
