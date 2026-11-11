#include <stdlib.h>
#include <string.h>
#include "t.h"
#include "arch_world.h"
#include "arch_cursor.h"
#include "arch_pads.h"
#include "arch_map.h"

static acursor_t on (double hx, double hy, double hz, double nx, double ny, double nz, int w, int h)
{
	avec3 hit, n;
	acursor_t c;
	hit[0] = hx; hit[1] = hy; hit[2] = hz;
	n[0] = nx; n[1] = ny; n[2] = nz;
	memset (&c, 0, sizeof(c));
	ArchCursor_Place (hit, n, 0, w, h, &c);
	return c;
}

static int touch (double x, double y, double z, double vel[3])
{
	/* a player-sized box with its origin at x y z */
	avec3 mn, mx;
	mn[0] = x - 16; mn[1] = y - 16; mn[2] = z - 24;
	mx[0] = x + 16; mx[1] = y + 16; mx[2] = z + 32;
	return ArchPads_Touch (mn, mx, vel);
}

static void carve_box (int x1, int y1, int z1, int x2, int y2, int z2, int add)
{
	int mn[3], mx[3];
	abrush_t *b;
	mn[0] = x1; mn[1] = y1; mn[2] = z1; mx[0] = x2; mx[1] = y2; mx[2] = z2;
	b = ArchBrush_Box (mn, mx);
	if (add) ArchWorld_Add (b); else ArchWorld_Carve (b);
	ArchBrush_Free (b);
}

