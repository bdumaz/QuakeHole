/*
QuakeArch - weapon digging: the crater an explosion removes, the cube the
lightning gun is working on, and recognising both in what the game code
reports to the engine.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#ifndef ARCH_DIG_H
#define ARCH_DIG_H

#include "arch_brush.h"

#define ARCH_DIG_CELL		16	/* digging removes cubes of this size, aligned to the world grid */
#define ARCH_DIG_MAX_RADIUS	256
#define ARCH_DIG_MAX_BOXES	4096
#define ARCH_DIG_GAP		0.3	/* seconds without a beam report before the dwell timer restarts */

/* ---- craters ---- */

/* Fills boxes (mins x y z, maxs x y z) that together cover exactly the grid cubes
   whose centres are within radius of centre, without overlapping. Returns the box
   count, or -1 if max is too small. Radius is limited to ARCH_DIG_MAX_RADIUS. */
int  ArchDig_Crater (const avec3 centre, double radius, int boxes[][6], int max);

/* ---- grid cubes ---- */

void ArchDig_CellAt (const avec3 p, int cell[3]);			/* the cube containing p, as grid indices */
void ArchDig_CellBox (const int cell[3], int mins[3], int maxs[3]);

/* The cube a beam ending at end, travelling along dir (unit length), is working on:
   the cube just past the end of the beam. When the beam runs exactly along a grid
   line or corner, several cubes share that point; the first one that is still
   solid is chosen. Returns 0 if there is none (the beam ended in open air). */
int  ArchDig_BeamCell (const avec3 end, const avec3 dir, int (*solid) (const avec3 p), int cell[3]);

/* ---- the lightning gun's dwell timer ---- */

typedef struct { int valid; int cell[3]; double since, last; } adwell_t;

void   ArchDig_DwellReset (adwell_t *d);
/* One beam report on the given cube at time now. Returns 1 when the beam has
   stayed on that cube for hold seconds; the timer then restarts. */
int    ArchDig_Dwell (adwell_t *d, const int cell[3], double now, double hold);
double ArchDig_DwellProgress (const adwell_t *d, double now, double hold);	/* 0..1 */

/* ---- recognising the game code's reports ---- */

/* The stock game announces an explosion as: byte 23 (temp entity), byte 3
   (explosion), three coordinates. A lightning beam is: byte 23, byte 6, an
   entity, three coordinates for the start and three for the end. The parser is
   fed every value the game code writes and reports when one of these completes. */
enum { ARCH_DIG_NONE, ARCH_DIG_BLAST, ARCH_DIG_BEAM };

typedef struct { int state, count; float values[6]; } adigparse_t;

void ArchDigParse_Reset (adigparse_t *p);
/* dest is the message destination; only 0 (broadcast) is listened to.
   On ARCH_DIG_BLAST out[0..2] is the position; on ARCH_DIG_BEAM out[0..2] is the start and out[3..5] the end. */
int  ArchDigParse_Byte (adigparse_t *p, int dest, int value, float out[6]);
int  ArchDigParse_Coord (adigparse_t *p, int dest, float value, float out[6]);
int  ArchDigParse_Entity (adigparse_t *p, int dest, float out[6]);

#endif /* ARCH_DIG_H */
