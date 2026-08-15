# Changelog

## v1.0.0

Initial release.

BOSPriority is a standalone tool for Skyrim Special Edition that lets you set an explicit
priority order for [Base Object Swapper](https://www.nexusmods.com/skyrimspecialedition/mods/60805)
(BOS) `*_SWAP.ini` files, instead of relying on BOS's own alphabetical-filename tie-break rule.

- Scans every `*_SWAP.ini` directly under your game's `Data\` folder and detects real conflicts
  (keys where two or more files disagree), correctly ignoring BOS's chance-weighted multi-entry
  keys and exact-duplicate lines, which are not conflicts.
- Shows only actual conflicts in a table, filterable by resolved record type (Static,
  MovableStatic, Tree, Flora, ...), read directly from the referenced plugin.
- Lets you pick a winner per conflict, or exclude a key entirely.
- Merges everything into one consolidated `AIO_SWAP.ini`, with a completeness check before
  writing anything, then replaces every original source `*_SWAP.ini` with an emptied stand-in in
  the output folder so the consolidated file is the only one BOS ends up reading, once the output
  mod loads last.
- Remembers your decisions and your Game/Output locations between runs.
- Options tab: UI language (English, Deutsch, Español, Français, Italiano, Português (Brasil))
  and theme (System, Light, Dark).
- Also usable from the command line for automation.
- Runs fully offline against your mod files - it never touches the running game.
