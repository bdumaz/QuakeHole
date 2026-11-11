/* Tests added for findings of the Stage 3 review. */
#include <stdlib.h>
#include <string.h>
#include "t.h"
#include "arch_world.h"
#include "arch_cursor.h"
#include "arch_pads.h"

static char text[1 << 16];

static acursor_t on (double hx, double hy, double hz, double nx, double ny, double nz, int w, int h, int *ok)
{
	avec3 hit, n;
	acursor_t c;
	hit[0] = hx; hit[1] = hy; hit[2] = hz;
	n[0] = nx; n[1] = ny; n[2] = nz;
	memset (&c, 0, sizeof(c));
	*ok = ArchCursor_Place (hit, n, 0, w, h, &c);
	return c;
}

/* writes the pads and reports whether the text contains the given key/value pair */
static int export_has (const char *needle)
{
	FILE *f = tmpfile ();
	size_t len;

	if (!f)
		return 0;
	ArchPads_Write (f);
	rewind (f);
	len = fread (text, 1, sizeof(text) - 1, f);
	text[len] = 0;
	fclose (f);
	return strstr (text, needle) != NULL;
}

void test_review3 (void)
{
	acursor_t c;
	int ok, mn[3], mx[3];
	abrush_t *b;

	ArchWorld_Reset ();
	ArchPads_Clear ();

	/* the angle key for every direction. Stock Quake ignores an angle of 0, so east must be 360. */
	c = on (-160, 0, 0, 1, 0, 0, 64, 64, &ok);	/* -x wall: pushes toward +x */
	ArchPads_Add (&c, 600);
	CHECK (export_has ("\"angle\" \"360\""));
	CHECK (!export_has ("\"angle\" \"0\""));
	ArchPads_Clear ();
	c = on (0, -160, 0, 0, 1, 0, 64, 64, &ok);	/* pushes toward +y */
	ArchPads_Add (&c, 600);
	CHECK (export_has ("\"angle\" \"90\""));
	ArchPads_Clear ();
	c = on (0, 160, 0, 0, -1, 0, 64, 64, &ok);	/* pushes toward -y */
	ArchPads_Add (&c, 600);
	CHECK (export_has ("\"angle\" \"270\""));
	ArchPads_Clear ();
	c = on (0, 0, -160, 0, 0, 1, 64, 64, &ok);	/* floor: up */
	ArchPads_Add (&c, 568);
	CHECK (export_has ("\"angle\" \"-1\""));
	/* strengths that are not multiples of 10 keep their value */
	CHECK (export_has ("\"speed\" \"56.8\""));
	ArchPads_Clear ();

	/* a pad needs surface under its centre: solid behind, empty in front */
	c = on (0, 0, -160, 0, 0, 1, 64, 64, &ok);
	CHECK (ArchPads_CanPlace (&c, ArchWorld_PointSolid) == 1);
	/* a 32-wide ledge beside a pit: a 160 cursor centred over the pit has no surface there */
	mn[0] = -160; mn[1] = -160; mn[2] = -400; mx[0] = 128; mx[1] = 160; mx[2] = -160;
	b = ArchBrush_Box (mn, mx);
	ArchWorld_Carve (b);
	ArchBrush_Free (b);
	c = on (144, 0, -160, 0, 0, 1, 160, 160, &ok);	/* centred on the ledge, hanging over the pit */
	c.vmin = 0; c.vmax = 160;			/* as the fit step would leave it: centre at x = 80, over the pit */
	CHECK (ArchPads_CanPlace (&c, ArchWorld_PointSolid) == 0);
	/* centre covered by solid in front */
	ArchWorld_Reset ();
	mn[0] = -16; mn[1] = -16; mn[2] = -160; mx[0] = 16; mx[1] = 16; mx[2] = -100;
	b = ArchBrush_Box (mn, mx);
	ArchWorld_Add (b);
	ArchBrush_Free (b);
	c = on (0, 0, -160, 0, 0, 1, 160, 160, &ok);
	CHECK (ArchPads_CanPlace (&c, ArchWorld_PointSolid) == 0);

	/* the cursor only lands on exactly axis-aligned faces: the nearly flat top segment of a low arch is refused */
	on (0, 0, 7.48, 0, -0.032, -0.9995, 160, 160, &ok);
	CHECK (!ok);
	on (0, 0, 8, 0, 0, -1, 160, 160, &ok);
	CHECK (ok);

	ArchPads_Clear ();
	ArchWorld_Reset ();
}
