/*
QuakeArch - digging in campaign maps: the drawing side.

A campaign map draws its walls from its own lit, textured surfaces. When a
box of solid is dug away, every surface that had that solid behind it is
replaced by fragments covering what is left of it. The fragments are extra
surfaces that look to the renderer exactly like the original (same texture
mapping, same baked lighting), so they go through the engine's normal world
drawing. The original is flagged and skipped.

The inside of a hole is drawn from the brushes themselves: the faces of every
brush near a dig, textured like the wall that was blasted.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include "quakedef.h"
#include "arch.h"
#include "arch_world.h"
#include "arch_poly.h"

#define SURF_ARCHDUG	0x10000		/* this map surface has been replaced by fragments */

qboolean R_BackFaceCull (msurface_t *surf);
extern vec3_t lightcolor;	/* set by R_LightPoint */

/* ------------------------------------------------------------- fragments */

typedef struct afrag_s
{
	msurface_t	surf;		/* a copy of the parent with its own polygon */
	msurface_t	*parent;
	struct afrag_s	*next;
} afrag_t;

static afrag_t	**frags;		/* per world surface: its fragments */
static int	frags_count;		/* size of that array */
static int	frag_verts;		/* total vertices in all fragments */
static qboolean	vbo_stale;
static qboolean	map_dug;

qboolean ArchSurf_Dug (void)
{
	return map_dug;
}

static void free_fragment (afrag_t *f)
{
	frag_verts -= f->surf.numedges;
	free (f->surf.polys);
	free (f);
}

/* the surface's texture and lightmap coordinates for a point, as the engine computes them at load */
static void fill_vertex (const msurface_t *fa, const double *p, float *out)
{
	const mtexinfo_t	*tex = fa->texinfo;
	vec3_t			v;
	float			s, t;

	v[0] = (float) p[0]; v[1] = (float) p[1]; v[2] = (float) p[2];
	VectorCopy (v, out);

	s = (DotProduct (v, tex->vecs[0]) + tex->vecs[0][3]) / tex->texture->width;
	t = (DotProduct (v, tex->vecs[1]) + tex->vecs[1][3]) / tex->texture->height;
	if (tex->texture->shift > 0)
	{
		s /= (2 * tex->texture->shift);
		t /= (2 * tex->texture->shift);
	}
	out[3] = s;
	out[4] = t;

	s = DotProduct (v, tex->vecs[0]) + tex->vecs[0][3];
	s -= fa->texturemins[0];
	s += fa->light_s * 16;
	s += 8;
	s /= LMBLOCK_WIDTH * 16;
	t = DotProduct (v, tex->vecs[1]) + tex->vecs[1][3];
	t -= fa->texturemins[1];
	t += fa->light_t * 16;
	t += 8;
	t /= LMBLOCK_HEIGHT * 16;
	out[5] = s;
	out[6] = t;
}

static afrag_t *make_fragment (msurface_t *parent, const apoly_t *p)
{
	afrag_t		*f = (afrag_t *) calloc (1, sizeof(*f));
	glpoly_t	*poly;
	int		i, k;

	f->parent = parent;
	f->surf = *parent;
	f->surf.flags &= ~SURF_ARCHDUG;
	f->surf.texturechain = NULL;
	f->surf.numedges = p->n;
	poly = (glpoly_t *) calloc (1, sizeof(glpoly_t) + (p->n > 4 ? p->n - 4 : 0) * VERTEXSIZE * sizeof(float));
	poly->numverts = p->n;
	f->surf.polys = poly;
	for (i = 0; i < p->n; i++)
	{
		fill_vertex (parent, p->v[i], poly->verts[i]);
		for (k = 0; k < 3; k++)
		{
			if (!i || poly->verts[i][k] < f->surf.mins[k]) f->surf.mins[k] = poly->verts[i][k];
			if (!i || poly->verts[i][k] > f->surf.maxs[k]) f->surf.maxs[k] = poly->verts[i][k];
		}
	}
	frag_verts += p->n;
	return f;
}

