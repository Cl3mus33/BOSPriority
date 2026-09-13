#pragma once

#include <filesystem>
#include <string>
#include <vector>

/// One line from a *_DISTR.ini: FormType = FormOrEditorID|StringFilters|FormFilters|LevelFilters|
/// TraitFilters|CountOrIndex|Chance (SPID's own documented syntax, "SPID: The Complete
/// Reference"). Only the fields needed to detect an Outfit/SleepOutfit/Skin conflict are kept -
/// this tool doesn't need to fully understand every SPID field for that.
struct SpidEntry {
    std::string sourceFile; // filename only, for display
    std::string line; // full raw ini line, for display
    std::string formOrEditorId;
    std::string stringFilters;
    std::string formFilters;
    std::string levelFilters;
    std::string traitFilters;
    /// Numeric chance value (0-100, default 100 if absent/NONE). A trailing "!" (deterministic
    /// per-actor/save chance) is stripped before parsing - the numeric value is the same either
    /// way, only which RNG seed SPID uses at runtime differs, irrelevant here.
    float chance = 100.0F;
};

/// Every SpidEntry sharing the same record type AND the same (normalized) filter fields - i.e.
/// entries that target exactly the same NPC audience. Grouping key mirrors BOSPriority's own
/// "same key" rule for BOS, just built from SPID's richer filter set instead of a single key
/// field - see SpidDistrMerger::scan()'s doc comment for why exact-match is used instead of a
/// full filter-overlap simulation.
struct SpidConflictGroup {
    std::string recordType; // "Outfit" | "SleepOutfit" | "Skin"
    std::vector<SpidEntry> candidates;

    /// True only for an actual cross-mod overlap: 2+ candidates from more than one distinct
    /// source file. A record type/filter signature defined only once, or repeated within a single
    /// file, isn't something to resolve - mirrors SwapKey::isRealConflict()'s reasoning for BOS.
    [[nodiscard]] auto isRealConflict() const -> bool;
};

/**
 * Detects Outfit/SleepOutfit/Skin distribution conflicts across Spell Perk Item Distributor
 * (SPID) *_DISTR.ini files - the SPID analogue of what BOSIniMerger does for BOS.
 *
 * SPID discovers *_DISTR.ini directly under Data\, sorts them alphabetically, and loads each one
 * top to bottom (verified against SPID's own "SPID: The Complete Reference" documentation and its
 * real source, powerof3/Spell-Perk-Item-Distributor on GitHub). For Outfit/SleepOutfit/Skin -
 * "single slot" properties a given NPC can only have one of - a rule processed later can silently
 * overwrite an already-successful chance roll from a rule processed earlier
 * (SPID/src/Outfits/OutfitManager+Resolution.cpp's ResolvePendingOutfit, confirmed directly in
 * SPID's own source: a later non-final outfit always overwrites a pending one). Every other
 * distributable type (Spell, Perk, Item, Shout, Keyword, Faction, Package, ...) is purely
 * additive and has no such conflict, so scan() doesn't even parse those lines.
 *
 * Read-only for now: scan() only detects and reports candidate conflicts. It does not let the
 * user pick a winner, does not compute the chance rebalancing the conflict actually calls for,
 * and does not generate or edit any file - see the project plan for why this is deliberately a
 * first, narrower slice.
 */
class SpidDistrMerger {
public:
    /// Two entries are grouped together (and therefore flagged as a real conflict, if from
    /// different files) only when their record type AND their four filter fields
    /// (String/Form/Level/Trait) are identical after normalizing away whitespace and case - i.e.
    /// they provably target the exact same NPC audience. This deliberately does NOT attempt to
    /// prove that two DIFFERENT filter expressions could still match overlapping NPCs (e.g. one
    /// filtering by Race, another by a Keyword that Race happens to carry) - that would need a
    /// full filter-evaluation engine against the real NPC database (in the spirit of what
    /// AutoSeasons uses Mutagen for), out of scope for this slice. A known, accepted limitation,
    /// not a bug: this still catches the exact case reported in the field (the same literal
    /// filter, e.g. "ActorTypeNPC", repeated across independent mods).
    [[nodiscard]] static auto scan(const std::filesystem::path& gameDir) -> std::vector<SpidConflictGroup>;
};
