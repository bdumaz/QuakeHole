/* The inside of dug holes: which brush faces are drawn, with which look, rebuilt
   only where a dig happened. Checked against a full scan done the old way. */
#include <stdlib.h>
#include <string.h>
#include "t.h"
#include "arch_world.h"
#include "arch_dig.h"
#include "arch_holes.h"

typedef struct { double mins[3], maxs[3]; int look; } rec_t;
static rec_t	recs[4096];
static int	nrecs;

/* the old rule, by brute force: every face reaching into a record, newest record wins;
   a face's own tag decides its look when it has one */
static void brute_counts (int counts[ARCH_HOLES_MAX_LOOKS])
{
	int id, i, v, k, r;

	memset (counts, 0, sizeof(int) * ARCH_HOLES_MAX_LOOKS);
	for (id = 0; id < ArchWorld_SlotCount (); id++)
	{
		const abrush_t *b = ArchWorld_Brush (id);
		if (!b || b->locked)
			continue;
		for (i = 0; i < b->nfaces; i++)
		{
			const aface_t *f = &b->faces[i];
			double fmin[3], fmax[3];
			int look = 0;

			for (k = 0; k < 3; k++)
				fmin[k] = fmax[k] = f->verts[0][k];
			for (v = 1; v < f->nverts; v++)
				for (k = 0; k < 3; k++)
				{
					if (f->verts[v][k] < fmin[k]) fmin[k] = f->verts[v][k];
					if (f->verts[v][k] > fmax[k]) fmax[k] = f->verts[v][k];
				}
			for (r = nrecs - 1; r >= 0 && !look; r--)
			{
				for (k = 0; k < 3; k++)
					if (fmin[k] > recs[r].maxs[k] + 1 || fmax[k] < recs[r].mins[k] - 1)
						break;
				if (k == 3)
					look = recs[r].look;
			}
			if (!look)
				continue;
			if (f->plane.tag >= 1 && f->plane.tag < ARCH_HOLES_MAX_LOOKS)
				look = f->plane.tag;
			counts[look] += 3 * (f->nverts - 2);
		}
	}
}

static void mesh_counts (int counts[ARCH_HOLES_MAX_LOOKS])
{
	int c, s;

	memset (counts, 0, sizeof(int) * ARCH_HOLES_MAX_LOOKS);
	for (c = 0; c < ARCH_NUM_CELLS; c++)
	{
		const aholecell_t *cell = ArchHoles_Cell (c);
		for (s = 0; cell && s < cell->nsegs; s++)
			counts[cell->segs[s].look] += cell->segs[s].count;
	}
}

/* one crater, carved and recorded the way the game does it */
static void blast (double x, double y, double z, int look)
{
	static int boxes[ARCH_DIG_MAX_BOXES][6];
	avec3 c;
	double lo[3], hi[3];
	int n, i, k;

	c[0] = x; c[1] = y; c[2] = z;
	n = ArchDig_Crater (c, 64, boxes, ARCH_DIG_MAX_BOXES);
	for (i = 0; i < n; i++)
	{
		abrush_t *b = ArchBrush_Box (boxes[i], boxes[i] + 3);
		for (k = 0; k < b->nfaces; k++)
			b->faces[k].plane.tag = look;
		ArchWorld_Carve (b);
		ArchBrush_Free (b);
	}
	for (k = 0; k < 3; k++)
	{
		lo[k] = c[k] - 80;
		hi[k] = c[k] + 80;
	}
	ArchHoles_NoteDig (lo, hi, look);
	/* the brute force sees records exactly as the module keeps them, merges included */
	nrecs = ArchHoles_RecordCount ();
	for (i = 0; i < nrecs; i++)
		ArchHoles_Record (i, recs[i].mins, recs[i].maxs, &recs[i].look);
}

void test_holes (void)
{
	int bmn[3] = {-1024, -1024, -512}, bmx[3] = {1024, 1024, 512};
	int smn[3] = {-1024, -1024, -512}, smx[3] = {1024, 1024, 512};
	int rmn[3] = {-600, -600, -128}, rmx[3] = {600, 600, 128};
	int got[ARCH_HOLES_MAX_LOOKS], want[ARCH_HOLES_MAX_LOOKS];
	int i, examined, total, mismatches = 0, look;
	abrush_t *room;

	ArchWorld_Select (ARCH_WORLD_CAMPAIGN);
	ArchWorld_BeginCustom (bmn, bmx);
	ArchWorld_Insert (ArchBrush_Box (smn, smx));
	room = ArchBrush_Box (rmn, rmx);
	ArchWorld_Carve (room);
	ArchBrush_Free (room);
	ArchHoles_Reset (1);
	for (look = 1; look < 5; look++)
		ArchHoles_SetLookSize (look, 64, 64);

	/* nothing dug: nothing drawn, and updating does no work */
	CHECK (ArchHoles_Update () == 0);
	mesh_counts (got);
	CHECK (got[1] + got[2] + got[3] + got[4] == 0);

	/* forty craters in the room's walls, floor and ceiling */
	srand (11);
	for (i = 0; i < 40; i++)
	{
		double x = -560 + rand () % 1120, y = -560 + rand () % 1120;
		int side = rand () % 4;
		if (side == 0) x = 590;
		if (side == 1) y = -590;
		blast (x, y, side == 2 ? 120 : side == 3 ? -120 : (rand () % 200) - 100, 1 + i % 4);
		ArchHoles_Update ();
		brute_counts (want);
		mesh_counts (got);
		if (memcmp (got, want, sizeof(got)))
			mismatches++;
	}
	CHECK (mismatches == 0);
	CHECK (ArchHoles_RecordCount () >= 10);

	/* one more crater looks at the brushes near it, not the whole world */
	total = ArchWorld_BrushCount ();
	blast (590, 0, 0, 2);
	examined = ArchHoles_Update ();
	CHECK (examined > 0);
	CHECK (examined * 5 < total);
	brute_counts (want);
	mesh_counts (got);
	CHECK (!memcmp (got, want, sizeof(got)));

	/* the newest record decides what a point in an existing hole looks like */
	{
		double p[3] = {590, 0, 0}, q[3] = {0, 0, 0};
		CHECK (ArchHoles_LookAt (p) == 2);
		CHECK (ArchHoles_LookAt (q) == 0);
	}

	/* a cell's mesh bounds cover its triangles */
	{
		int c, s, bad = 0;
		for (c = 0; c < ARCH_NUM_CELLS; c++)
		{
			const aholecell_t *cell = ArchHoles_Cell (c);
			for (s = 0; cell && s < cell->nverts; s++)
			{
				const float *v = cell->verts + s * ARCH_HOLES_VERT_FLOATS;
				int k;
				for (k = 0; k < 3; k++)
					if (v[k] < cell->mins[k] - 0.01f || v[k] > cell->maxs[k] + 0.01f)
						bad++;
			}
		}
		CHECK (bad == 0);
	}

	/* reset empties it */
	ArchHoles_Reset (1);
	CHECK (ArchHoles_RecordCount () == 0);
	mesh_counts (got);
	CHECK (got[1] + got[2] + got[3] + got[4] == 0);
	ArchWorld_Select (ARCH_WORLD_SANDBOX);
}
