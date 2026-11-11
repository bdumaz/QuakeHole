/*
QuakeArch - the inside of holes dug in a campaign map, as triangle meshes.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include <stdlib.h>
#include <string.h>
#include "arch_world.h"
#include "arch_holes.h"

#define MAX_RECORDS	65536
#define MERGE_RECENT	8	/* a dig may widen one of this many latest records ... */
#define MERGE_SPAN	384	/* ... while the result stays this compact */

typedef struct { double mins[3], maxs[3]; int look; } arecord_t;
typedef struct { int *ids; int count, cap; } alist_t;

static arecord_t	*records;
static int		num_records, cap_records;
static unsigned		*rec_stamp;
static unsigned		stamp;

static alist_t		cell_records[ARCH_NUM_CELLS];	/* records overlapping each grid cell */
static aholecell_t	cells[ARCH_NUM_CELLS];
static unsigned char	dirty[ARCH_NUM_CELLS];
static int		look_w[ARCH_HOLES_MAX_LOOKS], look_h[ARCH_HOLES_MAX_LOOKS];

/* ----------------------------------------------------------------- helpers */

static void list_add (alist_t *l, int id)
{
	if (l->count && l->ids[l->count - 1] == id)
		return;
	if (l->count == l->cap)
	{
		l->cap = l->cap ? l->cap * 2 : 8;
		l->ids = (int *) realloc (l->ids, l->cap * sizeof(int));
	}
	l->ids[l->count++] = id;
}

#define CELL_INDEX(x, y, z)	(((z) * ARCH_GRID + (y)) * ARCH_GRID + (x))

/* lists the record in every cell it overlaps */
static void index_record (int r)
{
	int	lo[3], hi[3], x, y, z;

	ArchWorld_CellRange (records[r].mins, records[r].maxs, lo, hi);
	for (z = lo[2]; z <= hi[2]; z++)
		for (y = lo[1]; y <= hi[1]; y++)
			for (x = lo[0]; x <= hi[0]; x++)
				list_add (&cell_records[CELL_INDEX (x, y, z)], r);
}

/* every brush that may now show a face inside the box: its cell is rebuilt */
static void mark_area (const double mins[3], const double maxs[3])
{
	const int	*ids;
	int		n, i, home;

	ids = ArchWorld_Query (mins, maxs, &n);
	for (i = 0; i < n; i++)
	{
		home = ArchWorld_HomeCell (ids[i]);
		if (home >= 0)
			dirty[home] = 1;
	}
}

static void take_world_changes (int keep)
{
	static int	changed[ARCH_NUM_CELLS];
	int		n, i;

	n = ArchWorld_TakeDirtyCells (changed, ARCH_NUM_CELLS);
	if (keep)
		for (i = 0; i < n; i++)
			dirty[changed[i]] = 1;
}

/* ------------------------------------------------------------------ records */

void ArchHoles_Reset (int drain)
{
	int	i;

	num_records = 0;
	for (i = 0; i < ARCH_NUM_CELLS; i++)
	{
		cell_records[i].count = 0;
		cells[i].nverts = 0;
		cells[i].nsegs = 0;
	}
	memset (dirty, 0, sizeof(dirty));
	if (drain)
		take_world_changes (0);
}

void ArchHoles_SetLookSize (int look, int width, int height)
{
	if (look <= 0 || look >= ARCH_HOLES_MAX_LOOKS)
		return;
	look_w[look] = width > 0 ? width : 64;
	look_h[look] = height > 0 ? height : 64;
}

void ArchHoles_NoteDig (const double mins[3], const double maxs[3], int look)
{
	arecord_t	*r = NULL;
	int		i, k, id = -1;

	/* a dig beside a recent one of the same look just widens that record */
	for (i = num_records - 1; i >= 0 && i >= num_records - MERGE_RECENT && id < 0; i--)
	{
		r = &records[i];
		if (r->look != look)
			continue;
		for (k = 0; k < 3; k++)
			if (mins[k] > r->maxs[k] + 1 || maxs[k] < r->mins[k] - 1 ||
			    (maxs[k] > r->maxs[k] ? maxs[k] : r->maxs[k]) - (mins[k] < r->mins[k] ? mins[k] : r->mins[k]) > MERGE_SPAN)
				break;
		if (k == 3)
			id = i;
	}
	if (id < 0 && num_records == MAX_RECORDS)
		id = num_records - 1;	/* out of room: the last record grows */
	if (id >= 0)
	{
		r = &records[id];
		for (k = 0; k < 3; k++)
		{
			if (mins[k] < r->mins[k]) r->mins[k] = mins[k];
			if (maxs[k] > r->maxs[k]) r->maxs[k] = maxs[k];
		}
	}
	else
	{
		if (num_records == cap_records)
		{
			cap_records = cap_records ? cap_records * 2 : 256;
			records = (arecord_t *) realloc (records, cap_records * sizeof(arecord_t));
			rec_stamp = (unsigned *) realloc (rec_stamp, cap_records * sizeof(unsigned));
		}
		id = num_records++;
		r = &records[id];
		for (k = 0; k < 3; k++)
		{
			r->mins[k] = mins[k];
			r->maxs[k] = maxs[k];
		}
		r->look = look;
		rec_stamp[id] = 0;
	}
	index_record (id);
	mark_area (r->mins, r->maxs);
}