static void poly_from_gl (const glpoly_t *gp, apoly_t *out)
{
	int	i, k;

	out->n = gp->numverts;
	for (i = 0; i < gp->numverts; i++)
		for (k = 0; k < 3; k++)
			out->v[i][k] = gp->verts[i][k];
}

/* The sky is a surface like any other with solid behind it, and on most maps
   that solid is ordinary rock. Sky surfaces are never cut, so the rock behind
   them must never be removed: a box that would cut one is not dug at all. */
qboolean ArchSurf_BoxCutsSky (const int mins[3], const int maxs[3])
{
	qmodel_t	*world = cl.worldmodel;
	apoly_t		poly, pieces[ARCH_POLY_MAX_PIECES];
	avec3		normal;
	int		i, k;

	if (!world)
		return false;
	for (i = 0; i < world->nummodelsurfaces; i++)
	{
		msurface_t	*surf = &world->surfaces[world->firstmodelsurface + i];
		glpoly_t	*gp;

		if (!(surf->flags & SURF_DRAWSKY))
			continue;
		if (surf->mins[0] > maxs[0] + 1 || surf->maxs[0] < mins[0] - 1 ||
		    surf->mins[1] > maxs[1] + 1 || surf->maxs[1] < mins[1] - 1 ||
		    surf->mins[2] > maxs[2] + 1 || surf->maxs[2] < mins[2] - 1)
			continue;
		for (k = 0; k < 3; k++)
			normal[k] = (surf->flags & SURF_PLANEBACK) ? -surf->plane->normal[k] : surf->plane->normal[k];
		for (gp = surf->polys; gp; gp = gp->next)
		{
			if (gp->numverts > ARCH_POLY_MAX_VERTS - 8)
				return true;	/* cannot tell: leave it alone */
			poly_from_gl (gp, &poly);
			if (ArchPoly_SubtractBox (&poly, normal, mins, maxs, pieces) != ARCH_POLY_UNTOUCHED)
				return true;
		}
	}
	return false;
}

