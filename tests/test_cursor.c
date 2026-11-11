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

static void test_axes (void)
{
	acursor_t c;
	int ok;

	/* the +x wall of the spawn room: surface at x=160, air on the -x side */
	c = place (160 - 1.0 / 32, 3, 5, -1, 0, 0, 0, 160, 96, &ok);
	CHECK (ok);
	CHECK (c.naxis == 0 && c.nsign == -1 && c.plane == 160);
	CHECK (c.uaxis == 1 && c.vaxis == 2);		/* width horizontal, height vertical */
	CHECK (c.umin == -80 && c.umax == 80);		/* centre 3 snaps to 0 */
	CHECK (c.vmin == -40 && c.vmax == 56);		/* centre 5 snaps to 8 */

	c = place (10, -160 + 1.0 / 32, 0, 0, 1, 0, 90, 160, 160, &ok);	/* the -y wall */
	CHECK (ok && c.naxis == 1 && c.nsign == 1 && c.plane == -160);
	CHECK (c.uaxis == 0 && c.vaxis == 2);

	/* floor: height runs along the axis the player faces */
	c = place (0, 0, -160 + 1.0 / 32, 0, 0, 1, 0, 160, 96, &ok);
	CHECK (ok && c.naxis == 2 && c.nsign == 1 && c.plane == -160);
	CHECK (c.vaxis == 0 && c.uaxis == 1);
	c = place (0, 0, -160, 0, 0, 1, 180, 160, 96, &ok);	CHECK (c.vaxis == 0 && c.uaxis == 1);
	c = place (0, 0, -160, 0, 0, 1, 90, 160, 96, &ok);	CHECK (c.vaxis == 1 && c.uaxis == 0);
	c = place (0, 0, -160, 0, 0, 1, 270, 160, 96, &ok);	CHECK (c.vaxis == 1 && c.uaxis == 0);
	c = place (0, 0, -160, 0, 0, 1, -90, 160, 96, &ok);	CHECK (c.vaxis == 1 && c.uaxis == 0);
	c = place (0, 0, -160, 0, 0, 1, 44, 160, 96, &ok);	CHECK (c.vaxis == 0);
	c = place (0, 0, -160, 0, 0, 1, 46, 160, 96, &ok);	CHECK (c.vaxis == 1);
	CHECK (c.vmax - c.vmin == 96 && c.umax - c.umin == 160);

	/* ceiling */
	c = place (0, 0, 160, 0, 0, -1, 0, 160, 160, &ok);
	CHECK (ok && c.naxis == 2 && c.nsign == -1 && c.plane == 160 && c.vaxis == 0);

	/* slanted surfaces take no cursor in this stage */
	place (0, 0, 0, 0.7071, 0.7071, 0, 0, 160, 160, &ok);
	CHECK (!ok);
}

static void test_snap (void)
{
	int i, ok, size, bad = 0;

	srand (7);
	for (i = 0; i < 2000; i++)
	{
		double hy = (rand () % 40000) / 10.0 - 2000, hz = (rand () % 40000) / 10.0 - 2000;
		acursor_t c;
		double cu, cv;

		size = 16 * (1 + rand () % 64);
		c = place (160, hy, hz, -1, 0, 0, 0, size, 16 * (1 + rand () % 64), &ok);
		cu = (c.umin + c.umax) * 0.5;
		cv = (c.vmin + c.vmax) * 0.5;
		if (!ok || c.umin % 8 || c.umax % 8 || c.vmin % 8 || c.vmax % 8) bad++;
		if (c.umax - c.umin != size) bad++;
		if (fabs (cu - hy) > 4.0 || fabs (cv - hz) > 4.0) bad++;
	}
	CHECK (bad == 0);
}

