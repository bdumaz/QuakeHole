/*
QuakeArch - writes the brush world as standard Quake .map text.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include "arch_map.h"

int ArchMap_Write (FILE *f, const char *const texnames[ARCH_NUM_SLOTS], const int spawn[3])
{
	int	id, i;

	fprintf (f, "// QuakeArch export\n{\n\"classname\" \"worldspawn\"\n");
	for (id = 0; id < ArchWorld_SlotCount (); id++)
	{
		const abrush_t *b = ArchWorld_Brush (id);

		if (!b)
			continue;
		fprintf (f, "{\n");
		for (i = 0; i < b->nfaces; i++)
		{
			const aplane_t *pl = &b->faces[i].plane;
			fprintf (f, "( %d %d %d ) ( %d %d %d ) ( %d %d %d ) %s 0 0 0 1 1\n",
				pl->p[0][0], pl->p[0][1], pl->p[0][2],
				pl->p[1][0], pl->p[1][1], pl->p[1][2],
				pl->p[2][0], pl->p[2][1], pl->p[2][2],
				texnames[b->faces[i].slot]);
		}
		fprintf (f, "}\n");
	}
	fprintf (f, "}\n{\n\"classname\" \"info_player_start\"\n\"origin\" \"%d %d %d\"\n}\n",
		spawn[0], spawn[1], spawn[2]);

	if (fflush (f) != 0 || ferror (f))
		return 0;
	return 1;
}
