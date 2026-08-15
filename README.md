# BOSPriority

A standalone tool for Skyrim Special Edition that lets you set an explicit priority order for
[Base Object Swapper](https://www.nexusmods.com/skyrimspecialedition/mods/60805) (BOS)
`*_SWAP.ini` files, instead of relying on BOS's own alphabetical-filename rule.

## What it does

BOS merges every `Data\*_SWAP.ini` in your load order itself, at runtime - but when two files
disagree about the same swap, the winner is whichever filename sorts last alphabetically. That's
rarely what you actually want.

Point BOSPriority at your **Game Location** (the folder containing `Data\` - same thing AutoSeasons
asks for) and it will:

- Scan for every `*_SWAP.ini` directly under `Data\`.
- Show only the keys that actually **conflict** (2+ files disagree) in a table - same idea as
  [AutoSeasons](https://github.com/Cl3mus33/AutoSeasons)'s "Manage Season Mod Conflicts" screen,
  applied to BOS files instead of `Data/Seasons` declarations - filterable by the resolved record
  **type** (Static, Tree, MovableStatic, ...), read directly from the plugin the swap references.
  Everything else (no conflict) is included automatically; nothing to decide.
- Let you pick which file wins each conflict, or **exclude** a key entirely.
- Merge everything into one consolidated `AIO_SWAP.ini`, then replace every original source
  `*_SWAP.ini` with an emptied stand-in in the output folder - once that output mod loads after
  every source mod, AIO_SWAP.ini becomes the only one actually read. A completeness check runs
  first and aborts (writing nothing) if any key wouldn't make it into the output.
- Remember your decisions (saved as JSON in the output folder) so re-running later doesn't ask
  again unless something new shows up.
- Run fully offline against your files - it never touches the running game.

BOSPriority only manages **existing** BOS ini files. It does not generate BOS entries from ESPs -
that's a separate tool (an xEdit script, `BOS AIO Patcher.pas`).

### Why "Game Location" and not an MO2 instance folder

BOSPriority reads `Data\` exactly like the game itself would. When launched through MO2 or
Vortex's tool list, the OS-level virtual file system those tools set up (USVFS) transparently
shows the merged view of every installed mod at that same path - there is no need to separately
resolve where your MO2 instance or mods folders physically live, which breaks in the very common
setup where they're on different drives/paths. This mirrors how AutoSeasons and PGPatcher actually
work: they scan the real Data path and rely on the mod manager's virtual file system, rather than
independently parsing `modorganizer.ini`/`modlist.txt` to reconstruct a merged view themselves.

## Installation

Install like any other mod: MO2 as a regular mod (add it to the executables list so it launches
through MO2's virtual file system), Vortex by extracting into a mod folder and launching from its
dashboard. The release also ships an empty `BP.ini` at the mod's root, same convention as
AutoSeasons/PGPatcher - a plain file there so MO2 has something to recognize the folder by, since
it otherwise holds nothing but a loose exe (no `Data\`-mirroring structure).

## ⚠️ Run it through your mod manager, not by double-clicking the exe

If you double-click `BOSPriority.exe` directly from Explorer, it only sees your game's real,
unmodified Data folder - not your installed mods. USVFS only applies to processes MO2/Vortex
themselves launch, same caveat as any other tool in this family (xEdit, AutoSeasons, etc).

## Usage

1. **Game Location**: the folder containing `Data\` - your Skyrim install directory, or wherever
   your mod manager launches the game from. Not the MO2 instance folder.
2. **Output Location**: a folder BOSPriority writes `AIO_SWAP.ini` into - make this its own mod
   entry in your manager (e.g. "BOSPriority Output"), placed last in your load order so nothing
   else overrides it.
3. Click **Scan Mods**.
4. If any keys conflict, **Manage Conflicts...** opens a table - filter by type, select a
   conflict to see its candidate files, pick a winner or check **Exclude this key**. A key scoped
   to several BOS locations (e.g. the same object swapped differently per town) shows as one row -
   picking a winning file resolves every location it covers in one go. **Set Priority by Type...**
   lets you rank source files once (globally, or per record type) and apply that ranking to every
   conflict at once instead of resolving them one by one.
5. **Preview only (dry run)** logs what would be merged without writing anything.
6. Click **Generate**.
7. Enable the output mod in your manager, placed after every mod that shipped a BOS ini it
   depends on - its emptied stand-ins need to load after the originals to take effect.

### Keep your mod load order aligned with your BOS priority choices

BOSPriority only resolves disagreements between `_SWAP.ini` files - it deliberately doesn't check
whether the mod you pick as a winner is also "winning" everywhere else that matters (its plugin's
own records, its meshes, its textures). If mod X's swap target is itself overwritten somewhere
else in your load order - another plugin editing the same record, or another mod's loose files
replacing X's mesh/texture at the same path - the swap can end up using something other than what
you picked here, even though BOSPriority did exactly what you asked.

In practice this is rarely an issue: BOS rules almost always target base vanilla objects broadly,
not something another specific mod has directly overwritten. But to keep the two systems agreeing
instead of fighting each other, place the mods that participate in a BOS conflict in your mod
manager in roughly the same priority order you give them here - the mod you pick as the winner for
a swap should also be the one your manager lets win everywhere else.

### Options tab

- **Language**: English, Deutsch, Español, Français, Italiano, or Português (Brasil). Changing it
  rebuilds the window immediately (no restart) - translation files live in
  `BOSPriority_translations/` next to the exe, one JSON per language; a missing/incomplete
  translation always falls back to English for that string.
- **Theme**: System, Light, or Dark. Changing it **restarts BOSPriority** (a real Windows
  limitation, not a bug: once dark-mode control rendering is turned on for a process it can't be
  reliably turned back off in the same process, so a clean process is the only way to guarantee
  "Light" stays fully light). Note that Windows' own dark-mode support in wxWidgets is still
  experimental - the title bar reliably follows the theme, but not every individual control is
  guaranteed to repaint dark.

### Command-line / automation

```
BOSPriority.exe <game-dir> [output] --file-priority "ModA_SWAP.ini,ModB_SWAP.ini" [--dry-run] [-v|-vv]
```

`game-dir` is the Game Location described above (not an MO2 instance folder). Per-key winner/
exclude decisions are GUI-only (saved to `BOSPriority_decisions.json` in the output folder, same
file the CLI reads); `--file-priority` is a fallback used only for conflicts with no saved
decision - `*_SWAP.ini` filenames, comma-separated, lowest priority first.

## Building from source

Requirements:
- Windows, Visual Studio 2022 (MSVC toolchain) or the standalone Build Tools
- [vcpkg](https://github.com/microsoft/vcpkg) (manifest mode; dependencies are pulled automatically)
- CMake 3.31+

```bash
git clone https://github.com/<your-username>/BOSPriority.git
cd BOSPriority
cmake -B buildRelease -S . -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build buildRelease --config RelWithDebInfo
```

## Credits

- Architecture (scanning the real Data path and relying on the mod manager's own virtual file
  system, rather than independently parsing MO2/Vortex instance data; the Options tab's
  translation system and theme/restart handling) mirrors
  [AutoSeasons](https://github.com/Cl3mus33/AutoSeasons) by Cl3mus33 (GPLv3), itself derived from
  [PGPatcher](https://github.com/hakasapl/PGPatcher) by hakasapl (GPLv3).
- [Base Object Swapper](https://www.nexusmods.com/skyrimspecialedition/mods/60805) by fenix31415 /
  powerof3, the SKSE plugin this tool manages ini priority for.
- [CLI11](https://github.com/CLIUtils/CLI11), [spdlog](https://github.com/gabime/spdlog),
  [nlohmann/json](https://github.com/nlohmann/json), [wxWidgets](https://www.wxwidgets.org/).

## License

GPLv3 - see [LICENSE](LICENSE). BOSPriority reuses ideas adapted
from AutoSeasons/PGPatcher, both also GPLv3-licensed.
