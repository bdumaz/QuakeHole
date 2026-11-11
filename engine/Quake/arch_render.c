/*
QuakeHole - draws the brush world.

Each grid cell owns one triangle list per texture slot, built from the
brushes whose home cell it is. Only cells touched by an edit are rebuilt.
Drawing is fullbright with no lightmaps.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include "quakedef.h"
#include "arch.h"
#include "arch_world.h"

#define VERT_FLOATS	5	/* x y z s t */

typedef struct
{
	float	*verts[ARCH_NUM_SLOTS];
	int	count[ARCH_NUM_SLOTS];		/* vertices */
	int	cap[ARCH_NUM_SLOTS];
	vec3_t	mins, maxs;
	qboolean used;
} cellmesh_t;

typedef struct
{
	gltexture_t	*gl;
	int		width, height;
	byte		*pixels;	/* kept: the texture manager reloads from it */
} archtex_t;

static cellmesh_t	meshes[ARCH_NUM_CELLS];
static byte		rebuild[ARCH_NUM_CELLS];
static int		dirty_cells[ARCH_NUM_CELLS];
static archtex_t	textures[ARCH_NUM_SLOTS];
static qboolean		textures_loaded;
static qboolean		rebuild_all = true;

/* ------------------------------------------------------------- textures */

static void use_checkerboard (archtex_t *tex, const char *name)
{
	int x, y;

	Con_Printf ("arch: texture %s not found, using checkerboard\n", name);
	tex->width = tex->height = 64;
	tex->pixels = (byte *) malloc (64 * 64);
	for (y = 0; y < 64; y++)
		for (x = 0; x < 64; x++)
			tex->pixels[y * 64 + x] = (((x >> 4) ^ (y >> 4)) & 1) ? 4 : 10;
}

/* copies the named texture out of a BSP file image; false if it is not there */
static qboolean find_miptex (byte *bsp, const char *name, archtex_t *tex)
{
	dheader_t	*header = (dheader_t *) bsp;
	lump_t		*lump;
	dmiptexlump_t	*m;
	int		i, nummiptex;

	if (!bsp)
		return false;
	lump = &header->lumps[LUMP_TEXTURES];
	if (LittleLong (lump->filelen) < (int) sizeof(int))
		return false;
	m = (dmiptexlump_t *) (bsp + LittleLong (lump->fileofs));
	nummiptex = LittleLong (m->nummiptex);
	for (i = 0; i < nummiptex; i++)
	{
		int		ofs = LittleLong (m->dataofs[i]);
		miptex_t	*mt;

		if (ofs < 0)
			continue;
		mt = (miptex_t *) ((byte *) m + ofs);
		if (q_strncasecmp (mt->name, name, sizeof(mt->name)))
			continue;
		tex->width = LittleLong (mt->width);
		tex->height = LittleLong (mt->height);
		tex->pixels = (byte *) malloc (tex->width * tex->height);
		memcpy (tex->pixels, (byte *) mt + LittleLong (mt->offsets[0]), tex->width * tex->height);
		return true;
	}
	return false;
}

/* the three textures come from a map inside the user's own pak */
static void load_textures (void)
{
	const char	*names[ARCH_NUM_SLOTS];
	byte		*bsp = COM_LoadMallocFile ("maps/start.bsp", NULL);
	int		i;

	Arch_TextureNames (names);
	for (i = 0; i < ARCH_NUM_SLOTS; i++)
	{
		archtex_t *tex = &textures[i];

		if (!find_miptex (bsp, names[i], tex))
			use_checkerboard (tex, names[i]);
		tex->gl = TexMgr_LoadImage (NULL, va ("arch_%s", names[i]), tex->width, tex->height, SRC_INDEXED,
					    tex->pixels, "", (src_offset_t) tex->pixels, TEXPREF_MIPMAP);
	}
	free (bsp);
	textures_loaded = true;
}

/* --------------------------------------------------------------- meshes */

/* qbsp's world-aligned texture axes: for each base normal, the S and T directions */
static const float baseaxis[6][3][3] =
{
	{{ 0, 0, 1}, {1, 0, 0}, {0, -1, 0}},	/* floor */
	{{ 0, 0,-1}, {1, 0, 0}, {0, -1, 0}},	/* ceiling */
	{{ 1, 0, 0}, {0, 1, 0}, {0, 0, -1}},	/* west wall */
	{{-1, 0, 0}, {0, 1, 0}, {0, 0, -1}},	/* east wall */
	{{ 0, 1, 0}, {1, 0, 0}, {0, 0, -1}},	/* south wall */
	{{ 0,-1, 0}, {1, 0, 0}, {0, 0, -1}},	/* north wall */
};

static int texture_axis (const avec3 normal)
{
	double	best = 0, dot;
	int	i, bestaxis = 0;

	for (i = 0; i < 6; i++)
	{
		dot = normal[0] * baseaxis[i][0][0] + normal[1] * baseaxis[i][0][1] + normal[2] * baseaxis[i][0][2];
		if (dot > best)
		{
			best = dot;
			bestaxis = i;
		}
	}
	return bestaxis;
}

