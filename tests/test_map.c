#include <stdlib.h>
#include <string.h>
#include "t.h"
#include "arch_world.h"
#include "arch_map.h"

typedef struct { int p[3][3]; char tex[32]; } pface_t;
typedef struct { pface_t faces[64]; int nfaces; } pbrush_t;

static pbrush_t	parsed[256];
static int	nparsed;
static char	text[1 << 20];

/* reads the worldspawn brushes of a .map back in */
static void parse_map (void)
{
	char *line = text, *next;
	int depth = 0;

	nparsed = 0;
	for (; line && *line; line = next)
	{
		next = strchr (line, '\n');
		if (next)
			*next++ = 0;
		if (line[0] == '{')
		{
			if (++depth == 2 && nparsed < 256)
				parsed[nparsed].nfaces = 0;
		}
		else if (line[0] == '}')
		{
			if (depth-- == 2)
				nparsed++;
		}
		else if (line[0] == '(' && depth == 2 && nparsed < 256)
		{
			pbrush_t *b = &parsed[nparsed];
			pface_t *f = &b->faces[b->nfaces];
			int off[3];
			float sx, sy;
			if (sscanf (line, "( %d %d %d ) ( %d %d %d ) ( %d %d %d ) %31s %d %d %d %f %f",
				&f->p[0][0], &f->p[0][1], &f->p[0][2], &f->p[1][0], &f->p[1][1], &f->p[1][2],
				&f->p[2][0], &f->p[2][1], &f->p[2][2], f->tex, &off[0], &off[1], &off[2], &sx, &sy) == 15
				&& off[0] == 0 && off[1] == 0 && off[2] == 0 && sx == 1 && sy == 1 && b->nfaces < 63)
				b->nfaces++;
		}
	}
}

void test_map (void)
{
	static const char *const tex[ARCH_NUM_SLOTS] = {"g", "w", "c"};
	static const int spawn[3] = {0, 0, -136};
	abrush_t *tunnel;
	int mn[3] = {160, -32, -32}, mx[3] = {480, 32, 32};
	FILE *f;
	size_t len;
	double volume = 0;
	int id, n = 0, i, j, k, m;

	ArchWorld_Reset ();
	tunnel = ArchBrush_Box (mn, mx);
	ArchWorld_Carve (tunnel);
	ArchBrush_Free (tunnel);

	f = tmpfile ();
	CHECK (f != NULL);
	if (!f)
		return;
	CHECK (ArchMap_Write (f, tex, spawn) == 1);
	rewind (f);
	len = fread (text, 1, sizeof(text) - 1, f);
	text[len] = 0;
	fclose (f);

	CHECK (strstr (text, "\"classname\" \"worldspawn\"") != NULL);
	CHECK (strstr (text, "\"classname\" \"info_player_start\"") != NULL);
	CHECK (strstr (text, "\"origin\" \"0 0 -136\"") != NULL);

	parse_map ();
	CHECK (nparsed == ArchWorld_BrushCount ());

	/* every live brush appears in order, with exact plane points and the texture for its slot */
	for (id = 0; id < ArchWorld_SlotCount () && n < nparsed; id++)
	{
		const abrush_t *b = ArchWorld_Brush (id);
		aplane_t planes[64];
		abrush_t *rebuilt;

		if (!b)
			continue;
		CHECK (parsed[n].nfaces == b->nfaces);
		for (i = 0; i < b->nfaces && i < parsed[n].nfaces; i++)
		{
			int same = 1;
			for (j = 0; j < 3; j++)
				for (k = 0; k < 3; k++)
					if (parsed[n].faces[i].p[j][k] != b->faces[i].plane.p[j][k])
						same = 0;
			CHECK (same);
			CHECK (!strcmp (parsed[n].faces[i].tex, tex[b->faces[i].slot]));
		}
		/* the text alone rebuilds the same solid */
		for (m = 0; m < parsed[n].nfaces; m++)
			CHECK (ArchPlane_FromPoints (parsed[n].faces[m].p[0], parsed[n].faces[m].p[1], parsed[n].faces[m].p[2], &planes[m]));
		rebuilt = ArchBrush_FromPlanes (planes, parsed[n].nfaces);
		CHECK (rebuilt != NULL);
		volume += ArchBrush_Volume (rebuilt);
		ArchBrush_Free (rebuilt);
		n++;
	}
	CHECK_NEAR (volume, ArchWorld_SolidVolume (), 1.0);

	/* a write error is reported */
	f = tmpfile ();
	if (f)
	{
		fclose (f);
		f = fopen ("build\\tests\\readonly.tmp", "w");
		if (f) fclose (f);
		f = fopen ("build\\tests\\readonly.tmp", "r");
		CHECK (f != NULL);
		if (f)
		{
			CHECK (ArchMap_Write (f, tex, spawn) == 0);
			fclose (f);
		}
	}
	ArchWorld_Reset ();
}
