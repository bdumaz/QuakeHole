/*
QuakeArch - build mode: the mode toggle, input routing, the per-frame cursor,
applying Extrude Quad, and drawing the cursor, preview and HUD line.
The rules themselves live in arch_cursor.c and arch_gesture.c.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include "quakedef.h"
#include "arch.h"
#include "arch_world.h"
#include "arch_cursor.h"
#include "arch_gesture.h"

#define CURSOR_RANGE	8192.0
#define CURSOR_LIFT	0.5

static cvar_t	arch_drag_speed = {"arch_drag_speed", "1", CVAR_ARCHIVE};

static qboolean		build_active;
static float		saved_movetype;
static agesture_t	gesture;
static acursor_t	cursor;
static qboolean		cursor_valid;
static qboolean		gesture_ready;

static edict_t *build_player (void)
{
	if (!sv.active || !svs.clients || !svs.clients[0].active)
		return NULL;
	return svs.clients[0].edict;
}

qboolean ArchBuild_Active (void)
{
	if (build_active && !Arch_Active ())
	{
		/* the map changed under us: build mode only exists in the arch map */
		build_active = false;
		ArchGesture_Cancel (&gesture);
	}
	return build_active;
}

/* ----------------------------------------------------------- mode toggle */

static void leave_build_mode (qboolean restore_player)
{
	edict_t	*ent = build_player ();

	if (!build_active)
		return;
	build_active = false;
	ArchGesture_Cancel (&gesture);
	gesture.secondary = 0;
	if (restore_player && ent)
	{
		ent->v.movetype = saved_movetype;
		ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0;
		/* not on the ground until physics says so: a player in mid-air must fall */
		ent->v.flags = (float) ((int) ent->v.flags & ~FL_ONGROUND);
	}
}

static void ArchBuild_Toggle_f (void)
{
	edict_t	*ent = build_player ();

	if (!Arch_Active () || !ent)
	{
		Con_Printf ("arch: not in the arch map\n");
		return;
	}
	if (build_active)
	{
		leave_build_mode (true);
		return;
	}
	if (ent->v.health <= 0)
		return;
	if (!gesture_ready)
	{
		ArchGesture_Init (&gesture);
		gesture_ready = true;
	}
	ArchGesture_Cancel (&gesture);
	gesture.secondary = 0;
	saved_movetype = ent->v.movetype == MOVETYPE_NONE ? MOVETYPE_WALK : ent->v.movetype;
	ent->v.movetype = MOVETYPE_NONE;
	/* weapon kick does not decay while frozen; clear it so the view is level */
	ent->v.punchangle[0] = ent->v.punchangle[1] = ent->v.punchangle[2] = 0;
	ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0;
	cursor_valid = false;
	build_active = true;
}

/* Called for the player every server frame. Anything else that changes the
   movement type (a respawn, the fly command) ends build mode; the new
   movement type stands. */
void ArchBuild_PlayerFrame (edict_t *ent)
{
	if (build_active && (ent->v.movetype != MOVETYPE_NONE || ent->v.health <= 0))
		leave_build_mode (false);
	/* a savegame made in build mode loads a frozen player with build mode off */
	if (!build_active && ent->v.movetype == MOVETYPE_NONE && ent->v.health > 0)
		ent->v.movetype = MOVETYPE_WALK;
}

/* -------------------------------------------------------------- the edit */

static qboolean box_overlaps_player (const int mins[3], const int maxs[3])
{
	edict_t	*ent = build_player ();
	int	k;

	if (!ent)
		return false;
	for (k = 0; k < 3; k++)
	{
		/* touching face to face is not an overlap */
		if (ent->v.origin[k] + ent->v.mins[k] >= maxs[k] || ent->v.origin[k] + ent->v.maxs[k] <= mins[k])
			return false;
	}
	return true;
}

/* Extrude Quad: depth > 0 carves into the surface, depth < 0 pulls solid out of it */
static void apply_extrude_quad (int depth)
{
	int		mins[3], maxs[3], ok;
	abrush_t	*box;

	if (!cursor_valid || !depth)
		return;
	ArchCursor_Box (&cursor, depth, mins, maxs);
	if (depth < 0 && box_overlaps_player (mins, maxs))
	{
		Con_Printf ("arch: can't build on top of yourself\n");
		return;
	}
	box = ArchBrush_Box (mins, maxs);
	if (!box)
		return;
	ok = depth > 0 ? ArchWorld_Carve (box) : ArchWorld_Add (box);
	ArchBrush_Free (box);
	if (!ok)
		Con_Printf ("arch: brush limit reached, edit refused\n");
}

/* ----------------------------------------------------------------- input */

static void primary_down (void)
{
	if (build_active)
		ArchGesture_PrimaryDown (&gesture, cursor_valid);
}

