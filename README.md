# QuakeHole

*Quake, but weapons make holes.*

The full original campaign with hole-making weapons, and a live level-building prototype on the open-source Quake engine. You stand inside the level and shape it, then run, fly and shoot through what you built, with no compile step in between.

The game opens on its main menu, titled QuakeHole:

- **Single Player** and **Multiplayer** start Quake as normal, except that every weapon digs into the level.
- **Build Mode** puts you in a room made of live-editable brushes, where you carve holes, pull out blocks, cut arches and place jump pads with the mouse. From there you can load any map through the console, for example `map E1M1`, and keep building in it. Levels in the cube room export as standard `.map` files; maps cannot be saved in this version yet.

This is the very first version of the game. It has been entirely vibe coded by a moron. It has not been thoroughly tested. This is for fun.

## What you need

- **Your own copy of Quake.** QuakeHole ships none of id Software's game data. It reads `id1\pak0.pak` and `id1\pak1.pak` from your installation (Steam, GOG or an original CD). The full game is required; the shareware version lacks several weapon models.
- **Windows 10 or 11, 64-bit.**
- **Visual Studio 2019 or later** with the "Desktop development with C++" workload, to build the engine.

## Build and run

```
tools\build.bat      builds the engine
run.bat              starts QuakeHole
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
| Space | Jump (swim, or rise while flying) |
| C or Ctrl | Sink while flying |
| Shift | Walk |
| Left mouse | Fire |
| 1 to 8, mouse wheel | Choose weapon |
| F | Toggle flying. Flying works like a camera: you move where you look and stop when you let go |
| B | Toggle build mode (Build Mode only) |
| H | Show or hide the build-mode key list (Build Mode only) |
| ` | Console |
| F12 | Screenshot |

These defaults live in `game\arch\default.cfg`. Rebind keys in the Options menu; your changes are saved to `game\arch\config.cfg`.

You have every weapon, ammunition does not run out, and nothing damages you. Rocket jumps still push you.

## Build mode

Choose Build Mode in the main menu. In a game started from Single Player or Multiplayer, B does nothing apart from a reminder.

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

While you drag, the view locks and a coloured preview shows what will be carved (orange) or added (green). Release to commit. For the two extrude actions, bringing the depth back to zero cancels; for a jump pad it places one at the default strength. The line at the top of the screen shows the cursor size and drag depth in metres.

A block that would bury you is refused.

## Digging with weapons

In move mode every weapon reshapes the level. The size of the hole follows the damage the hit does in the game, with a rocket as the reference:

| Weapon | What one hit removes |
|---|---|
| Rocket, grenade | A stepped crater about 4 m across |
| Exploding barrel | A slightly larger crater (it does a third more damage than a rocket) |
| Super nailgun | A pocket a little over 2 m across, per nail |
| Nailgun | A pocket a little under 2 m across, per nail |
| Shotgun, super shotgun | A dent a little over 1 m across for each pellet that reaches a wall |
| Axe | The same dent as one pellet, where it strikes a wall |
| Lightning gun | A block 1.5 m across, bored along the beam |

Those are the widths of the sphere each hit clears. Most of it is in front of the wall, so the bite out of a flat wall is smaller: about 6 half-metre cubes for a shotgun blast against about 120 for a rocket.

The lightning gun digs for as long as you hold it on a surface, several metres deeper every second, with a burst of debris. Monsters' nails, bolts and grenades dig too.

Everything dug is made of half-metre cubes on the world grid, so the build cursor works on it and it exports exactly. Jump pads whose surface is dug away disappear. There is no undo; set `arch_dig 0` to shoot without changing the level.

Each crater splits the level into more pieces. In testing, three thousand craters used about a third of the limit, so this rarely matters; past the limit the engine refuses further edits with "brush limit reached".

## The Quake campaign

The campaign and any other map still play in this engine: choose New Game from the menu, or type `map e1m1` in the console. Type `map arch` to return to your sandbox level, which is kept as you left it.

When a campaign map loads, its solid space is rebuilt as the same kind of brushes the sandbox uses, and collision with the world runs on those. That lets weapons dig into campaign maps as they do in the sandbox:

- A rocket or grenade blasts a stepped crater out of a wall, floor or ceiling. The wall around it keeps its texture and lighting, and the inside of the hole takes the texture of the wall it was blasted from.
- Every other weapon digs too, as described above, and so do exploding barrels.
- Dig through to another room and you can see, shoot and walk through the hole. So can the monsters.
- Items and monsters standing on a floor that is dug away fall.
- Doors, lifts, buttons, liquids and the sky cannot be dug. Restarting the map restores it; holes are not kept in savegames.

