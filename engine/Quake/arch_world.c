/*
QuakeHole - the brush world.
No engine dependencies.

There are two worlds: the sandbox level the player builds, and the brushes
converted from whichever campaign map is loaded. One is current at a time;
every call below works on the current one.

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

typedef struct
{
	abrush_t	**brushes;		/* indexed by id; NULL = free slot */
	unsigned	*stamps;		/* per id: last query that visited it */
	int		num_slots, cap_slots;
	int		*free_ids;
	int		num_free, cap_free;
	int		num_live;
	int		revision;
	int		max_brushes;
	unsigned	query_stamp;

	int		bmins[3], bmaxs[3];	/* the world's bounds; everything outside is solid */
	int		gcell[3];		/* size of one grid cell on each axis */

	cell_t		cells[ARCH_NUM_CELLS];
	unsigned char	dirty[ARCH_NUM_CELLS];
} aworld_t;

static aworld_t	worlds[ARCH_NUM_WORLDS];
static aworld_t	*W = &worlds[ARCH_WORLD_SANDBOX];

void ArchWorld_Select (int which)
{
	if (which >= 0 && which < ARCH_NUM_WORLDS)
		W = &worlds[which];
}

/* ------------------------------------------------------------------ grid */

static int cell_coord (double v, int axis)
{
	int c = W->gcell[axis] > 0 ? (int) floor ((v - W->bmins[axis]) / W->gcell[axis]) : 0;
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
		lo[k] = cell_coord (mins[k] - pad, k);
		hi[k] = cell_coord (maxs[k] + pad, k);
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

	if (W->num_free)
		id = W->free_ids[--W->num_free];
	else
	{
		if (W->num_slots == W->cap_slots)
		{
			W->cap_slots = W->cap_slots ? W->cap_slots * 2 : 256;
			W->brushes = (abrush_t **) realloc (W->brushes, W->cap_slots * sizeof(abrush_t *));
			W->stamps = (unsigned *) realloc (W->stamps, W->cap_slots * sizeof(unsigned));
		}
		id = W->num_slots++;
	}
	ArchBrush_BuildBevels (b);
	W->brushes[id] = b;
	W->stamps[id] = 0;
	W->num_live++;
	W->revision++;

	brush_cells (b, lo, hi);
	for (z = lo[2]; z <= hi[2]; z++)
		for (y = lo[1]; y <= hi[1]; y++)
			for (x = lo[0]; x <= hi[0]; x++)
				cell_add (&W->cells[CELL_INDEX (x, y, z)], id);
	W->dirty[CELL_INDEX (lo[0], lo[1], lo[2])] = 1;
	return id;
}

static void remove_brush (int id)
{
	abrush_t *b = W->brushes[id];
	int lo[3], hi[3], x, y, z;

	brush_cells (b, lo, hi);
	for (z = lo[2]; z <= hi[2]; z++)
		for (y = lo[1]; y <= hi[1]; y++)
			for (x = lo[0]; x <= hi[0]; x++)
				cell_remove (&W->cells[CELL_INDEX (x, y, z)], id);
	W->dirty[CELL_INDEX (lo[0], lo[1], lo[2])] = 1;

	ArchBrush_Free (b);
	W->brushes[id] = NULL;
	W->num_live--;
	W->revision++;
	if (W->num_free == W->cap_free)
	{
		W->cap_free = W->cap_free ? W->cap_free * 2 : 256;
		W->free_ids = (int *) realloc (W->free_ids, W->cap_free * sizeof(int));
	}
	W->free_ids[W->num_free++] = id;
}

/* ids of live brushes whose cells overlap the box. The result is a shared
   scratch array, valid until the next call; edits must not start until the
   caller has finished reading it. */
