/*
QuakeArch - build cursor geometry.
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
		if (fabs (normal[k]) > 0.999)
			axis = k;
	if (axis < 0)
		return 0;

	out->naxis = axis;
	out->nsign = normal[axis] > 0 ? 1 : -1;
	out->plane = round_to (hit[axis], 1);

	if (axis == 2)
	{
		/* floor or ceiling: height along the world axis the player faces */
		double yaw = yaw_degrees * PI / 180.0;
		out->vaxis = fabs (cos (yaw)) >= fabs (sin (yaw)) ? 0 : 1;
		out->uaxis = 1 - out->vaxis;
	}
	else
	{
		/* wall: width horizontal, height vertical */
		out->uaxis = 1 - axis;
		out->vaxis = 2;
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
