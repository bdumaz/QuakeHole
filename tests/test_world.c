#include <stdlib.h>
#include "t.h"
#include "arch_world.h"

static const avec3 zero = {0, 0, 0};
static const avec3 pmin = {-16, -16, -24}, pmax = {16, 16, 32};
static const double initial_volume = 4096.0 * 4096 * 4096 - 320.0 * 320 * 320;

static abrush_t *wbox (int x1, int y1, int z1, int x2, int y2, int z2)
{
	int mn[3], mx[3];
	mn[0] = x1; mn[1] = y1; mn[2] = z1;
	mx[0] = x2; mx[1] = y2; mx[2] = z2;
	return ArchBrush_Box (mn, mx);
}

static int carve (int x1, int y1, int z1, int x2, int y2, int z2)
{
	abrush_t *b = wbox (x1, y1, z1, x2, y2, z2);
	int r = ArchWorld_Carve (b);
	ArchBrush_Free (b);
	return r;
}

static int add (int x1, int y1, int z1, int x2, int y2, int z2)
{
	abrush_t *b = wbox (x1, y1, z1, x2, y2, z2);
	int r = ArchWorld_Add (b);
	ArchBrush_Free (b);
	return r;
}

/* distance travelled by a ray before it hits something */
static double ray (double x1, double y1, double z1, double x2, double y2, double z2)
{
	avec3 s, e;
	atrace_t tr;
	double dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
	s[0] = x1; s[1] = y1; s[2] = z1;
	e[0] = x2; e[1] = y2; e[2] = z2;
	ArchWorld_Trace (s, e, zero, zero, &tr);
	return tr.fraction * sqrt (dx * dx + dy * dy + dz * dz);
}

static int drain_dirty (void)
{
	static int cells[ARCH_NUM_CELLS];
	return ArchWorld_TakeDirtyCells (cells, ARCH_NUM_CELLS);
}

static void carve_tunnel (void)
{
	CHECK (carve (160, -32, -32, 480, 32, 32) == 1);
}

static void test_initial (void)
{
	avec3 p;
	int axis, side;

	ArchWorld_Reset ();
	CHECK (ArchWorld_BrushCount () == 6);
	CHECK_NEAR (ArchWorld_SolidVolume (), initial_volume, 1.0);
	for (axis = 0; axis < 3; axis++)
	{
		for (side = -1; side <= 1; side += 2)
		{
			double e[3] = {0, 0, 0}, d;
			e[axis] = side * 1000.0;
			d = ray (0, 0, 0, e[0], e[1], e[2]);
			CHECK (d >= 160.0 - 2.0 / 32 && d <= 160.0);
		}
	}
	p[0] = 0; p[1] = 0; p[2] = 0;		CHECK (!ArchWorld_PointSolid (p));
	p[2] = 200;				CHECK (ArchWorld_PointSolid (p));
	p[2] = 160;				CHECK (ArchWorld_PointSolid (p));
	p[0] = 5000; p[2] = 0;			CHECK (ArchWorld_PointSolid (p));
	CHECK (drain_dirty () == ARCH_NUM_CELLS);
	CHECK (drain_dirty () == 0);
}

static void test_carve (void)
{
	double d;
	int n, count;

	ArchWorld_Reset ();
	drain_dirty ();
	carve_tunnel ();
	d = ray (0, 0, 0, 1000, 0, 0);
	CHECK (d >= 480.0 - 2.0 / 32 && d <= 480.0);
	CHECK_NEAR (ArchWorld_SolidVolume (), initial_volume - 320.0 * 64 * 64, 1.0);
	n = drain_dirty ();
	CHECK (n > 0 && n < ARCH_NUM_CELLS);

	/* Review Focus 3: carving the same tunnel again changes nothing */
	count = ArchWorld_BrushCount ();
	carve_tunnel ();
	CHECK (ArchWorld_BrushCount () == count);
	CHECK_NEAR (ArchWorld_SolidVolume (), initial_volume - 320.0 * 64 * 64, 1.0);
	CHECK (drain_dirty () == 0);

	/* Review Focus 3: carving air changes nothing */
	ArchWorld_Reset ();
	drain_dirty ();
	CHECK (carve (-64, -64, -64, 64, 64, 64) == 1);
	CHECK (ArchWorld_BrushCount () == 6);
	CHECK_NEAR (ArchWorld_SolidVolume (), initial_volume, 1.0);
	CHECK (drain_dirty () == 0);

	/* a carve that only touches the wall face changes nothing */
	CHECK (carve (100, -32, -32, 160, 32, 32) == 1);
	CHECK (ArchWorld_BrushCount () == 6);
	CHECK (drain_dirty () == 0);
}

