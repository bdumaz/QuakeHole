/*
QuakeArch - the brush world.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "arch_world.h"

typedef struct { int *ids; int count, cap; } cell_t;

static abrush_t	**brushes;		/* indexed by id; NULL = free slot */
static unsigned	*stamps;		/* per id: last query that visited it */
static int	num_slots, cap_slots;
static int	*free_ids;
static int	num_free, cap_free;
static int	num_live;
static int	revision;
static int	max_brushes = ARCH_MAX_BRUSHES;
static unsigned	query_stamp;

static cell_t		cells[ARCH_NUM_CELLS];
static unsigned char	dirty[ARCH_NUM_CELLS];

/* ------------------------------------------------------------------ grid */

static int cell_coord (double v)
{
	int c = (int) floor ((v + ARCH_HALF) / ARCH_CELL);
	if (c < 0) c = 0;
	if (c >= ARCH_GRID) c = ARCH_GRID - 1;
	return c;
}

#define CELL_INDEX(x, y, z)	(((z) * ARCH_GRID + (y)) * ARCH_GRID + (x))

/* cells overlapped by a box, grown by pad so that touching brushes are found */
static void cell_range (const avec3 mins, const avec3 maxs, double pad, int lo[3], int hi[3])
{
	int k;
	for (k = 0; k < 3; k++)
	{
		lo[k] = cell_coord (mins[k] - pad);
		hi[k] = cell_coord (maxs[k] + pad);
	}
}

/* cells a brush is listed in: its bounding box, not counting faces that only touch a cell */
static void brush_cells (const abrush_t *b, int lo[3], int hi[3])
{
	cell_range (b->mins, b->maxs, -ARCH_THIN_EPS, lo, hi);
}

int ArchWorld_HomeCell (int id)
{
	const abrush_t *b = ArchWorld_Brush (id);
	int lo[3], hi[3];

	if (!b)
		return -1;
	brush_cells (b, lo, hi);
	return CELL_INDEX (lo[0], lo[1], lo[2]);
}

static void cell_add (cell_t *c, int id)
{
	if (c->count == c->cap)
	{
		c->cap = c->cap ? c->cap * 2 : 8;
		c->ids = (int *) realloc (c->ids, c->cap * sizeof(int));
	}
	c->ids[c->count++] = id;
}

static void cell_remove (cell_t *c, int id)
{
	int i;
	for (i = 0; i < c->count; i++)
	{
		if (c->ids[i] == id)
		{
			c->ids[i] = c->ids[--c->count];
			return;
		}
	}
}

/* ----------------------------------------------------------- brush slots */

/* takes ownership of b */
static int insert_brush (abrush_t *b)
{
	int id, lo[3], hi[3], x, y, z;

	if (num_free)
		id = free_ids[--num_free];
	else
	{
		if (num_slots == cap_slots)
		{
			cap_slots = cap_slots ? cap_slots * 2 : 256;
			brushes = (abrush_t **) realloc (brushes, cap_slots * sizeof(abrush_t *));
			stamps = (unsigned *) realloc (stamps, cap_slots * sizeof(unsigned));
		}
		id = num_slots++;
	}
	brushes[id] = b;
	stamps[id] = 0;
	num_live++;
	revision++;

	brush_cells (b, lo, hi);
	for (z = lo[2]; z <= hi[2]; z++)
		for (y = lo[1]; y <= hi[1]; y++)
			for (x = lo[0]; x <= hi[0]; x++)
				cell_add (&cells[CELL_INDEX (x, y, z)], id);
	dirty[CELL_INDEX (lo[0], lo[1], lo[2])] = 1;
	return id;
}

static void remove_brush (int id)
{
	abrush_t *b = brushes[id];
	int lo[3], hi[3], x, y, z;

	brush_cells (b, lo, hi);
	for (z = lo[2]; z <= hi[2]; z++)
		for (y = lo[1]; y <= hi[1]; y++)
			for (x = lo[0]; x <= hi[0]; x++)
				cell_remove (&cells[CELL_INDEX (x, y, z)], id);
	dirty[CELL_INDEX (lo[0], lo[1], lo[2])] = 1;

	ArchBrush_Free (b);
	brushes[id] = NULL;
	num_live--;
	revision++;
	if (num_free == cap_free)
	{
		cap_free = cap_free ? cap_free * 2 : 256;
		free_ids = (int *) realloc (free_ids, cap_free * sizeof(int));
	}
	free_ids[num_free++] = id;
}

/* ids of live brushes whose cells overlap the box. The result is a shared
   scratch array, valid until the next call; edits must not start until the
   caller has finished reading it. */