static void test_boxes (void)
{
	acursor_t c;
	avec3 corners[4];
	int ok, mn[3], mx[3], i;

	c = place (160, 0, 0, -1, 0, 0, 0, 160, 96, &ok);	/* +x wall, air toward -x */
	ArchCursor_Box (&c, 160, mn, mx);			/* carve goes into the wall: +x */
	CHECK (mn[0] == 160 && mx[0] == 320 && mn[1] == -80 && mx[1] == 80 && mn[2] == -48 && mx[2] == 48);
	ArchCursor_Box (&c, -64, mn, mx);			/* extrude comes out: -x */
	CHECK (mn[0] == 96 && mx[0] == 160 && mn[1] == -80 && mx[1] == 80);

	c = place (-160, 0, 0, 1, 0, 0, 0, 160, 160, &ok);	/* -x wall */
	ArchCursor_Box (&c, 32, mn, mx);	CHECK (mn[0] == -192 && mx[0] == -160);
	ArchCursor_Box (&c, -32, mn, mx);	CHECK (mn[0] == -160 && mx[0] == -128);

	c = place (0, 160, 0, 0, -1, 0, 0, 160, 160, &ok);	/* +y wall */
	ArchCursor_Box (&c, 32, mn, mx);	CHECK (mn[1] == 160 && mx[1] == 192 && mn[0] == -80 && mx[0] == 80);
	c = place (0, -160, 0, 0, 1, 0, 0, 160, 160, &ok);	/* -y wall */
	ArchCursor_Box (&c, -32, mn, mx);	CHECK (mn[1] == -160 && mx[1] == -128);

	c = place (0, 0, -160, 0, 0, 1, 0, 64, 32, &ok);	/* floor, facing +x: height 32 along x */
	ArchCursor_Box (&c, 16, mn, mx);
	CHECK (mn[2] == -176 && mx[2] == -160 && mn[0] == -16 && mx[0] == 16 && mn[1] == -32 && mx[1] == 32);
	ArchCursor_Box (&c, -16, mn, mx);	CHECK (mn[2] == -160 && mx[2] == -144);

	c = place (0, 0, 160, 0, 0, -1, 0, 64, 64, &ok);	/* ceiling */
	ArchCursor_Box (&c, 16, mn, mx);	CHECK (mn[2] == 160 && mx[2] == 176);
	ArchCursor_Box (&c, -16, mn, mx);	CHECK (mn[2] == 144 && mx[2] == 160);

	/* corners lie on the surface, lifted toward the air */
	c = place (160, 0, 0, -1, 0, 0, 0, 160, 96, &ok);
	ArchCursor_Corners (&c, 0.5, corners);
	for (i = 0; i < 4; i++)
	{
		CHECK_NEAR (corners[i][0], 159.5, 1e-9);
		CHECK (fabs (corners[i][1]) == 80.0 && fabs (corners[i][2]) == 48.0);
	}
	CHECK (corners[0][1] != corners[2][1] && corners[0][2] != corners[2][2]);	/* opposite corners */
}

static void test_steps (void)
{
	int w = 160, h = 160;

	ArchCursor_StepSize (ARCH_SCALE_UNIFORM, 1, &w, &h);	CHECK (w == 176 && h == 176);
	ArchCursor_StepSize (ARCH_SCALE_UNIFORM, -1, &w, &h);	CHECK (w == 160 && h == 160);
	ArchCursor_StepSize (ARCH_SCALE_HEIGHT, 1, &w, &h);	CHECK (w == 160 && h == 176);
	ArchCursor_StepSize (ARCH_SCALE_WIDTH, -1, &w, &h);	CHECK (w == 144 && h == 176);

	w = 320; h = 160;	/* the larger side steps; the other keeps the proportion, rounded to 16 */
	ArchCursor_StepSize (ARCH_SCALE_UNIFORM, 1, &w, &h);	CHECK (w == 336 && h == 176);
	w = 64; h = 320;
	ArchCursor_StepSize (ARCH_SCALE_UNIFORM, -1, &w, &h);	CHECK (h == 304 && w == 64);
	w = 16; h = 320;	/* the smaller side never drops below the minimum */
	ArchCursor_StepSize (ARCH_SCALE_UNIFORM, -1, &w, &h);	CHECK (h == 304 && w == 16);

	w = 1024; h = 512;	/* limits: a step that would leave the range does nothing */
	ArchCursor_StepSize (ARCH_SCALE_UNIFORM, 1, &w, &h);	CHECK (w == 1024 && h == 512);
	ArchCursor_StepSize (ARCH_SCALE_WIDTH, 1, &w, &h);	CHECK (w == 1024);
	w = 16; h = 16;
	ArchCursor_StepSize (ARCH_SCALE_UNIFORM, -1, &w, &h);	CHECK (w == 16 && h == 16);
	ArchCursor_StepSize (ARCH_SCALE_HEIGHT, -1, &w, &h);	CHECK (h == 16);
}

