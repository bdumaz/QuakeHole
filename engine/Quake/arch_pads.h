/*
QuakeArch - jump pads ("bumpers"): markers on a surface that launch the
player away from it. They are not part of the solid world.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#ifndef ARCH_PADS_H
#define ARCH_PADS_H

#include <stdio.h>
#include "arch_cursor.h"

#define ARCH_MAX_PADS		256
#define ARCH_PAD_THICK		4	/* thickness of the drawn slab */
#define ARCH_PAD_REACH		16	/* how far the trigger extends from the surface */
#define ARCH_PAD_DEFAULT	600	/* launch speed, units per second */
#define ARCH_PAD_MIN		200
#define ARCH_PAD_MAX		2000
#define ARCH_PAD_PER_UNIT	4	/* speed change per unit of drag depth */

typedef struct
{
	int	naxis, nsign, plane;	/* the surface, as in acursor_t */
	int	mins[3], maxs[3];	/* the trigger volume */
	int	speed;
} apad_t;

void ArchPads_Select (int which);	/* 0 the sandbox's pads, 1 the campaign map's; the calls below use the selected list */
void ArchPads_Clear (void);
int  ArchPads_Count (void);
const apad_t *ArchPads_Get (int i);	/* NULL when out of range */

/* Whether a pad over the cursor rectangle would have surface under its centre:
   solid just behind and empty just in front. This is the same rule Prune removes by,
   so a pad that can be placed is not removed until the world changes there. */
int  ArchPads_CanPlace (const acursor_t *c, int (*solid) (const avec3 p));

/* Places a pad over the cursor rectangle. Pads it overlaps on the same surface
   are replaced. Returns 0, changing nothing, if the limit would be exceeded. */
int  ArchPads_Add (const acursor_t *c, int speed);

/* Removes pads whose surface is gone or covered: solid(p) must be true just
   behind the pad's centre and false just in front. Returns how many went. */
int  ArchPads_Prune (int (*solid) (const avec3 p));

/* If the box overlaps any pad's trigger, sets the velocity along that pad's
   normal to its speed, away from the surface. Returns 1 if any pad acted. */
int  ArchPads_Touch (const avec3 absmin, const avec3 absmax, double velocity[3]);

/* launch speed for a drag: pulling (negative depth) raises it, pushing lowers it */
int  ArchPads_SpeedFromDrag (int depth);

/* writes each pad as a trigger_push entity in .map text; 1 on success */
int  ArchPads_Write (FILE *f);

#endif /* ARCH_PADS_H */
