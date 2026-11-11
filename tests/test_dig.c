#include <stdlib.h>
#include <string.h>
#include "t.h"
#include "arch_world.h"
#include "arch_dig.h"

static int boxes[ARCH_DIG_MAX_BOXES][6];

/* how many grid cubes have their centre within radius of c */
static int brute_count (const avec3 c, double radius)
{
	int x, y, z, n = 0;
	int lo[3], hi[3], k;

	for (k = 0; k < 3; k++)
	{
		lo[k] = (int) floor ((c[k] - radius) / ARCH_DIG_CELL) - 1;
		hi[k] = (int) floor ((c[k] + radius) / ARCH_DIG_CELL) + 1;
	}
	for (z = lo[2]; z <= hi[2]; z++)
		for (y = lo[1]; y <= hi[1]; y++)
			for (x = lo[0]; x <= hi[0]; x++)
			{
				double dx = (x + 0.5) * ARCH_DIG_CELL - c[0], dy = (y + 0.5) * ARCH_DIG_CELL - c[1], dz = (z + 0.5) * ARCH_DIG_CELL - c[2];
				if (dx * dx + dy * dy + dz * dz <= radius * radius)
					n++;
			}
	return n;
}

static void check_crater (double cx, double cy, double cz, double radius)
{
	avec3 c;
	int n, i, j, k, bad_grid = 0, overlap = 0, outside = 0;
	double cells = 0;

	c[0] = cx; c[1] = cy; c[2] = cz;
	n = ArchDig_Crater (c, radius, boxes, ARCH_DIG_MAX_BOXES);
	CHECK (n >= 0 && n <= ARCH_DIG_MAX_BOXES);
	for (i = 0; i < n; i++)
	{
		double vol = 1;
		for (k = 0; k < 3; k++)
		{
			if (boxes[i][k] % ARCH_DIG_CELL || boxes[i][k + 3] % ARCH_DIG_CELL || boxes[i][k + 3] <= boxes[i][k])
				bad_grid++;
			vol *= (boxes[i][k + 3] - boxes[i][k]) / ARCH_DIG_CELL;
			/* no box reaches further than the radius plus one cube from the centre */
			if (boxes[i][k] < c[k] - radius - ARCH_DIG_CELL || boxes[i][k + 3] > c[k] + radius + ARCH_DIG_CELL)
				outside++;
		}
		cells += vol;
		for (j = i + 1; j < n; j++)
		{
			int sep = 0;
			for (k = 0; k < 3; k++)
				if (boxes[i][k] >= boxes[j][k + 3] || boxes[j][k] >= boxes[i][k + 3])
					sep = 1;
			if (!sep)
				overlap++;
		}
	}
	CHECK (bad_grid == 0);
	CHECK (overlap == 0);
	CHECK (outside == 0);
	/* disjoint boxes holding exactly as many cubes as the brute-force count cover exactly those cubes */
	CHECK_NEAR (cells, brute_count (c, radius), 0.5);
}

static void test_crater (void)
{
	avec3 c = {0, 0, 0};

	check_crater (0, 0, 0, 64);
	check_crater (152, 3, -7, 64);
	check_crater (-100.5, 37.25, 12, 48);
	check_crater (8, 8, 8, 16);
	check_crater (1000, -1000, 500, 256);
	check_crater (5, 5, 5, 100);

	/* a radius smaller than half a cube removes at most the one cube the centre is in */
	CHECK (ArchDig_Crater (c, 4, boxes, ARCH_DIG_MAX_BOXES) == 0);
	c[0] = c[1] = c[2] = 8;
	CHECK (ArchDig_Crater (c, 4, boxes, ARCH_DIG_MAX_BOXES) == 1);
	CHECK (boxes[0][0] == 0 && boxes[0][3] == 16);
	/* a buffer that is too small is reported, not overrun */
	CHECK (ArchDig_Crater (c, 64, boxes, 2) == -1);
}

