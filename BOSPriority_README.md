# BOSPriority

A standalone tool for Skyrim Special Edition that lets you set an explicit priority order for
[Base Object Swapper](https://www.nexusmods.com/skyrimspecialedition/mods/60805) (BOS)
`*_SWAP.ini` files, instead of relying on BOS's own alphabetical-filename rule.

## What it does

BOS merges every `Data\SKSE\Plugins\BaseObjectSwapper\*_SWAP.ini` in your load order itself, at
runtime - but when two mods disagree about the same swap, the winner is whichever filename sorts
last alphabetically. That's rarely what you actually want.

Point BOSPriority at your MO2 instance (or Vortex staging folder, or a plain Data folder) and it
will:

- Scan your active mods for anything shipping a `SKSE\Plugins\BaseObjectSwapper\*_SWAP.ini`.
- Let you reorder those mods explicitly (same idea as
  [AutoSeasons](https://github.com/Cl3mus33/AutoSeasons)'s "Manage Season Mod Conflicts" screen,
  applied to BOS instead of `Data/Seasons`).
- Merge every matching ini into one consolidated `AIO_SWAP.ini`, resolving conflicting entries by
  your chosen order instead of raw alphabetical filename order.
- Remember your priority choices (saved as JSON next to the generated output) so re-running after
  a load-order change doesn't ask again unless something new shows up.
- Run fully offline against your files - it never touches the running game.

BOSPriority only manages **existing** BOS ini files. It does not generate BOS entries from ESPs -
that's a separate tool (an xEdit script, `BOS AIO Patcher.pas`).

## Installation

Install like any other mod: MO2 as a regular mod (add it to the executables list so it launches
through MO2's virtual file system), Vortex by extracting into a mod folder and launching from its
dashboard.

## ⚠️ Run it through your mod manager, not by double-clicking the exe

If you double-click `BOSPriority.exe` directly from Explorer, it only sees your game's real,
unmodified Data folder - not your installed mods. MO2's virtual file system (USVFS) only applies
to processes MO2 itself launches, same caveat as any other tool in this family (xEdit,
AutoSeasons, etc).

## Usage

1. **Game/Instance Location**: your MO2 instance folder (contains `modorganizer.ini`), your
   Vortex staging folder (contains `vortex.deployment.json`), or a plain merged Data folder.
2. **Output Location**: a folder BOSPriority writes `AIO_SWAP.ini` into - make this its own mod
   entry in your manager (e.g. "BOSPriority Output"), placed last in your load order so nothing
   else overrides it.
3. Click **Scan Mods**.
4. If more than one mod shipped a BOS ini, **Manage Priority...** opens a reorderable list - top
   is applied first (loses conflicts), bottom is applied last (wins).
5. **Preview only (dry run)** logs what would be merged without writing anything.
6. Click **Generate**.
7. Enable the output mod in your manager, placed after every mod that shipped a BOS ini it
   depends on.

### Command-line / automation

```
BOSPriority.exe <game-dir> [output] --mod-priority "Mod A,Mod B,Mod C" [--dry-run] [-v|-vv]
```

`--mod-priority` takes mod names, comma-separated, lowest priority first (later in the list wins
conflicting keys). Mods not named keep their native mod-manager order, below every named mod.

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

- `ModManager` is ported and trimmed from `PGModManager` in
  [AutoSeasons](https://github.com/Cl3mus33/AutoSeasons) by Cl3mus33 (GPLv3), itself derived from
  [PGPatcher](https://github.com/hakasapl/PGPatcher) by hakasapl (GPLv3) - MO2/Vortex detection,
  `modorganizer.ini` parsing, and mod-priority tracking all trace back to that code.
- [Base Object Swapper](https://www.nexusmods.com/skyrimspecialedition/mods/60805) by fenix31415 /
  powerof3, the SKSE plugin this tool manages ini priority for.
- [CLI11](https://github.com/CLIUtils/CLI11), [spdlog](https://github.com/gabime/spdlog),
  [nlohmann/json](https://github.com/nlohmann/json), [wxWidgets](https://www.wxwidgets.org/).

## License

GPLv3 - see [BOSPriority_LICENSE.txt](BOSPriority_LICENSE.txt). BOSPriority reuses adapted code from AutoSeasons/PGPatcher, both
also GPLv3-licensed.
