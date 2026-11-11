/*
QuakeArch - the build action wheel.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include <math.h>
#include "arch_wheel.h"

#define PI	3.14159265358979323846

void ArchWheel_Open (awheel_t *w)
{
	w->open = 1;
	w->x = w->y = 0;
}

void ArchWheel_Move (awheel_t *w, double right, double up)
{
	double	len;

	if (!w->open)
		return;
	w->x += right;
	w->y += up;
	len = sqrt (w->x * w->x + w->y * w->y);
	if (len > ARCH_WHEEL_RADIUS)
	{
		w->x *= ARCH_WHEEL_RADIUS / len;
		w->y *= ARCH_WHEEL_RADIUS / len;
	}
}

/* three equal segments: quad at the top, arch lower right, bumper lower left */
int ArchWheel_Highlight (const awheel_t *w)
{
	double	angle;

	if (!w->open || w->x * w->x + w->y * w->y < (double) ARCH_WHEEL_DEADZONE * ARCH_WHEEL_DEADZONE)
		return -1;
	angle = atan2 (w->y, w->x) * 180.0 / PI;	/* -180..180 */
	if (angle >= 30 && angle < 150)
		return ARCH_ACTION_QUAD;
	if (angle >= -90 && angle < 30)
		return ARCH_ACTION_ARCH;
	return ARCH_ACTION_BUMPER;
}

int ArchWheel_Close (awheel_t *w)
{
	int	action = ArchWheel_Highlight (w);

	w->open = 0;
	w->x = w->y = 0;
	return action;
}

const char *ArchWheel_ActionName (int action)
{
	switch (action)
	{
	case ARCH_ACTION_QUAD:		return "Extrude Quad";
	case ARCH_ACTION_ARCH:		return "Extrude Arch";
	case ARCH_ACTION_BUMPER:	return "Place Bumper";
	default:			return "";
	}
}

double ArchWheel_SegmentAngle (int action)
{
	switch (action)
	{
	case ARCH_ACTION_QUAD:		return 90;
	case ARCH_ACTION_ARCH:		return -30;
	default:			return 210;
	}
}
