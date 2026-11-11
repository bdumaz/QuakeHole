/*
QuakeHole - answers the engine's world collision queries from the brush world.

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
	float		endpos[3];
	int		i;

	ArchWorld_MoveFloat (start, end, mins, maxs, &tr, endpos);

	memset (&trace, 0, sizeof(trace));
	trace.fraction = (float) tr.fraction;
	trace.startsolid = tr.startsolid ? true : false;
	trace.allsolid = tr.allsolid ? true : false;
	trace.inopen = true;
	VectorCopy (endpos, trace.endpos);
	if (tr.fraction < 1)
	{
		for (i = 0; i < 3; i++)
			trace.plane.normal[i] = (float) tr.normal[i];
		trace.plane.dist = (float) tr.dist;
	}
	if (tr.fraction < 1 || tr.startsolid)
		trace.ent = ent;

	/* Sight lines: the game code asks whether a line crossed a water surface
	   (monsters cannot see through one). Liquids are not brushes, so the map's
	   own point tree answers that for the part of the line that was open. */
	if (!Arch_Active () && sv.worldmodel && mins[0] == maxs[0] && mins[1] == maxs[1] && mins[2] == maxs[2])
	{
		hull_t	*hull = &sv.worldmodel->hulls[0];
		trace_t	liquid;

		memset (&liquid, 0, sizeof(liquid));
		liquid.fraction = 1;
		liquid.allsolid = true;
		VectorCopy (trace.endpos, liquid.endpos);
		SV_RecursiveHullCheck (hull, hull->firstclipnode, 0, 1, start, trace.endpos, &liquid);
		trace.inopen = liquid.inopen;
		trace.inwater = liquid.inwater;
	}
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