/* Removes from the map's surfaces whatever had the box mins..maxs of solid behind it. */
void ArchSurf_Dig (const int mins[3], const int maxs[3])
{
	qmodel_t	*world = cl.worldmodel;
	apoly_t		poly, pieces[ARCH_POLY_MAX_PIECES];
	avec3		normal;
	int		i, k, n, p;

	if (!world || !frags)
		return;
	for (i = 0; i < world->nummodelsurfaces && i < frags_count; i++)
	{
		msurface_t	*surf = &world->surfaces[world->firstmodelsurface + i];
		afrag_t		**link, *f, *fresh;

		if (surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
			continue;	/* liquids and sky are never cut */
		if (surf->mins[0] > maxs[0] + 1 || surf->maxs[0] < mins[0] - 1 ||
		    surf->mins[1] > maxs[1] + 1 || surf->maxs[1] < mins[1] - 1 ||
		    surf->mins[2] > maxs[2] + 1 || surf->maxs[2] < mins[2] - 1)
			continue;
		if (!surf->polys || surf->polys->numverts > ARCH_POLY_MAX_VERTS - 8)
			continue;
		for (k = 0; k < 3; k++)
			normal[k] = (surf->flags & SURF_PLANEBACK) ? -surf->plane->normal[k] : surf->plane->normal[k];

		if (!(surf->flags & SURF_ARCHDUG))
		{
			poly_from_gl (surf->polys, &poly);
			n = ArchPoly_SubtractBox (&poly, normal, mins, maxs, pieces);
			if (n == ARCH_POLY_UNTOUCHED)
				continue;
			surf->flags |= SURF_ARCHDUG;
			for (p = 0; p < n; p++)
			{
				fresh = make_fragment (surf, &pieces[p]);
				fresh->next = frags[i];
				frags[i] = fresh;
			}
			vbo_stale = true;
			continue;
		}

		/* already in fragments: cut each of those */
		for (link = &frags[i]; (f = *link) != NULL; )
		{
			if (f->surf.polys->numverts > ARCH_POLY_MAX_VERTS - 8)
			{
				link = &f->next;
				continue;
			}
			poly_from_gl (f->surf.polys, &poly);
			n = ArchPoly_SubtractBox (&poly, normal, mins, maxs, pieces);
			if (n == ARCH_POLY_UNTOUCHED)
			{
				link = &f->next;
				continue;
			}
			*link = f->next;
			free_fragment (f);
			for (p = 0; p < n; p++)
			{
				/* new pieces go in front of the link, so they are not cut again by this box */
				fresh = make_fragment (surf, &pieces[p]);
				fresh->next = *link;
				*link = fresh;
				link = &fresh->next;
			}
			vbo_stale = true;
		}
	}
	map_dug = true;
}

/* vertex buffer: the engine keeps all map vertices in one buffer; fragments are appended to it */
int ArchSurf_VertexCount (void)
{
	return frag_verts;
}

unsigned int ArchSurf_WriteVerts (float *varray, unsigned int index)
{
	afrag_t	*f;
	int	i;

	for (i = 0; i < frags_count; i++)
	{
		for (f = frags ? frags[i] : NULL; f; f = f->next)
		{
			f->surf.vbo_firstvert = index;
			memcpy (&varray[VERTEXSIZE * index], f->surf.polys->verts, VERTEXSIZE * sizeof(float) * f->surf.numedges);
			index += f->surf.numedges;
		}
	}
	return index;
}

/* called once all the boxes of a dig have been applied */
void ArchSurf_Commit (void)
{
	if (!vbo_stale)
		return;
	vbo_stale = false;
	GL_BuildBModelVertexBuffer ();
}

/* called while the engine gathers the surfaces to draw this frame */
void ArchSurf_Chain (void)
{
	afrag_t	*f;
	int	i;

	if (!map_dug || !frags)
		return;
	for (i = 0; i < frags_count; i++)
	{
		for (f = frags[i]; f; f = f->next)
		{
			msurface_t *s = &f->surf;

			if (R_CullBox (s->mins, s->maxs) || R_BackFaceCull (s))
				continue;
			/* flashes from explosions and the like are worked out for the parent */
			s->dlightframe = f->parent->dlightframe;
			memcpy (s->dlightbits, f->parent->dlightbits, sizeof(s->dlightbits));
			rs_brushpolys++;
			R_ChainSurface (s, chain_world);
			R_RenderDynamicLightmaps (s);
		}
	}
}

qboolean ArchSurf_Hidden (const msurface_t *surf)
{
	return (surf->flags & SURF_ARCHDUG) != 0;
}

/* ------------------------------------------------- what a hole looks like */

#define MAX_LOOKS	255
#define MAX_DIGS	2048

typedef struct { gltexture_t *texture; int width, height; float light; } look_t;
typedef struct { float mins[3], maxs[3]; int look; } digrec_t;

static look_t	looks[MAX_LOOKS + 1];	/* index 0 is unused: a plane tag of 0 means "no tag" */
static int	num_looks;
static digrec_t	digs[MAX_DIGS];
static int	num_digs;

/* the texture of the map surface nearest to a point, and how bright the map is there */
int ArchSurf_LookFor (vec3_t pos)
{
	qmodel_t	*world = cl.worldmodel;
	texture_t	*best = NULL;
	float		best_dist, light;
	int		i, k;

	int		pass;

	if (!world)
		return 0;
	/* Pass 0: a wall right at the point, even one already partly dug, decides.
	   Failing that, a point inside an existing hole keeps that hole's look: the
	   walls it came from are gone and whatever else is nearest is a poor guess.
	   Pass 1: otherwise the nearest wall within reach. */
	for (pass = 0; pass < 2 && !best; pass++)
	{
	if (pass == 1)
	{
		for (i = num_digs - 1; i >= 0; i--)
		{
			for (k = 0; k < 3; k++)
				if (pos[k] < digs[i].mins[k] || pos[k] > digs[i].maxs[k])
					break;
			if (k == 3 && digs[i].look)
				return digs[i].look;
		}
	}
	best_dist = pass ? 128.0f : 24.0f;
	for (i = 0; i < world->nummodelsurfaces; i++)
	{
		msurface_t	*surf = &world->surfaces[world->firstmodelsurface + i];
		float		d, size[3];

		if (surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY | SURF_NOTEXTURE))
			continue;
		if ((surf->flags & SURF_ARCHDUG) && (pass || !frags || !frags[i]))
			continue;	/* dug away entirely, or only wanted at close range */
		/* trim, lights and other small details make poor rock */
		VectorSubtract (surf->maxs, surf->mins, size);
		if ((size[0] >= 32) + (size[1] >= 32) + (size[2] >= 32) < 2)
			continue;
		for (k = 0; k < 3; k++)
			if (pos[k] < surf->mins[k] - 24 || pos[k] > surf->maxs[k] + 24)
				break;
		if (k < 3)
			continue;
		d = (float) fabs (DotProduct (pos, surf->plane->normal) - surf->plane->dist);
		if (d < best_dist)
		{
			best_dist = d;
			best = surf->texinfo->texture;
		}
	}
	}
	if (!best)
	{
		/* nothing close: reuse the last look, or failing that any ordinary wall texture of this map */
		if (num_looks)
			return num_looks;
		for (i = 0; i < world->numtextures && !best; i++)
			if (world->textures[i] && world->textures[i]->gltexture && world->textures[i]->width >= 64 &&
			    world->textures[i]->name[0] != '*' && world->textures[i]->name[0] != '+' &&
			    q_strncasecmp (world->textures[i]->name, "sky", 3))
				best = world->textures[i];
	}
	if (!best || !best->gltexture)
		return 0;

	R_LightPoint (pos);
	light = (lightcolor[0] + lightcolor[1] + lightcolor[2]) / (3 * 200.0f);
	if (light < 0.3f) light = 0.3f;
	if (light > 1.0f) light = 1.0f;

	for (i = 1; i <= num_looks; i++)
		if (looks[i].texture == best->gltexture && fabs (looks[i].light - light) < 0.08f)
			return i;
	if (num_looks == MAX_LOOKS)
		return num_looks;
	i = ++num_looks;
	looks[i].texture = best->gltexture;
	looks[i].width = best->width;
	looks[i].height = best->height;
	looks[i].light = light;
	return i;
}