static void dig (double x, double y, double z, double radius)
{
	avec3 c;
	int n, i;

	c[0] = x; c[1] = y; c[2] = z;
	n = ArchDig_Crater (c, radius, boxes, ARCH_DIG_MAX_BOXES);
	for (i = 0; i < n; i++)
	{
		abrush_t *b = ArchBrush_Box (boxes[i], boxes[i] + 3);
		ArchWorld_Carve (b);
		ArchBrush_Free (b);
	}
}

/* solid cubes among those the crater covers, counted before digging */
static int solid_cells (double cx, double cy, double cz, double radius)
{
	avec3 c, p;
	int n, i, x, y, z, count = 0;

	c[0] = cx; c[1] = cy; c[2] = cz;
	n = ArchDig_Crater (c, radius, boxes, ARCH_DIG_MAX_BOXES);
	for (i = 0; i < n; i++)
		for (z = boxes[i][2]; z < boxes[i][5]; z += ARCH_DIG_CELL)
			for (y = boxes[i][1]; y < boxes[i][4]; y += ARCH_DIG_CELL)
				for (x = boxes[i][0]; x < boxes[i][3]; x += ARCH_DIG_CELL)
				{
					p[0] = x + 8; p[1] = y + 8; p[2] = z + 8;
					if (ArchWorld_PointSolid (p))
						count++;
				}
	return count;
}

static void test_crater_in_world (void)
{
	double before, cube = (double) ARCH_DIG_CELL * ARCH_DIG_CELL * ARCH_DIG_CELL;
	avec3 p;
	atrace_t tr;
	avec3 s, e, zero = {0, 0, 0};
	int expected, count;

	/* a rocket explodes 8 units in front of the +x wall */
	ArchWorld_Reset ();
	before = ArchWorld_SolidVolume ();
	expected = solid_cells (152, 0, 0, 64);
	CHECK (expected > 50);
	dig (152, 0, 0, 64);
	CHECK_NEAR (before - ArchWorld_SolidVolume (), expected * cube, 1.0);
	count = ArchWorld_BrushCount ();
	CHECK (count > 6 && count < 6 + 250);	/* fragmentation bound, spec section 6 */
	p[0] = 176; p[1] = 0; p[2] = 0;
	CHECK (!ArchWorld_PointSolid (p));	/* the middle of the crater is open */

	/* a second blast in the same place digs deeper, never less */
	before = ArchWorld_SolidVolume ();
	dig (200, 0, 0, 64);
	CHECK (ArchWorld_SolidVolume () < before);
	CHECK (ArchWorld_BrushCount () < 6 + 500);

	/* open air: nothing happens */
	ArchWorld_Reset ();
	before = ArchWorld_SolidVolume ();
	dig (0, 0, 0, 64);
	CHECK (ArchWorld_BrushCount () == 6);
	CHECK_NEAR (ArchWorld_SolidVolume (), before, 1.0);

	/* at the world shell: the world stays closed */
	{
		int mn[3] = {160, -32, -32}, mx[3] = {3000, 32, 32};
		abrush_t *b = ArchBrush_Box (mn, mx);
		ArchWorld_Carve (b);
		ArchBrush_Free (b);
	}
	dig (ARCH_HALF - ARCH_SHELL - 8, 0, 0, 64);
	s[0] = 0; s[1] = 0; s[2] = 0;
	e[0] = 4000; e[1] = 0; e[2] = 0;
	ArchWorld_Trace (s, e, zero, zero, &tr);
	CHECK (tr.fraction * 4000 <= ARCH_HALF - ARCH_SHELL);
	ArchWorld_Reset ();
}

static void test_cells (void)
{
	avec3 p;
	int cell[3], mn[3], mx[3];

	p[0] = 161; p[1] = -0.5; p[2] = 31.9;
	ArchDig_CellAt (p, cell);
	CHECK (cell[0] == 10 && cell[1] == -1 && cell[2] == 1);
	ArchDig_CellBox (cell, mn, mx);
	CHECK (mn[0] == 160 && mx[0] == 176 && mn[1] == -16 && mx[1] == 0 && mn[2] == 16 && mx[2] == 32);
}

