/* Stage 5: bevel planes, the two worlds, and turning a collision tree into brushes. */
#include <stdlib.h>
#include <string.h>
#include "t.h"
#include "arch_world.h"
#include "arch_bsp.h"

static abrush_t *cbox (int x1, int y1, int z1, int x2, int y2, int z2)
{
	int mn[3], mx[3];
	mn[0] = x1; mn[1] = y1; mn[2] = z1; mx[0] = x2; mx[1] = y2; mx[2] = z2;
	return ArchBrush_Box (mn, mx);
}

/* a box swept at the sharp corner of a wedge must stop where it really touches */
static void test_bevels (void)
{
	int tri[3][3] = {{0, 0, 0}, {64, 0, 0}, {0, 64, 0}}, ex[3] = {0, 0, 64};
	abrush_t *wedge = ArchBrush_Prism (tri, 3, ex);
	avec3 mn = {-16, -16, -16}, mx = {16, 16, 16}, zero = {0, 0, 0};
	avec3 s = {200, -10, 32}, e = {0, -10, 32};
	atrace_t tr;
	double x;

	/* the corner at (64, 0): a 32-wide box centred at y = -10 first touches it at x = 80 */
	ArchTrace_Init (&tr);
	ArchBrush_TraceBox (wedge, s, e, mn, mx, &tr);
	x = 200 - 200 * tr.fraction;
	CHECK (x >= 80.0 && x <= 80.0 + 2.0 / 32);
	CHECK_NEAR (tr.normal[0], 1.0, 1e-9);

	/* the sloping face itself is unaffected: a box coming straight at its middle */
	s[0] = 100; s[1] = 100; e[0] = 0; e[1] = 0;
	ArchTrace_Init (&tr);
	ArchBrush_TraceBox (wedge, s, e, mn, mx, &tr);
	CHECK_NEAR (tr.normal[0], 0.70710678, 1e-6);
	x = 100 - 100 * tr.fraction;
	CHECK_NEAR (x, 32 + 16, 0.1);		/* box centre 16 out from the face along both axes */

	/* rays are untouched by the bevels */
	s[0] = 64; s[1] = 64; e[0] = 0; e[1] = 0;
	ArchTrace_Init (&tr);
	ArchBrush_TraceBox (wedge, s, e, zero, zero, &tr);
	CHECK_NEAR (64 - 64 * tr.fraction, 32.0, 0.1);
	ArchBrush_Free (wedge);
}

/* Walking down a slope: after the engine clips the velocity to the slope, the move
   is parallel to it apart from rounding error. That must not count as a hit. */
static void test_slide_along_slope (void)
{
	/* a ramp dropping 16 over 64 toward +y, like the first one in E1M1 */
	int tri[3][3] = {{0, 0, 0}, {0, 64, 0}, {0, 0, 16}}, ex[3] = {64, 0, 0};
	abrush_t *ramp = ArchBrush_Prism (tri, 3, ex);
	avec3 mn = {-16, -16, -24}, mx = {16, 16, 32};
	avec3 s = {32, 24, 80}, e = {32, 24, 0}, e2;
	atrace_t tr;
	int i, blocked = 0;

	CHECK (ramp != NULL);
	ArchTrace_Init (&tr);
	ArchBrush_TraceBox (ramp, s, e, mn, mx, &tr);	/* drop onto the slope */
	CHECK (tr.fraction > 0 && tr.fraction < 1 && tr.normal[2] > 0.9 && tr.normal[1] > 0.2);
	s[2] = 80 - 80 * tr.fraction;

	/* twenty steps down the slope, each one parallel to it but off by a rounding error into the surface */
	for (i = 0; i < 20; i++)
	{
		e2[0] = s[0]; e2[1] = s[1] + 1.0; e2[2] = s[2] - 0.25 - 1e-5;
		ArchTrace_Init (&tr);
		ArchBrush_TraceBox (ramp, s, e2, mn, mx, &tr);
		if (tr.fraction < 1)
			blocked++;
		s[1] += tr.fraction * 1.0;
		s[2] += tr.fraction * (-0.25 - 1e-5);
	}
	CHECK (blocked == 0);

	/* a move that really goes into the slope is still stopped, and without sinking into it */
	e2[0] = s[0]; e2[1] = s[1]; e2[2] = s[2] - 8;
	ArchTrace_Init (&tr);
	ArchBrush_TraceBox (ramp, s, e2, mn, mx, &tr);
	CHECK (tr.fraction < 0.01 && !tr.startsolid);
	ArchBrush_Free (ramp);
}