/* a face on the outer boundary of the build volume can never be seen */
static qboolean on_volume_boundary (const aplane_t *pl)
{
	int k;
	for (k = 0; k < 3; k++)
		if (fabs (pl->n[k]) > 0.999999 && fabs (pl->d - ARCH_HALF) < 0.001)
			return true;
	return false;
}

static void add_vertex (cellmesh_t *mesh, int slot, const avec3 p, int axis)
{
	const archtex_t	*tex = &textures[slot];
	float		*v;
	int		k;

	if (mesh->count[slot] == mesh->cap[slot])
	{
		mesh->cap[slot] = mesh->cap[slot] ? mesh->cap[slot] * 2 : 96;
		mesh->verts[slot] = (float *) realloc (mesh->verts[slot], mesh->cap[slot] * VERT_FLOATS * sizeof(float));
	}
	v = mesh->verts[slot] + mesh->count[slot]++ * VERT_FLOATS;
	for (k = 0; k < 3; k++)
	{
		v[k] = (float) p[k];
		if (!mesh->used || v[k] < mesh->mins[k]) mesh->mins[k] = v[k];
		if (!mesh->used || v[k] > mesh->maxs[k]) mesh->maxs[k] = v[k];
	}
	mesh->used = true;
	v[3] = (float) ((p[0] * baseaxis[axis][1][0] + p[1] * baseaxis[axis][1][1] + p[2] * baseaxis[axis][1][2]) / tex->width);
	v[4] = (float) ((p[0] * baseaxis[axis][2][0] + p[1] * baseaxis[axis][2][1] + p[2] * baseaxis[axis][2][2]) / tex->height);
}

static void add_brush (cellmesh_t *mesh, const abrush_t *b)
{
	int i, v;

	for (i = 0; i < b->nfaces; i++)
	{
		const aface_t	*f = &b->faces[i];
		int		axis;

		if (on_volume_boundary (&f->plane))
			continue;
		axis = texture_axis (f->plane.n);
		/* fan in stored (clockwise) order, which the engine treats as front-facing */
		for (v = 1; v + 1 < f->nverts; v++)
		{
			add_vertex (mesh, f->slot, f->verts[0], axis);
			add_vertex (mesh, f->slot, f->verts[v], axis);
			add_vertex (mesh, f->slot, f->verts[v + 1], axis);
		}
	}
}

static void rebuild_meshes (void)
{
	int i, n, id, any = rebuild_all;

	n = ArchWorld_TakeDirtyCells (dirty_cells, ARCH_NUM_CELLS);
	if (rebuild_all)
		memset (rebuild, 1, sizeof(rebuild));
	else
	{
		for (i = 0; i < n; i++)
			rebuild[dirty_cells[i]] = 1;
		any = n > 0;
	}
	rebuild_all = false;
	if (!any)
		return;

	for (i = 0; i < ARCH_NUM_CELLS; i++)
	{
		if (rebuild[i])
		{
			meshes[i].count[0] = meshes[i].count[1] = meshes[i].count[2] = 0;
			meshes[i].used = false;
		}
	}
	for (id = 0; id < ArchWorld_SlotCount (); id++)
	{
		const abrush_t *b = ArchWorld_Brush (id);
		if (b && rebuild[ArchWorld_HomeCell (id)])
			add_brush (&meshes[ArchWorld_HomeCell (id)], b);
	}
	memset (rebuild, 0, sizeof(rebuild));
}

/* ----------------------------------------------------------------- hooks */

void ArchRender_NewMap (void)
{
	if (!textures_loaded)
		load_textures ();
	rebuild_all = true;
}

void ArchRender_Draw (void)
{
	int slot, i;

	if (!textures_loaded)
		return;
	rebuild_meshes ();

	GL_BindBuffer (GL_ARRAY_BUFFER, 0);
	GL_ClientActiveTextureFunc (GL_TEXTURE0_ARB);
	glEnableClientState (GL_VERTEX_ARRAY);
	glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	glColor3f (1, 1, 1);

	for (slot = 0; slot < ARCH_NUM_SLOTS; slot++)
	{
		GL_Bind (textures[slot].gl);
		for (i = 0; i < ARCH_NUM_CELLS; i++)
		{
			cellmesh_t *mesh = &meshes[i];

			if (!mesh->used || !mesh->count[slot] || R_CullBox (mesh->mins, mesh->maxs))
				continue;
			glVertexPointer (3, GL_FLOAT, VERT_FLOATS * sizeof(float), mesh->verts[slot]);
			glTexCoordPointer (2, GL_FLOAT, VERT_FLOATS * sizeof(float), mesh->verts[slot] + 3);
			glDrawArrays (GL_TRIANGLES, 0, mesh->count[slot]);
			rs_brushpolys += mesh->count[slot] / 3;
		}
	}

	glDisableClientState (GL_VERTEX_ARRAY);
	glDisableClientState (GL_TEXTURE_COORD_ARRAY);
}
