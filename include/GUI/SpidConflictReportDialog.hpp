#pragma once

#include "SpidDistrMerger.hpp"

#include <vector>
#include <wx/listctrl.h>
#include <wx/wx.h>

/// Read-only report of detected SPID Outfit/SleepOutfit/Skin conflicts (see SpidDistrMerger's doc
/// comment for exactly what counts as a conflict here, and its known limits). Deliberately no
/// winner-picking, no exclude, no Generate button yet - this first slice only needs to prove the
/// detection is useful against real data before any of that gets built.
class SpidConflictReportDialog : public wxDialog {
public:
    /// groups: the full SpidDistrMerger::scan() result - only isRealConflict() ones are shown.
    SpidConflictReportDialog(wxWindow* parent, std::vector<SpidConflictGroup> groups);

private:
    std::vector<SpidConflictGroup> m_groups;
};
