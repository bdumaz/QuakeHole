/*
QuakeArch - answers the engine's world collision queries from the brush world.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include "quakedef.h"
#include "arch.h"
#include "arch_world.h"

trace_t ArchCollide_ClipMove (edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	trace_t		trace;
	atrace_t	tr;
	avec3		s, e, mn, mx;
	int		i;

	for (i = 0; i < 3; i++)
	{
		s[i] = start[i];
		e[i] = end[i];
		mn[i] = mins[i];
		mx[i] = maxs[i];
	}
	ArchWorld_Trace (s, e, mn, mx, &tr);

	memset (&trace, 0, sizeof(trace));
	trace.fraction = (float) tr.fraction;
	trace.startsolid = tr.startsolid ? true : false;
	trace.allsolid = tr.allsolid ? true : false;
	trace.inopen = true;
	if (tr.fraction < 1)
	{
		for (i = 0; i < 3; i++)
		{
			trace.endpos[i] = (float) (s[i] + tr.fraction * (e[i] - s[i]));
			trace.plane.normal[i] = (float) tr.normal[i];
		}
		trace.plane.dist = (float) tr.dist;
	}
	else
		VectorCopy (end, trace.endpos);
	if (tr.fraction < 1 || tr.startsolid)
		trace.ent = ent;
	return trace;
}

int ArchCollide_PointContents (vec3_t p)
{
	avec3	point;

	point[0] = p[0];
	point[1] = p[1];
	point[2] = p[2];
	return ArchWorld_PointSolid (point) ? CONTENTS_SOLID : CONTENTS_EMPTY;
}
