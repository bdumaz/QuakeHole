#include "t.h"
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

void test_brush (void)
{
	test_box ();
	test_plane ();
	test_prism ();
}
