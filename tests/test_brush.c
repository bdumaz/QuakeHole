#include "t.h"
#include <stdlib.h>
#include "arch_brush.h"

static int count_faces (const abrush_t *b, double nx, double ny, double nz, int slot)
{
	int i, n = 0;
	for (i = 0; i < b->nfaces; i++)
	{
		const aface_t *f = &b->faces[i];
		if (fabs (f->plane.n[0] - nx) < 1e-9 && fabs (f->plane.n[1] - ny) < 1e-9 &&
			fabs (f->plane.n[2] - nz) < 1e-9 && f->slot == slot && f->nverts == 4)
			n++;
	}
	return n;
}

static void test_box (void)
{
	int mn[3] = {0, 0, 0}, mx[3] = {64, 32, 16}, bad[3] = {64, 0, 16};
	avec3 in = {32, 16, 8}, on = {0, 16, 8}, out = {-1, 16, 8};
	abrush_t *b = ArchBrush_Box (mn, mx), *cl;

	CHECK (b && b->nfaces == 6);
	if (!b) return;
	CHECK_NEAR (ArchBrush_Volume (b), 64.0 * 32 * 16, 1e-6);
	CHECK_NEAR (b->mins[0], 0, 1e-9);
	CHECK_NEAR (b->mins[1], 0, 1e-9);
	CHECK_NEAR (b->maxs[0], 64, 1e-9);
	CHECK_NEAR (b->maxs[2], 16, 1e-9);
	CHECK (count_faces (b, 0, 0, 1, ARCH_SLOT_GROUND) == 1);
	CHECK (count_faces (b, 0, 0, -1, ARCH_SLOT_CEILING) == 1);
	CHECK (count_faces (b, 1, 0, 0, ARCH_SLOT_WALL) == 1);
	CHECK (count_faces (b, -1, 0, 0, ARCH_SLOT_WALL) == 1);
	CHECK (count_faces (b, 0, 1, 0, ARCH_SLOT_WALL) == 1);
	CHECK (count_faces (b, 0, -1, 0, ARCH_SLOT_WALL) == 1);
	CHECK (ArchBrush_PointInside (b, in));
	CHECK (!ArchBrush_PointInside (b, on));
	CHECK (!ArchBrush_PointInside (b, out));
	CHECK (ArchBrush_Box (mn, bad) == NULL);

	cl = ArchBrush_Clone (b);
	ArchBrush_Free (b);
	CHECK_NEAR (ArchBrush_Volume (cl), 32768.0, 1e-6);
	ArchBrush_Free (cl);
}

static void test_plane (void)
{
	/* (p0-p1)x(p2-p1) = (0,-64,0)x(64,-64,0) = (0,0,4096) -> n = (0,0,1), d = 0 */
	int a[3] = {0, 0, 0}, c1[3] = {0, 64, 0}, c2[3] = {64, 0, 0}, col[3] = {0, 128, 0};
	aplane_t pl, fl;

	CHECK (ArchPlane_FromPoints (a, c1, c2, &pl));
	CHECK_NEAR (pl.n[2], 1.0, 1e-9);
	CHECK_NEAR (pl.d, 0.0, 1e-9);
	ArchPlane_Flip (&pl, &fl);
	CHECK_NEAR (fl.n[2], -1.0, 1e-9);
	CHECK (fl.p[0][0] == 64 && fl.p[2][0] == 0);
	CHECK (!ArchPlane_FromPoints (a, c1, col, &pl));
}

static void test_prism (void)
{
	int tri[3][3] = {{0, 0, 0}, {64, 0, 0}, {0, 64, 0}};
	int tri2[3][3] = {{0, 64, 0}, {64, 0, 0}, {0, 0, 0}};
	int line[3][3] = {{0, 0, 0}, {32, 0, 0}, {64, 0, 0}};
	int ex[3] = {0, 0, 32};
	abrush_t *p = ArchBrush_Prism (tri, 3, ex), *p2 = ArchBrush_Prism (tri2, 3, ex);

	CHECK (p && p->nfaces == 5);
	if (p) CHECK_NEAR (ArchBrush_Volume (p), 65536.0, 1e-6);
	CHECK (p2 != NULL);
	if (p2) CHECK_NEAR (ArchBrush_Volume (p2), 65536.0, 1e-6);
	CHECK (ArchBrush_Prism (line, 3, ex) == NULL);
	ArchBrush_Free (p);
	ArchBrush_Free (p2);
}