static void test_add (void)
{
	double d;

	ArchWorld_Reset ();
	CHECK (add (-32, -32, -160, 32, 32, 0) == 1);
	CHECK_NEAR (ArchWorld_SolidVolume (), initial_volume + 64.0 * 64 * 160, 1.0);
	d = ray (-100, 0, -80, 100, 0, -80);
	CHECK (d >= 68.0 - 2.0 / 32 && d <= 68.0);	/* stops at x = -32 */

	/* only the part in air is added */
	ArchWorld_Reset ();
	CHECK (add (100, -64, -64, 400, 64, 64) == 1);
	CHECK_NEAR (ArchWorld_SolidVolume (), initial_volume + 60.0 * 128 * 128, 1.0);

	/* wholly outside the build volume */
	ArchWorld_Reset ();
	drain_dirty ();
	CHECK (add (3000, 0, 0, 3100, 64, 64) == 1);
	CHECK (ArchWorld_BrushCount () == 6);
	CHECK_NEAR (ArchWorld_SolidVolume (), initial_volume, 1.0);
	CHECK (drain_dirty () == 0);

	/* adding fills a carved hole back in exactly */
	ArchWorld_Reset ();
	carve_tunnel ();
	CHECK (add (160, -32, -32, 480, 32, 32) == 1);
	CHECK_NEAR (ArchWorld_SolidVolume (), initial_volume, 1.0);
}

/* Review Focus 2: a box sliding over the seam between coplanar brushes must not snag */
static void test_seams (void)
{
	avec3 s, e;
	atrace_t tr;

	ArchWorld_Reset ();
	CHECK (carve (-64, -64, -224, 64, 64, -160) == 1);	/* a pit splits the floor slab */
	CHECK (ArchWorld_BrushCount () > 6);

	s[0] = -140; s[1] = 120; s[2] = -136;
	e[0] = 140; e[1] = 120; e[2] = -136;
	ArchWorld_Trace (s, e, pmin, pmax, &tr);		/* along the floor beside the pit */
	CHECK (tr.fraction == 1.0 && !tr.startsolid);

	s[1] = e[1] = 0;
	ArchWorld_Trace (s, e, pmin, pmax, &tr);		/* across the pit at floor level */
	CHECK (tr.fraction == 1.0 && !tr.startsolid);

	s[0] = 0; s[1] = 0; s[2] = -136;
	e[0] = 0; e[1] = 0; e[2] = -400;
	ArchWorld_Trace (s, e, pmin, pmax, &tr);		/* dropping into the pit */
	CHECK (-136 - 264 * tr.fraction >= -200.0 && -136 - 264 * tr.fraction <= -200.0 + 2.0 / 32);
	CHECK_NEAR (tr.normal[2], 1.0, 1e-9);
}

static void test_limit (void)
{
	int id, live = 0;

	ArchWorld_Reset ();
	ArchWorld_SetMaxBrushes (8);
	CHECK (carve (160, -32, -32, 480, 32, 32) == 0);
	CHECK (ArchWorld_BrushCount () == 6);
	CHECK_NEAR (ArchWorld_SolidVolume (), initial_volume, 1.0);
	CHECK (add (-32, -32, -160, 32, 32, 0) == 1);		/* 7 brushes: still allowed */
	CHECK (add (-100, -100, -160, -90, -90, 0) == 1);	/* 8 */
	CHECK (add (100, 100, -160, 110, 110, 0) == 0);		/* 9: refused */
	CHECK (ArchWorld_BrushCount () == 8);

	ArchWorld_Reset ();	/* restores the real limit */
	carve_tunnel ();
	CHECK (ArchWorld_BrushCount () > 6);
	for (id = 0; id < ArchWorld_SlotCount (); id++)
	{
		if (ArchWorld_Brush (id))
		{
			live++;
			CHECK (ArchWorld_HomeCell (id) >= 0 && ArchWorld_HomeCell (id) < ARCH_NUM_CELLS);
		}
	}
	CHECK (live == ArchWorld_BrushCount ());
}

/* Review Focus 4 */
static void test_corners (void)
{
	int a[3] = {64, 64, 64}, b[3] = {0, 0, 0}, c[3] = {64, 0, 5}, mn[3], mx[3];

	CHECK (ArchWorld_BoxFromCorners (a, b, mn, mx));
	CHECK (mn[0] == 0 && mn[1] == 0 && mn[2] == 0 && mx[0] == 64 && mx[1] == 64 && mx[2] == 64);
	CHECK (!ArchWorld_BoxFromCorners (a, c, mn, mx));	/* zero size on x */
}

void test_world (void)
{
	test_initial ();
	test_carve ();
	test_add ();
	test_seams ();
	test_limit ();
	test_corners ();
	ArchWorld_Reset ();
}