static int *gather (const avec3 mins, const avec3 maxs, double pad, int *count)
{
	static int *ids, cap;
	int lo[3], hi[3], x, y, z, i, n = 0;

	if (++query_stamp == 0)
	{
		memset (stamps, 0, num_slots * sizeof(unsigned));
		query_stamp = 1;
	}
	cell_range (mins, maxs, pad, lo, hi);
	for (z = lo[2]; z <= hi[2]; z++)
	{
		for (y = lo[1]; y <= hi[1]; y++)
		{
			for (x = lo[0]; x <= hi[0]; x++)
			{
				const cell_t *c = &cells[CELL_INDEX (x, y, z)];
				for (i = 0; i < c->count; i++)
				{
					int id = c->ids[i];
					if (stamps[id] == query_stamp)
						continue;
					stamps[id] = query_stamp;
					if (n == cap)
					{
						cap = cap ? cap * 2 : 256;
						ids = (int *) realloc (ids, cap * sizeof(int));
					}
					ids[n++] = id;
				}
			}
		}
	}
	*count = n;
	return ids;
}

/* ---------------------------------------------------------------- access */

int ArchWorld_BrushCount (void)	{ return num_live; }
int ArchWorld_Revision (void)	{ return revision; }
int ArchWorld_SlotCount (void)	{ return num_slots; }

const abrush_t *ArchWorld_Brush (int id)
{
	if (id < 0 || id >= num_slots)
		return NULL;
	return brushes[id];
}

double ArchWorld_SolidVolume (void)
{
	double v = 0;
	int i;
	for (i = 0; i < num_slots; i++)
		if (brushes[i])
			v += ArchBrush_Volume (brushes[i]);
	return v;
}

void ArchWorld_SetMaxBrushes (int n)
{
	max_brushes = n;
}

int ArchWorld_TakeDirtyCells (int *out, int max)
{
	int i, n = 0;
	for (i = 0; i < ARCH_NUM_CELLS && n < max; i++)
	{
		if (dirty[i])
		{
			dirty[i] = 0;
			out[n++] = i;
		}
	}
	return n;
}

int ArchWorld_BoxFromCorners (const int a[3], const int b[3], int mins[3], int maxs[3])
{
	int k;
	for (k = 0; k < 3; k++)
	{
		mins[k] = a[k] < b[k] ? a[k] : b[k];
		maxs[k] = a[k] < b[k] ? b[k] : a[k];
		if (mins[k] == maxs[k])
			return 0;
	}
	return 1;
}

/* ----------------------------------------------------------------- edits */

typedef struct { abrush_t **v; int count, cap; } list_t;

static void list_push (list_t *l, abrush_t *b)
{
	if (l->count == l->cap)
	{
		l->cap = l->cap ? l->cap * 2 : 16;
		l->v = (abrush_t **) realloc (l->v, l->cap * sizeof(abrush_t *));
	}
	l->v[l->count++] = b;
}

static void list_free (list_t *l, int free_brushes)
{
	int i;
	if (free_brushes)
		for (i = 0; i < l->count; i++)
			ArchBrush_Free (l->v[i]);
	free (l->v);
	memset (l, 0, sizeof(*l));
}

int ArchWorld_Carve (const abrush_t *shape)
{
	list_t		pieces = {0};
	abrush_t	*limit, *clipped;
	int		*ids, *removed, n, i, k, nremoved = 0;
	int		mn[3] = {-ARCH_HALF + ARCH_SHELL, -ARCH_HALF + ARCH_SHELL, -ARCH_HALF + ARCH_SHELL};
	int		mx[3] = {ARCH_HALF - ARCH_SHELL, ARCH_HALF - ARCH_SHELL, ARCH_HALF - ARCH_SHELL};

	if (!shape)
		return 1;
	/* the outer skin of the build volume is never carved, so the world cannot be opened to the void */
	limit = ArchBrush_Box (mn, mx);
	clipped = ArchBrush_Intersection (shape, limit);
	ArchBrush_Free (limit);
	if (!clipped)
		return 1;
	shape = clipped;
	ids = gather (shape->mins, shape->maxs, 0, &n);
	removed = (int *) malloc ((n ? n : 1) * sizeof(int));
	for (i = 0; i < n; i++)
	{
		abrush_t **out;
		int count;

		if (!ArchBrush_Intersects (brushes[ids[i]], shape))
			continue;
		count = ArchBrush_Subtract (brushes[ids[i]], shape, &out);
		for (k = 0; k < count; k++)
			list_push (&pieces, out[k]);
		free (out);
		removed[nremoved++] = ids[i];
	}
	ArchBrush_Free (clipped);

	if (num_live - nremoved + pieces.count > max_brushes)
	{
		list_free (&pieces, 1);
		free (removed);
		return 0;
	}
	for (i = 0; i < nremoved; i++)
		remove_brush (removed[i]);
	for (i = 0; i < pieces.count; i++)
		insert_brush (pieces.v[i]);
	list_free (&pieces, 0);
	free (removed);
	return 1;
}