static abrush_t *box6 (int x1, int y1, int z1, int x2, int y2, int z2)
{
	int mn[3], mx[3];
	mn[0] = x1; mn[1] = y1; mn[2] = z1;
	mx[0] = x2; mx[1] = y2; mx[2] = z2;
	return ArchBrush_Box (mn, mx);
}

static abrush_t *wedge (void)
{
	/* right triangle (0,0,0),(64,0,0),(0,64,0) extruded 64 up */
	int tri[3][3] = {{0, 0, 0}, {64, 0, 0}, {0, 64, 0}}, ex[3] = {0, 0, 64};
	return ArchBrush_Prism (tri, 3, ex);
}

static void free_pieces (abrush_t **pieces, int n)
{
	int i;
	for (i = 0; i < n; i++)
		ArchBrush_Free (pieces[i]);
	free (pieces);
}

static double pieces_volume (abrush_t **pieces, int n)
{
	double v = 0;
	int i;
	for (i = 0; i < n; i++)
		v += ArchBrush_Volume (pieces[i]);
	return v;
}

/* pieces must not overlap each other or the subtracted brush */
static void check_disjoint (abrush_t **pieces, int n, const abrush_t *cut)
{
	int i, j;
	for (i = 0; i < n; i++)
	{
		CHECK (!ArchBrush_Intersects (pieces[i], cut));
		for (j = i + 1; j < n; j++)
			CHECK (!ArchBrush_Intersects (pieces[i], pieces[j]));
	}
}

static aplane_t plane_x (int x)
{
	/* normal +x at the given x */
	int a[3], b[3], c[3];
	avec3 inside;
	aplane_t pl;
	a[0] = b[0] = c[0] = x;
	a[1] = 0; a[2] = 0; b[1] = 64; b[2] = 0; c[1] = 0; c[2] = 64;
	ArchPlane_FromPoints (a, b, c, &pl);
	inside[0] = x - 1.0; inside[1] = 0; inside[2] = 0;
	if (pl.n[0] < 0)
		ArchPlane_Flip (&pl, &pl);
	(void) inside;
	return pl;
}

static void test_split (void)
{
	abrush_t *A = box6 (0, 0, 0, 64, 64, 64), *front, *back;
	aplane_t pl = plane_x (16), thin[6];
	int i;

	CHECK_NEAR (pl.n[0], 1.0, 1e-9);
	ArchBrush_Split (A, &pl, &front, &back);
	CHECK (front && back);
	CHECK_NEAR (ArchBrush_Volume (front), 48.0 * 64 * 64, 1e-6);
	CHECK_NEAR (ArchBrush_Volume (back), 16.0 * 64 * 64, 1e-6);
	ArchBrush_Free (front); ArchBrush_Free (back);

	pl = plane_x (100);	/* misses: everything is behind */
	ArchBrush_Split (A, &pl, &front, &back);
	CHECK (front == NULL && back != NULL);
	CHECK_NEAR (ArchBrush_Volume (back), 64.0 * 64 * 64, 1e-6);
	ArchBrush_Free (back);

	pl = plane_x (64);	/* coincident with a face */
	ArchBrush_Split (A, &pl, &front, &back);
	CHECK (front == NULL && back != NULL);
	CHECK_NEAR (ArchBrush_Volume (back), 64.0 * 64 * 64, 1e-6);
	ArchBrush_Free (back);

	/* a slab 0.005 thick is discarded by the thin rule */
	for (i = 0; i < 6; i++)
		thin[i] = A->faces[i].plane;
	for (i = 0; i < 6; i++)
		if (thin[i].n[0] > 0.5)
			thin[i].d = 0.005;
	CHECK (ArchBrush_FromPlanes (thin, 6) == NULL);
	ArchBrush_Free (A);
}

