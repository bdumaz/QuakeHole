/* Stage 7: build mode in campaign maps needs surfaces cut by any convex shape,
   a pad list per world, and no cursor on planes that are not on whole units. */
#include <stdlib.h>
#include <string.h>
#include "t.h"
#include "arch_world.h"
#include "arch_poly.h"
#include "arch_cursor.h"
#include "arch_pads.h"

static double area_of (const apoly_t *p)
{
	double n[3] = {0, 0, 0};
	int i;
	for (i = 0; i < p->n; i++)
	{
		const double *a = p->v[i], *b = p->v[(i + 1) % p->n];
		n[0] += (a[1] - b[1]) * (a[2] + b[2]);
		n[1] += (a[2] - b[2]) * (a[0] + b[0]);
		n[2] += (a[0] - b[0]) * (a[1] + b[1]);
	}
	return 0.5 * sqrt (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
}

static void test_subtract_brush (void)
{
	/* the wall at x = 160 facing -x, y and z -64..64 */
	static const double c[4][3] = {{160, -64, -64}, {160, 64, -64}, {160, 64, 64}, {160, -64, 64}};
	apoly_t sq, pieces[ARCH_POLY_MAX_CUT_PIECES];
	avec3 nrm = {-1, 0, 0};
	int tri[3][3] = {{100, -32, -32}, {100, 32, -32}, {100, 0, 32}}, through[3] = {120, 0, 0}, beside[3][3] = {{100, 200, -32}, {100, 264, -32}, {100, 232, 32}};
	abrush_t *prism = ArchBrush_Prism (tri, 3, through), *other = ArchBrush_Prism (beside, 3, through);
	double total = 0;
	int n, i;

	memcpy (sq.v, c, sizeof(c));
	sq.n = 4;
	CHECK (prism && other);

	/* a triangular prism driven through the wall leaves the wall minus a triangle */
	n = ArchPoly_SubtractBrush (&sq, nrm, prism, pieces);
	CHECK (n >= 1 && n <= ARCH_POLY_MAX_CUT_PIECES);
	for (i = 0; i < n; i++)
		total += area_of (&pieces[i]);
	CHECK_NEAR (total, 128.0 * 128 - 64.0 * 64 / 2, 0.01);

	CHECK (ArchPoly_SubtractBrush (&sq, nrm, other, pieces) == ARCH_POLY_UNTOUCHED);

	/* the box form still agrees with the general one */
	{
		int mn[3] = {144, -16, -16}, mx[3] = {176, 16, 16};
		apoly_t bp[ARCH_POLY_MAX_PIECES];
		abrush_t *box = ArchBrush_Box (mn, mx);
		double a1 = 0, a2 = 0;
		int n1 = ArchPoly_SubtractBox (&sq, nrm, mn, mx, bp), n2 = ArchPoly_SubtractBrush (&sq, nrm, box, pieces);
		for (i = 0; i < n1; i++) a1 += area_of (&bp[i]);
		for (i = 0; i < n2; i++) a2 += area_of (&pieces[i]);
		CHECK (n1 == 4 && n2 >= 1);
		CHECK_NEAR (a1, a2, 0.01);
		ArchBrush_Free (box);
	}
	ArchBrush_Free (prism);
	ArchBrush_Free (other);
}

static void test_pad_sets (void)
{
	avec3 hit = {0, 0, -160}, up = {0, 0, 1};
	acursor_t c;

	ArchWorld_Select (ARCH_WORLD_SANDBOX);
	ArchWorld_Reset ();
	ArchPads_Select (ARCH_WORLD_SANDBOX);
	ArchPads_Clear ();
	ArchCursor_Place (hit, up, 0, 64, 64, &c);
	CHECK (ArchPads_Add (&c, 600) == 1 && ArchPads_Count () == 1);

	/* the campaign's pads are a separate list */
	ArchPads_Select (ARCH_WORLD_CAMPAIGN);
	ArchPads_Clear ();
	CHECK (ArchPads_Count () == 0);
	CHECK (ArchPads_Add (&c, 700) == 1 && ArchPads_Count () == 1 && ArchPads_Get (0)->speed == 700);
	ArchPads_Clear ();

	ArchPads_Select (ARCH_WORLD_SANDBOX);
	CHECK (ArchPads_Count () == 1 && ArchPads_Get (0)->speed == 600);	/* untouched */
	ArchPads_Clear ();
}

static void test_cursor_plane (void)
{
	avec3 n = {-1, 0, 0}, hit;
	acursor_t c;

	hit[1] = 0; hit[2] = 0;
	hit[0] = 160 - 1.0 / 32;	CHECK (ArchCursor_Place (hit, n, 0, 160, 160, &c) == 1 && c.plane == 160);
	/* a campaign wall that is not on a whole unit: edits there would not line up, so no cursor */
	hit[0] = 160.4;		CHECK (ArchCursor_Place (hit, n, 0, 160, 160, &c) == 0);
	hit[0] = 159.5;		CHECK (ArchCursor_Place (hit, n, 0, 160, 160, &c) == 0);
}

void test_stage7 (void)
{
	test_subtract_brush ();
	test_pad_sets ();
	test_cursor_plane ();
}
