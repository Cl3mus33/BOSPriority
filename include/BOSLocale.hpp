#pragma once

#include <wx/string.h>

#include <filesystem>
#include <string>
#include <vector>

/**
 * GUI localization system backed by JSON translation files.
 *
 * Translation files live in the "BOSPriority_translations" folder next to the executable, one
 * file per language named by its IETF/ISO language code (e.g. "en.json", "fr.json"). The schema
 * is i18next-style nested JSON: nested objects whose leaf string values are addressed by
 * dot-separated keys, e.g. {"launcher": {"title": "..."}} is looked up as "launcher.title". A
 * reserved top-level "_language" key holds the language's native display name shown in the
 * language selector.
 *
 * Every lookup carries a hardcoded English default which is returned when the key is missing
 * from the active translation (or when no translation file is loaded at all), so English is the
 * built-in fallback language. Ported from AutoSeasons' ASLocale
 * (github.com/Cl3mus33/AutoSeasons, GPLv3) - same design, renamed for this project.
 */
class BOSLocale {
public:
    struct Language {
        std::string code; ///< Language code, also the translation filename stem (e.g. "en")
        wxString displayName; ///< Native display name (e.g. "English", "Français")
    };

    /// Initializes the locale system and loads the translation for the given language code.
    /// translationsDir: folder containing the translation JSON files.
    /// langCode: language code to load (file <langCode>.json); missing files fall back to
    /// hardcoded English.
    static void init(const std::filesystem::path& translationsDir, const std::string& langCode);

    /// Looks up a translated string by key. key: dot-separated translation key (e.g.
    /// "launcher.title"). defaultValue: hardcoded English fallback used when the key is missing
    /// from the active translation.
    [[nodiscard]] static auto tr(const std::string& key, const char* defaultValue) -> wxString;

    /// Gets the language code that is currently active.
    [[nodiscard]] static auto getCurrentLanguage() -> std::string;

    /// Lists the languages available in the translations folder (sorted by display name).
    [[nodiscard]] static auto getAvailableLanguages() -> std::vector<Language>;
};

/// Shorthand for BOSLocale::tr.
[[nodiscard]] inline auto BOSTr(const std::string& key, const char* defaultValue) -> wxString
{
    return BOSLocale::tr(key, defaultValue);
}