void test_pads (void)
{
	static const char *const tex[ARCH_NUM_SLOTS] = {"g", "w", "c"};
	static const int spawn[3] = {0, 0, -136};
	static char text[1 << 20];
	acursor_t floor_c, wall_c, ceil_c;
	const apad_t *p;
	double vel[3];
	size_t len;
	FILE *f;
	int i;

	ArchWorld_Reset ();
	ArchPads_Clear ();
	CHECK (ArchPads_Count () == 0);

	/* a floor pad: trigger from the floor to 16 above it */
	floor_c = on (0, 0, -160, 0, 0, 1, 64, 64);
	CHECK (ArchPads_Add (&floor_c, ARCH_PAD_DEFAULT) == 1);
	CHECK (ArchPads_Count () == 1);
	p = ArchPads_Get (0);
	CHECK (p && p->naxis == 2 && p->nsign == 1 && p->plane == -160 && p->speed == 600);
	CHECK (p->mins[0] == -32 && p->maxs[0] == 32 && p->mins[2] == -160 && p->maxs[2] == -144);

	/* standing on it launches straight up and keeps sideways speed */
	vel[0] = 100; vel[1] = -50; vel[2] = -20;
	CHECK (touch (0, 0, -136, vel) == 1);
	CHECK (vel[0] == 100 && vel[1] == -50 && vel[2] == 600);
	/* beside it, or well above it, nothing happens */
	vel[2] = 0;
	CHECK (touch (100, 0, -136, vel) == 0 && vel[2] == 0);
	CHECK (touch (0, 0, -100, vel) == 0);
	/* touching the trigger's edge face to face is not an overlap */
	CHECK (touch (48, 0, -136, vel) == 0);

	/* an overlapping pad on the same surface replaces the old one */
	floor_c = on (16, 0, -160, 0, 0, 1, 64, 64);
	CHECK (ArchPads_Add (&floor_c, 1000) == 1);
	CHECK (ArchPads_Count () == 1 && ArchPads_Get (0)->speed == 1000);
	/* one that does not overlap is added */
	floor_c = on (-120, -120, -160, 0, 0, 1, 32, 32);
	CHECK (ArchPads_Add (&floor_c, 300) == 1 && ArchPads_Count () == 2);

	/* a wall pad pushes away from the wall; a ceiling pad pushes down */
	wall_c = on (160, 0, 0, -1, 0, 0, 64, 64);
	CHECK (ArchPads_Add (&wall_c, 500) == 1);
	vel[0] = 300; vel[1] = 7; vel[2] = 9;
	CHECK (touch (140, 0, 0, vel) == 1);
	CHECK (vel[0] == -500 && vel[1] == 7 && vel[2] == 9);
	ceil_c = on (100, 100, 160, 0, 0, -1, 64, 64);
	CHECK (ArchPads_Add (&ceil_c, 400) == 1);
	vel[0] = vel[1] = 0; vel[2] = 200;
	CHECK (touch (100, 100, 124, vel) == 1 && vel[2] == -400);
	CHECK (ArchPads_Count () == 4);

	/* nothing is removed while the world is unchanged */
	CHECK (ArchPads_Prune (ArchWorld_PointSolid) == 0);
	/* carve the floor from under the first pad: it goes */
	carve_box (-32, -64, -200, 64, 64, -160, 0);
	CHECK (ArchPads_Prune (ArchWorld_PointSolid) == 1 && ArchPads_Count () == 3);
	/* build a block over the small floor pad: it goes */
	carve_box (-140, -140, -160, -100, -100, -120, 1);
	CHECK (ArchPads_Prune (ArchWorld_PointSolid) == 1 && ArchPads_Count () == 2);

	/* drag depth to strength: pull (negative) raises, push lowers, within limits */
	CHECK (ArchPads_SpeedFromDrag (0) == 600);
	CHECK (ArchPads_SpeedFromDrag (-40) == 760);
	CHECK (ArchPads_SpeedFromDrag (40) == 440);
	CHECK (ArchPads_SpeedFromDrag (-2048) == ARCH_PAD_MAX);
	CHECK (ArchPads_SpeedFromDrag (2048) == ARCH_PAD_MIN);

	/* export: one trigger_push per pad, appended to the map */
	f = tmpfile ();
	CHECK (f != NULL);
	if (f)
	{
		char *q;
		int count = 0;

		CHECK (ArchMap_Write (f, tex, spawn) == 1);
		rewind (f);
		len = fread (text, 1, sizeof(text) - 1, f);
		text[len] = 0;
		fclose (f);
		for (q = text; (q = strstr (q, "\"classname\" \"trigger_push\"")) != NULL; q++)
			count++;
		CHECK (count == 2);
		CHECK (strstr (text, "\"speed\" \"50\"") != NULL);	/* wall pad: 500 / 10 */
		CHECK (strstr (text, "\"angle\" \"180\"") != NULL);	/* pushes toward -x */
		CHECK (strstr (text, "\"speed\" \"40\"") != NULL);	/* ceiling pad */
		CHECK (strstr (text, "\"angle\" \"-2\"") != NULL);	/* down */
		CHECK (strstr (text, ") trigger 0 0 0 1 1") != NULL);
		/* the wall pad's trigger brush spans x 144..160 */
		CHECK (strstr (text, "( 144 ") != NULL);
	}

	/* the limit */
	ArchPads_Clear ();
	for (i = 0; i < ARCH_MAX_PADS; i++)
	{
		acursor_t c = on (-152 + 16 * (i % 16), -152 + 16 * (i / 16), -160, 0, 0, 1, 16, 16);
		if (ArchPads_Add (&c, 600) != 1)
			break;
	}
	CHECK (i == ARCH_MAX_PADS && ArchPads_Count () == ARCH_MAX_PADS);
	floor_c = on (-152, -152, -160, 0, 0, 1, 16, 16);
	/* every square is taken, so this overlaps one and replaces it rather than exceeding the limit */
	CHECK (ArchPads_Add (&floor_c, 700) == 1 && ArchPads_Count () == ARCH_MAX_PADS);
	wall_c = on (160, 0, 0, -1, 0, 0, 64, 64);
	CHECK (ArchPads_Add (&wall_c, 500) == 0 && ArchPads_Count () == ARCH_MAX_PADS);

	ArchPads_Clear ();
	CHECK (ArchPads_Count () == 0 && ArchPads_Get (0) == NULL);
	ArchWorld_Reset ();
}
