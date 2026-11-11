/*
QuakeArch - weapon digging.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include <string.h>
#include <math.h>
#include "arch_dig.h"

/* ---------------------------------------------------------------- crater */

#define GRID_MAX	(2 * (ARCH_DIG_MAX_RADIUS / ARCH_DIG_CELL) + 2)	/* cubes per axis around a blast */

static unsigned char	filled[GRID_MAX][GRID_MAX][GRID_MAX];	/* [z][y][x]: 1 = to remove, 2 = already in a box */

int ArchDig_Crater (const avec3 centre, double radius, int boxes[][6], int max)
{
	int	lo[3], size[3], x, y, z, k, n = 0;

	if (radius > ARCH_DIG_MAX_RADIUS)
		radius = ARCH_DIG_MAX_RADIUS;
	if (radius <= 0)
		return 0;
	for (k = 0; k < 3; k++)
	{
		int hi = (int) floor ((centre[k] + radius) / ARCH_DIG_CELL);
		lo[k] = (int) floor ((centre[k] - radius) / ARCH_DIG_CELL);
		size[k] = hi - lo[k] + 1;
		if (size[k] > GRID_MAX)
			size[k] = GRID_MAX;
	}

	/* mark the cubes whose centres are inside the sphere */
	for (z = 0; z < size[2]; z++)
		for (y = 0; y < size[1]; y++)
			for (x = 0; x < size[0]; x++)
			{
				double dx = (lo[0] + x + 0.5) * ARCH_DIG_CELL - centre[0];
				double dy = (lo[1] + y + 0.5) * ARCH_DIG_CELL - centre[1];
				double dz = (lo[2] + z + 0.5) * ARCH_DIG_CELL - centre[2];
				filled[z][y][x] = dx * dx + dy * dy + dz * dz <= radius * radius;
			}

	/* Cover them with boxes. The middle slab of the sphere goes first as tall
	   boxes; taking the biggest pieces early keeps the count, and so the
	   fragmentation of the world, down. Each box grows along x, then y, then z. */
	for (z = 0; z < size[2]; z++)
		for (y = 0; y < size[1]; y++)
			for (x = 0; x < size[0]; x++)
			{
				int x2, y2, z2, xx, yy, ok;

				if (filled[z][y][x] != 1)
					continue;
				for (x2 = x; x2 + 1 < size[0] && filled[z][y][x2 + 1] == 1; x2++)
					;
				for (y2 = y; y2 + 1 < size[1]; y2++)
				{
					for (ok = 1, xx = x; xx <= x2 && ok; xx++)
						ok = filled[z][y2 + 1][xx] == 1;
					if (!ok)
						break;
				}
				for (z2 = z; z2 + 1 < size[2]; z2++)
				{
					for (ok = 1, yy = y; yy <= y2 && ok; yy++)
						for (xx = x; xx <= x2 && ok; xx++)
							ok = filled[z2 + 1][yy][xx] == 1;
					if (!ok)
						break;
				}
				{
					int zz;
					for (zz = z; zz <= z2; zz++)
						for (yy = y; yy <= y2; yy++)
							for (xx = x; xx <= x2; xx++)
								filled[zz][yy][xx] = 2;
				}
				if (n >= max)
					return -1;
				boxes[n][0] = (lo[0] + x) * ARCH_DIG_CELL;
				boxes[n][1] = (lo[1] + y) * ARCH_DIG_CELL;
				boxes[n][2] = (lo[2] + z) * ARCH_DIG_CELL;
				boxes[n][3] = (lo[0] + x2 + 1) * ARCH_DIG_CELL;
				boxes[n][4] = (lo[1] + y2 + 1) * ARCH_DIG_CELL;
				boxes[n][5] = (lo[2] + z2 + 1) * ARCH_DIG_CELL;
				n++;
			}
	return n;
}

/* ----------------------------------------------------------------- cubes */

void ArchDig_CellAt (const avec3 p, int cell[3])
{
	int	k;

	for (k = 0; k < 3; k++)
		cell[k] = (int) floor (p[k] / ARCH_DIG_CELL);
}

void ArchDig_CellBox (const int cell[3], int mins[3], int maxs[3])
{
	int	k;

	for (k = 0; k < 3; k++)
	{
		mins[k] = cell[k] * ARCH_DIG_CELL;
		maxs[k] = mins[k] + ARCH_DIG_CELL;
	}
}