static void test_subtract (void)
{
	abrush_t *A = box6 (0, 0, 0, 64, 64, 64);
	abrush_t *B = box6 (16, 16, 16, 48, 48, 48);
	abrush_t *B2 = box6 (100, 100, 100, 120, 120, 120);
	abrush_t *B3 = box6 (64, 0, 0, 96, 64, 64);	/* touches on a face only */
	abrush_t *B4 = box6 (-8, -8, -8, 72, 72, 72);
	abrush_t *B5 = wedge ();
	abrush_t *B6 = box6 (32, 32, 32, 96, 96, 96);
	abrush_t **pieces, *both;
	const double volA = 64.0 * 64 * 64;
	int n;

	n = ArchBrush_Subtract (A, B, &pieces);
	CHECK (n == 6);
	CHECK_NEAR (pieces_volume (pieces, n), volA - 32.0 * 32 * 32, 1e-6);
	check_disjoint (pieces, n, B);
	free_pieces (pieces, n);

	n = ArchBrush_Subtract (A, B2, &pieces);
	CHECK (n == 1);
	CHECK_NEAR (pieces_volume (pieces, n), volA, 1e-6);
	free_pieces (pieces, n);
	CHECK (!ArchBrush_Intersects (A, B2));

	n = ArchBrush_Subtract (A, B3, &pieces);	/* Review Focus 3 */
	CHECK (n == 1);
	CHECK_NEAR (pieces_volume (pieces, n), volA, 1e-6);
	free_pieces (pieces, n);
	CHECK (!ArchBrush_Intersects (A, B3));
	CHECK (ArchBrush_Intersection (A, B3) == NULL);

	n = ArchBrush_Subtract (A, B4, &pieces);
	CHECK (n == 0);
	CHECK (pieces == NULL);

	CHECK (B5 != NULL);
	n = ArchBrush_Subtract (A, B5, &pieces);
	CHECK (n >= 1);
	CHECK_NEAR (pieces_volume (pieces, n), volA - 64.0 * 64 / 2 * 64, 1e-6);
	check_disjoint (pieces, n, B5);
	free_pieces (pieces, n);

	/* volume(A) == volume(A - X) + volume(A n X) */
	n = ArchBrush_Subtract (A, B6, &pieces);
	both = ArchBrush_Intersection (A, B6);
	CHECK (both != NULL);
	CHECK_NEAR (ArchBrush_Volume (both), 32.0 * 32 * 32, 1e-6);
	CHECK_NEAR (pieces_volume (pieces, n) + ArchBrush_Volume (both), volA, 1e-6);
	check_disjoint (pieces, n, B6);
	free_pieces (pieces, n);
	ArchBrush_Free (both);

	both = ArchBrush_Intersection (A, B5);
	n = ArchBrush_Subtract (A, B5, &pieces);
	CHECK_NEAR (pieces_volume (pieces, n) + ArchBrush_Volume (both), volA, 1e-6);
	free_pieces (pieces, n);
	ArchBrush_Free (both);

	ArchBrush_Free (A); ArchBrush_Free (B); ArchBrush_Free (B2); ArchBrush_Free (B3);
	ArchBrush_Free (B4); ArchBrush_Free (B5); ArchBrush_Free (B6);
}

