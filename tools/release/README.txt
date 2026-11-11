QuakeHole
Quake, but weapons make holes.
=============================

Every weapon digs into the level: rockets and grenades blast craters, the
lightning gun bores tunnels, nails and shotgun pellets chip the walls. Dig
through to the next room and you can see, shoot and walk through the hole.
Build Mode lets you carve and build with the mouse in a test room or in any
Quake map.


GETTING STARTED
---------------

QuakeHole contains none of Quake's game data. You need your own copy of
Quake (Steam, GOG or the original CD; the full game, not the shareware
version).

1. Copy pak0.pak and pak1.pak from your Quake folder into the "Quake paks"
   folder here. On Steam they are in
   C:\Program Files (x86)\Steam\steamapps\common\Quake\id1
   (If Quake is installed there, you can skip this step.)
2. Run "Play QuakeHole.bat".

Windows 10 or 11, 64-bit.


PLAYING
-------

Single Player and Multiplayer play Quake as normal, except that weapons dig.
Build Mode starts you in a test room; press B there to build. Load any map
from the console (the ` key), for example:  map e1m1

Build mode, in short:
  B                        enter or leave build mode
  Left click               carve a hole the shape of the cursor
  Hold left, move mouse    carve deeper (push) or pull out a block (pull)
  Mouse wheel              resize the cursor
  Hold middle mouse        choose an action: box, arch or jump pad
  R                        fit the cursor to the wall you aim at
  F                        fly on or off; while flying, Space rises, C sinks,
                           and you move where you look
  H                        show or hide the full key list

Settings are typed in the console:
  arch_dig 0               stop weapons digging
  arch_dig_weapons 0       only explosions and the lightning gun dig
  arch_campaign 0          play maps on their original collision (from the next map)

Holes last until the map restarts; they are not kept in saved games, and
Build Mode maps cannot be saved yet.


CREDITS AND LICENCE
-------------------

Based on QuakeSpasm. Made with Claude AI.
QuakeHole is free software under the GNU General Public Licence, Version 2
or later; see LICENSE.txt. Source code: https://github.com/bdumaz/QuakeHole

The included libraries and their licences are listed in THIRD-PARTY.txt.

Quake is a trademark of id Software. This project is not affiliated with
id Software or Bethesda.
