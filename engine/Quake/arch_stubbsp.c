/*
QuakeArch - a minimal BSP world model built in memory.

Quake needs a BSP world for leaf and visibility lookups. The real geometry
lives in the brush world (arch_world.c); this stub is one empty, always
visible leaf whose clip hulls are empty everywhere.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include "quakedef.h"
#include "arch.h"
#include "arch_world.h"

const int arch_spawn_origin[3] = {0, 0, -ARCH_ROOM_HALF + 24};

static byte	stub[4096];
static int	stub_size;

static void *add_lump (dheader_t *header, int lump, const void *data, int len)
{
	void *dest = stub + stub_size;

	header->lumps[lump].fileofs = stub_size;
	header->lumps[lump].filelen = len;
	if (len)
		memcpy (dest, data, len);
	stub_size += (len + 3) & ~3;
	return dest;
}

byte *ArchStubBSP_Build (int *size)
{
	dheader_t	*header = (dheader_t *) stub;
	char		entities[256];
	dplane_t	plane;
	dsnode_t	node;
	dsleaf_t	leafs[2];
	dsclipnode_t	clipnode;
	dmodel_t	model;
	int		i;

	memset (stub, 0, sizeof(stub));
	stub_size = sizeof(dheader_t);
	header->version = BSPVERSION;

	q_snprintf (entities, sizeof(entities),
		"{\n\"classname\" \"worldspawn\"\n}\n"
		"{\n\"classname\" \"info_player_start\"\n\"origin\" \"%d %d %d\"\n\"angle\" \"0\"\n}\n",
		arch_spawn_origin[0], arch_spawn_origin[1], arch_spawn_origin[2]);
	add_lump (header, LUMP_ENTITIES, entities, (int) strlen (entities) + 1);

	memset (&plane, 0, sizeof(plane));
	plane.normal[2] = 1;
	plane.type = PLANE_Z;
	add_lump (header, LUMP_PLANES, &plane, sizeof(plane));

	/* one node whose two children are both leaf 1 */
	memset (&node, 0, sizeof(node));
	node.children[0] = node.children[1] = -2;
	memset (leafs, 0, sizeof(leafs));
	leafs[0].contents = CONTENTS_SOLID;
	leafs[0].visofs = -1;
	leafs[1].contents = CONTENTS_EMPTY;
	leafs[1].visofs = -1;
	memset (&model, 0, sizeof(model));
	for (i = 0; i < 3; i++)
	{
		node.mins[i] = leafs[1].mins[i] = -ARCH_HALF;
		node.maxs[i] = leafs[1].maxs[i] = ARCH_HALF;
		model.mins[i] = -ARCH_HALF;
		model.maxs[i] = ARCH_HALF;
	}
	model.visleafs = 1;
	add_lump (header, LUMP_NODES, &node, sizeof(node));
	add_lump (header, LUMP_LEAFS, leafs, sizeof(leafs));

	/* clip hulls: empty everywhere; collision comes from arch_collide.c */
	clipnode.planenum = 0;
	clipnode.children[0] = clipnode.children[1] = CONTENTS_EMPTY;
	add_lump (header, LUMP_CLIPNODES, &clipnode, sizeof(clipnode));

	add_lump (header, LUMP_MODELS, &model, sizeof(model));

	/* everything else is empty */
	add_lump (header, LUMP_TEXTURES, NULL, 0);
	add_lump (header, LUMP_VERTEXES, NULL, 0);
	add_lump (header, LUMP_VISIBILITY, NULL, 0);
	add_lump (header, LUMP_TEXINFO, NULL, 0);
	add_lump (header, LUMP_FACES, NULL, 0);
	add_lump (header, LUMP_LIGHTING, NULL, 0);
	add_lump (header, LUMP_MARKSURFACES, NULL, 0);
	add_lump (header, LUMP_EDGES, NULL, 0);
	add_lump (header, LUMP_SURFEDGES, NULL, 0);

	*size = stub_size;
	return stub;
}