static void test_trace (void)
{
	abrush_t *B = box6 (0, 0, 0, 64, 64, 64), *W = wedge ();
	avec3 zero = {0, 0, 0}, pmin = {-16, -16, -24}, pmax = {16, 16, 32};
	atrace_t tr;
	int axis, side, k;

	/* a ray from each of the six directions through the centre */
	for (axis = 0; axis < 3; axis++)
	{
		for (side = 0; side < 2; side++)
		{
			avec3 s = {32, 32, 32}, e = {32, 32, 32};
			s[axis] = side ? 164 : -100;
			e[axis] = side ? -36 : 100;
			ArchTrace_Init (&tr);
			CHECK (tr.fraction == 1.0 && !tr.startsolid && !tr.allsolid);
			ArchBrush_TraceBox (B, s, e, zero, zero, &tr);
			CHECK_NEAR (tr.fraction, (100.0 - 1.0 / 32) / 200.0, 1e-9);
			CHECK (!tr.startsolid && !tr.allsolid);
			for (k = 0; k < 3; k++)
				CHECK_NEAR (tr.normal[k], k == axis ? (side ? 1.0 : -1.0) : 0.0, 1e-9);
		}
	}
	{	/* passes beside */
		avec3 s = {-100, 100, 32}, e = {100, 100, 32};
		ArchTrace_Init (&tr);
		ArchBrush_TraceBox (B, s, e, zero, zero, &tr);
		CHECK (tr.fraction == 1.0 && !tr.startsolid);
	}
	{	/* starts inside, ends outside */
		avec3 s = {32, 32, 32}, e = {200, 32, 32};
		ArchTrace_Init (&tr);
		ArchBrush_TraceBox (B, s, e, zero, zero, &tr);
		CHECK (tr.startsolid == 1 && !tr.allsolid);
	}
	{	/* wholly inside */
		avec3 s = {10, 32, 32}, e = {50, 32, 32};
		ArchTrace_Init (&tr);
		ArchBrush_TraceBox (B, s, e, zero, zero, &tr);
		CHECK (tr.startsolid == 1 && tr.allsolid == 1 && tr.fraction == 0.0);
	}
	{	/* player box dropping onto the top */
		avec3 s = {32, 32, 200}, e = {32, 32, 0};
		double z;
		ArchTrace_Init (&tr);
		ArchBrush_TraceBox (B, s, e, pmin, pmax, &tr);
		z = 200 - 200 * tr.fraction;
		CHECK (z >= 88.0 && z <= 88.0 + 2.0 / 32);
		CHECK_NEAR (tr.normal[2], 1.0, 1e-9);
		CHECK_NEAR (tr.dist, 64.0, 1e-9);
	}
	{	/* Review Focus 1: resting on top, sliding along it */
		avec3 s = {32, 32, 88}, e = {48, 32, 88};
		ArchTrace_Init (&tr);
		ArchBrush_TraceBox (B, s, e, pmin, pmax, &tr);
		CHECK (tr.fraction == 1.0 && !tr.startsolid);
	}
	{	/* Review Focus 1: resting on top, moving up */
		avec3 s = {32, 32, 88}, e = {32, 32, 120};
		ArchTrace_Init (&tr);
		ArchBrush_TraceBox (B, s, e, pmin, pmax, &tr);
		CHECK (tr.fraction == 1.0 && !tr.startsolid);
	}
	{	/* resting on top, pushing down */
		avec3 s = {32, 32, 88}, e = {32, 32, 80};
		ArchTrace_Init (&tr);
		ArchBrush_TraceBox (B, s, e, pmin, pmax, &tr);
		CHECK (tr.fraction == 0.0 && !tr.startsolid);
	}
	{	/* an earlier hit is kept */
		avec3 s = {-100, 32, 32}, e = {100, 32, 32};
		ArchTrace_Init (&tr);
		tr.fraction = 0.1;
		ArchBrush_TraceBox (B, s, e, zero, zero, &tr);
		CHECK (tr.fraction == 0.1);
	}
	{	/* angled face: the wedge hypotenuse */
		avec3 s = {64, 64, 32}, e = {0, 0, 32};
		ArchTrace_Init (&tr);
		ArchBrush_TraceBox (W, s, e, zero, zero, &tr);
		CHECK_NEAR (tr.normal[0], 0.70710678, 1e-6);
		CHECK_NEAR (tr.normal[1], 0.70710678, 1e-6);
		CHECK_NEAR (64 - 64 * tr.fraction, 32.0, 0.1);
	}
	ArchBrush_Free (B);
	ArchBrush_Free (W);
}

void test_brush (void)
{
	test_box ();
	test_plane ();
	test_prism ();
	test_split ();
	test_subtract ();
	test_trace ();
}