int ArchHoles_RecordCount (void)
{
	return num_records;
}

void ArchHoles_Record (int i, double mins[3], double maxs[3], int *look)
{
	int	k;

	if (i < 0 || i >= num_records)
		return;
	for (k = 0; k < 3; k++)
	{
		mins[k] = records[i].mins[k];
		maxs[k] = records[i].maxs[k];
	}
	*look = records[i].look;
}

/* the newest record whose box, grown by one unit, overlaps mins..maxs; -1 if none */
static int newest_record (const double mins[3], const double maxs[3])
{
	int	lo[3], hi[3], x, y, z, i, k, best = -1;

	if (++stamp == 0)
	{
		if (num_records)
			memset (rec_stamp, 0, num_records * sizeof(unsigned));
		stamp = 1;
	}
	ArchWorld_CellRange (mins, maxs, lo, hi);
	for (z = lo[2]; z <= hi[2]; z++)
		for (y = lo[1]; y <= hi[1]; y++)
			for (x = lo[0]; x <= hi[0]; x++)
			{
				const alist_t *l = &cell_records[CELL_INDEX (x, y, z)];
				for (i = 0; i < l->count; i++)
				{
					int r = l->ids[i];
					if (r <= best || rec_stamp[r] == stamp)
						continue;
					rec_stamp[r] = stamp;
					for (k = 0; k < 3; k++)
						if (mins[k] > records[r].maxs[k] + 1 || maxs[k] < records[r].mins[k] - 1)
							break;
					if (k == 3)
						best = r;
				}
			}
	return best;
}

int ArchHoles_LookAt (const double p[3])
{
	int	r = newest_record (p, p);

	/* the test is grown by one unit; a point must really be inside */
	while (r >= 0)
	{
		int k;
		for (k = 0; k < 3; k++)
			if (p[k] < records[r].mins[k] || p[k] > records[r].maxs[k])
				break;
		if (k == 3)
			return records[r].look;
		r--;
		/* rare: fall back to an exact scan of older records */
		for (; r >= 0; r--)
		{
			for (k = 0; k < 3; k++)
				if (p[k] < records[r].mins[k] || p[k] > records[r].maxs[k])
					break;
			if (k == 3)
				return records[r].look;
		}
	}
	return 0;
}

/* -------------------------------------------------------------------- meshes */

/* qbsp's world-aligned texture axes, as used for sandbox brushes */
static const float baseaxis[6][3][3] =
{
	{{ 0, 0, 1}, {1, 0, 0}, {0, -1, 0}},
	{{ 0, 0,-1}, {1, 0, 0}, {0, -1, 0}},
	{{ 1, 0, 0}, {0, 1, 0}, {0, 0, -1}},
	{{-1, 0, 0}, {0, 1, 0}, {0, 0, -1}},
	{{ 0, 1, 0}, {1, 0, 0}, {0, 0, -1}},
	{{ 0,-1, 0}, {1, 0, 0}, {0, 0, -1}},
};

/* triangles waiting to be placed in a cell, per look */
typedef struct { float *v; int n, cap; } abucket_t;
static abucket_t	buckets[ARCH_HOLES_MAX_LOOKS];
static int		used_looks[ARCH_HOLES_MAX_LOOKS];
static int		num_used;

static void bucket_face (const aface_t *f, int look)
{
	abucket_t	*b = &buckets[look];
	double		best = 0, dot;
	int		axis = 0, i, v, k, w = look_w[look] ? look_w[look] : 64, h = look_h[look] ? look_h[look] : 64;

	if (!b->n)
		used_looks[num_used++] = look;
	for (i = 0; i < 6; i++)
	{
		dot = f->plane.n[0] * baseaxis[i][0][0] + f->plane.n[1] * baseaxis[i][0][1] + f->plane.n[2] * baseaxis[i][0][2];
		if (dot > best)
		{
			best = dot;
			axis = i;
		}
	}
	for (v = 1; v + 1 < f->nverts; v++)
	{
		const double *tri[3];

		tri[0] = f->verts[0]; tri[1] = f->verts[v]; tri[2] = f->verts[v + 1];
		if (b->n + 3 > b->cap)
		{
			b->cap = b->cap ? b->cap * 2 : 96;
			b->v = (float *) realloc (b->v, b->cap * ARCH_HOLES_VERT_FLOATS * sizeof(float));
		}
		for (i = 0; i < 3; i++)
		{
			float		*out = b->v + b->n++ * ARCH_HOLES_VERT_FLOATS;
			const double	*p = tri[i];

			for (k = 0; k < 3; k++)
				out[k] = (float) p[k];
			out[3] = (float) ((p[0] * baseaxis[axis][1][0] + p[1] * baseaxis[axis][1][1] + p[2] * baseaxis[axis][1][2]) / w);
			out[4] = (float) ((p[0] * baseaxis[axis][2][0] + p[1] * baseaxis[axis][2][1] + p[2] * baseaxis[axis][2][2]) / h);
		}
	}
}

