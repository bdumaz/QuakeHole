/*
QuakeHole - cutting a dig box out of a flat surface polygon.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#ifndef ARCH_POLY_H
#define ARCH_POLY_H

#include "arch_brush.h"

#define ARCH_POLY_MAX_VERTS	72	/* a map surface has at most 64 edges; a box cut adds at most one per side */
#define ARCH_POLY_MAX_PIECES	6
#define ARCH_POLY_UNTOUCHED	(-1)

typedef struct { int n; avec3 v[ARCH_POLY_MAX_VERTS]; } apoly_t;

/* A map surface is a convex polygon facing the open side (normal), with solid
   directly behind it. When the box mins..maxs of solid is removed, the part of
   the surface that had that solid behind it goes too.

   Writes what is left of poly as up to ARCH_POLY_MAX_PIECES convex pieces and
   returns how many: 0 if the whole surface went, or ARCH_POLY_UNTOUCHED if the
   box took nothing from it (pieces is then not written). "Behind" is judged
   just under the surface, so a box flush against it from the solid side cuts
   it, and one flush against it from the open side does not. */
int ArchPoly_SubtractBox (const apoly_t *poly, const avec3 normal, const int mins[3], const int maxs[3],
			  apoly_t pieces[ARCH_POLY_MAX_PIECES]);

#define ARCH_POLY_MAX_CUT_PIECES	32

/* The same for any convex brush (an arch, for build mode). pieces must hold
   ARCH_POLY_MAX_CUT_PIECES; a brush with more faces than that is treated as untouched. */
int ArchPoly_SubtractBrush (const apoly_t *poly, const avec3 normal, const abrush_t *brush,
			    apoly_t pieces[ARCH_POLY_MAX_CUT_PIECES]);

#endif /* ARCH_POLY_H */
