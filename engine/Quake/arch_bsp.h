/*
QuakeArch - turns a map's collision tree into convex brushes.
No engine dependencies: the caller copies the tree into these plain structures.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#ifndef ARCH_BSP_H
#define ARCH_BSP_H

#include "arch_brush.h"

typedef struct { float normal[3]; float dist; } abspplane_t;

/* children[0] is the side the plane normal points to. A child below zero is
   not a node but the contents of that side: -1 empty, -2 solid, -6 sky, and
   other values for liquids. */
typedef struct { int planenum; int children[2]; } abspnode_t;

#define ARCH_BSP_SOLID	(-2)
#define ARCH_BSP_SKY	(-6)

/* Walks the tree from head and calls emit once for every solid or sky region,
   with a new brush clipped to the box mins..maxs. The receiver owns the brush.
   Returns the number of brushes emitted. *dropped, if given, receives the number
   of solid regions that could not be turned into a brush; anything above zero
   means the result has holes and should not be used for collision. */
int ArchBsp_Convert (const abspnode_t *nodes, const abspplane_t *planes, int head,
		     const int mins[3], const int maxs[3], void (*emit) (abrush_t *brush), int *dropped);

#endif /* ARCH_BSP_H */