/* remember where a dig happened: brushes near it are the ones whose faces may now show */
void ArchSurf_NoteDig (const float mins[3], const float maxs[3], int look)
{
	digrec_t	*d;
	int		k, i;

	/* a dig beside a recent one of the same look just widens that record, while it stays compact */
	for (i = num_digs - 1; i >= 0 && i >= num_digs - 8; i--)
	{
		d = &digs[i];
		if (d->look != look)
			continue;
		for (k = 0; k < 3; k++)
			if (mins[k] > d->maxs[k] + 1 || maxs[k] < d->mins[k] - 1 ||
			    (maxs[k] > d->maxs[k] ? maxs[k] : d->maxs[k]) - (mins[k] < d->mins[k] ? mins[k] : d->mins[k]) > 384)
				break;
		if (k < 3)
			continue;
		for (k = 0; k < 3; k++)
		{
			if (mins[k] < d->mins[k]) d->mins[k] = mins[k];
			if (maxs[k] > d->maxs[k]) d->maxs[k] = maxs[k];
		}
		return;
	}
	if (num_digs == MAX_DIGS)
	{
		/* out of room: let the last record grow to cover the new dig */
		d = &digs[MAX_DIGS - 1];
		for (k = 0; k < 3; k++)
		{
			if (mins[k] < d->mins[k]) d->mins[k] = mins[k];
			if (maxs[k] > d->maxs[k]) d->maxs[k] = maxs[k];
		}
		return;
	}
	d = &digs[num_digs++];
	VectorCopy (mins, d->mins);
	VectorCopy (maxs, d->maxs);
	d->look = look;
}