After the first dig in a map the engine stops skipping rooms it believes are out of sight, since a hole may have joined them. Large maps draw somewhat slower from then until the map restarts.

Build mode works in campaign maps you load from Build Mode (for example `map E1M1` in the console): press B, and the cursor, both extrude actions, jump pads, the action wheel and snap behave as in the sandbox. What you add is textured from the nearest wall and lit evenly; it does not pick up the map's baked shadows. Two limits apply: the cursor only lands on surfaces that face straight along an axis and sit on whole units, so angled walls and a few odd ones will not take it; and monsters do not wait while you build.

The sandbox's free weapons and its damage rules stay in the sandbox. In the campaign you dig with the weapons and ammunition you find.

Three differences from original Quake are known. A few invisible barriers that mappers placed are gone, so the odd spot is reachable that was not. Movers collide with their real size where the original uses three fixed sizes, which can matter in very tight spaces. And on two maps (E1M5 and E4M7) rockets and nails explode against the sky where the original removes them silently. Set `arch_campaign 0` and restart the map to get the original collision back.

## Editing from the console

The world is solid everywhere, 4096 units across, with a 320-unit room carved out of its centre. The outermost 16 units cannot be carved. One metre is 32 units. Open the console and use:

| Command | Effect |
|---|---|
| `arch_carve x1 y1 z1 x2 y2 z2` | Carves a box of empty space between two corners |
| `arch_add x1 y1 z1 x2 y2 z2` | Fills a box with solid |
| `arch_reset` | Restores the starting room and removes all jump pads |
| `arch_action quad\|arch\|bumper` | Selects a build action without the wheel |
| `arch_export name` | Writes `game\arch\maps\name.map` |
| `arch_stats` | Prints the brush count, solid volume and number of jump pads |

Changes are visible and solid immediately. For example, `arch_carve 160 -48 -160 480 48 -32` opens a corridor in the wall you face when you start.

Exported `.map` files use the standard Quake format and open in editors such as TrenchBroom. Jump pads are written as `trigger_push` entities, which work in stock Quake but are invisible there. To compile one into a `.bsp` you need a map compiler and a texture wad containing the three texture names used.

## Settings

| Variable | Default | Effect |
|---|---|---|
| `arch_selfdamage` | 0 | Set to 1 to take damage |
| `arch_falldamage` | 0 | Set to 1 for fall damage (also needs `arch_selfdamage 1`) |
| `arch_infiniteammo` | 1 | Set to 0 for limited ammunition |
| `arch_dig` | 1 | Set to 0 to stop weapons digging into the level |
| `arch_dig_radius` | 64 | Crater radius for a rocket, in units (32 units = 1 m). Every other weapon scales from it |
| `arch_dig_time` | 0.05 | Seconds per lightning gun bite. The gun reports ten times a second, so values below 0.1 mean several bites per report (0.05 is two); 0.1 or more makes it wait on one spot that long |
| `arch_dig_beam` | 3 | Width of the lightning gun's bite, in half-metre cubes (1 to 9) |
| `arch_dig_weapons` | 1 | Scales what bullets, nails and bolts dig. 0 turns them off; explosions and the lightning gun are not affected |
| `arch_campaign` | 1 | Set to 0 to run campaign maps on their original collision. Takes effect when a map starts |
| `arch_drag_speed` | 1 | Units of depth per mouse count while dragging in build mode. Raise it if drags feel slow |
| `arch_tex_ground`, `arch_tex_wall`, `arch_tex_ceiling` | `sfloor4_5`, `wbrick1_5`, `ceiling1_3` | Texture names, taken from `maps/start.bsp` in your pak. Applied at the next launch |

## Development

```
tools\test.bat               unit tests for the geometry code
tools\selftest.bat boot      scripted in-engine checks: boot, collide, render, rules,
                             build_freeze, build_click, build_drag, build_save, binds,
                             act_arch, act_bumper, act_snap, act_wheel,
                             dig_rocket, dig_lightning, campaign, campaign_stock,
                             campaign_dig, campaign_dig2, campaign_build,
                             dig_weapons, dig_barrel
```

Selftests write `game\arch\qconsole.log` and screenshots into `game\arch`.

- `engine/` is QuakeSpasm 0.97.0 (see `engine/UPSTREAM.txt`). QuakeHole code is in `engine/Quake/arch_*.c`; changes to QuakeSpasm's own files are short hooks marked `// QUAKEARCH`.

## Licence

QuakeHole is free software under the GNU General Public License, version 2 or later, as is QuakeSpasm, which it is built on. See `LICENSE.txt`. Quake is a trademark of id Software; this project is not affiliated with id Software or Bethesda.
