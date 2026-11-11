/*
QuakeArch - the build action wheel: which segment the mouse is pointing at.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#ifndef ARCH_WHEEL_H
#define ARCH_WHEEL_H

enum { ARCH_ACTION_QUAD, ARCH_ACTION_ARCH, ARCH_ACTION_BUMPER, ARCH_NUM_ACTIONS };

#define ARCH_WHEEL_DEADZONE	40	/* mouse counts from the centre before anything is highlighted */
#define ARCH_WHEEL_RADIUS	120	/* the pointer is held within this, so reversing responds at once */

typedef struct { int open; double x, y; } awheel_t;	/* pointer offset from the centre; y is up */

void        ArchWheel_Open (awheel_t *w);
void        ArchWheel_Move (awheel_t *w, double right, double up);	/* ignored while closed */
int         ArchWheel_Highlight (const awheel_t *w);			/* an action, or -1 */
int         ArchWheel_Close (awheel_t *w);				/* the selected action, or -1 for no change */
const char *ArchWheel_ActionName (int action);

/* the direction of each segment's middle, in degrees (0 = right, counter-clockwise), for drawing */
double      ArchWheel_SegmentAngle (int action);

#endif /* ARCH_WHEEL_H */
