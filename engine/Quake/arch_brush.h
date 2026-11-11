/*
QuakeArch - convex brush maths.
No engine dependencies: C standard library only.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#ifndef ARCH_BRUSH_H
#define ARCH_BRUSH_H

typedef double avec3[3];

enum { ARCH_SLOT_GROUND, ARCH_SLOT_WALL, ARCH_SLOT_CEILING, ARCH_NUM_SLOTS };

#define ARCH_ON_EPS    0.001	/* point-on-plane tolerance */
#define ARCH_THIN_EPS  0.01	/* pieces thinner than this are discarded */

/* n.x = d on the plane; n points out of the brush.
   n = normalize((p[0]-p[1]) x (p[2]-p[1])), the .map convention. */
typedef struct { int p[3][3]; avec3 n; double d; } aplane_t;

/* verts are clockwise when seen from outside the brush */
typedef struct { aplane_t plane; int slot; int nverts; avec3 *verts; } aface_t;

typedef struct { int nfaces; aface_t *faces; avec3 mins, maxs; } abrush_t;

int       ArchPlane_FromPoints (const int a[3], const int b[3], const int c[3], aplane_t *out); /* 0 if collinear */
void      ArchPlane_Flip (const aplane_t *in, aplane_t *out);

abrush_t *ArchBrush_FromPlanes (const aplane_t *planes, int nplanes);	/* NULL if empty or thin */
abrush_t *ArchBrush_Box (const int mins[3], const int maxs[3]);		/* NULL if any maxs[i] <= mins[i] */
abrush_t *ArchBrush_Prism (const int (*poly)[3], int npoly, const int extrude[3]); /* NULL if degenerate */
abrush_t *ArchBrush_Clone (const abrush_t *b);
void      ArchBrush_Free (abrush_t *b);						/* NULL is allowed */
double    ArchBrush_Volume (const abrush_t *b);
int       ArchBrush_PointInside (const abrush_t *b, const avec3 p);	/* strictly inside by more than ARCH_ON_EPS */

/* front is the part on the normal side of pl; either result may be NULL */
void      ArchBrush_Split (const abrush_t *b, const aplane_t *pl, abrush_t **front, abrush_t **back);
abrush_t *ArchBrush_Intersection (const abrush_t *a, const abrush_t *b);	/* NULL if no shared volume */
int       ArchBrush_Intersects (const abrush_t *a, const abrush_t *b);
/* a minus b as non-overlapping convex pieces. Returns the piece count;
   *out is malloc'd (NULL when the count is 0), caller frees the array and the pieces. */
int       ArchBrush_Subtract (const abrush_t *a, const abrush_t *b, abrush_t ***out);

#define ARCH_TRACE_EPS (1.0 / 32.0)	/* movers stop this far short of a surface */

typedef struct { double fraction; int startsolid, allsolid; avec3 normal; double dist; } atrace_t;

void      ArchTrace_Init (atrace_t *tr);	/* fraction 1, flags 0 */
/* Sweeps the box [mins,maxs] (relative to the point) from start to end.
   Updates *tr only if this brush is hit earlier than tr->fraction, or to set startsolid/allsolid.
   A box that exactly touches a face counts as outside the brush. */
void      ArchBrush_TraceBox (const abrush_t *b, const avec3 start, const avec3 end,
			      const avec3 mins, const avec3 maxs, atrace_t *tr);

#endif /* ARCH_BRUSH_H */