static void test_format (void)
{
	char buf[16];

	ArchCursor_FormatMetres (160, buf, sizeof(buf));	CHECK (!strcmp (buf, "5.0"));
	ArchCursor_FormatMetres (80, buf, sizeof(buf));		CHECK (!strcmp (buf, "2.5"));
	ArchCursor_FormatMetres (40, buf, sizeof(buf));		CHECK (!strcmp (buf, "1.25"));
	ArchCursor_FormatMetres (16, buf, sizeof(buf));		CHECK (!strcmp (buf, "0.5"));
	ArchCursor_FormatMetres (1024, buf, sizeof(buf));	CHECK (!strcmp (buf, "32.0"));
	ArchCursor_FormatMetres (8, buf, sizeof(buf));		CHECK (!strcmp (buf, "0.25"));
}

/* Final review: the cursor must not sink into an adjoining floor, wall or ceiling */
static void test_fit (void)
{
	acursor_t c;
	int ok;

	ArchWorld_Reset ();	/* room -160..160 */

	/* level aim from the floor: eye height -114, default size. Unfitted it reaches 32 below the floor. */
	c = place (160 - 1.0 / 32, 0, -114, -1, 0, 0, 0, 160, 160, &ok);
	CHECK (c.vmin == -192);
	ArchCursor_Fit (&c, ArchWorld_PointSolid);
	CHECK (c.vmin == -160 && c.vmax == 0);
	CHECK (c.umin == -80 && c.umax == 80);

	/* near the ceiling and a side wall at once */
	c = place (160 - 1.0 / 32, 150, 150, -1, 0, 0, 0, 160, 96, &ok);
	ArchCursor_Fit (&c, ArchWorld_PointSolid);
	CHECK (c.vmax == 160 && c.vmin == 64);
	CHECK (c.umax == 160 && c.umin == 0);

	/* already clear of everything: unchanged */
	c = place (160 - 1.0 / 32, 8, 8, -1, 0, 0, 0, 160, 160, &ok);
	ArchCursor_Fit (&c, ArchWorld_PointSolid);
	CHECK (c.umin == -72 && c.umax == 88 && c.vmin == -72 && c.vmax == 88);

	/* bigger than the wall: it cannot fit, so it stays centred where it was aimed */
	c = place (160 - 1.0 / 32, 0, 0, -1, 0, 0, 0, 512, 512, &ok);
	ArchCursor_Fit (&c, ArchWorld_PointSolid);
	CHECK (c.umin == -256 && c.umax == 256 && c.vmin == -256 && c.vmax == 256);

	/* on the floor next to a wall */
	c = place (150, 0, -160 + 1.0 / 32, 0, 0, 1, 0, 160, 160, &ok);
	ArchCursor_Fit (&c, ArchWorld_PointSolid);
	CHECK (c.vaxis == 0 && c.vmax == 160 && c.vmin == 0);

	/* hanging off a ledge into open air is left alone: only solid neighbours push the cursor */
	{
		int mn[3] = {-32, -32, -160}, mx[3] = {32, 32, -96};
		abrush_t *pillar = ArchBrush_Box (mn, mx);
		ArchWorld_Add (pillar);
		ArchBrush_Free (pillar);
	}
	c = place (0, 0, -96 + 1.0 / 32, 0, 0, 1, 0, 160, 160, &ok);	/* top of a 64-wide pillar */
	ArchCursor_Fit (&c, ArchWorld_PointSolid);
	CHECK (c.umin == -80 && c.umax == 80 && c.vmin == -80 && c.vmax == 80);
	ArchWorld_Reset ();
}

void test_cursor (void)
{
	test_fit ();
	test_axes ();
	test_snap ();
	test_boxes ();
	test_steps ();
	test_format ();
}