int ArchDig_BeamCell (const avec3 end, const avec3 dir, int (*solid) (const avec3 p), int cell[3])
{
	avec3	p, q, centre;
	int	on_line[3], combo, k, mins[3], maxs[3];

	for (k = 0; k < 3; k++)
	{
		double nearest;
		p[k] = end[k] + dir[k];
		nearest = ARCH_DIG_CELL * floor (p[k] / ARCH_DIG_CELL + 0.5);
		on_line[k] = fabs (p[k] - nearest) < 0.05;
	}
	/* try each side of every grid line the point sits on */
	for (combo = 0; combo < 8; combo++)
	{
		int skip = 0;
		for (k = 0; k < 3; k++)
		{
			int low = (combo >> k) & 1;
			if (low && !on_line[k])
				skip = 1;	/* this axis has only one side to try */
			q[k] = p[k] + (on_line[k] ? (low ? -0.5 : 0.5) : 0);
		}
		if (skip)
			continue;
		ArchDig_CellAt (q, cell);
		ArchDig_CellBox (cell, mins, maxs);
		for (k = 0; k < 3; k++)
			centre[k] = (mins[k] + maxs[k]) * 0.5;
		if (solid (centre))
			return 1;
	}
	return 0;
}

/* ----------------------------------------------------------------- dwell */

void ArchDig_DwellReset (adwell_t *d)
{
	memset (d, 0, sizeof(*d));
}

int ArchDig_Dwell (adwell_t *d, const int cell[3], double now, double hold)
{
	if (!d->valid || cell[0] != d->cell[0] || cell[1] != d->cell[1] || cell[2] != d->cell[2] ||
	    now - d->last > ARCH_DIG_GAP)
	{
		/* a new cube, or the beam was interrupted: start timing again */
		d->valid = 1;
		d->cell[0] = cell[0]; d->cell[1] = cell[1]; d->cell[2] = cell[2];
		d->since = d->last = now;
		return 0;
	}
	d->last = now;
	if (now - d->since >= hold - 1e-9)
	{
		d->since = now;
		return 1;
	}
	return 0;
}

double ArchDig_DwellProgress (const adwell_t *d, double now, double hold)
{
	double	t;

	if (!d->valid || hold <= 0)
		return 0;
	t = (now - d->since) / hold;
	return t < 0 ? 0 : t > 1 ? 1 : t;
}

/* ---------------------------------------------------------------- parser */

#define SVC_TEMP_ENTITY	23
#define TE_EXPLOSION	3
#define TE_LIGHTNING2	6

enum { P_IDLE, P_TYPE, P_BLAST, P_BEAM_ENTITY, P_BEAM };

void ArchDigParse_Reset (adigparse_t *p)
{
	memset (p, 0, sizeof(*p));
}

int ArchDigParse_Byte (adigparse_t *p, int dest, int value, float out[6])
{
	(void) out;
	if (dest != 0)
		return ARCH_DIG_NONE;
	if (p->state == P_TYPE)
	{
		p->count = 0;
		if (value == TE_EXPLOSION)
			p->state = P_BLAST;
		else if (value == TE_LIGHTNING2)
			p->state = P_BEAM_ENTITY;
		else
			p->state = P_IDLE;
		return ARCH_DIG_NONE;
	}
	/* anywhere else a byte either starts a new report or means the one in progress was not what we thought */
	p->state = value == SVC_TEMP_ENTITY ? P_TYPE : P_IDLE;
	return ARCH_DIG_NONE;
}

int ArchDigParse_Coord (adigparse_t *p, int dest, float value, float out[6])
{
	int	need;

	if (dest != 0)
		return ARCH_DIG_NONE;
	if (p->state != P_BLAST && p->state != P_BEAM)
	{
		p->state = P_IDLE;
		return ARCH_DIG_NONE;
	}
	need = p->state == P_BLAST ? 3 : 6;
	p->values[p->count++] = value;
	if (p->count < need)
		return ARCH_DIG_NONE;
	memcpy (out, p->values, need * sizeof(float));
	p->state = P_IDLE;
	return need == 3 ? ARCH_DIG_BLAST : ARCH_DIG_BEAM;
}

int ArchDigParse_Entity (adigparse_t *p, int dest, float out[6])
{
	(void) out;
	if (dest != 0)
		return ARCH_DIG_NONE;
	if (p->state == P_BEAM_ENTITY)
	{
		p->state = P_BEAM;
		p->count = 0;
	}
	else
		p->state = P_IDLE;
	return ARCH_DIG_NONE;
}
