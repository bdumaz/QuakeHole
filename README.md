# QuakeArch

A live level-building prototype on the open-source Quake engine. You stand inside the level and shape it, then run, fly and shoot through what you built, with no compile step in between.

This is Stage 1 (Foundation): the engine boots into a room made of live-editable brushes, with full Quake movement and console commands to edit and export. Mouse-driven build mode comes in later stages.

## What you need

- **Your own copy of Quake.** QuakeArch ships none of id Software's game data. It reads `id1\pak0.pak` and `id1\pak1.pak` from your installation (Steam, GOG or an original CD). The full game is required; the shareware version lacks several weapon models.
- **Windows 10 or 11, 64-bit.**
- **Visual Studio 2019 or later** with the "Desktop development with C++" workload, to build the engine.

## Build and run

```
tools\build.bat      builds the engine
run.bat              starts QuakeArch
```

`run.bat` looks for Quake in the default Steam location, `C:\Program Files (x86)\Steam\steamapps\common\Quake`. If yours is elsewhere, create a file named `quakepath.txt` next to `run.bat` containing one line: the path to your Quake folder.

Arguments after `run.bat` are passed to the engine, for example `run.bat -window -width 1280 -height 720`.

## Controls

| Key | Action |
|---|---|
| W A S D | Move |
| Mouse | Look |
| Space | Jump (swim or fly up) |
| Ctrl | Fly down |
| Shift | Walk |
| Left mouse | Fire |
| 1 to 8, mouse wheel | Choose weapon |
| F | Toggle flying |
| ` | Console |
| F12 | Screenshot |

These defaults live in `game\arch\default.cfg`. Rebind keys in the Options menu; your changes are saved to `game\arch\config.cfg`.

You have every weapon, ammunition does not run out, and nothing damages you. Rocket jumps still push you.

## Editing the level

The world is solid everywhere, 4096 units across, with a 320-unit room carved out of its centre. One metre is 32 units. Open the console and use:

| Command | Effect |
|---|---|
| `arch_carve x1 y1 z1 x2 y2 z2` | Carves a box of empty space between two corners |
| `arch_add x1 y1 z1 x2 y2 z2` | Fills a box with solid |
| `arch_reset` | Restores the starting room |
| `arch_export name` | Writes `game\arch\maps\name.map` |
| `arch_stats` | Prints the brush count and solid volume |

Changes are visible and solid immediately. For example, `arch_carve 160 -48 -160 480 48 -32` opens a corridor in the wall you face when you start.

Exported `.map` files use the standard Quake format and open in editors such as TrenchBroom. To compile one into a `.bsp` you need a map compiler and a texture wad containing the three texture names used.

## Settings

| Variable | Default | Effect |
|---|---|---|
| `arch_selfdamage` | 0 | Set to 1 to take damage |
| `arch_falldamage` | 0 | Set to 1 for fall damage (also needs `arch_selfdamage 1`) |
| `arch_infiniteammo` | 1 | Set to 0 for limited ammunition |
| `arch_tex_ground`, `arch_tex_wall`, `arch_tex_ceiling` | `sfloor4_5`, `wbrick1_5`, `ceiling1_3` | Texture names, taken from `maps/start.bsp` in your pak. Applied at the next launch |

## Development

```
tools\test.bat               unit tests for the geometry code
tools\selftest.bat boot      scripted in-engine checks: boot, collide, render, rules
```

Selftests write `game\arch\qconsole.log` and screenshots into `game\arch`.

- `engine/` is QuakeSpasm 0.97.0 (see `engine/UPSTREAM.txt`). QuakeArch code is in `engine/Quake/arch_*.c`; changes to QuakeSpasm's own files are short hooks marked `// QUAKEARCH`.
- `docs/superpowers/specs/` holds the design.

## Licence

QuakeArch is free software under the GNU General Public License, version 2 or later, as is QuakeSpasm, which it is built on. See `engine/LICENSE.txt`. Quake is a trademark of id Software; this project is not affiliated with id Software or Bethesda.
