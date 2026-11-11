/* Stage 6: cutting a dig box out of a flat surface polygon; plane tags; locked brushes. */
#include <stdlib.h>
#include <string.h>
#include "t.h"
#include "arch_world.h"
#include "arch_poly.h"

static double poly_area (const apoly_t *p)
{
	/* Newell */
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

static double total_area (const apoly_t *pieces, int n)
{
	double a = 0;
	int i;
	for (i = 0; i < n; i++)
		a += poly_area (&pieces[i]);
	return a;
}

/* a wall at x = 160 facing -x (air on the -x side, solid behind at x > 160), spanning y and z -64..64 */
static void wall_square (apoly_t *p, avec3 normal)
{
	static const double c[4][3] = {{160, -64, -64}, {160, 64, -64}, {160, 64, 64}, {160, -64, 64}};
	memcpy (p->v, c, sizeof(c));
	p->n = 4;
	normal[0] = -1; normal[1] = 0; normal[2] = 0;
}

static void test_poly (void)
{
	apoly_t sq, pieces[ARCH_POLY_MAX_PIECES];
	avec3 nrm;
	int n, i;

	wall_square (&sq, nrm);
	CHECK_NEAR (poly_area (&sq), 128.0 * 128, 1e-6);

	/* a box through the middle, straddling the wall: a frame is left */
	{
		int mn[3] = {144, -16, -16}, mx[3] = {176, 16, 16};
		n = ArchPoly_SubtractBox (&sq, nrm, mn, mx, pieces);
		CHECK (n == 4);
		CHECK_NEAR (total_area (pieces, n), 128.0 * 128 - 32.0 * 32, 1e-6);
		for (i = 0; i < n; i++)
			CHECK (pieces[i].n >= 3 && pieces[i].n <= ARCH_POLY_MAX_VERTS);
	}
	/* a box covering the whole polygon: nothing left */
	{
		int mn[3] = {100, -100, -100}, mx[3] = {200, 100, 100};
		CHECK (ArchPoly_SubtractBox (&sq, nrm, mn, mx, pieces) == 0);
	}
	/* a box beside it: untouched */
	{
		int mn[3] = {144, 100, -16}, mx[3] = {176, 132, 16};
		CHECK (ArchPoly_SubtractBox (&sq, nrm, mn, mx, pieces) == ARCH_POLY_UNTOUCHED);
	}
	/* a box in the air in front, flush against the wall: untouched (no solid behind the wall was removed) */
	{
		int mn[3] = {128, -16, -16}, mx[3] = {160, 16, 16};
		CHECK (ArchPoly_SubtractBox (&sq, nrm, mn, mx, pieces) == ARCH_POLY_UNTOUCHED);
	}
	/* a box in the solid behind, flush against the wall: the overlap goes */
	{
		int mn[3] = {160, -16, -16}, mx[3] = {192, 16, 16};
		n = ArchPoly_SubtractBox (&sq, nrm, mn, mx, pieces);
		CHECK (n == 4);
		CHECK_NEAR (total_area (pieces, n), 128.0 * 128 - 32.0 * 32, 1e-6);
	}
	/* a box over one corner */
	{
		int mn[3] = {144, 32, 32}, mx[3] = {176, 96, 96};
		n = ArchPoly_SubtractBox (&sq, nrm, mn, mx, pieces);
		CHECK (n >= 1 && n <= 2);
		CHECK_NEAR (total_area (pieces, n), 128.0 * 128 - 32.0 * 32, 1e-6);
	}
	/* a box that only shares an edge line with the polygon: untouched */
	{
		int mn[3] = {144, 64, -16}, mx[3] = {176, 96, 16};
		CHECK (ArchPoly_SubtractBox (&sq, nrm, mn, mx, pieces) == ARCH_POLY_UNTOUCHED);
	}
	/* a slanted polygon: a triangle on the plane x + z = 0, solid on the +x+z side... facing (-0.707, 0, -0.707) */
	{
		apoly_t tri;
		avec3 tn = {-0.70710678, 0, -0.70710678};
		int mn[3] = {-16, -16, -16}, mx[3] = {16, 16, 16};
		static const double t[3][3] = {{-64, -64, 64}, {64, -64, -64}, {0, 64, 0}};
		memcpy (tri.v, t, sizeof(t));
		tri.n = 3;
		n = ArchPoly_SubtractBox (&tri, tn, mn, mx, pieces);
		CHECK (n >= 3);
		CHECK (total_area (pieces, n) < poly_area (&tri) - 100);
		CHECK (total_area (pieces, n) > poly_area (&tri) - 32.0 * 46);	/* the cut is at most the box's diagonal section */
	}
}

static void test_tags (void)
{
	int amn[3] = {0, 0, 0}, amx[3] = {64, 64, 64}, bmn[3] = {16, 16, 16}, bmx[3] = {48, 48, 100};
	abrush_t *a = ArchBrush_Box (amn, amx), *b = ArchBrush_Box (bmn, bmx), **pieces;
	int n, i, f, tagged = 0, untagged = 0;

	for (f = 0; f < a->nfaces; f++)
		CHECK (a->faces[f].plane.tag == 0);
	for (f = 0; f < b->nfaces; f++)
		b->faces[f].plane.tag = 7;
	n = ArchBrush_Subtract (a, b, &pieces);
	CHECK (n >= 4);
	for (i = 0; i < n; i++)
	{
		for (f = 0; f < pieces[i]->nfaces; f++)
		{
			const aface_t *face = &pieces[i]->faces[f];
			/* the faces of the square hole (planes x or y = 16 or 48, and its floor z = 16) come from the cutter */
			int on_cut = (fabs (fabs (face->plane.n[0]) - 1) < 1e-9 && (fabs (fabs (face->plane.d) - 16) < 1e-9 || fabs (fabs (face->plane.d) - 48) < 1e-9)) ||
				     (fabs (fabs (face->plane.n[1]) - 1) < 1e-9 && (fabs (fabs (face->plane.d) - 16) < 1e-9 || fabs (fabs (face->plane.d) - 48) < 1e-9)) ||
				     (fabs (fabs (face->plane.n[2]) - 1) < 1e-9 && fabs (fabs (face->plane.d) - 16) < 1e-9);
			if (face->plane.tag == 7)
			{
				tagged++;
				CHECK (on_cut);
			}
			else
			{
				untagged++;
				CHECK (face->plane.tag == 0);
			}
		}
		ArchBrush_Free (pieces[i]);
	}
	free (pieces);
	CHECK (tagged >= 5);
	CHECK (untagged >= 6);
	ArchBrush_Free (a);
	ArchBrush_Free (b);
}

static void test_locked (void)
{
	int bmn[3] = {-512, -512, -512}, bmx[3] = {512, 512, 512};
	int amn[3] = {0, 0, 0}, amx[3] = {64, 64, 64}, smn[3] = {64, 0, 0}, smx[3] = {128, 64, 64}, cmn[3] = {32, 16, 16}, cmx[3] = {96, 48, 48};
	abrush_t *rock = ArchBrush_Box (amn, amx), *sky = ArchBrush_Box (smn, smx), *cut = ArchBrush_Box (cmn, cmx);
	avec3 p;

	ArchWorld_Select (ARCH_WORLD_CAMPAIGN);
	ArchWorld_BeginCustom (bmn, bmx);
	sky->locked = 1;
	ArchWorld_Insert (rock);
	ArchWorld_Insert (sky);
	CHECK (ArchWorld_Carve (cut) == 1);
	ArchBrush_Free (cut);
	p[0] = 48; p[1] = 32; p[2] = 32;
	CHECK (!ArchWorld_PointSolid (p));	/* the rock was carved */
	p[0] = 80;
	CHECK (ArchWorld_PointSolid (p));	/* the locked brush was not */
	CHECK_NEAR (ArchWorld_SolidVolume (), 64.0 * 64 * 64 * 2 - 32.0 * 32 * 32, 1.0);
	ArchWorld_Select (ARCH_WORLD_SANDBOX);
}

void test_stage6 (void)
{
	test_poly ();
	test_tags ();
	test_locked ();
}