/* moves the buckets into the cell's mesh */
static void flush_buckets (aholecell_t *c)
{
	int	i, k, v;

	c->nverts = 0;
	c->nsegs = 0;
	for (i = 0; i < num_used; i++)
	{
		abucket_t	*b = &buckets[used_looks[i]];

		if (c->nverts + b->n > c->capverts)
		{
			while (c->nverts + b->n > c->capverts)
				c->capverts = c->capverts ? c->capverts * 2 : 192;
			c->verts = (float *) realloc (c->verts, c->capverts * ARCH_HOLES_VERT_FLOATS * sizeof(float));
		}
		if (c->nsegs == c->capsegs)
		{
			c->capsegs = c->capsegs ? c->capsegs * 2 : 4;
			c->segs = (aholeseg_t *) realloc (c->segs, c->capsegs * sizeof(aholeseg_t));
		}
		memcpy (c->verts + c->nverts * ARCH_HOLES_VERT_FLOATS, b->v, b->n * ARCH_HOLES_VERT_FLOATS * sizeof(float));
		c->segs[c->nsegs].look = used_looks[i];
		c->segs[c->nsegs].first = c->nverts;
		c->segs[c->nsegs].count = b->n;
		c->nsegs++;
		c->nverts += b->n;
		b->n = 0;
	}
	num_used = 0;
	for (v = 0; v < c->nverts; v++)
		for (k = 0; k < 3; k++)
		{
			float f = c->verts[v * ARCH_HOLES_VERT_FLOATS + k];
			if (!v || f < c->mins[k]) c->mins[k] = f;
			if (!v || f > c->maxs[k]) c->maxs[k] = f;
		}
}

static int cmp_pairs (const void *a, const void *b)
{
	const int *x = (const int *) a, *y = (const int *) b;
	return x[0] != y[0] ? x[0] - y[0] : x[1] - y[1];
}

int ArchHoles_Update (void)
{
	static int	*pairs;		/* (home cell, brush id) */
	static int	cap_pairs;
	int		npairs = 0, id, i, f, v, k, examined = 0, any = 0;

	take_world_changes (1);
	for (i = 0; i < ARCH_NUM_CELLS && !any; i++)
		any = dirty[i];
	if (!any)
		return 0;

	for (id = 0; id < ArchWorld_SlotCount (); id++)
	{
		const abrush_t	*b = ArchWorld_Brush (id);
		int		home;

		if (!b || b->locked)
			continue;
		home = ArchWorld_HomeCell (id);
		if (home < 0 || !dirty[home])
			continue;
		if (npairs == cap_pairs)
		{
			cap_pairs = cap_pairs ? cap_pairs * 2 : 1024;
			pairs = (int *) realloc (pairs, cap_pairs * 2 * sizeof(int));
		}
		pairs[npairs * 2] = home;
		pairs[npairs * 2 + 1] = id;
		npairs++;
	}
	qsort (pairs, npairs, 2 * sizeof(int), cmp_pairs);

	/* cells that were marked but now hold nothing are emptied too */
	for (i = 0; i < ARCH_NUM_CELLS; i++)
		if (dirty[i])
		{
			cells[i].nverts = 0;
			cells[i].nsegs = 0;
		}

	for (i = 0; i < npairs; )
	{
		int home = pairs[i * 2];

		for (; i < npairs && pairs[i * 2] == home; i++)
		{
			const abrush_t *b = ArchWorld_Brush (pairs[i * 2 + 1]);

			examined++;
			if (!num_records)
				continue;
			/* Of a brush near a dig, only the faces that themselves reach a dug
			   area can have been exposed: converted regions are large. */
			for (f = 0; f < b->nfaces; f++)
			{
				const aface_t	*face = &b->faces[f];
				double		fmin[3], fmax[3];
				int		look, r;

				for (k = 0; k < 3; k++)
					fmin[k] = fmax[k] = face->verts[0][k];
				for (v = 1; v < face->nverts; v++)
					for (k = 0; k < 3; k++)
					{
						if (face->verts[v][k] < fmin[k]) fmin[k] = face->verts[v][k];
						if (face->verts[v][k] > fmax[k]) fmax[k] = face->verts[v][k];
					}
				r = newest_record (fmin, fmax);
				if (r < 0 || !records[r].look)
					continue;
				look = face->plane.tag;
				if (look < 1 || look >= ARCH_HOLES_MAX_LOOKS)
					look = records[r].look;
				if (look >= 1 && look < ARCH_HOLES_MAX_LOOKS)
					bucket_face (face, look);
			}
		}
		flush_buckets (&cells[home]);
	}
	memset (dirty, 0, sizeof(dirty));
	return examined;
}

const aholecell_t *ArchHoles_Cell (int cell)
{
	if (cell < 0 || cell >= ARCH_NUM_CELLS || !cells[cell].nverts)
		return NULL;
	return &cells[cell];
}