static void primary_up (void)
{
	int		depth = 0;
	aaction_t	act;

	if (!build_active)
		return;
	if (key_dest != key_game || cl.paused)
	{
		/* released behind a menu, the console or a pause: never commit */
		ArchGesture_Cancel (&gesture);
		return;
	}
	act = ArchGesture_PrimaryUp (&gesture, &depth);
	if (act != ARCH_ACT_NONE)
		apply_extrude_quad (depth);
}

static void secondary (qboolean down)
{
	if (build_active)
		ArchGesture_Secondary (&gesture, down);
}

static void wheel (int dir)
{
	if (build_active)
		ArchGesture_Wheel (&gesture, dir);
}

/* Called by the key handler. Returns true if the key was used by build mode
   and must not run its binding. A release is only taken if its press was, so
   a button held since before build mode still reaches its own release. */
qboolean ArchBuild_KeyEvent (int key, qboolean down)
{
	static qboolean	taken[4];
	int		idx;

	switch (key)
	{
	case K_MOUSE1:		idx = 0; break;
	case K_MOUSE2:		idx = 1; break;
	case K_MWHEELUP:	idx = 2; break;
	case K_MWHEELDOWN:	idx = 3; break;
	default:		return false;
	}

	if (down)
	{
		if (!ArchBuild_Active () || key_dest != key_game)
			return false;
		taken[idx] = true;
		switch (idx)
		{
		case 0: primary_down (); break;
		case 1: secondary (true); break;
		case 2: wheel (1); break;
		case 3: wheel (-1); break;
		}
		return true;
	}

	if (!taken[idx])
		return false;
	taken[idx] = false;
	if (idx == 0)
		primary_up ();
	else if (idx == 1)
		secondary (false);
	return true;
}

/* Called by the mouse handler with raw counts. Returns true if the movement
   must not turn the view. */
qboolean ArchBuild_MouseMove (int dx, int dy)
{
	(void) dx;
	if (!ArchBuild_Active ())
		return false;
	if (cl.paused || key_dest != key_game)
	{
		/* console or menu opened mid-gesture: drop it, so the later release does nothing */
		ArchGesture_Cancel (&gesture);
		return false;
	}
	/* mouse pushed away from the player is negative dy */
	ArchGesture_Motion (&gesture, -dy, arch_drag_speed.value);
	return gesture.dragging != 0;
}

/* console versions of the inputs, so scripted tests can drive build mode */
static void ArchBuild_PrimaryDown_f (void)	{ primary_down (); }
static void ArchBuild_PrimaryUp_f (void)	{ primary_up (); }
static void ArchBuild_SecondaryDown_f (void)	{ secondary (true); }
static void ArchBuild_SecondaryUp_f (void)	{ secondary (false); }

static void ArchBuild_Wheel_f (void)
{
	int	n = Cmd_Argc () == 2 ? atoi (Cmd_Argv (1)) : 0;

	if (n)
		wheel (n > 0 ? 1 : -1);
}

static void ArchBuild_Drag_f (void)
{
	if (Cmd_Argc () == 2 && build_active)
		ArchGesture_Motion (&gesture, atof (Cmd_Argv (1)), arch_drag_speed.value);
}

/* --------------------------------------------------------------- drawing */

/* finds the surface under the crosshair; the cursor stays where it is during a drag */
static void update_cursor (void)
{
	vec3_t		forward, right, up;
	avec3		start, end, hit, zero = {0, 0, 0};
	atrace_t	tr;
	int		k;

	if (gesture.dragging)
		return;
	AngleVectors (r_refdef.viewangles, forward, right, up);
	for (k = 0; k < 3; k++)
	{
		start[k] = r_refdef.vieworg[k];
		end[k] = start[k] + forward[k] * CURSOR_RANGE;
	}
	ArchWorld_Trace (start, end, zero, zero, &tr);
	cursor_valid = false;
	if (tr.fraction >= 1.0 || tr.startsolid)
		return;
	for (k = 0; k < 3; k++)
		hit[k] = start[k] + tr.fraction * (end[k] - start[k]);
	if (ArchCursor_Place (hit, tr.normal, cl.viewangles[YAW], gesture.width, gesture.height, &cursor))
	{
		ArchCursor_Fit (&cursor, ArchWorld_PointSolid);
		cursor_valid = true;
	}
}

static void quad (const avec3 a, const avec3 b, const avec3 c, const avec3 d)
{
	glVertex3d (a[0], a[1], a[2]);
	glVertex3d (b[0], b[1], b[2]);
	glVertex3d (c[0], c[1], c[2]);
	glVertex3d (d[0], d[1], d[2]);
}