static int *gather (const avec3 mins, const avec3 maxs, double pad, int *count)
{
	static int *ids, cap;
	int lo[3], hi[3], x, y, z, i, n = 0;

	if (++W->query_stamp == 0)
	{
		memset (W->stamps, 0, W->num_slots * sizeof(unsigned));
		W->query_stamp = 1;
	}
	cell_range (mins, maxs, pad, lo, hi);
	for (z = lo[2]; z <= hi[2]; z++)
	{
		for (y = lo[1]; y <= hi[1]; y++)
		{
			for (x = lo[0]; x <= hi[0]; x++)
			{
				const cell_t *c = &W->cells[CELL_INDEX (x, y, z)];
				for (i = 0; i < c->count; i++)
				{
					int id = c->ids[i];
					if (W->stamps[id] == W->query_stamp)
						continue;
					W->stamps[id] = W->query_stamp;
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

void ArchWorld_CellRange (const avec3 mins, const avec3 maxs, int lo[3], int hi[3])
{
	cell_range (mins, maxs, 1.0, lo, hi);
}

const int *ArchWorld_Query (const avec3 mins, const avec3 maxs, int *count)
{
	return gather (mins, maxs, 1.0, count);
}

/* empties the current world and gives it new bounds */
static void clear_world (const int mins[3], const int maxs[3])
{
	int i, k;

	for (i = 0; i < W->num_slots; i++)
		ArchBrush_Free (W->brushes[i]);
	for (i = 0; i < ARCH_NUM_CELLS; i++)
		W->cells[i].count = 0;
	W->num_slots = W->num_free = W->num_live = 0;
	W->revision++;
	W->max_brushes = ARCH_MAX_BRUSHES;
	for (k = 0; k < 3; k++)
	{
		W->bmins[k] = mins[k];
		W->bmaxs[k] = maxs[k];
		W->gcell[k] = (maxs[k] - mins[k] + ARCH_GRID - 1) / ARCH_GRID;
		if (W->gcell[k] < 1)
			W->gcell[k] = 1;
	}
	memset (W->dirty, 1, sizeof(W->dirty));
}

/* ---------------------------------------------------------------- access */

int ArchWorld_BrushCount (void)	{ return W->num_live; }
int ArchWorld_Revision (void)	{ return W->revision; }
int ArchWorld_SlotCount (void)	{ return W->num_slots; }

const abrush_t *ArchWorld_Brush (int id)
{
	if (id < 0 || id >= W->num_slots)
		return NULL;
	return W->brushes[id];
}

double ArchWorld_SolidVolume (void)
{
	double v = 0;
	int i;
	for (i = 0; i < W->num_slots; i++)
		if (W->brushes[i])
			v += ArchBrush_Volume (W->brushes[i]);
	return v;
}

void ArchWorld_SetMaxBrushes (int n)
{
	W->max_brushes = n;
}

int ArchWorld_TakeDirtyCells (int *out, int max)
{
	int i, n = 0;
	for (i = 0; i < ARCH_NUM_CELLS && n < max; i++)
	{
		if (W->dirty[i])
		{
			W->dirty[i] = 0;
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

/* the world's bounds, pulled in by inset on every side */
static abrush_t *bounds_box (int inset)
{
	int mn[3], mx[3], k;

	for (k = 0; k < 3; k++)
	{
		mn[k] = W->bmins[k] + inset;
		mx[k] = W->bmaxs[k] - inset;
	}
	return ArchBrush_Box (mn, mx);
}

int ArchWorld_Carve (const abrush_t *shape)
{
	list_t		pieces = {0};
	abrush_t	*limit, *clipped;
	int		*ids, *removed, n, i, k, nremoved = 0;

	if (!shape)
		return 1;
	/* the outer skin of the world is never carved, so it cannot be opened to the void */
	limit = bounds_box (ARCH_SHELL);
	if (!limit)
		return 1;
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

		if (W->brushes[ids[i]]->locked || !ArchBrush_Intersects (W->brushes[ids[i]], shape))
			continue;
		count = ArchBrush_Subtract (W->brushes[ids[i]], shape, &out);
		for (k = 0; k < count; k++)
			list_push (&pieces, out[k]);
		free (out);
		removed[nremoved++] = ids[i];
	}
	ArchBrush_Free (clipped);

	if (W->num_live - nremoved + pieces.count > W->max_brushes)
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

int ArchWorld_Add (const abrush_t *shape)
{
	list_t		work = {0}, next = {0};
	abrush_t	*vol, *clipped;
	int		*ids, n, i, j, k;

	if (!shape)
		return 1;
	vol = bounds_box (0);
	if (!vol)
		return 1;
	clipped = ArchBrush_Intersection (shape, vol);
	ArchBrush_Free (vol);
	if (!clipped)
		return 1;

	/* keep only the part that is currently air, so brushes never overlap */
	ids = gather (clipped->mins, clipped->maxs, 0, &n);
	list_push (&work, clipped);
	for (i = 0; i < n && work.count; i++)
	{
		const abrush_t *solid = W->brushes[ids[i]];

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

	if (W->num_live + work.count > W->max_brushes)
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
	int bmn[3] = {-ARCH_HALF, -ARCH_HALF, -ARCH_HALF}, bmx[3] = {ARCH_HALF, ARCH_HALF, ARCH_HALF};
	int mn[3] = {-ARCH_ROOM_HALF, -ARCH_ROOM_HALF, -ARCH_ROOM_HALF};
	int mx[3] = {ARCH_ROOM_HALF, ARCH_ROOM_HALF, ARCH_ROOM_HALF};
	abrush_t *room;

	clear_world (bmn, bmx);
	insert_brush (bounds_box (0));
	room = ArchBrush_Box (mn, mx);
	ArchWorld_Carve (room);
	ArchBrush_Free (room);
	memset (W->dirty, 1, sizeof(W->dirty));
}

void ArchWorld_BeginCustom (const int mins[3], const int maxs[3])
{
	clear_world (mins, maxs);
}

void ArchWorld_Insert (abrush_t *brush)
{
	if (brush)
		insert_brush (brush);
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
		const abrush_t *b = W->brushes[ids[i]];
		if (b->mins[0] > hi[0] + 1 || b->maxs[0] < lo[0] - 1 ||
		    b->mins[1] > hi[1] + 1 || b->maxs[1] < lo[1] - 1 ||
		    b->mins[2] > hi[2] + 1 || b->maxs[2] < lo[2] - 1)
			continue;
		ArchBrush_TraceBox (b, start, end, mins, maxs, tr);
	}
}

void ArchWorld_MoveFloat (const float start[3], const float end[3], const float mins[3], const float maxs[3],
			  atrace_t *tr, float endpos[3])
{
	avec3		s, e, mn, mx, p;
	atrace_t	check;
	double		f;
	int		k, tries;

	for (k = 0; k < 3; k++)
	{
		s[k] = start[k]; e[k] = end[k];
		mn[k] = mins[k]; mx[k] = maxs[k];
		endpos[k] = end[k];
	}
	ArchWorld_Trace (s, e, mn, mx, tr);
	if (tr->fraction >= 1 || tr->startsolid)
		return;

	f = tr->fraction;
	for (tries = 0; tries < 12; tries++)
	{
		for (k = 0; k < 3; k++)
		{
			endpos[k] = f > 0 ? (float) (s[k] + f * (e[k] - s[k])) : start[k];
			p[k] = endpos[k];
		}
		if (f <= 0)
			break;		/* back at the start, which was clear */
		ArchWorld_Trace (p, p, mn, mx, &check);
		if (!check.startsolid)
			break;
		f -= 0.1;
		if (f < 0)
			f = 0;
	}
	tr->fraction = f;
}

int ArchWorld_PointSolid (const avec3 p)
{
	int	*ids, n, i, f, k, solid = 0;

	for (k = 0; k < 3; k++)
		if (p[k] <= W->bmins[k] || p[k] >= W->bmaxs[k])
			return 1;
	ids = gather (p, p, 1.0, &n);
	for (i = 0; i < n && !solid; i++)
	{
		const abrush_t *b = W->brushes[ids[i]];
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
