/* Stage 3 cursor rules: the arch profile and snap-to-surface. */
#include <stdlib.h>
#include <string.h>
#include "t.h"
#include "arch_cursor.h"
#include "arch_world.h"

static acursor_t place (double hx, double hy, double hz, double nx, double ny, double nz, double yaw, int w, int h, int *ok)
{
	avec3 hit, n;
	acursor_t c;
	hit[0] = hx; hit[1] = hy; hit[2] = hz;
	n[0] = nx; n[1] = ny; n[2] = nz;
	memset (&c, 0, sizeof(c));
	*ok = ArchCursor_Place (hit, n, yaw, w, h, &c);
	return c;
}

static int has_point (int pts[][3], int n, int x, int y, int z)
{
	int i;
	for (i = 0; i < n; i++)
		if (pts[i][0] == x && pts[i][1] == y && pts[i][2] == z)
			return 1;
	return 0;
}

/* area of the profile, measured in the cursor u/v axes */
static double profile_area (const acursor_t *c, int pts[][3], int n)
{
	double a = 0;
	int i;
	for (i = 0; i < n; i++)
	{
		int j = (i + 1) % n;
		a += (double) pts[i][c->uaxis] * pts[j][c->vaxis] - (double) pts[j][c->uaxis] * pts[i][c->vaxis];
	}
	return fabs (a) * 0.5;
}

static int profile_convex (const acursor_t *c, int pts[][3], int n)
{
	int i, pos = 0, neg = 0;
	for (i = 0; i < n; i++)
	{
		int *a = pts[i], *b = pts[(i + 1) % n], *d = pts[(i + 2) % n];
		double cr = (double) (b[c->uaxis] - a[c->uaxis]) * (d[c->vaxis] - b[c->vaxis])
			  - (double) (b[c->vaxis] - a[c->vaxis]) * (d[c->uaxis] - b[c->uaxis]);
		if (cr > 0) pos++;
		if (cr < 0) neg++;
	}
	return !(pos && neg);
}

static void test_arch (void)
{
	int pts[ARCH_PROFILE_MAX][3], n, ok, i, sizes[3][2] = {{16, 16}, {1024, 16}, {16, 1024}};
	acursor_t c;
	abrush_t *b;
	double before, area;

	/* tall: straight sides then a semicircle. +x wall, cursor u (y) -80..80, v (z) -80..80 */
	c = place (160, 0, 0, -1, 0, 0, 0, 160, 160, &ok);
	CHECK (c.vdir == 1);
	n = ArchCursor_ArchProfile (&c, pts);
	CHECK (n == 11);
	CHECK (has_point (pts, n, 160, -80, -80) && has_point (pts, n, 160, 80, -80));	/* base corners */
	CHECK (has_point (pts, n, 160, 80, 0) && has_point (pts, n, 160, -80, 0));	/* springing line */
	CHECK (has_point (pts, n, 160, 0, 80));						/* crown */
	CHECK (profile_convex (&c, pts, n));
	for (i = 0; i < n; i++)
		CHECK (pts[i][0] == 160 && abs (pts[i][1]) <= 80 && abs (pts[i][2]) <= 80);

	/* the brush has the profile area times the depth, and carving removes exactly that */
	area = profile_area (&c, pts, n);
	CHECK (area > 160.0 * 80 + 3.0 * 80 * 80 / 2 && area < 160.0 * 80 + 3.15 * 80 * 80 / 2);
	b = ArchCursor_ArchBrush (&c, 160);
	CHECK (b != NULL);
	if (b)
	{
		CHECK_NEAR (ArchBrush_Volume (b), area * 160, area * 160 * 0.01);
		CHECK (b->mins[0] == 160 && b->maxs[0] == 320);		/* into the wall */
		ArchWorld_Reset ();
		before = ArchWorld_SolidVolume ();
		CHECK (ArchWorld_Carve (b) == 1);
		CHECK_NEAR (before - ArchWorld_SolidVolume (), ArchBrush_Volume (b), 1.0);
		ArchBrush_Free (b);
	}
	b = ArchCursor_ArchBrush (&c, -64);				/* out of the wall */
	CHECK (b && b->mins[0] == 96 && b->maxs[0] == 160);
	ArchBrush_Free (b);
	CHECK (ArchCursor_ArchBrush (&c, 0) == NULL);

	/* wide and low: a half-ellipse with no straight sides */
	c = place (160, 0, 0, -1, 0, 0, 0, 160, 32, &ok);		/* v -16..16 */
	n = ArchCursor_ArchProfile (&c, pts);
	CHECK (n == 9);
	CHECK (has_point (pts, n, 160, -80, -16) && has_point (pts, n, 160, 80, -16) && has_point (pts, n, 160, 0, 16));
	CHECK (profile_convex (&c, pts, n));

	/* on a floor the curved end points the way the player faces */
	c = place (0, 0, -160, 0, 0, 1, 0, 64, 64, &ok);		/* facing +x: height along x */
	CHECK (c.vaxis == 0 && c.vdir == 1);
	n = ArchCursor_ArchProfile (&c, pts);
	CHECK (has_point (pts, n, 32, 0, -160) && has_point (pts, n, -32, 32, -160) && has_point (pts, n, -32, -32, -160));
	c = place (0, 0, -160, 0, 0, 1, 180, 64, 64, &ok);		/* facing -x */
	CHECK (c.vaxis == 0 && c.vdir == -1);
	n = ArchCursor_ArchProfile (&c, pts);
	CHECK (has_point (pts, n, -32, 0, -160) && has_point (pts, n, 32, 32, -160));
	c = place (0, 0, -160, 0, 0, 1, 270, 64, 64, &ok);		/* facing -y */
	CHECK (c.vaxis == 1 && c.vdir == -1);
	n = ArchCursor_ArchProfile (&c, pts);
	CHECK (has_point (pts, n, 0, -32, -160));
	c = place (0, 0, 160, 0, 0, -1, 90, 64, 64, &ok);		/* ceiling, facing +y */
	CHECK (c.vaxis == 1 && c.vdir == 1);
	n = ArchCursor_ArchProfile (&c, pts);
	CHECK (has_point (pts, n, 0, 32, 160));

	/* Review Focus 1: extreme sizes give a valid brush inside the rectangle, or nothing */
	for (i = 0; i < 3; i++)
	{
		c = place (160, 0, 0, -1, 0, 0, 0, sizes[i][0], sizes[i][1], &ok);
		n = ArchCursor_ArchProfile (&c, pts);
		CHECK (n >= 3 && n <= ARCH_PROFILE_MAX);
		b = ArchCursor_ArchBrush (&c, 64);
		if (b)
		{
			CHECK (ArchBrush_Volume (b) > 0);
			CHECK (b->mins[1] >= c.umin && b->maxs[1] <= c.umax && b->mins[2] >= c.vmin && b->maxs[2] <= c.vmax);
			CHECK (b->mins[0] == 160 && b->maxs[0] == 224);
			ArchBrush_Free (b);
		}
	}
	ArchWorld_Reset ();
}