static void test_two_worlds (void)
{
	int mn[3] = {-600, -400, -600}, mx[3] = {1600, 3100, 300};
	avec3 s, e, zero = {0, 0, 0}, p;
	atrace_t tr;

	ArchWorld_Select (ARCH_WORLD_SANDBOX);
	ArchWorld_Reset ();
	CHECK (ArchWorld_BrushCount () == 6);

	ArchWorld_Select (ARCH_WORLD_CAMPAIGN);
	ArchWorld_BeginCustom (mn, mx);
	CHECK (ArchWorld_BrushCount () == 0);
	ArchWorld_Insert (cbox (1000, 2500, 0, 1100, 2600, 100));	/* far outside the sandbox's +-2048 */
	CHECK (ArchWorld_BrushCount () == 1);

	s[0] = 900; s[1] = 2550; s[2] = 50;
	e[0] = 1300; e[1] = 2550; e[2] = 50;
	ArchWorld_Trace (s, e, zero, zero, &tr);
	CHECK (tr.fraction * 400 >= 100.0 - 2.0 / 32 && tr.fraction * 400 <= 100.0);
	CHECK (!ArchWorld_PointSolid (s));		/* open space, although y is beyond 2048 */
	p[0] = 1050; p[1] = 2550; p[2] = 50;
	CHECK (ArchWorld_PointSolid (p));
	p[0] = 5000;
	CHECK (ArchWorld_PointSolid (p));		/* outside this world's bounds */

	/* a carve respects this world's own shell: the brush beside the +y edge keeps its outer 16 units */
	ArchWorld_Insert (cbox (0, 3000, 0, 100, 3100, 100));
	{
		abrush_t *cut = cbox (-50, 2900, -50, 150, 3200, 150);
		CHECK (ArchWorld_Carve (cut) == 1);
		ArchBrush_Free (cut);
	}
	p[0] = 50; p[1] = 3092; p[2] = 50;
	CHECK (ArchWorld_PointSolid (p));		/* inside the shell: kept */
	p[1] = 3050;
	CHECK (!ArchWorld_PointSolid (p));		/* carved */

	/* the sandbox is untouched and its own bounds still apply */
	ArchWorld_Select (ARCH_WORLD_SANDBOX);
	CHECK (ArchWorld_BrushCount () == 6);
	s[0] = s[1] = s[2] = 0;
	e[0] = 1000; e[1] = 0; e[2] = 0;
	ArchWorld_Trace (s, e, zero, zero, &tr);
	CHECK (tr.fraction * 1000 >= 160.0 - 2.0 / 32 && tr.fraction * 1000 <= 160.0);
	p[0] = 0; p[1] = 2550; p[2] = 0;
	CHECK (ArchWorld_PointSolid (p));		/* beyond 2048 is outside the sandbox */

	/* and the campaign world is still there */
	ArchWorld_Select (ARCH_WORLD_CAMPAIGN);
	CHECK (ArchWorld_BrushCount () >= 1);
	ArchWorld_Select (ARCH_WORLD_SANDBOX);
}

static abrush_t	*got[64];
static int	ngot;
static void collect (abrush_t *b) { if (ngot < 64) got[ngot++] = b; else ArchBrush_Free (b); }

static int in_any (double x, double y, double z)
{
	avec3 p;
	int i;
	p[0] = x; p[1] = y; p[2] = z;
	for (i = 0; i < ngot; i++)
		if (ArchBrush_PointInside (got[i], p))
			return 1;
	return 0;
}

/* a hollow room -64..64 in solid, as a collision tree. Children below zero are contents:
   -1 empty, -2 solid, -3 water, -6 sky. Child 0 is the side the plane normal points to. */
static void test_convert (void)
{
	static const abspplane_t planes[7] =
	{
		{{1, 0, 0}, 64}, {{1, 0, 0}, -64}, {{0, 1, 0}, 64}, {{0, 1, 0}, -64}, {{0, 0, 1}, 64}, {{0, 0, 1}, -64},
		{{0, 0, 1}, 0}
	};
	abspnode_t nodes[7] =
	{
		{0, {-2, 1}}, {1, {2, -2}}, {2, {-2, 3}}, {3, {4, -2}}, {4, {-6, 5}}, {5, {6, -2}},
		{6, {-1, -3}}		/* inside the room: air above z = 0, water below */
	};
	int mn[3] = {-256, -256, -256}, mx[3] = {256, 256, 256}, n, i;
	double vol = 0;

	ngot = 0;
	n = ArchBsp_Convert (nodes, planes, 0, mn, mx, collect);
	CHECK (n == 6 && ngot == 6);			/* five solid regions and the sky ceiling */
	for (i = 0; i < ngot; i++)
		vol += ArchBrush_Volume (got[i]);
	CHECK_NEAR (vol, 512.0 * 512 * 512 - 128.0 * 128 * 128, 1.0);
	CHECK (!in_any (0, 0, 32));			/* air */
	CHECK (!in_any (0, 0, -32));			/* water is not solid */
	CHECK (in_any (100, 0, 0) && in_any (-100, 0, 0) && in_any (0, 100, 0) && in_any (0, -100, 0));
	CHECK (in_any (0, 0, 100));			/* sky counts as solid */
	CHECK (in_any (0, 0, -100));
	CHECK (in_any (200, 200, 200));
	for (i = 0; i < ngot; i++)
		ArchBrush_Free (got[i]);

	/* a tree that is nothing but solid, or nothing but air */
	ngot = 0;
	CHECK (ArchBsp_Convert (nodes, planes, -2, mn, mx, collect) == 1 && ngot == 1);
	CHECK_NEAR (ArchBrush_Volume (got[0]), 512.0 * 512 * 512, 1.0);
	ArchBrush_Free (got[0]);
	ngot = 0;
	CHECK (ArchBsp_Convert (nodes, planes, -1, mn, mx, collect) == 0 && ngot == 0);
}

void test_campaign (void)
{
	test_bevels ();
	test_slide_along_slope ();
	test_two_worlds ();
	test_convert ();
}