static abrush_t *volume_box (void)
{
	int mn[3] = {-ARCH_HALF, -ARCH_HALF, -ARCH_HALF}, mx[3] = {ARCH_HALF, ARCH_HALF, ARCH_HALF};
	return ArchBrush_Box (mn, mx);
}

int ArchWorld_Add (const abrush_t *shape)
{
	list_t		work = {0}, next = {0};
	abrush_t	*vol, *clipped;
	int		*ids, n, i, j, k;

	if (!shape)
		return 1;
	vol = volume_box ();
	clipped = ArchBrush_Intersection (shape, vol);
	ArchBrush_Free (vol);
	if (!clipped)
		return 1;

	/* keep only the part that is currently air, so brushes never overlap */
	ids = gather (clipped->mins, clipped->maxs, 0, &n);
	list_push (&work, clipped);
	for (i = 0; i < n && work.count; i++)
	{
		const abrush_t *solid = brushes[ids[i]];

		for (j = 0; j < work.count; j++)
		{
			abrush_t **out;
			int count = ArchBrush_Subtract (work.v[j], solid, &out);
			for (k = 0; k < count; k++)
				list_push (&next, out[k]);
			free (out);
		}
		list_free (&work, 1);
		work = next;
		memset (&next, 0, sizeof(next));
	}

	if (num_live + work.count > max_brushes)
	{
		list_free (&work, 1);
		return 0;
	}
	for (i = 0; i < work.count; i++)
		insert_brush (work.v[i]);
	list_free (&work, 0);
	return 1;
}

void ArchWorld_Reset (void)
{
	int mn[3] = {-ARCH_ROOM_HALF, -ARCH_ROOM_HALF, -ARCH_ROOM_HALF};
	int mx[3] = {ARCH_ROOM_HALF, ARCH_ROOM_HALF, ARCH_ROOM_HALF};
	abrush_t *room;
	int i;

	for (i = 0; i < num_slots; i++)
		ArchBrush_Free (brushes[i]);
	for (i = 0; i < ARCH_NUM_CELLS; i++)
		cells[i].count = 0;
	num_slots = num_free = num_live = 0;
	revision++;
	max_brushes = ARCH_MAX_BRUSHES;

	insert_brush (volume_box ());
	room = ArchBrush_Box (mn, mx);
	ArchWorld_Carve (room);
	ArchBrush_Free (room);
	memset (dirty, 1, sizeof(dirty));
}

/* --------------------------------------------------------------- queries */

void ArchWorld_Trace (const avec3 start, const avec3 end, const avec3 mins, const avec3 maxs, atrace_t *tr)
{
	avec3	lo, hi;
	int	*ids, n, i, k;

	ArchTrace_Init (tr);
	for (k = 0; k < 3; k++)
	{
		lo[k] = (start[k] < end[k] ? start[k] : end[k]) + mins[k];
		hi[k] = (start[k] < end[k] ? end[k] : start[k]) + maxs[k];
	}
	ids = gather (lo, hi, 1.0, &n);
	for (i = 0; i < n; i++)
	{
		const abrush_t *b = brushes[ids[i]];
		if (b->mins[0] > hi[0] + 1 || b->maxs[0] < lo[0] - 1 ||
		    b->mins[1] > hi[1] + 1 || b->maxs[1] < lo[1] - 1 ||
		    b->mins[2] > hi[2] + 1 || b->maxs[2] < lo[2] - 1)
			continue;
		ArchBrush_TraceBox (b, start, end, mins, maxs, tr);
	}
}

int ArchWorld_PointSolid (const avec3 p)
{
	int	*ids, n, i, f, k, solid = 0;

	for (k = 0; k < 3; k++)
		if (p[k] <= -ARCH_HALF || p[k] >= ARCH_HALF)
			return 1;
	ids = gather (p, p, 1.0, &n);
	for (i = 0; i < n && !solid; i++)
	{
		const abrush_t *b = brushes[ids[i]];
		for (f = 0; f < b->nfaces; f++)
		{
			const aplane_t *pl = &b->faces[f].plane;
			if (p[0] * pl->n[0] + p[1] * pl->n[1] + p[2] * pl->n[2] - pl->d > ARCH_ON_EPS)
				break;
		}
		if (f == b->nfaces)
			solid = 1;
	}
	return solid;
}
