/*
QuakeHole - cutting a dig box out of a flat surface polygon.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include <string.h>
#include <math.h>
#include "arch_poly.h"

#define BEHIND		0.05	/* how far under the surface "directly behind" is judged */
#define SPLIT_EPS	0.001
#define MIN_AREA	0.01	/* pieces smaller than this are slivers from rounding and are dropped */

/* Splits a convex polygon by the plane coord[axis] = value, judged at points
   moved by shift (the surface pushed slightly into its solid). inside gets the
   part on the given side (sign -1: below value, +1: above), outside the rest. */
static void split (const apoly_t *in, int axis, double value, int sign, double shift,
		   apoly_t *inside, apoly_t *outside)
{
	double	d[ARCH_POLY_MAX_VERTS];
	int	i, k;

	inside->n = outside->n = 0;
	for (i = 0; i < in->n; i++)
		d[i] = sign * (in->v[i][axis] + shift - value);	/* > 0 means on the wanted side */

	for (i = 0; i < in->n; i++)
	{
		int		j = (i + 1) % in->n;
		const double	*p1 = in->v[i], *p2 = in->v[j];

		if (d[i] >= -SPLIT_EPS && inside->n < ARCH_POLY_MAX_VERTS)
			memcpy (inside->v[inside->n++], p1, sizeof(avec3));
		if (d[i] <= SPLIT_EPS && outside->n < ARCH_POLY_MAX_VERTS)
			memcpy (outside->v[outside->n++], p1, sizeof(avec3));
		if ((d[i] > SPLIT_EPS && d[j] < -SPLIT_EPS) || (d[i] < -SPLIT_EPS && d[j] > SPLIT_EPS))
		{
			double	f = d[i] / (d[i] - d[j]);
			avec3	mid;

			for (k = 0; k < 3; k++)
				mid[k] = p1[k] + f * (p2[k] - p1[k]);
			if (inside->n < ARCH_POLY_MAX_VERTS)
				memcpy (inside->v[inside->n++], mid, sizeof(avec3));
			if (outside->n < ARCH_POLY_MAX_VERTS)
				memcpy (outside->v[outside->n++], mid, sizeof(avec3));
		}
	}
}

static double area (const apoly_t *p)
{
	double	n[3] = {0, 0, 0};
	int	i;

	if (p->n < 3)
		return 0;
	for (i = 0; i < p->n; i++)
	{
		const double *a = p->v[i], *b = p->v[(i + 1) % p->n];
		n[0] += (a[1] - b[1]) * (a[2] + b[2]);
		n[1] += (a[2] - b[2]) * (a[0] + b[0]);
		n[2] += (a[0] - b[0]) * (a[1] + b[1]);
	}
	return 0.5 * sqrt (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
}

/* Splits a convex polygon by the plane n.x = d, judged at points moved by shift
   along the surface normal. inside gets the part behind the plane. */
static void split_plane (const apoly_t *in, const double *n, double d, const avec3 normal, double behind,
			 apoly_t *inside, apoly_t *outside)
{
	double	dist[ARCH_POLY_MAX_VERTS];
	double	offset = -behind * (n[0] * normal[0] + n[1] * normal[1] + n[2] * normal[2]);	/* the point moved against the surface normal */
	int	i, k;

	inside->n = outside->n = 0;
	for (i = 0; i < in->n; i++)
		dist[i] = d - (in->v[i][0] * n[0] + in->v[i][1] * n[1] + in->v[i][2] * n[2] + offset);	/* > 0 is behind the plane */

	for (i = 0; i < in->n; i++)
	{
		int		j = (i + 1) % in->n;
		const double	*p1 = in->v[i], *p2 = in->v[j];

		if (dist[i] >= -SPLIT_EPS && inside->n < ARCH_POLY_MAX_VERTS)
			memcpy (inside->v[inside->n++], p1, sizeof(avec3));
		if (dist[i] <= SPLIT_EPS && outside->n < ARCH_POLY_MAX_VERTS)
			memcpy (outside->v[outside->n++], p1, sizeof(avec3));
		if ((dist[i] > SPLIT_EPS && dist[j] < -SPLIT_EPS) || (dist[i] < -SPLIT_EPS && dist[j] > SPLIT_EPS))
		{
			double	f = dist[i] / (dist[i] - dist[j]);
			avec3	mid;

			for (k = 0; k < 3; k++)
				mid[k] = p1[k] + f * (p2[k] - p1[k]);
			if (inside->n < ARCH_POLY_MAX_VERTS)
				memcpy (inside->v[inside->n++], mid, sizeof(avec3));
			if (outside->n < ARCH_POLY_MAX_VERTS)
				memcpy (outside->v[outside->n++], mid, sizeof(avec3));
		}
	}
}

int ArchPoly_SubtractBrush (const apoly_t *poly, const avec3 normal, const abrush_t *brush,
			    apoly_t pieces[ARCH_POLY_MAX_CUT_PIECES])
{
	static apoly_t	kept[ARCH_POLY_MAX_CUT_PIECES];
	apoly_t		rest = *poly, in, out;
	int		i, n = 0;

	if (brush->nfaces > ARCH_POLY_MAX_CUT_PIECES)
		return ARCH_POLY_UNTOUCHED;
	for (i = 0; i < brush->nfaces; i++)
	{
		split_plane (&rest, brush->faces[i].plane.n, brush->faces[i].plane.d, normal, BEHIND, &in, &out);
		if (area (&in) < MIN_AREA)
			return ARCH_POLY_UNTOUCHED;
		if (area (&out) >= MIN_AREA)
			kept[n++] = out;
		rest = in;
	}
	for (i = 0; i < n; i++)
		pieces[i] = kept[i];
	return n;
}

int ArchPoly_SubtractBox (const apoly_t *poly, const avec3 normal, const int mins[3], const int maxs[3],
			  apoly_t pieces[ARCH_POLY_MAX_PIECES])
{
	apoly_t	rest = *poly, in, out, kept[ARCH_POLY_MAX_PIECES];
	int	axis, side, n = 0, i;

	/* Peel off what lies outside each side of the box in turn; what is left at
	   the end is inside all six and is the part that goes. */
	for (axis = 0; axis < 3; axis++)
	{
		double shift = -normal[axis] * BEHIND;	/* behind the surface is against its normal */

		for (side = 0; side < 2; side++)
		{
			/* side 0: keep what is above mins; side 1: keep what is below maxs */
			split (&rest, axis, side ? maxs[axis] : mins[axis], side ? -1 : 1, shift, &in, &out);
			if (area (&in) < MIN_AREA)
				return ARCH_POLY_UNTOUCHED;	/* nothing of the surface is inside the box */
			if (area (&out) >= MIN_AREA)
				kept[n++] = out;
			rest = in;
		}
	}
	for (i = 0; i < n; i++)
		pieces[i] = kept[i];
	return n;
}
