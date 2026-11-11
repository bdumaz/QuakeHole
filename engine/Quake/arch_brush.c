/*
QuakeArch - convex brush maths.
No engine dependencies: C standard library only.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "arch_brush.h"

#define BIG_EXTENT	65536.0
#define MAX_FACE_VERTS	128

#define DOT(a, b)	((a)[0] * (b)[0] + (a)[1] * (b)[1] + (a)[2] * (b)[2])

static void cross (const avec3 a, const avec3 b, avec3 out)
{
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

/* ---------------------------------------------------------------- planes */

int ArchPlane_FromPoints (const int a[3], const int b[3], const int c[3], aplane_t *out)
{
	avec3	t1, t2, n;
	double	len;
	int	i;

	for (i = 0; i < 3; i++)
	{
		t1[i] = (double)a[i] - b[i];
		t2[i] = (double)c[i] - b[i];
		out->p[0][i] = a[i];
		out->p[1][i] = b[i];
		out->p[2][i] = c[i];
	}
	cross (t1, t2, n);
	len = sqrt (DOT (n, n));
	if (len < 1e-9)
		return 0;
	for (i = 0; i < 3; i++)
	{
		out->n[i] = n[i] / len;
		if (fabs (out->n[i]) < 1e-12)
			out->n[i] = 0;	/* avoid -0 and noise on axial planes */
	}
	out->d = out->n[0] * b[0] + out->n[1] * b[1] + out->n[2] * b[2];
	return 1;
}

void ArchPlane_Flip (const aplane_t *in, aplane_t *out)
{
	aplane_t	t = *in;
	int		i;

	for (i = 0; i < 3; i++)
	{
		out->p[0][i] = t.p[2][i];
		out->p[1][i] = t.p[1][i];
		out->p[2][i] = t.p[0][i];
		out->n[i] = t.n[i] == 0 ? 0 : -t.n[i];
	}
	out->d = t.d == 0 ? 0 : -t.d;
}

/* plane through a, b, c whose normal points away from the point "inside" */
static int plane_facing_away (const int a[3], const int b[3], const int c[3], const avec3 inside, aplane_t *out)
{
	if (!ArchPlane_FromPoints (a, b, c, out))
		return 0;
	if (DOT (inside, out->n) - out->d > 0)
		ArchPlane_Flip (out, out);
	return 1;
}

static int slot_for_normal (const avec3 n)
{
	if (n[2] > 0.7)
		return ARCH_SLOT_GROUND;
	if (n[2] < -0.7)
		return ARCH_SLOT_CEILING;
	return ARCH_SLOT_WALL;
}

/* -------------------------------------------------------------- windings */

/* a large quad on the plane */
static int base_winding (const aplane_t *pl, avec3 *w)
{
	avec3	up = {0, 0, 1}, right, org;
	double	len;
	int	i;

	if (fabs (pl->n[2]) > 0.9)
	{
		up[0] = 1;
		up[2] = 0;
	}
	len = DOT (up, pl->n);
	for (i = 0; i < 3; i++)
		up[i] -= len * pl->n[i];
	len = sqrt (DOT (up, up));
	for (i = 0; i < 3; i++)
		up[i] /= len;
	cross (up, pl->n, right);
	for (i = 0; i < 3; i++)
	{
		org[i] = pl->n[i] * pl->d;
		w[0][i] = org[i] - right[i] * BIG_EXTENT + up[i] * BIG_EXTENT;
		w[1][i] = org[i] + right[i] * BIG_EXTENT + up[i] * BIG_EXTENT;
		w[2][i] = org[i] + right[i] * BIG_EXTENT - up[i] * BIG_EXTENT;
		w[3][i] = org[i] - right[i] * BIG_EXTENT - up[i] * BIG_EXTENT;
	}
	return 4;
}

/* keeps the part of the winding behind the plane; returns the new count */
static int clip_winding (avec3 *in, int n, const aplane_t *pl, avec3 *out)
{
	double	dists[MAX_FACE_VERTS + 1];
	int	i, k, count = 0;

	for (i = 0; i < n; i++)
		dists[i] = DOT (in[i], pl->n) - pl->d;
	dists[n] = dists[0];

	for (i = 0; i < n; i++)
	{
		const double	*p1 = in[i], *p2 = in[(i + 1) % n];
		double		d1 = dists[i], d2 = dists[i + 1];

		if (d1 <= ARCH_ON_EPS)
		{
			if (count >= MAX_FACE_VERTS) return 0;
			memcpy (out[count++], p1, sizeof(avec3));
		}
		if ((d1 > ARCH_ON_EPS && d2 < -ARCH_ON_EPS) || (d1 < -ARCH_ON_EPS && d2 > ARCH_ON_EPS))
		{
			double f = d1 / (d1 - d2);
			if (count >= MAX_FACE_VERTS) return 0;
			for (k = 0; k < 3; k++)
				out[count][k] = p1[k] + f * (p2[k] - p1[k]);
			count++;
		}
	}
	return count;
}

