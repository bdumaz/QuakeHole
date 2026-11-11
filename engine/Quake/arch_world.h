/*
QuakeArch - the brush world: the list of solid convex brushes that is the
only source of truth for level geometry, with a spatial grid over it.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#ifndef ARCH_WORLD_H
#define ARCH_WORLD_H

#include "arch_brush.h"

#define ARCH_HALF		2048	/* build volume is -ARCH_HALF..+ARCH_HALF on each axis */
#define ARCH_SHELL		16	/* outer skin of the build volume that can never be carved */
#define ARCH_ROOM_HALF		160	/* spawn room is -160..+160 on each axis */
#define ARCH_CELL		256
#define ARCH_GRID		16	/* cells per axis */
#define ARCH_NUM_CELLS		(ARCH_GRID * ARCH_GRID * ARCH_GRID)
#define ARCH_MAX_BRUSHES	65536

void   ArchWorld_Reset (void);			/* frees everything and builds the initial world */
void   ArchWorld_SetMaxBrushes (int n);		/* for tests; Reset restores ARCH_MAX_BRUSHES */

/* Edits return 1 when done (including "nothing to do") and 0 when refused by
   the brush limit, in which case the world is unchanged. The shape is not kept. */
int    ArchWorld_Carve (const abrush_t *shape);
int    ArchWorld_Add (const abrush_t *shape);

/* sorts two corners per axis; returns 0 if the box is empty on any axis */
int    ArchWorld_BoxFromCorners (const int a[3], const int b[3], int mins[3], int maxs[3]);

/* initialises *tr itself */
void   ArchWorld_Trace (const avec3 start, const avec3 end, const avec3 mins, const avec3 maxs, atrace_t *tr);
int    ArchWorld_PointSolid (const avec3 p);	/* inside or on a brush, or outside the build volume */

int    ArchWorld_Revision (void);		/* changes whenever a brush is added or removed */
int    ArchWorld_BrushCount (void);		/* live brushes */
int    ArchWorld_SlotCount (void);		/* highest id + 1 */
const abrush_t *ArchWorld_Brush (int id);	/* NULL for a free slot */
double ArchWorld_SolidVolume (void);

int    ArchWorld_HomeCell (int id);		/* cell containing the brush's mins corner */
int    ArchWorld_TakeDirtyCells (int *cells, int max);	/* returns how many were written, and clears them */

#endif /* ARCH_WORLD_H */
