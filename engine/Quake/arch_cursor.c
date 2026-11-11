/*
QuakeHole - build cursor geometry.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "arch_cursor.h"

#define PI	3.14159265358979323846

static int round_to (double v, int step)
{
	return step * (int) floor (v / step + 0.5);
}

int ArchCursor_Place (const avec3 hit, const avec3 normal, double yaw_degrees, int width, int height, acursor_t *out)
{
	int	axis = -1, k, cu, cv;

	for (k = 0; k < 3; k++)
		if (fabs (normal[k]) > 0.999999)	/* exactly axial: a nearly flat arch segment does not count */
			axis = k;
	if (axis < 0)
		return 0;

	out->naxis = axis;
	out->nsign = normal[axis] > 0 ? 1 : -1;
	out->plane = round_to (hit[axis], 1);
	/* The hit stops 1/32 short of the surface. Further than that from a whole
	   unit means the surface itself is not on one (some campaign walls are not),
	   and a box built from a rounded plane would not line up with it. */
	if (fabs (hit[axis] - out->plane) > 0.1)
		return 0;

	if (axis == 2)
	{
		/* floor or ceiling: height along the world axis the player faces */
		double yaw = yaw_degrees * PI / 180.0;
		out->vaxis = fabs (cos (yaw)) >= fabs (sin (yaw)) ? 0 : 1;
		out->uaxis = 1 - out->vaxis;
		out->vdir = (out->vaxis == 0 ? cos (yaw) : sin (yaw)) >= 0 ? 1 : -1;
	}
	else
	{
		/* wall: width horizontal, height vertical */
		out->uaxis = 1 - axis;
		out->vaxis = 2;
		out->vdir = 1;
	}

	cu = round_to (hit[out->uaxis], ARCH_GRID_SNAP);
	cv = round_to (hit[out->vaxis], ARCH_GRID_SNAP);
	out->umin = cu - width / 2;
	out->umax = out->umin + width;
	out->vmin = cv - height / 2;
	out->vmax = out->vmin + height;
	return 1;
}

/* is the strip just inside one edge of the cursor buried in neighbouring solid?
   Sampled on the cursor's centre line, one unit off the surface on the air side. */
static int edge_buried (const acursor_t *c, int (*solid) (const avec3 p), int along_v, int high)
{
	avec3	p;

	p[c->naxis] = c->plane + c->nsign;
	if (along_v)
	{
		p[c->uaxis] = (c->umin + c->umax) * 0.5;
		p[c->vaxis] = high ? c->vmax - ARCH_GRID_SNAP * 0.5 : c->vmin + ARCH_GRID_SNAP * 0.5;
	}
	else
	{
		p[c->vaxis] = (c->vmin + c->vmax) * 0.5;
		p[c->uaxis] = high ? c->umax - ARCH_GRID_SNAP * 0.5 : c->umin + ARCH_GRID_SNAP * 0.5;
	}
	return solid (p);
}

void ArchCursor_Fit (acursor_t *c, int (*solid) (const avec3 p))
{
	int	along_v, dir, steps, i;

	for (along_v = 1; along_v >= 0; along_v--)
	{
		int		*lo = along_v ? &c->vmin : &c->umin, *hi = along_v ? &c->vmax : &c->umax;
		const int	lo0 = *lo, hi0 = *hi;
		int		low_buried = edge_buried (c, solid, along_v, 0);
		int		high_buried = edge_buried (c, solid, along_v, 1);

		if (low_buried == high_buried)
			continue;	/* clear on both sides, or too big to fit */
		dir = low_buried ? ARCH_GRID_SNAP : -ARCH_GRID_SNAP;
		steps = (hi0 - lo0) / ARCH_GRID_SNAP;
		for (i = 0; i < steps; i++)
		{
			*lo += dir;
			*hi += dir;
			if (edge_buried (c, solid, along_v, low_buried ? 1 : 0))
			{
				/* ran into the opposite side: it does not fit, so leave it as aimed */
				*lo = lo0;
				*hi = hi0;
				break;
			}
			if (!edge_buried (c, solid, along_v, low_buried ? 0 : 1))
				break;
		}
	}
}

/* ----------------------------------------------------------------- arch */

int ArchCursor_ArchProfile (const acursor_t *c, int pts[ARCH_PROFILE_MAX][3])
{
	double	half = (c->umax - c->umin) * 0.5, centre = (c->umin + c->umax) * 0.5;
	double	height = c->vmax - c->vmin;
	double	rise = height < half ? height : half;	/* height of the curved part */
	double	straight = height - rise;		/* height of the straight sides */
	int	uv[ARCH_PROFILE_MAX][2], n = 0, i, k;

	/* local coordinates: u across, t up from the base */
	uv[n][0] = c->umin; uv[n][1] = 0; n++;
	uv[n][0] = c->umax; uv[n][1] = 0; n++;
	for (i = 0; i <= ARCH_ARCH_SEGMENTS; i++)
	{
		double	a = PI * i / ARCH_ARCH_SEGMENTS;
		int	u = (int) floor (centre + half * cos (a) + 0.5);
		int	t = (int) floor (straight + rise * sin (a) + 0.5);

		/* rounding and zero-height sides can repeat a point */
		for (k = 0; k < n; k++)
			if (uv[k][0] == u && uv[k][1] == t)
				break;
		if (k < n)
			continue;
		uv[n][0] = u; uv[n][1] = t; n++;
	}
	for (i = 0; i < n; i++)
	{
		pts[i][c->naxis] = c->plane;
		pts[i][c->uaxis] = uv[i][0];
		pts[i][c->vaxis] = c->vdir > 0 ? c->vmin + uv[i][1] : c->vmax - uv[i][1];
	}
	return n;
}

