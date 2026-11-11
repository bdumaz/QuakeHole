/*
QuakeArch - the inside of holes dug in a campaign map, as triangle meshes.

Every dig leaves a record of the area it touched. A face of a brush is drawn
when it reaches into a record: those are the faces a dig may have exposed. The
meshes are kept per cell of the world's grid (a brush belongs to the cell
holding its lowest corner), and after a dig only the cells near it are
rebuilt, so the cost of a dig does not grow with the number of earlier ones.
No engine dependencies; works on the current brush world.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#ifndef ARCH_HOLES_H
#define ARCH_HOLES_H

#define ARCH_HOLES_MAX_LOOKS	256	/* look 0 means "none" */
#define ARCH_HOLES_VERT_FLOATS	5	/* x y z s t */

typedef struct { int look, first, count; } aholeseg_t;	/* a run of vertices drawn with one look */

typedef struct
{
	float		*verts;
	int		nverts, capverts;
	aholeseg_t	*segs;
	int		nsegs, capsegs;
	float		mins[3], maxs[3];	/* bounds of the vertices, for culling */
} aholecell_t;

/* forgets every record and mesh. drain: also discard the world's pending change
   marks (a freshly converted map marks everything changed) */
void ArchHoles_Reset (int drain);
void ArchHoles_SetLookSize (int look, int width, int height);	/* texture size, for texture coordinates */

/* records a dig over mins..maxs; faces near it are drawn with this look unless they carry their own */
void ArchHoles_NoteDig (const double mins[3], const double maxs[3], int look);
int  ArchHoles_RecordCount (void);
void ArchHoles_Record (int i, double mins[3], double maxs[3], int *look);
int  ArchHoles_LookAt (const double p[3]);	/* look of the newest record containing p, 0 if none */

/* rebuilds the cells changed since the last call; returns how many brushes it examined */
int  ArchHoles_Update (void);
const aholecell_t *ArchHoles_Cell (int cell);	/* NULL if the cell has nothing to draw */

#endif /* ARCH_HOLES_H */
