/*
QuakeArch - jump pads.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include <string.h>
#include "arch_pads.h"

/* the sandbox and the campaign map each have their own pads */
static apad_t	pad_sets[2][ARCH_MAX_PADS];
static int	pad_counts[2];
static int	pad_set;

#define pads		(pad_sets[pad_set])
#define num_pads	(pad_counts[pad_set])

void ArchPads_Select (int which)
{
	pad_set = which ? 1 : 0;
}

void ArchPads_Clear (void)
{
	num_pads = 0;
}

int ArchPads_Count (void)
{
	return num_pads;
}

const apad_t *ArchPads_Get (int i)
{
	return (i >= 0 && i < num_pads) ? &pads[i] : NULL;
}

static void remove_pad (int i)
{
	pads[i] = pads[--num_pads];
}

/* same surface, and the rectangles share area */
static int pads_overlap (const apad_t *a, const apad_t *b)
{
	int	k;

	if (a->naxis != b->naxis || a->nsign != b->nsign || a->plane != b->plane)
		return 0;
	for (k = 0; k < 3; k++)
		if (a->mins[k] >= b->maxs[k] || b->mins[k] >= a->maxs[k])
			return 0;
	return 1;
}

/* the removal rule: solid just behind the centre of the rectangle, empty just in front */
static int centre_supported (int naxis, int nsign, int plane, const int mins[3], const int maxs[3],
			     int (*solid) (const avec3 p))
{
	avec3	behind, front;
	int	k;

	for (k = 0; k < 3; k++)
		behind[k] = front[k] = (mins[k] + maxs[k]) * 0.5;
	behind[naxis] = plane - nsign;
	front[naxis] = plane + nsign;
	return solid (behind) && !solid (front);
}

int ArchPads_CanPlace (const acursor_t *c, int (*solid) (const avec3 p))
{
	int	mins[3], maxs[3];

	ArchCursor_Box (c, -ARCH_PAD_REACH, mins, maxs);
	return centre_supported (c->naxis, c->nsign, c->plane, mins, maxs, solid);
}

int ArchPads_Add (const acursor_t *c, int speed)
{
	apad_t	pad;
	int	i, replaced = 0;

	pad.naxis = c->naxis;
	pad.nsign = c->nsign;
	pad.plane = c->plane;
	pad.speed = speed;
	ArchCursor_Box (c, -ARCH_PAD_REACH, pad.mins, pad.maxs);	/* out of the surface */

	for (i = 0; i < num_pads; i++)
		if (pads_overlap (&pads[i], &pad))
			replaced++;
	if (num_pads - replaced + 1 > ARCH_MAX_PADS)
		return 0;
	for (i = num_pads - 1; i >= 0; i--)
		if (pads_overlap (&pads[i], &pad))
			remove_pad (i);
	pads[num_pads++] = pad;
	return 1;
}

int ArchPads_Prune (int (*solid) (const avec3 p))
{
	int	i, removed = 0;

	for (i = num_pads - 1; i >= 0; i--)
	{
		const apad_t	*pad = &pads[i];

		if (!centre_supported (pad->naxis, pad->nsign, pad->plane, pad->mins, pad->maxs, solid))
		{
			remove_pad (i);
			removed++;
		}
	}
	return removed;
}

int ArchPads_Touch (const avec3 absmin, const avec3 absmax, double velocity[3])
{
	int	i, k, touched = 0;

	for (i = 0; i < num_pads; i++)
	{
		const apad_t *pad = &pads[i];

		for (k = 0; k < 3; k++)
			if (absmin[k] >= pad->maxs[k] || absmax[k] <= pad->mins[k])
				break;
		if (k < 3)
			continue;
		velocity[pad->naxis] = (double) pad->nsign * pad->speed;
		touched = 1;
	}
	return touched;
}

int ArchPads_SpeedFromDrag (int depth)
{
	int	speed = ARCH_PAD_DEFAULT - depth * ARCH_PAD_PER_UNIT;

	if (speed < ARCH_PAD_MIN) speed = ARCH_PAD_MIN;
	if (speed > ARCH_PAD_MAX) speed = ARCH_PAD_MAX;
	return speed;
}

/* the "angle" key the stock game reads: -1 up, -2 down, otherwise a compass direction */
static int pad_angle (const apad_t *pad)
{
	if (pad->naxis == 2)
		return pad->nsign > 0 ? -1 : -2;
	if (pad->naxis == 0)
		return pad->nsign > 0 ? 360 : 180;	/* 0 would mean "no direction" to the stock game */
	return pad->nsign > 0 ? 90 : 270;
}

int ArchPads_Write (FILE *f)
{
	int	i, k;

	for (i = 0; i < num_pads; i++)
	{
		const apad_t	*pad = &pads[i];
		abrush_t	*box = ArchBrush_Box (pad->mins, pad->maxs);

		if (!box)
			continue;
		/* the stock trigger_push multiplies its speed by 10 */
		fprintf (f, "{\n\"classname\" \"trigger_push\"\n\"speed\" \"%g\"\n\"angle\" \"%d\"\n{\n",
			 pad->speed / 10.0, pad_angle (pad));
		for (k = 0; k < box->nfaces; k++)
		{
			const aplane_t *pl = &box->faces[k].plane;
			fprintf (f, "( %d %d %d ) ( %d %d %d ) ( %d %d %d ) trigger 0 0 0 1 1\n",
				 pl->p[0][0], pl->p[0][1], pl->p[0][2],
				 pl->p[1][0], pl->p[1][1], pl->p[1][2],
				 pl->p[2][0], pl->p[2][1], pl->p[2][2]);
		}
		fprintf (f, "}\n}\n");
		ArchBrush_Free (box);
	}
	return ferror (f) ? 0 : 1;
}