abrush_t *ArchCursor_ArchBrush (const acursor_t *c, int depth)
{
	int	pts[ARCH_PROFILE_MAX][3], extrude[3] = {0, 0, 0}, n;

	if (!depth)
		return NULL;
	n = ArchCursor_ArchProfile (c, pts);
	extrude[c->naxis] = -c->nsign * depth;	/* into the surface is against the normal */
	return ArchBrush_Prism (pts, n, extrude);
}

/* ----------------------------------------------------------------- snap */

/* is the grid square whose low corner is (u, v) flat surface: solid behind, empty in front? */
static int square_is_surface (const acursor_t *c, int (*solid) (const avec3 p), int u, int v)
{
	avec3	p;

	p[c->uaxis] = u + ARCH_GRID_SNAP * 0.5;
	p[c->vaxis] = v + ARCH_GRID_SNAP * 0.5;
	p[c->naxis] = c->plane - c->nsign;
	if (!solid (p))
		return 0;
	p[c->naxis] = c->plane + c->nsign;
	return !solid (p);
}

/* a strip of squares: along u from a to b at fixed v, or along v at fixed u */
static int strip_is_surface (const acursor_t *c, int (*solid) (const avec3 p), int along_u, int fixed, int a, int b)
{
	int	i;

	for (i = a; i < b; i += ARCH_GRID_SNAP)
		if (!square_is_surface (c, solid, along_u ? i : fixed, along_u ? fixed : i))
			return 0;
	return 1;
}

int ArchCursor_Snap (acursor_t *c, const avec3 hit, int (*solid) (const avec3 p))
{
	int	umin = ARCH_GRID_SNAP * (int) floor (hit[c->uaxis] / ARCH_GRID_SNAP);
	int	vmin = ARCH_GRID_SNAP * (int) floor (hit[c->vaxis] / ARCH_GRID_SNAP);
	int	umax = umin + ARCH_GRID_SNAP, vmax = vmin + ARCH_GRID_SNAP;
	int	grew = 1;

	if (!square_is_surface (c, solid, umin, vmin))
		return 0;
	/* grow each side in turn; the world is bounded, so this ends */
	while (grew)
	{
		grew = 0;
		if (strip_is_surface (c, solid, 0, umin - ARCH_GRID_SNAP, vmin, vmax)) { umin -= ARCH_GRID_SNAP; grew = 1; }
		if (strip_is_surface (c, solid, 0, umax, vmin, vmax))			 { umax += ARCH_GRID_SNAP; grew = 1; }
		if (strip_is_surface (c, solid, 1, vmin - ARCH_GRID_SNAP, umin, umax)) { vmin -= ARCH_GRID_SNAP; grew = 1; }
		if (strip_is_surface (c, solid, 1, vmax, umin, umax))			 { vmax += ARCH_GRID_SNAP; grew = 1; }
	}
	c->umin = umin; c->umax = umax;
	c->vmin = vmin; c->vmax = vmax;
	return 1;
}

void ArchCursor_Box (const acursor_t *c, int depth, int mins[3], int maxs[3])
{
	/* into the surface is against the normal */
	int far = c->plane - c->nsign * depth;

	mins[c->naxis] = far < c->plane ? far : c->plane;
	maxs[c->naxis] = far < c->plane ? c->plane : far;
	mins[c->uaxis] = c->umin;
	maxs[c->uaxis] = c->umax;
	mins[c->vaxis] = c->vmin;
	maxs[c->vaxis] = c->vmax;
}

void ArchCursor_Corners (const acursor_t *c, double lift, avec3 corners[4])
{
	int	i;

	for (i = 0; i < 4; i++)
	{
		corners[i][c->naxis] = c->plane + c->nsign * lift;
		corners[i][c->uaxis] = (i == 0 || i == 3) ? c->umin : c->umax;
		corners[i][c->vaxis] = (i < 2) ? c->vmin : c->vmax;
	}
}

static int in_range (int v)
{
	return v >= ARCH_CURSOR_MIN && v <= ARCH_CURSOR_MAX;
}

void ArchCursor_StepSize (int mode, int dir, int *width, int *height)
{
	int	step = dir > 0 ? ARCH_CURSOR_STEP : -ARCH_CURSOR_STEP;
	int	*big, *small, newbig, newsmall;

	if (mode == ARCH_SCALE_HEIGHT)
	{
		if (in_range (*height + step))
			*height += step;
		return;
	}
	if (mode == ARCH_SCALE_WIDTH)
	{
		if (in_range (*width + step))
			*width += step;
		return;
	}

	/* uniform: the larger side steps, the other keeps the proportion */
	big = *width >= *height ? width : height;
	small = *width >= *height ? height : width;
	newbig = *big + step;
	if (!in_range (newbig))
		return;
	newsmall = round_to ((double) *small * newbig / *big, ARCH_CURSOR_STEP);
	if (newsmall < ARCH_CURSOR_MIN)
		newsmall = ARCH_CURSOR_MIN;
	*big = newbig;
	*small = newsmall;
}

void ArchCursor_FormatMetres (int units, char *buf, int size)
{
	size_t	len;

	snprintf (buf, size, "%.2f", units / 32.0);
	len = strlen (buf);
	if (len > 3 && buf[len - 1] == '0')
		buf[len - 1] = 0;	/* 5.00 -> 5.0, 2.50 -> 2.5; 1.25 stays */
}