/* which cube a beam is working on */
static void test_beam_cell (void)
{
	avec3 end, dir = {1, 0, 0};
	int cell[3], mn[3] = {160, 0, -128}, mx[3] = {176, 16, -112};
	abrush_t *b;

	ArchWorld_Reset ();
	/* an ordinary hit on the +x wall */
	end[0] = 160 - 1.0 / 32; end[1] = 5; end[2] = -114;
	CHECK (ArchDig_BeamCell (end, dir, ArchWorld_PointSolid, cell) == 1);
	CHECK (cell[0] == 10 && cell[1] == 0 && cell[2] == -8);

	/* a beam ending in open air has no target */
	end[0] = 0;
	CHECK (ArchDig_BeamCell (end, dir, ArchWorld_PointSolid, cell) == 0);

	/* From the spawn pose the beam runs exactly along the grid line y = 0. Once the
	   cube on one side of that line is gone, the target must be the solid cube on
	   the other side, not the empty one again. */
	b = ArchBrush_Box (mn, mx);
	ArchWorld_Carve (b);
	ArchBrush_Free (b);
	end[0] = 160 - 1.0 / 32; end[1] = 0; end[2] = -114;
	CHECK (ArchDig_BeamCell (end, dir, ArchWorld_PointSolid, cell) == 1);
	CHECK (cell[0] == 10 && cell[1] == -1 && cell[2] == -8);

	/* on a grid corner with three of the four cubes gone, the last one is found */
	mn[1] = -16; mx[1] = 16; mn[2] = -112; mx[2] = -96;
	b = ArchBrush_Box (mn, mx);
	ArchWorld_Carve (b);
	ArchBrush_Free (b);
	end[2] = -112;
	CHECK (ArchDig_BeamCell (end, dir, ArchWorld_PointSolid, cell) == 1);
	CHECK (cell[0] == 10 && cell[1] == -1 && cell[2] == -8);
	ArchWorld_Reset ();
}

static void test_dwell (void)
{
	adwell_t d;
	int a[3] = {10, 0, 0}, b[3] = {10, 1, 0};

	ArchDig_DwellReset (&d);
	CHECK (ArchDig_Dwell (&d, a, 10.0, 1.0) == 0);	/* first report starts the timer */
	CHECK (ArchDig_Dwell (&d, a, 10.2, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, a, 10.4, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, a, 10.6, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, a, 10.8, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, a, 11.0, 1.0) == 1);	/* held for the full time */
	CHECK (ArchDig_Dwell (&d, a, 11.1, 1.0) == 0);	/* the timer restarted */
	CHECK (ArchDig_Dwell (&d, a, 11.3, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, a, 11.5, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, a, 11.7, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, a, 11.9, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, a, 12.1, 1.0) == 1);

	/* changing cube restarts */
	ArchDig_DwellReset (&d);
	ArchDig_Dwell (&d, a, 20.0, 1.0);
	ArchDig_Dwell (&d, a, 20.2, 1.0);
	ArchDig_Dwell (&d, a, 20.4, 1.0);
	ArchDig_Dwell (&d, a, 20.6, 1.0);
	CHECK (ArchDig_Dwell (&d, b, 20.8, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, b, 21.0, 1.0) == 0);	/* would have fired had it stayed on a */
	CHECK (ArchDig_Dwell (&d, b, 21.2, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, b, 21.4, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, b, 21.6, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, b, 21.8, 1.0) == 1);

	/* a gap in the reports (the player stopped firing) restarts */
	ArchDig_DwellReset (&d);
	ArchDig_Dwell (&d, a, 30.0, 1.0);
	ArchDig_Dwell (&d, a, 30.2, 1.0);
	CHECK (ArchDig_Dwell (&d, a, 30.9, 1.0) == 0);	/* 0.7 s gap */
	CHECK (ArchDig_Dwell (&d, a, 31.1, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, a, 31.3, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, a, 31.5, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, a, 31.7, 1.0) == 0);
	CHECK (ArchDig_Dwell (&d, a, 31.9, 1.0) == 1);

	/* no target resets */
	ArchDig_DwellReset (&d);
	ArchDig_Dwell (&d, a, 40.0, 1.0);
	ArchDig_Dwell (&d, a, 40.2, 1.0);
	ArchDig_Dwell (&d, a, 40.4, 1.0);
	ArchDig_Dwell (&d, a, 40.6, 1.0);
	ArchDig_Dwell (&d, a, 40.8, 1.0);
	ArchDig_DwellReset (&d);
	CHECK (ArchDig_Dwell (&d, a, 41.0, 1.0) == 0);
	CHECK (ArchDig_DwellProgress (&d, 41.5, 1.0) > 0.45 && ArchDig_DwellProgress (&d, 41.5, 1.0) < 0.55);
}