static double dist2 (const avec3 a, const avec3 b)
{
	double dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
	return dx * dx + dy * dy + dz * dz;
}

/* snaps near-integer coordinates and removes repeated points */
static int tidy_winding (avec3 *w, int n)
{
	int	i, k, count = 0;

	for (i = 0; i < n; i++)
	{
		for (k = 0; k < 3; k++)
		{
			double r = floor (w[i][k] + 0.5);
			if (fabs (w[i][k] - r) < 1e-6)
				w[i][k] = r;
		}
	}
	for (i = 0; i < n; i++)
	{
		if (count && dist2 (w[i], w[count - 1]) < ARCH_ON_EPS * ARCH_ON_EPS)
			continue;
		if (count != i)
			memcpy (w[count], w[i], sizeof(avec3));
		count++;
	}
	if (count > 1 && dist2 (w[0], w[count - 1]) < ARCH_ON_EPS * ARCH_ON_EPS)
		count--;
	return count;
}

/* Newell normal: points toward a viewer who sees the winding counter-clockwise */
static void winding_normal (avec3 *w, int n, avec3 out)
{
	int	i;

	out[0] = out[1] = out[2] = 0;
	for (i = 0; i < n; i++)
	{
		const double *a = w[i], *b = w[(i + 1) % n];
		out[0] += (a[1] - b[1]) * (a[2] + b[2]);
		out[1] += (a[2] - b[2]) * (a[0] + b[0]);
		out[2] += (a[0] - b[0]) * (a[1] + b[1]);
	}
}

/* --------------------------------------------------------------- brushes */

static int same_plane (const aplane_t *a, const aplane_t *b)
{
	return fabs (a->n[0] - b->n[0]) < 1e-9 && fabs (a->n[1] - b->n[1]) < 1e-9 &&
		fabs (a->n[2] - b->n[2]) < 1e-9 && fabs (a->d - b->d) < 1e-6;
}

void ArchBrush_Free (abrush_t *b)
{
	int	i;

	if (!b)
		return;
	for (i = 0; i < b->nfaces; i++)
		free (b->faces[i].verts);
	free (b->faces);
	free (b);
}

abrush_t *ArchBrush_FromPlanes (const aplane_t *planes, int nplanes)
{
	avec3		bufa[MAX_FACE_VERTS], bufb[MAX_FACE_VERTS];
	abrush_t	*b;
	int		i, j, k, v;

	b = (abrush_t *) calloc (1, sizeof(*b));
	b->faces = (aface_t *) calloc (nplanes > 0 ? nplanes : 1, sizeof(aface_t));

	for (i = 0; i < nplanes; i++)
	{
		avec3	*w = bufa, *other = bufb, *tmp, nrm;
		int	n, dup = 0;
		aface_t	*f;

		for (j = 0; j < i && !dup; j++)
			dup = same_plane (&planes[i], &planes[j]);
		if (dup)
			continue;

		n = base_winding (&planes[i], w);
		for (j = 0; j < nplanes && n >= 3; j++)
		{
			if (j == i || same_plane (&planes[i], &planes[j]))
				continue;
			n = clip_winding (w, n, &planes[j], other);
			tmp = w; w = other; other = tmp;
		}
		if (n >= 3)
			n = tidy_winding (w, n);
		if (n < 3)
			continue;
		winding_normal (w, n, nrm);
		if (DOT (nrm, nrm) < 1e-12)
			continue;	/* no area */

		f = &b->faces[b->nfaces++];
		f->plane = planes[i];
		f->slot = slot_for_normal (planes[i].n);
		f->nverts = n;
		f->verts = (avec3 *) malloc (n * sizeof(avec3));
		/* store clockwise as seen from outside: the Newell normal must oppose the plane normal */
		if (DOT (nrm, planes[i].n) > 0)
		{
			for (v = 0; v < n; v++)
				memcpy (f->verts[v], w[n - 1 - v], sizeof(avec3));
		}
		else
			memcpy (f->verts, w, n * sizeof(avec3));
	}

	if (b->nfaces < 4)
	{
		ArchBrush_Free (b);
		return NULL;
	}

	for (k = 0; k < 3; k++)
	{
		b->mins[k] = 1e30;
		b->maxs[k] = -1e30;
	}
	for (i = 0; i < b->nfaces; i++)
	{
		for (v = 0; v < b->faces[i].nverts; v++)
		{
			for (k = 0; k < 3; k++)
			{
				double c = b->faces[i].verts[v][k];
				if (c < b->mins[k]) b->mins[k] = c;
				if (c > b->maxs[k]) b->maxs[k] = c;
			}
		}
	}

	/* thin rule: the brush must extend at least ARCH_THIN_EPS behind every face */
	for (i = 0; i < b->nfaces; i++)
	{
		const aplane_t	*pl = &b->faces[i].plane;
		double		depth = 0;

		for (j = 0; j < b->nfaces; j++)
		{
			for (v = 0; v < b->faces[j].nverts; v++)
			{
				double d = pl->d - DOT (b->faces[j].verts[v], pl->n);
				if (d > depth) depth = d;
			}
		}
		if (depth < ARCH_THIN_EPS)
		{
			ArchBrush_Free (b);
			return NULL;
		}
	}
	/* an unbounded brush would still reach the base windings */
	for (k = 0; k < 3; k++)
	{
		if (b->mins[k] < -BIG_EXTENT / 2 || b->maxs[k] > BIG_EXTENT / 2)
		{
			ArchBrush_Free (b);
			return NULL;
		}
	}
	return b;
}