static void edit_box (int x1, int y1, int z1, int x2, int y2, int z2, int add)
{
	int mn[3], mx[3];
	abrush_t *b;
	mn[0] = x1; mn[1] = y1; mn[2] = z1; mx[0] = x2; mx[1] = y2; mx[2] = z2;
	b = ArchBrush_Box (mn, mx);
	if (add) ArchWorld_Add (b); else ArchWorld_Carve (b);
	ArchBrush_Free (b);
}

static void test_snap_surface (void)
{
	acursor_t c;
	avec3 hit;
	int ok, i;

	/* the whole spawn-room wall, from several aim points */
	ArchWorld_Reset ();
	for (i = 0; i < 4; i++)
	{
		hit[0] = 160 - 1.0 / 32; hit[1] = -150.0 + i * 97; hit[2] = 140.0 - i * 91;
		c = place (hit[0], hit[1], hit[2], -1, 0, 0, 0, 160, 160, &ok);
		CHECK (ArchCursor_Snap (&c, hit, ArchWorld_PointSolid) == 1);
		CHECK (c.umin == -160 && c.umax == 160 && c.vmin == -160 && c.vmax == 160);
	}

	/* a doorway in the wall stops it */
	edit_box (160, -48, -160, 480, 48, -32, 0);
	hit[0] = 160 - 1.0 / 32; hit[1] = 100; hit[2] = 0;
	c = place (hit[0], hit[1], hit[2], -1, 0, 0, 0, 160, 160, &ok);
	CHECK (ArchCursor_Snap (&c, hit, ArchWorld_PointSolid) == 1);
	CHECK (c.umin == 48 && c.umax == 160 && c.vmin == -160 && c.vmax == 160);

	/* aiming into the doorway itself: no flat surface under the aim, nothing changes */
	hit[1] = 0; hit[2] = -100;
	c = place (hit[0], hit[1], hit[2], -1, 0, 0, 0, 160, 160, &ok);
	CHECK (ArchCursor_Snap (&c, hit, ArchWorld_PointSolid) == 0);
	CHECK (c.umax - c.umin == 160 && c.vmax - c.vmin == 160);

	/* the top of a pillar */
	ArchWorld_Reset ();
	edit_box (-32, -32, -160, 32, 32, -96, 1);
	hit[0] = 5; hit[1] = -7; hit[2] = -96 + 1.0 / 32;
	c = place (hit[0], hit[1], hit[2], 0, 0, 1, 0, 160, 160, &ok);
	CHECK (ArchCursor_Snap (&c, hit, ArchWorld_PointSolid) == 1);
	CHECK (c.umin == -32 && c.umax == 32 && c.vmin == -32 && c.vmax == 32);

	/* Review Focus 4: a surface one grid square wide */
	ArchWorld_Reset ();
	edit_box (0, 0, -160, 8, 8, -96, 1);
	hit[0] = 4; hit[1] = 4; hit[2] = -96 + 1.0 / 32;
	c = place (hit[0], hit[1], hit[2], 0, 0, 1, 0, 160, 160, &ok);
	CHECK (ArchCursor_Snap (&c, hit, ArchWorld_PointSolid) == 1);
	CHECK (c.umin == 0 && c.umax == 8 && c.vmin == 0 && c.vmax == 8);

	/* Review Focus 4: the end wall of a tunnel driven to the world shell */
	ArchWorld_Reset ();
	edit_box (160, -32, -32, 3000, 32, 32, 0);
	hit[0] = (ARCH_HALF - ARCH_SHELL) - 1.0 / 32; hit[1] = 3; hit[2] = 3;
	c = place (hit[0], hit[1], hit[2], -1, 0, 0, 0, 160, 160, &ok);
	CHECK (ok && ArchCursor_Snap (&c, hit, ArchWorld_PointSolid) == 1);
	CHECK (c.umin == -32 && c.umax == 32 && c.vmin == -32 && c.vmax == 32);
	ArchWorld_Reset ();
}

void test_actions (void)
{
	test_arch ();
	test_snap_surface ();
}
