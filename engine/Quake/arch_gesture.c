/*
QuakeArch - the build-mode input gesture.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include <string.h>
#include <math.h>
#include "arch_gesture.h"
#include "arch_cursor.h"

void ArchGesture_Init (agesture_t *g)
{
	memset (g, 0, sizeof(*g));
	g->width = g->height = ARCH_CURSOR_DEFAULT;
}

static void end_press (agesture_t *g)
{
	g->primary = g->armed = g->scrolled = g->dragging = 0;
	g->travel = g->origin = 0;
	g->depth = 0;
}

void ArchGesture_PrimaryDown (agesture_t *g, int cursor_valid)
{
	end_press (g);
	g->primary = 1;
	g->armed = cursor_valid ? 1 : 0;
}

aaction_t ArchGesture_PrimaryUp (agesture_t *g, int *depth)
{
	aaction_t	act = ARCH_ACT_NONE;

	if (g->primary && g->armed)
	{
		if (g->dragging)
		{
			if (g->depth != 0)
			{
				act = ARCH_ACT_DRAG;
				*depth = g->depth;
			}
		}
		else if (!g->scrolled)
		{
			act = ARCH_ACT_CLICK;
			*depth = ARCH_CLICK_DEPTH;
		}
	}
	end_press (g);
	return act;
}

void ArchGesture_Secondary (agesture_t *g, int down)
{
	g->secondary = down ? 1 : 0;
}

void ArchGesture_Wheel (agesture_t *g, int dir)
{
	int	mode = ARCH_SCALE_UNIFORM;

	if (g->dragging || !dir)
		return;
	if (g->primary)
	{
		g->scrolled = 1;
		mode = g->secondary ? ARCH_SCALE_WIDTH : ARCH_SCALE_HEIGHT;
	}
	ArchCursor_StepSize (mode, dir, &g->width, &g->height);
}

int ArchGesture_Motion (agesture_t *g, double forward_counts, double speed)
{
	double	limit, d;
	int	started = 0;

	if (!g->primary || !g->armed || g->scrolled)
		return 0;
	if (speed <= 0)
		speed = 1;

	/* stop counting at the limit so the way back is not a long haul */
	g->travel += forward_counts;
	if (!g->dragging && fabs (g->travel) >= ARCH_DRAG_THRESHOLD)
	{
		/* depth starts from zero here, so the block grows from the surface without a jump */
		g->origin = g->travel > 0 ? ARCH_DRAG_THRESHOLD : -ARCH_DRAG_THRESHOLD;
		g->dragging = 1;
		started = 1;
	}
	if (g->dragging)
	{
		limit = ARCH_DRAG_LIMIT / speed;
		if (g->travel - g->origin > limit) g->travel = g->origin + limit;
		if (g->travel - g->origin < -limit) g->travel = g->origin - limit;
		d = (g->travel - g->origin) * speed;
		g->depth = ARCH_GRID_SNAP * (int) floor (d / ARCH_GRID_SNAP + 0.5);
		if (g->depth > ARCH_DRAG_LIMIT) g->depth = ARCH_DRAG_LIMIT;
		if (g->depth < -ARCH_DRAG_LIMIT) g->depth = -ARCH_DRAG_LIMIT;
	}
	return started;
}

void ArchGesture_Cancel (agesture_t *g)
{
	end_press (g);
}