abrush_t *ArchBrush_Box (const int mins[3], const int maxs[3])
{
	aplane_t	planes[6];
	avec3		centre;
	int		axis, side, k, n = 0;

	for (k = 0; k < 3; k++)
	{
		if (maxs[k] <= mins[k])
			return NULL;
		centre[k] = ((double)mins[k] + maxs[k]) * 0.5;
	}
	for (axis = 0; axis < 3; axis++)
	{
		int a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;
		for (side = 0; side < 2; side++)
		{
			int p0[3], p1[3], p2[3];
			p0[axis] = p1[axis] = p2[axis] = side ? maxs[axis] : mins[axis];
			p0[a1] = mins[a1]; p0[a2] = mins[a2];
			p1[a1] = maxs[a1]; p1[a2] = mins[a2];
			p2[a1] = mins[a1]; p2[a2] = maxs[a2];
			plane_facing_away (p0, p1, p2, centre, &planes[n++]);
		}
	}
	return ArchBrush_FromPlanes (planes, 6);
}

abrush_t *ArchBrush_Prism (const int (*poly)[3], int npoly, const int extrude[3])
{
	aplane_t	*planes, cap;
	abrush_t	*b;
	avec3		centre = {0, 0, 0}, ex;
	int		i, k, third = -1, n = 0;
	int		far0[3], far1[3], far2[3];

	if (npoly < 3)
		return NULL;
	for (k = 0; k < 3; k++)
		ex[k] = extrude[k];
	/* three polygon points that are not collinear */
	for (i = 2; i < npoly && third < 0; i++)
	{
		if (ArchPlane_FromPoints (poly[0], poly[1], poly[i], &cap))
			third = i;
	}
	if (third < 0 || fabs (DOT (ex, cap.n)) < 1e-9)
		return NULL;

	for (i = 0; i < npoly; i++)
		for (k = 0; k < 3; k++)
			centre[k] += poly[i][k];
	for (k = 0; k < 3; k++)
		centre[k] = centre[k] / npoly + ex[k] * 0.5;

	planes = (aplane_t *) malloc ((npoly + 2) * sizeof(aplane_t));
	plane_facing_away (poly[0], poly[1], poly[third], centre, &planes[n++]);
	for (k = 0; k < 3; k++)
	{
		far0[k] = poly[0][k] + extrude[k];
		far1[k] = poly[1][k] + extrude[k];
		far2[k] = poly[third][k] + extrude[k];
	}
	plane_facing_away (far0, far1, far2, centre, &planes[n++]);
	for (i = 0; i < npoly; i++)
	{
		const int	*a = poly[i], *c = poly[(i + 1) % npoly];
		int		top[3];

		for (k = 0; k < 3; k++)
			top[k] = a[k] + extrude[k];
		if (plane_facing_away (a, c, top, centre, &planes[n]))
			n++;
	}
	b = ArchBrush_FromPlanes (planes, n);
	free (planes);
	return b;
}

