/*
QuakeHole - build cursor geometry: where the cursor rectangle sits on a
surface, the box an action applies to, and how the cursor size steps.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#ifndef ARCH_CURSOR_H
#define ARCH_CURSOR_H

#include "arch_brush.h"

#define ARCH_CURSOR_MIN		16
#define ARCH_CURSOR_MAX		1024
#define ARCH_CURSOR_STEP	16	/* sizes are multiples of this, so a snapped centre puts every edge on the grid */
#define ARCH_CURSOR_DEFAULT	160
#define ARCH_GRID_SNAP		8

enum { ARCH_SCALE_UNIFORM, ARCH_SCALE_HEIGHT, ARCH_SCALE_WIDTH };

typedef struct
{
	int	naxis, nsign;	/* surface normal: axis 0..2 and +1/-1; it points into the air */
	int	plane;		/* the surface's coordinate on naxis */
	int	uaxis, vaxis;	/* axis the width runs along, axis the height runs along */
	int	umin, umax, vmin, vmax;
	int	vdir;		/* +1 or -1: the direction along vaxis that is "up" for an arch */
} acursor_t;

#define ARCH_ARCH_SEGMENTS	8
#define ARCH_PROFILE_MAX	12	/* two base corners plus the curve's points */

/* Places a width x height cursor on the surface through hit, centred on hit and
   snapped to the grid. On floors and ceilings the height runs along the world
   axis nearest to the facing direction. Returns 0 if the normal is not axis-aligned. */
int  ArchCursor_Place (const avec3 hit, const avec3 normal, double yaw_degrees, int width, int height, acursor_t *out);

/* Slides the cursor along the surface so that it does not sink into an adjoining
   floor, wall or ceiling, when it can fit. solid(p) says whether a point is solid.
   A cursor hanging over open air, or too big to fit, is left where it is. */
void ArchCursor_Fit (acursor_t *c, int (*solid) (const avec3 p));

/* the box an action covers: depth > 0 goes into the surface (carve), depth < 0 comes out of it (extrude) */
void ArchCursor_Box (const acursor_t *c, int depth, int mins[3], int maxs[3]);

/* The arch outline fitted inside the cursor: a rectangle topped by a semicircle,
   or a half-ellipse when the cursor is less than half as high as it is wide.
   Points are whole units, on the surface, in order around the outline.
   Returns the number of points. */
int  ArchCursor_ArchProfile (const acursor_t *c, int pts[ARCH_PROFILE_MAX][3]);

/* the arch-shaped volume for an action, like ArchCursor_Box. NULL if depth is 0 or the shape is degenerate. */
abrush_t *ArchCursor_ArchBrush (const acursor_t *c, int depth);

/* Replaces the cursor rectangle with one grown outward on the 8-unit grid from
   the square under hit, for as long as each new strip is flat surface (solid
   behind, empty in front). Returns 0 and leaves the cursor alone if the square
   under hit is not flat surface. */
int  ArchCursor_Snap (acursor_t *c, const avec3 hit, int (*solid) (const avec3 p));

/* the rectangle's corners in order around it, moved lift units off the surface toward the air */
void ArchCursor_Corners (const acursor_t *c, double lift, avec3 corners[4]);

/* one wheel notch: dir is +1 to grow, -1 to shrink. A step that would leave the range does nothing. */
void ArchCursor_StepSize (int mode, int dir, int *width, int *height);

/* units as metres (32 units = 1 m) with one or two decimals: 160 -> "5.0", 40 -> "1.25" */
void ArchCursor_FormatMetres (int units, char *buf, int size);

#endif /* ARCH_CURSOR_H */
