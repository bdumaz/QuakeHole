# QuakeArch

A live level-building prototype on the open-source Quake engine. You stand inside the level and shape it, then run, fly and shoot through what you built, with no compile step in between.

The engine boots into a room made of live-editable brushes, with full Quake movement and a build mode that carves holes, pulls out blocks, cuts arches and places jump pads with the mouse. Levels export as standard `.map` files.

## What you need

- **Your own copy of Quake.** QuakeArch ships none of id Software's game data. It reads `id1\pak0.pak` and `id1\pak1.pak` from your installation (Steam, GOG or an original CD). The full game is required; the shareware version lacks several weapon models.
- **Windows 10 or 11, 64-bit.**
- **Visual Studio 2019 or later** with the "Desktop development with C++" workload, to build the engine.

## Build and run

```
tools\build.bat      builds the engine
run.bat              starts QuakeArch
```

`run.bat` looks for your pak files in this order:

1. The folder named in `quakepath.txt`, a one-line text file you can create next to `run.bat`.
2. A folder named `Quake paks` next to `run.bat`. Copy `pak0.pak` and `pak1.pak` into it.
3. The default Steam location, `C:\Program Files (x86)\Steam\steamapps\common\Quake`.

A folder may hold the two paks directly or inside an `id1` subfolder. The `Quake paks` folder is ignored by git, so the paks are never committed.

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
| B | Toggle build mode |
| ` | Console |
| F12 | Screenshot |

These defaults live in `game\arch\default.cfg`. Rebind keys in the Options menu; your changes are saved to `game\arch\config.cfg`.

You have every weapon, ammunition does not run out, and nothing damages you. Rocket jumps still push you.

## Build mode

Press B. You stop where you are (even in mid-air), the weapon disappears, and a rectangle, the build cursor, appears on whatever surface you aim at. It snaps to a quarter-metre grid and slides clear of adjoining floors, walls and ceilings, so aiming level at a wall gives a doorway that sits on the floor. Press B again to go back to moving.

| Input | Effect |
|---|---|
| Mouse | Aim the cursor |
| Left click | Carve a hole the shape of the cursor, 5 m deep |
| Hold left and push the mouse forward | Carve a hole as deep as you push |
| Hold left and pull the mouse back | Pull a solid block out of the surface toward you |
| Mouse wheel | Resize the cursor |
| Mouse wheel with left held | Change the cursor's height only |
| Mouse wheel with left and right held | Change the cursor's width only |
| Hold middle mouse | Open the action wheel; move the mouse toward an action and release to select it |
| R | Snap: make the cursor fill the flat surface you are aiming at. R again, scrolling, or aiming elsewhere lets go |

### Actions

| Action | Click | Hold and drag |
|---|---|---|
| Extrude Quad | Carve a box 5 m deep | Carve (push) or pull out (pull) a box of any depth |
| Extrude Arch | Carve an arched tunnel 5 m deep | The same with an arch: a rectangle topped by a half-circle. On floors and ceilings the curved end points the way you face |
| Place Bumper | Place a jump pad the size of the cursor | Set its strength first: pull for stronger, push for weaker |

A jump pad launches you away from its surface: up from a floor, sideways off a wall. You can walk through it; it is not solid. A pad disappears when the surface under it is carved away or built over, and a new pad placed over an old one replaces it.

The cursor does not appear on slanted surfaces, such as the curved underside of an arch.

While you drag, the view locks and a coloured preview shows what will be carved (orange) or added (green). Release to commit; bring the depth back to zero to cancel. The line at the top of the screen shows the cursor size and drag depth in metres.

A block that would bury you is refused.

## Editing from the console

The world is solid everywhere, 4096 units across, with a 320-unit room carved out of its centre. The outermost 16 units cannot be carved. One metre is 32 units. Open the console and use:

| Command | Effect |
|---|---|
| `arch_carve x1 y1 z1 x2 y2 z2` | Carves a box of empty space between two corners |
| `arch_add x1 y1 z1 x2 y2 z2` | Fills a box with solid |
| `arch_reset` | Restores the starting room and removes all jump pads |
| `arch_action quad\|arch\|bumper` | Selects a build action without the wheel |
| `arch_export name` | Writes `game\arch\maps\name.map` |
| `arch_stats` | Prints the brush count and solid volume |

Changes are visible and solid immediately. For example, `arch_carve 160 -48 -160 480 48 -32` opens a corridor in the wall you face when you start.

Exported `.map` files use the standard Quake format and open in editors such as TrenchBroom. Jump pads are written as `trigger_push` entities, which work in stock Quake but are invisible there. To compile one into a `.bsp` you need a map compiler and a texture wad containing the three texture names used.

## Settings

| Variable | Default | Effect |
|---|---|---|
| `arch_selfdamage` | 0 | Set to 1 to take damage |
| `arch_falldamage` | 0 | Set to 1 for fall damage (also needs `arch_selfdamage 1`) |
| `arch_infiniteammo` | 1 | Set to 0 for limited ammunition |
| `arch_drag_speed` | 1 | Units of depth per mouse count while dragging in build mode. Raise it if drags feel slow |
| `arch_tex_ground`, `arch_tex_wall`, `arch_tex_ceiling` | `sfloor4_5`, `wbrick1_5`, `ceiling1_3` | Texture names, taken from `maps/start.bsp` in your pak. Applied at the next launch |

## Development

```
tools\test.bat               unit tests for the geometry code
tools\selftest.bat boot      scripted in-engine checks: boot, collide, render, rules,
                             build_freeze, build_click, build_drag, build_save, binds,
                             act_arch, act_bumper, act_snap, act_wheel
```

Selftests write `game\arch\qconsole.log` and screenshots into `game\arch`.

- `engine/` is QuakeSpasm 0.97.0 (see `engine/UPSTREAM.txt`). QuakeArch code is in `engine/Quake/arch_*.c`; changes to QuakeSpasm's own files are short hooks marked `// QUAKEARCH`.
- `docs/superpowers/specs/` holds the design.

## Licence

QuakeArch is free software under the GNU General Public License, version 2 or later, as is QuakeSpasm, which it is built on. See `engine/LICENSE.txt`. Quake is a trademark of id Software; this project is not affiliated with id Software or Bethesda.