/* ------------------------------------------------------ hole interior mesh */

#define VERT_FLOATS	5

typedef struct { float *verts; int count, cap; } lookmesh_t;

static lookmesh_t	meshes[MAX_LOOKS + 1];
static int		mesh_revision = -1;

/* qbsp's world-aligned texture axes, as used for sandbox brushes */
static const float baseaxis[6][3][3] =
{
	{{ 0, 0, 1}, {1, 0, 0}, {0, -1, 0}},
	{{ 0, 0,-1}, {1, 0, 0}, {0, -1, 0}},
	{{ 1, 0, 0}, {0, 1, 0}, {0, 0, -1}},
	{{-1, 0, 0}, {0, 1, 0}, {0, 0, -1}},
	{{ 0, 1, 0}, {1, 0, 0}, {0, 0, -1}},
	{{ 0,-1, 0}, {1, 0, 0}, {0, 0, -1}},
};

static void add_face (const aface_t *f, int look)
{
	lookmesh_t	*m = &meshes[look];
	const look_t	*l = &looks[look];
	double		best = 0, dot;
	int		axis = 0, i, v, k;

	for (i = 0; i < 6; i++)
	{
		dot = f->plane.n[0] * baseaxis[i][0][0] + f->plane.n[1] * baseaxis[i][0][1] + f->plane.n[2] * baseaxis[i][0][2];
		if (dot > best)
		{
			best = dot;
			axis = i;
		}
	}
	for (v = 1; v + 1 < f->nverts; v++)
	{
		const double *tri[3];

		tri[0] = f->verts[0]; tri[1] = f->verts[v]; tri[2] = f->verts[v + 1];
		if (m->count + 3 > m->cap)
		{
			m->cap = m->cap ? m->cap * 2 : 384;
			m->verts = (float *) realloc (m->verts, m->cap * VERT_FLOATS * sizeof(float));
		}
		for (i = 0; i < 3; i++)
		{
			float *out = m->verts + m->count++ * VERT_FLOATS;
			const double *p = tri[i];

			for (k = 0; k < 3; k++)
				out[k] = (float) p[k];
			out[3] = (float) ((p[0] * baseaxis[axis][1][0] + p[1] * baseaxis[axis][1][1] + p[2] * baseaxis[axis][1][2]) / l->width);
			out[4] = (float) ((p[0] * baseaxis[axis][2][0] + p[1] * baseaxis[axis][2][1] + p[2] * baseaxis[axis][2][2]) / l->height);
		}
	}
}

/* Every face of every brush that reaches into a dug area. Faces that coincide
   with a surviving map surface are drawn too, pushed back in depth so the map's
   own surface wins; where that surface was cut away, the solid behind it, and
   so the face, are gone as well. */