abrush_t *ArchBrush_Clone (const abrush_t *b)
{
	abrush_t	*c;
	int		i;

	if (!b)
		return NULL;
	c = (abrush_t *) malloc (sizeof(*c));
	*c = *b;
	c->faces = (aface_t *) malloc (b->nfaces * sizeof(aface_t));
	for (i = 0; i < b->nfaces; i++)
	{
		c->faces[i] = b->faces[i];
		c->faces[i].verts = (avec3 *) malloc (b->faces[i].nverts * sizeof(avec3));
		memcpy (c->faces[i].verts, b->faces[i].verts, b->faces[i].nverts * sizeof(avec3));
	}
	return c;
}

double ArchBrush_Volume (const abrush_t *b)
{
	const double	*o;
	double		total = 0;
	int		i, v, k;

	if (!b || !b->nfaces)
		return 0;
	o = b->faces[0].verts[0];
	for (i = 0; i < b->nfaces; i++)
	{
		const aface_t *f = &b->faces[i];
		for (v = 1; v + 1 < f->nverts; v++)
		{
			avec3 a, c, e, x;
			for (k = 0; k < 3; k++)
			{
				a[k] = f->verts[0][k] - o[k];
				c[k] = f->verts[v][k] - o[k];
				e[k] = f->verts[v + 1][k] - o[k];
			}
			cross (c, e, x);
			total += DOT (a, x);
		}
	}
	return fabs (total) / 6.0;
}

int ArchBrush_PointInside (const abrush_t *b, const avec3 p)
{
	int	i;

	for (i = 0; i < b->nfaces; i++)
	{
		if (DOT (p, b->faces[i].plane.n) - b->faces[i].plane.d >= -ARCH_ON_EPS)
			return 0;
	}
	return 1;
}

/* ------------------------------------------------------------------- CSG */

static abrush_t *brush_with_plane (const abrush_t *b, const aplane_t *extra)
{
	aplane_t	*planes = (aplane_t *) malloc ((b->nfaces + 1) * sizeof(aplane_t));
	abrush_t	*r;
	int		i;

	for (i = 0; i < b->nfaces; i++)
		planes[i] = b->faces[i].plane;
	planes[b->nfaces] = *extra;
	r = ArchBrush_FromPlanes (planes, b->nfaces + 1);
	free (planes);
	return r;
}

void ArchBrush_Split (const abrush_t *b, const aplane_t *pl, abrush_t **front, abrush_t **back)
{
	aplane_t	flipped;

	ArchPlane_Flip (pl, &flipped);
	*front = brush_with_plane (b, &flipped);
	*back = brush_with_plane (b, pl);
}

static int boxes_overlap (const abrush_t *a, const abrush_t *b)
{
	int	k;

	for (k = 0; k < 3; k++)
	{
		if (a->mins[k] >= b->maxs[k] - ARCH_THIN_EPS || b->mins[k] >= a->maxs[k] - ARCH_THIN_EPS)
			return 0;
	}
	return 1;
}

abrush_t *ArchBrush_Intersection (const abrush_t *a, const abrush_t *b)
{
	aplane_t	*planes;
	abrush_t	*r;
	int		i;

	if (!boxes_overlap (a, b))
		return NULL;
	planes = (aplane_t *) malloc ((a->nfaces + b->nfaces) * sizeof(aplane_t));
	for (i = 0; i < a->nfaces; i++)
		planes[i] = a->faces[i].plane;
	for (i = 0; i < b->nfaces; i++)
		planes[a->nfaces + i] = b->faces[i].plane;
	r = ArchBrush_FromPlanes (planes, a->nfaces + b->nfaces);
	free (planes);
	return r;
}

int ArchBrush_Intersects (const abrush_t *a, const abrush_t *b)
{
	abrush_t	*r = ArchBrush_Intersection (a, b);

	if (!r)
		return 0;
	ArchBrush_Free (r);
	return 1;
}

int ArchBrush_Subtract (const abrush_t *a, const abrush_t *b, abrush_t ***out)
{
	abrush_t	**pieces, *rest, *front, *back;
	int		i, n = 0;

	pieces = (abrush_t **) malloc ((b->nfaces + 1) * sizeof(abrush_t *));
	if (!ArchBrush_Intersects (a, b))
	{
		pieces[0] = ArchBrush_Clone (a);
		*out = pieces;
		return 1;
	}

	rest = ArchBrush_Clone (a);
	for (i = 0; i < b->nfaces && rest; i++)
	{
		ArchBrush_Split (rest, &b->faces[i].plane, &front, &back);
		if (front)
			pieces[n++] = front;
		ArchBrush_Free (rest);
		rest = back;
	}
	ArchBrush_Free (rest);	/* the part inside b */

	if (!n)
	{
		free (pieces);
		pieces = NULL;
	}
	*out = pieces;
	return n;
}