static int feed_explosion (adigparse_t *p, float x, float y, float z, float out[6])
{
	int r = 0;
	r |= ArchDigParse_Byte (p, 0, 23, out);		/* svc_temp_entity */
	r |= ArchDigParse_Byte (p, 0, 3, out);		/* TE_EXPLOSION */
	r |= ArchDigParse_Coord (p, 0, x, out);
	r |= ArchDigParse_Coord (p, 0, y, out);
	r |= ArchDigParse_Coord (p, 0, z, out);
	return r;
}

static void test_parser (void)
{
	adigparse_t p;
	float out[6];
	int r;

	ArchDigParse_Reset (&p);
	CHECK (ArchDigParse_Byte (&p, 0, 23, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Byte (&p, 0, 3, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, 1.5f, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, -2.5f, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, 3.0f, out) == ARCH_DIG_BLAST);
	CHECK (out[0] == 1.5f && out[1] == -2.5f && out[2] == 3.0f);

	/* a lightning report: entity, start, end */
	CHECK (ArchDigParse_Byte (&p, 0, 23, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Byte (&p, 0, 6, out) == ARCH_DIG_NONE);	/* TE_LIGHTNING2 */
	CHECK (ArchDigParse_Entity (&p, 0, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, 1, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, 2, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, 3, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, 4, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, 5, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, 6, out) == ARCH_DIG_BEAM);
	CHECK (out[0] == 1 && out[2] == 3 && out[3] == 4 && out[5] == 6);

	/* other temp-entity types are ignored, and the parser is ready for the next report */
	CHECK (ArchDigParse_Byte (&p, 0, 23, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Byte (&p, 0, 2, out) == ARCH_DIG_NONE);	/* TE_GUNSHOT */
	CHECK (ArchDigParse_Coord (&p, 0, 9, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, 9, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, 9, out) == ARCH_DIG_NONE);
	CHECK (feed_explosion (&p, 7, 8, 9, out) == ARCH_DIG_BLAST && out[0] == 7);

	/* other destinations are ignored entirely, even in the middle of a report */
	CHECK (ArchDigParse_Byte (&p, 1, 23, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Byte (&p, 1, 3, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 1, 1, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 1, 1, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 1, 1, out) == ARCH_DIG_NONE);

	/* a truncated report followed by a whole one */
	ArchDigParse_Byte (&p, 0, 23, out);
	ArchDigParse_Byte (&p, 0, 3, out);
	ArchDigParse_Coord (&p, 0, 1, out);
	r = feed_explosion (&p, 4, 5, 6, out);
	CHECK (r == ARCH_DIG_BLAST && out[0] == 4 && out[1] == 5 && out[2] == 6);

	/* a lightning report missing its entity is dropped */
	ArchDigParse_Byte (&p, 0, 23, out);
	ArchDigParse_Byte (&p, 0, 6, out);
	CHECK (ArchDigParse_Coord (&p, 0, 1, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, 1, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, 1, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, 1, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, 1, out) == ARCH_DIG_NONE);
	CHECK (ArchDigParse_Coord (&p, 0, 1, out) == ARCH_DIG_NONE);
}

void test_dig (void)
{
	test_crater ();
	test_crater_in_world ();
	test_cells ();
	test_beam_cell ();
	test_dwell ();
	test_parser ();
}