static void draw_rect (avec3 corners[4], float r, float g, float b, float fill_alpha)
{
	int	i;

	glColor4f (r, g, b, fill_alpha);
	glBegin (GL_QUADS);
	quad (corners[0], corners[1], corners[2], corners[3]);
	glEnd ();
	glColor4f (r, g, b, 1);
	glBegin (GL_LINE_LOOP);
	for (i = 0; i < 4; i++)
		glVertex3d (corners[i][0], corners[i][1], corners[i][2]);
	glEnd ();
}

/* the volume a drag would carve or add */
static void draw_preview (int depth)
{
	avec3	nearc[4], farc[4];
	float	r = depth > 0 ? 1.0f : 0.3f, g = depth > 0 ? 0.45f : 1.0f, b = depth > 0 ? 0.2f : 0.5f;
	int	i, j;

	ArchCursor_Corners (&cursor, 0, nearc);
	ArchCursor_Corners (&cursor, -(double) depth, farc);	/* lift is toward the air; depth is into the surface */

	glDisable (GL_DEPTH_TEST);	/* a carve preview is inside the wall */
	draw_rect (farc, r, g, b, 0.25f);
	for (i = 0; i < 4; i++)
	{
		j = (i + 1) % 4;
		glColor4f (r, g, b, 0.15f);
		glBegin (GL_QUADS);
		quad (nearc[i], nearc[j], farc[j], farc[i]);
		glEnd ();
		glColor4f (r, g, b, 1);
		glBegin (GL_LINES);
		glVertex3d (nearc[i][0], nearc[i][1], nearc[i][2]);
		glVertex3d (farc[i][0], farc[i][1], farc[i][2]);
		glEnd ();
	}
	glEnable (GL_DEPTH_TEST);
}

/* called after the world and entities are drawn */
void ArchBuild_Draw (void)
{
	avec3	corners[4];

	if (!ArchBuild_Active ())
		return;
	update_cursor ();
	if (!cursor_valid)
		return;

	if (GL_UseProgramFunc)	/* absent with -noglsl or an old driver */
		GL_UseProgramFunc (0);
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_CULL_FACE);
	glEnable (GL_BLEND);
	glDepthMask (GL_FALSE);
	glLineWidth (2);

	if (gesture.dragging && gesture.depth)
		draw_preview (gesture.depth);
	ArchCursor_Corners (&cursor, CURSOR_LIFT, corners);
	draw_rect (corners, 1.0f, 0.85f, 0.3f, 0.2f);

	glLineWidth (1);
	glDepthMask (GL_TRUE);
	glDisable (GL_BLEND);
	if (gl_cull.value)
		glEnable (GL_CULL_FACE);
	glEnable (GL_TEXTURE_2D);
	glColor4f (1, 1, 1, 1);
}

/* one line near the top centre of the screen */
void ArchBuild_DrawHUD (void)
{
	char	w[16], h[16], d[16], line[96];
	int	scale;

	if (!ArchBuild_Active ())
		return;
	ArchCursor_FormatMetres (gesture.width, w, sizeof(w));
	ArchCursor_FormatMetres (gesture.height, h, sizeof(h));
	if (gesture.dragging && gesture.depth)
	{
		ArchCursor_FormatMetres (abs (gesture.depth), d, sizeof(d));
		q_snprintf (line, sizeof(line), "BUILD  Extrude Quad  %s x %s m  %s %s m", w, h,
			    gesture.depth > 0 ? "carve" : "extrude", d);
	}
	else
		q_snprintf (line, sizeof(line), "BUILD  Extrude Quad  %s x %s m", w, h);

	/* pixel canvas, with the 8-pixel font scaled up to stay readable at any resolution */
	scale = glheight / 360;
	if (scale < 1)
		scale = 1;
	GL_SetCanvas (CANVAS_DEFAULT);
	glPushMatrix ();
	glScalef ((float) scale, (float) scale, 1);
	Draw_String ((glwidth / scale - (int) strlen (line) * 8) / 2, 12, line);
	glPopMatrix ();
}

/* ------------------------------------------------------------------ init */

void ArchBuild_Init (void)
{
	Cvar_RegisterVariable (&arch_drag_speed);
	Cmd_AddCommand ("arch_build", ArchBuild_Toggle_f);
	Cmd_AddCommand ("+arch_primary", ArchBuild_PrimaryDown_f);
	Cmd_AddCommand ("-arch_primary", ArchBuild_PrimaryUp_f);
	Cmd_AddCommand ("+arch_secondary", ArchBuild_SecondaryDown_f);
	Cmd_AddCommand ("-arch_secondary", ArchBuild_SecondaryUp_f);
	Cmd_AddCommand ("arch_wheel", ArchBuild_Wheel_f);
	Cmd_AddCommand ("arch_drag", ArchBuild_Drag_f);
	ArchGesture_Init (&gesture);
	gesture_ready = true;
}