static void rebuild_meshes (void)
{
	int	id, i, d, k;

	for (i = 0; i <= MAX_LOOKS; i++)
		meshes[i].count = 0;
	for (id = 0; id < ArchWorld_SlotCount (); id++)
	{
		const abrush_t	*b = ArchWorld_Brush (id);
		int		near_brush = 0, v;

		if (!b || b->locked)
			continue;
		for (d = num_digs - 1; d >= 0 && !near_brush; d--)
		{
			for (k = 0; k < 3; k++)
				if (b->mins[k] > digs[d].maxs[k] + 1 || b->maxs[k] < digs[d].mins[k] - 1)
					break;
			near_brush = k == 3;
		}
		if (!near_brush)
			continue;
		/* converted regions are large: of a brush near a dig, only the faces that
		   themselves reach a dug area can have been exposed */
		for (i = 0; i < b->nfaces; i++)
		{
			const aface_t	*f = &b->faces[i];
			double		fmin[3], fmax[3];
			int		look = f->plane.tag, near_look = 0;

			for (k = 0; k < 3; k++)
				fmin[k] = fmax[k] = f->verts[0][k];
			for (v = 1; v < f->nverts; v++)
				for (k = 0; k < 3; k++)
				{
					if (f->verts[v][k] < fmin[k]) fmin[k] = f->verts[v][k];
					if (f->verts[v][k] > fmax[k]) fmax[k] = f->verts[v][k];
				}
			for (d = num_digs - 1; d >= 0 && !near_look; d--)	/* newest first */
			{
				for (k = 0; k < 3; k++)
					if (fmin[k] > digs[d].maxs[k] + 1 || fmax[k] < digs[d].mins[k] - 1)
						break;
				if (k == 3)
					near_look = digs[d].look ? digs[d].look : -1;
			}
			if (!near_look)
				continue;
			if (look < 1 || look > num_looks)
				look = near_look;
			if (look >= 1 && look <= num_looks)
				add_face (f, look);
		}
	}
	mesh_revision = ArchWorld_Revision ();
}

/* called after the map's own surfaces are drawn */
void ArchSurf_DrawHoles (void)
{
	int	i;

	if (!map_dug || !num_looks)
		return;
	if (mesh_revision != ArchWorld_Revision ())
		rebuild_meshes ();

	if (GL_UseProgramFunc)
		GL_UseProgramFunc (0);
	GL_DisableMultitexture ();
	GL_BindBuffer (GL_ARRAY_BUFFER, 0);
	GL_ClientActiveTextureFunc (GL_TEXTURE0_ARB);
	glEnableClientState (GL_VERTEX_ARRAY);
	glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	glEnable (GL_POLYGON_OFFSET_FILL);
	glPolygonOffset (1, 2);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);	/* the engine's default is replace, which ignores the colour */

	for (i = 1; i <= num_looks; i++)
	{
		lookmesh_t *m = &meshes[i];

		if (!m->count)
			continue;
		GL_Bind (looks[i].texture);
		glColor3f (looks[i].light, looks[i].light, looks[i].light);
		glVertexPointer (3, GL_FLOAT, VERT_FLOATS * sizeof(float), m->verts);
		glTexCoordPointer (2, GL_FLOAT, VERT_FLOATS * sizeof(float), m->verts + 3);
		glDrawArrays (GL_TRIANGLES, 0, m->count);
		rs_brushpolys += m->count / 3;
	}

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glPolygonOffset (0, 0);
	glDisable (GL_POLYGON_OFFSET_FILL);
	glDisableClientState (GL_VERTEX_ARRAY);
	glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	glColor3f (1, 1, 1);
}

/* ----------------------------------------------------------------- reset */

/* a map is starting: nothing is dug */
void ArchSurf_NewMap (void)
{
	qmodel_t	*world = cl.worldmodel;
	afrag_t		*f, *next;
	int		i;

	for (i = 0; i < frags_count; i++)
	{
		for (f = frags ? frags[i] : NULL; f; f = next)
		{
			next = f->next;
			free_fragment (f);
		}
	}
	free (frags);
	frags = NULL;
	frags_count = 0;
	frag_verts = 0;
	vbo_stale = false;
	map_dug = false;
	num_looks = 0;
	num_digs = 0;
	mesh_revision = -1;

	if (!world)
		return;
	/* a map restarted without reloading keeps its surfaces, flags and all */
	for (i = 0; i < world->nummodelsurfaces; i++)
		world->surfaces[world->firstmodelsurface + i].flags &= ~SURF_ARCHDUG;
	frags_count = world->nummodelsurfaces;
	frags = (afrag_t **) calloc (frags_count ? frags_count : 1, sizeof(afrag_t *));
}
