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
- Let you reorder those files explicitly (same idea as
  [AutoSeasons](https://github.com/Cl3mus33/AutoSeasons)'s "Manage Season Mod Conflicts" screen,
  applied to BOS files instead of `Data/Seasons` declarations).
- Merge every matching ini into one consolidated `AIO_SWAP.ini`, resolving conflicting entries by
  your chosen order instead of raw alphabetical filename order.
- Remember your priority choices (saved as JSON in the output folder) so re-running later doesn't
  ask again unless a new file shows up.
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
dashboard.

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
4. If more than one `*_SWAP.ini` was found, **Manage Priority...** opens a reorderable list - top
   is applied first (loses conflicts), bottom is applied last (wins).
5. **Preview only (dry run)** logs what would be merged without writing anything.
6. Click **Generate**.
7. Enable the output mod in your manager, placed after every mod that shipped a BOS ini it
   depends on.

### Command-line / automation

```
BOSPriority.exe <game-dir> [output] --file-priority "ModA_SWAP.ini,ModB_SWAP.ini" [--dry-run] [-v|-vv]
```

`game-dir` is the Game Location described above (not an MO2 instance folder). `--file-priority`
takes `*_SWAP.ini` filenames, comma-separated, lowest priority first (later in the list wins
conflicting keys). Files not named keep BOS's own alphabetical order, below every named file.

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
  system, rather than independently parsing MO2/Vortex instance data) mirrors
  [AutoSeasons](https://github.com/Cl3mus33/AutoSeasons) by Cl3mus33 (GPLv3), itself derived from
  [PGPatcher](https://github.com/hakasapl/PGPatcher) by hakasapl (GPLv3).
- [Base Object Swapper](https://www.nexusmods.com/skyrimspecialedition/mods/60805) by fenix31415 /
  powerof3, the SKSE plugin this tool manages ini priority for.
- [CLI11](https://github.com/CLIUtils/CLI11), [spdlog](https://github.com/gabime/spdlog),
  [nlohmann/json](https://github.com/nlohmann/json), [wxWidgets](https://www.wxwidgets.org/).

## License

GPLv3 - see [BOSPriority_LICENSE.txt](BOSPriority_LICENSE.txt). BOSPriority reuses ideas adapted
from AutoSeasons/PGPatcher, both also GPLv3-licensed.
