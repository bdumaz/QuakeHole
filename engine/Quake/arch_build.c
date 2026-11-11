/*
QuakeHole - build mode: the mode toggle, input routing, the per-frame cursor,
the action wheel, applying the selected action, and drawing the cursor,
previews, pads, wheel and HUD line.
The rules themselves live in arch_cursor.c, arch_gesture.c, arch_wheel.c and
arch_pads.c.

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
#include "arch_wheel.h"
#include "arch_pads.h"

#define CURSOR_RANGE	8192.0
#define CURSOR_LIFT	0.5
#define OUTLINE_MAX	ARCH_PROFILE_MAX

static cvar_t	arch_drag_speed = {"arch_drag_speed", "1", CVAR_ARCHIVE};

static qboolean		build_active;
static float		saved_movetype;
static agesture_t	gesture;
static qboolean		gesture_ready;
static awheel_t		wheel;
static int		current_action = ARCH_ACTION_QUAD;

static acursor_t	cursor;
static qboolean		cursor_valid;
static qboolean		cursor_pinned;	/* snapped to a surface with R: it no longer follows the aim */
static avec3		cursor_hit;	/* where the aim last met a surface */
static qboolean		unfreeze_check;	/* the first player frame of a map has not run yet */

/* Build mode is offered in the cube room, and in any map reached from it
   through the console. A game started from the menu is played as normal. */
static qboolean		build_session;
static qboolean		intro_pending;	/* show the welcome text once the cube room is on screen */
static double		intro_until;

static edict_t *build_player (void)
{
	if (!sv.active || !svs.clients || !svs.clients[0].active)
		return NULL;
	return svs.clients[0].edict;
}

qboolean ArchBuild_Active (void)
{
	if (build_active && !Arch_BrushWorld ())
	{
		/* the map changed under us: build mode only exists in the arch map */
		build_active = false;
		ArchGesture_Cancel (&gesture);
		wheel.open = 0;
		cursor_pinned = false;
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
	wheel.open = 0;
	cursor_pinned = false;
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

	if (!Arch_BrushWorld () || !ent)
	{
		Con_Printf ("arch: build mode needs the cube room or a converted campaign map\n");
		return;
	}
	if (build_active)
	{
		leave_build_mode (true);
		return;
	}
	if (!Arch_Active () && !build_session)
	{
		Con_Printf ("Build mode is not available in a normal game. Choose Build Mode in the main menu.\n");
		return;
	}
	if (ent->v.health <= 0 || ent->v.movetype == MOVETYPE_NONE)
		return;		/* dead, or frozen by the game (the intermission) */
	if (!gesture_ready)
	{
		ArchGesture_Init (&gesture);
		gesture_ready = true;
	}
	ArchGesture_Cancel (&gesture);
	gesture.secondary = 0;
	wheel.open = 0;
	saved_movetype = ent->v.movetype == MOVETYPE_NONE ? MOVETYPE_WALK : ent->v.movetype;
	ent->v.movetype = MOVETYPE_NONE;
	/* weapon kick does not decay while frozen; clear it so the view is level */
	ent->v.punchangle[0] = ent->v.punchangle[1] = ent->v.punchangle[2] = 0;
	ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0;
	cursor_valid = false;
	cursor_pinned = false;
	build_active = true;
}

/* the server is starting a map: whatever mode we were in belonged to the last one */
void ArchBuild_MapChanged (void)
{
	unfreeze_check = true;
	build_active = false;
	if (gesture_ready)
		ArchGesture_Cancel (&gesture);
	gesture.secondary = 0;
	wheel.open = 0;
	cursor_pinned = false;
	cursor_valid = false;
}

/* Called for the player every server frame. Anything else that changes the
   movement type (a respawn, the fly command) ends build mode; the new
   movement type stands. */
void ArchBuild_PlayerFrame (edict_t *ent)
{
	if (build_active && (ent->v.movetype != MOVETYPE_NONE || ent->v.health <= 0))
		leave_build_mode (false);
	/* A savegame made in build mode loads a frozen player with build mode off.
	   Only the first frame of a map is checked: later, a frozen player is the
	   game's own doing (the intermission at the end of a level). */
	if (unfreeze_check && !build_active)
	{
		unfreeze_check = false;
		if (ent->v.movetype == MOVETYPE_NONE && ent->v.health > 0)
			ent->v.movetype = MOVETYPE_WALK;
	}
}

/* ----------------------------------------------------------- the actions */

static qboolean bounds_overlap_player (const avec3 mins, const avec3 maxs)
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

/* carves the shape (depth > 0) or adds it as solid (depth < 0); takes ownership of the brush */
static void extrude (abrush_t *shape, int depth)
{
	int	ok;

	if (!shape)
		return;
	if (depth < 0 && bounds_overlap_player (shape->mins, shape->maxs))
	{
		Con_Printf ("arch: can't build on top of yourself\n");
		ArchBrush_Free (shape);
		return;
	}
	if (!Arch_Active ())
	{
		/* a campaign map: the map's own surfaces and the hole drawing have to follow */
		vec3_t at;
		at[0] = (float) cursor_hit[0]; at[1] = (float) cursor_hit[1]; at[2] = (float) cursor_hit[2];
		ok = ArchGame_CampaignEdit (shape, depth > 0, at);
	}
	else
		ok = depth > 0 ? ArchWorld_Carve (shape) : ArchWorld_Add (shape);
	ArchBrush_Free (shape);
	if (!ok)
	{
		Con_Printf ("arch: brush limit reached, edit refused\n");
		return;
	}
	ArchPads_Prune (ArchWorld_PointSolid);	/* pads whose surface just went */
}

/* runs the selected action on the cursor. A click passes the default depth. */
static void apply_action (aaction_t act, int depth)
{
	int	mins[3], maxs[3];

	if (!cursor_valid)
		return;
	switch (current_action)
	{
	case ARCH_ACTION_QUAD:
		if (!depth)
			return;
		ArchCursor_Box (&cursor, depth, mins, maxs);
		extrude (ArchBrush_Box (mins, maxs), depth);
		break;
	case ARCH_ACTION_ARCH:
		if (!depth)
			return;
		extrude (ArchCursor_ArchBrush (&cursor, depth), depth);
		break;
	case ARCH_ACTION_BUMPER:
		if (!ArchPads_CanPlace (&cursor, ArchWorld_PointSolid))
		{
			Con_Printf ("arch: no surface under the middle of the pad\n");
			return;
		}
		/* a click places at the default strength; a drag sets it, including a drag back to zero */
		if (!ArchPads_Add (&cursor, act == ARCH_ACT_CLICK ? ARCH_PAD_DEFAULT : ArchPads_SpeedFromDrag (depth)))
			Con_Printf ("arch: pad limit reached\n");
		break;
	}
	cursor_pinned = false;	/* the surface may have changed */
}

/* the world was edited from outside build mode (console commands): a pinned cursor may be stale */
void ArchBuild_WorldChanged (void)
{
	cursor_pinned = false;
}

static void set_action (int action)
{
	if (action < 0 || action >= ARCH_NUM_ACTIONS)
		return;
	current_action = action;
}

/* ----------------------------------------------------------------- input */

static void primary_down (void)
{
	if (build_active && !wheel.open)
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
		apply_action (act, depth);
}

static void secondary (qboolean down)
{
	if (build_active)
		ArchGesture_Secondary (&gesture, down);
}

static void scroll (int dir)
{
	if (!build_active || wheel.open)
		return;
	if (!gesture.dragging)
		cursor_pinned = false;	/* resizing takes the cursor back from a snap */
	ArchGesture_Wheel (&gesture, dir);
}

static void menu_down (void)
{
	if (!build_active)
		return;
	ArchGesture_Cancel (&gesture);	/* opening the wheel abandons any gesture */
	ArchWheel_Open (&wheel);
}

static void menu_up (void)
{
	int	action;

	if (!wheel.open)
		return;
	action = ArchWheel_Close (&wheel);
	if (build_active && key_dest == key_game && !cl.paused)
		set_action (action);
}

/* Called by the key handler. Returns true if the key was used by build mode
   and must not run its binding. A release is only taken if its press was, so
   a button held since before build mode still reaches its own release. */
qboolean ArchBuild_KeyEvent (int key, qboolean down)
{
	static qboolean	taken[5];
	int		idx;

	switch (key)
	{
	case K_MOUSE1:		idx = 0; break;
	case K_MOUSE2:		idx = 1; break;
	case K_MWHEELUP:	idx = 2; break;
	case K_MWHEELDOWN:	idx = 3; break;
	case K_MOUSE3:		idx = 4; break;
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
		case 2: scroll (1); break;
		case 3: scroll (-1); break;
		case 4: menu_down (); break;
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
	else if (idx == 4)
		menu_up ();
	return true;
}

/* Called by the mouse handler with raw counts. Returns true if the movement
   must not turn the view. */
qboolean ArchBuild_MouseMove (int dx, int dy)
{
	if (!ArchBuild_Active ())
		return false;
	if (cl.paused || key_dest != key_game)
	{
		/* console or menu opened mid-gesture: drop it, so the later release does nothing */
		ArchGesture_Cancel (&gesture);
		wheel.open = 0;
		return false;
	}
	if (wheel.open)
	{
		ArchWheel_Move (&wheel, dx, -dy);	/* mouse pushed away is negative dy */
		return true;
	}
	ArchGesture_Motion (&gesture, -dy, arch_drag_speed.value);
	return gesture.dragging != 0;
}

/* console versions of the inputs, so scripted tests can drive build mode */
static void ArchBuild_PrimaryDown_f (void)	{ primary_down (); }
static void ArchBuild_PrimaryUp_f (void)	{ primary_up (); }
static void ArchBuild_SecondaryDown_f (void)	{ secondary (true); }
static void ArchBuild_SecondaryUp_f (void)	{ secondary (false); }
static void ArchBuild_MenuDown_f (void)		{ menu_down (); }
static void ArchBuild_MenuUp_f (void)		{ menu_up (); }

static void ArchBuild_Wheel_f (void)
{
	int	n = Cmd_Argc () == 2 ? atoi (Cmd_Argv (1)) : 0;

	if (n)
		scroll (n > 0 ? 1 : -1);
}

static void ArchBuild_Drag_f (void)
{
	if (Cmd_Argc () == 2 && build_active && !wheel.open)
		ArchGesture_Motion (&gesture, atof (Cmd_Argv (1)), arch_drag_speed.value);
}

/* test input: pointer movement while the wheel is open, right and up */
static void ArchBuild_WheelMove_f (void)
{
	if (Cmd_Argc () == 3)
		ArchWheel_Move (&wheel, atof (Cmd_Argv (1)), atof (Cmd_Argv (2)));
}

static void ArchBuild_Action_f (void)
{
	const char	*name = Cmd_Argv (1);

	if (Cmd_Argc () == 2 && !q_strcasecmp (name, "quad"))
		set_action (ARCH_ACTION_QUAD);
	else if (Cmd_Argc () == 2 && !q_strcasecmp (name, "arch"))
		set_action (ARCH_ACTION_ARCH);
	else if (Cmd_Argc () == 2 && !q_strcasecmp (name, "bumper"))
		set_action (ARCH_ACTION_BUMPER);
	else
		Con_Printf ("usage: arch_action quad|arch|bumper\n");
}

/* R: fill the surface around the aim point, or let go of it */
static void ArchBuild_Snap_f (void)
{
	if (!build_active || !cursor_valid || gesture.dragging || wheel.open)
		return;
	if (cursor_pinned)
	{
		cursor_pinned = false;
		return;
	}
	if (ArchCursor_Snap (&cursor, cursor_hit, ArchWorld_PointSolid))
		cursor_pinned = true;
}

/* ---------------------------------------------------------------- cursor */

/* finds the surface under the crosshair; the cursor stays put during a drag and while the wheel is open */
static void update_cursor (void)
{
	vec3_t		forward, right, up;
	avec3		start, end, hit, zero = {0, 0, 0};
	acursor_t	fresh;
	atrace_t	tr;
	int		k;

	if (gesture.dragging || wheel.open)
		return;
	AngleVectors (r_refdef.viewangles, forward, right, up);
	for (k = 0; k < 3; k++)
	{
		start[k] = r_refdef.vieworg[k];
		end[k] = start[k] + forward[k] * CURSOR_RANGE;
	}
	ArchWorld_Trace (start, end, zero, zero, &tr);
	for (k = 0; k < 3; k++)
		hit[k] = start[k] + tr.fraction * (end[k] - start[k]);

	if (tr.fraction >= 1.0 || tr.startsolid ||
	    !ArchCursor_Place (hit, tr.normal, cl.viewangles[YAW], gesture.width, gesture.height, &fresh))
	{
		cursor_valid = false;
		cursor_pinned = false;
		return;
	}
	for (k = 0; k < 3; k++)
		cursor_hit[k] = hit[k];

	if (cursor_pinned)
	{
		/* a pinned cursor holds while the aim stays on it */
		if (fresh.naxis == cursor.naxis && fresh.nsign == cursor.nsign && fresh.plane == cursor.plane &&
		    hit[cursor.uaxis] >= cursor.umin && hit[cursor.uaxis] <= cursor.umax &&
		    hit[cursor.vaxis] >= cursor.vmin && hit[cursor.vaxis] <= cursor.vmax)
		{
			/* the rectangle holds, but the arch still points the way the player faces */
			if (fresh.vaxis != cursor.vaxis)
			{
				int umin = cursor.vmin, umax = cursor.vmax;
				cursor.vmin = cursor.umin; cursor.vmax = cursor.umax;
				cursor.umin = umin; cursor.umax = umax;
				cursor.uaxis = fresh.uaxis;
				cursor.vaxis = fresh.vaxis;
			}
			cursor.vdir = fresh.vdir;
			cursor_valid = true;
			return;
		}
		cursor_pinned = false;
	}
	ArchCursor_Fit (&fresh, ArchWorld_PointSolid);
	cursor = fresh;
	cursor_valid = true;
}

/* --------------------------------------------------------------- drawing */

/* the cursor outline for the selected action, moved offset units along the surface normal */
static int cursor_outline (double offset, avec3 out[OUTLINE_MAX])
{
	int	pts[ARCH_PROFILE_MAX][3], n, i, k;

	if (current_action != ARCH_ACTION_ARCH)
	{
		ArchCursor_Corners (&cursor, offset, out);
		return 4;
	}
	n = ArchCursor_ArchProfile (&cursor, pts);
	for (i = 0; i < n; i++)
	{
		for (k = 0; k < 3; k++)
			out[i][k] = pts[i][k];
		out[i][cursor.naxis] += cursor.nsign * offset;
	}
	return n;
}

static void draw_outline (avec3 *pts, int n, const float *rgb, float fill_alpha)
{
	int	i;

	glColor4f (rgb[0], rgb[1], rgb[2], fill_alpha);
	glBegin (GL_POLYGON);
	for (i = 0; i < n; i++)
		glVertex3d (pts[i][0], pts[i][1], pts[i][2]);
	glEnd ();
	glColor4f (rgb[0], rgb[1], rgb[2], 1);
	glBegin (GL_LINE_LOOP);
	for (i = 0; i < n; i++)
		glVertex3d (pts[i][0], pts[i][1], pts[i][2]);
	glEnd ();
}

/* the volume a drag would carve or add */
static void draw_preview (int depth)
{
	static const float	carve_rgb[3] = {1.0f, 0.45f, 0.2f}, add_rgb[3] = {0.3f, 1.0f, 0.5f};
	const float		*rgb = depth > 0 ? carve_rgb : add_rgb;
	avec3			nearp[OUTLINE_MAX], farp[OUTLINE_MAX];
	int			n, i, j;

	n = cursor_outline (0, nearp);
	cursor_outline (-(double) depth, farp);	/* the offset is toward the air; depth is into the surface */

	glDisable (GL_DEPTH_TEST);	/* a carve preview is inside the wall */
	draw_outline (farp, n, rgb, 0.25f);
	for (i = 0; i < n; i++)
	{
		j = (i + 1) % n;
		glColor4f (rgb[0], rgb[1], rgb[2], 0.15f);
		glBegin (GL_QUADS);
		glVertex3d (nearp[i][0], nearp[i][1], nearp[i][2]);
		glVertex3d (nearp[j][0], nearp[j][1], nearp[j][2]);
		glVertex3d (farp[j][0], farp[j][1], farp[j][2]);
		glVertex3d (farp[i][0], farp[i][1], farp[i][2]);
		glEnd ();
		glColor4f (rgb[0], rgb[1], rgb[2], 1);
		glBegin (GL_LINES);
		glVertex3d (nearp[i][0], nearp[i][1], nearp[i][2]);
		glVertex3d (farp[i][0], farp[i][1], farp[i][2]);
		glEnd ();
	}
	glEnable (GL_DEPTH_TEST);
}

/* jump pads: a flat coloured slab on the surface, always visible in the arch map */
static void draw_pads (void)
{
	int	i, k, axis, side;

	for (i = 0; i < ArchPads_Count (); i++)
	{
		const apad_t	*pad = ArchPads_Get (i);
		double		lo[3], hi[3];

		for (k = 0; k < 3; k++)
		{
			lo[k] = pad->mins[k];
			hi[k] = pad->maxs[k];
		}
		/* the slab is thinner than the trigger */
		if (pad->nsign > 0)
			hi[pad->naxis] = pad->plane + ARCH_PAD_THICK;
		else
			lo[pad->naxis] = pad->plane - ARCH_PAD_THICK;

		glBegin (GL_QUADS);
		for (axis = 0; axis < 3; axis++)
		{
			int a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;
			for (side = 0; side < 2; side++)
			{
				double v[3];
				/* the face looking away from the surface is the bright one */
				if (axis == pad->naxis && (side == 1) == (pad->nsign > 0))
					glColor3f (0.15f, 0.95f, 0.85f);
				else
					glColor3f (0.05f, 0.45f, 0.45f);
				v[axis] = side ? hi[axis] : lo[axis];
				v[a1] = lo[a1]; v[a2] = lo[a2]; glVertex3dv (v);
				v[a1] = hi[a1]; v[a2] = lo[a2]; glVertex3dv (v);
				v[a1] = hi[a1]; v[a2] = hi[a2]; glVertex3dv (v);
				v[a1] = lo[a1]; v[a2] = hi[a2]; glVertex3dv (v);
			}
		}
		/* a darker panel inside a bright rim, so the pad reads as an object and not a hole */
		{
			int	a1 = (pad->naxis + 1) % 3, a2 = (pad->naxis + 2) % 3;
			double	rim1 = (hi[a1] - lo[a1]) > 32 ? 8 : 2, rim2 = (hi[a2] - lo[a2]) > 32 ? 8 : 2;
			double	v[3];

			glColor3f (0.05f, 0.6f, 0.6f);
			v[pad->naxis] = pad->plane + pad->nsign * (ARCH_PAD_THICK + 0.25);
			v[a1] = lo[a1] + rim1; v[a2] = lo[a2] + rim2; glVertex3dv (v);
			v[a1] = hi[a1] - rim1; v[a2] = lo[a2] + rim2; glVertex3dv (v);
			v[a1] = hi[a1] - rim1; v[a2] = hi[a2] - rim2; glVertex3dv (v);
			v[a1] = lo[a1] + rim1; v[a2] = hi[a2] - rim2; glVertex3dv (v);
		}
		glEnd ();
	}
}

/* called after the world and entities are drawn */
void ArchBuild_Draw (void)
{
	static const float	cursor_rgb[3] = {1.0f, 0.85f, 0.3f}, pad_rgb[3] = {0.2f, 0.95f, 0.85f};
	avec3			outline[OUTLINE_MAX];
	qboolean		building = ArchBuild_Active ();
	int			n;

	if (!Arch_BrushWorld ())
		return;
	if (building)
		update_cursor ();
	if (!ArchPads_Count () && !(building && cursor_valid))
		return;

	if (GL_UseProgramFunc)	/* absent with -noglsl or an old driver */
		GL_UseProgramFunc (0);
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_CULL_FACE);

	draw_pads ();

	if (building && cursor_valid)
	{
		glEnable (GL_BLEND);
		glDepthMask (GL_FALSE);
		glLineWidth (2);

		if (gesture.dragging && gesture.depth && current_action != ARCH_ACTION_BUMPER)
			draw_preview (gesture.depth);
		/* pads are drawn solid and stand off the surface, so the cursor goes above them */
		n = cursor_outline (ArchPads_Count () ? ARCH_PAD_THICK + 0.75 : CURSOR_LIFT, outline);
		draw_outline (outline, n, current_action == ARCH_ACTION_BUMPER ? pad_rgb : cursor_rgb,
			      current_action == ARCH_ACTION_BUMPER ? 0.4f : 0.2f);

		glLineWidth (1);
		glDepthMask (GL_TRUE);
		glDisable (GL_BLEND);
	}

	if (gl_cull.value)
		glEnable (GL_CULL_FACE);
	glEnable (GL_TEXTURE_2D);
	glColor4f (1, 1, 1, 1);
}

#define WHEEL_RADIUS	110	/* in scaled pixels */
#define WHEEL_LABEL	64
#define WHEEL_PI	3.14159265358979323846

static void wheel_wedge (float cx, float cy, double from_deg, double to_deg, float radius)
{
	double	a;

	glBegin (GL_TRIANGLE_FAN);
	glVertex2f (cx, cy);
	for (a = from_deg; a <= to_deg + 0.01; a += 5)
		glVertex2f (cx + radius * (float) cos (a * WHEEL_PI / 180), cy - radius * (float) sin (a * WHEEL_PI / 180));
	glEnd ();
}

/* the action wheel: three segments around the screen centre */
static void draw_wheel (int scale)
{
	float	cx = glwidth / scale * 0.5f, cy = glheight / scale * 0.5f;
	int	highlight = ArchWheel_Highlight (&wheel), action, i;

	glDisable (GL_TEXTURE_2D);
	glDisable (GL_ALPHA_TEST);
	glEnable (GL_BLEND);

	glColor4f (0, 0, 0, 0.55f);
	wheel_wedge (cx, cy, 0, 360, WHEEL_RADIUS);
	if (highlight >= 0)
	{
		double mid = ArchWheel_SegmentAngle (highlight);
		glColor4f (1.0f, 0.85f, 0.3f, 0.35f);
		wheel_wedge (cx, cy, mid - 60, mid + 60, WHEEL_RADIUS);
	}
	glColor4f (1, 1, 1, 0.6f);
	glBegin (GL_LINES);
	for (i = 0; i < 3; i++)
	{
		double a = (30 + 120 * i) * WHEEL_PI / 180;
		glVertex2f (cx, cy);
		glVertex2f (cx + WHEEL_RADIUS * (float) cos (a), cy - WHEEL_RADIUS * (float) sin (a));
	}
	glEnd ();

	glColor4f (1, 1, 1, 1);
	glDisable (GL_BLEND);
	glEnable (GL_ALPHA_TEST);
	glEnable (GL_TEXTURE_2D);

	for (action = 0; action < ARCH_NUM_ACTIONS; action++)
	{
		char	label[32];
		double	a = ArchWheel_SegmentAngle (action) * WHEEL_PI / 180;
		int	len, x, y;

		/* the current action is bracketed; the highlighted one uses the alternate colour */
		q_snprintf (label, sizeof(label), action == current_action ? "[%s]" : "%s", ArchWheel_ActionName (action));
		len = (int) strlen (label);
		if (action == highlight)
			for (i = 0; i < len; i++)
				label[i] |= 0x80;
		x = (int) (cx + WHEEL_LABEL * cos (a)) - len * 4;
		y = (int) (cy - WHEEL_LABEL * sin (a)) - 4;
		Draw_String (x, y, label);
	}
}

/* one line near the top centre of the screen, and the wheel when it is open */
void ArchBuild_DrawHUD (void)
{
	char	w[16], h[16], d[16], line[112];
	int	scale, width = gesture.width, height = gesture.height;

	if (!ArchBuild_Active ())
		return;
	if (cursor_pinned && cursor_valid)
	{
		width = cursor.umax - cursor.umin;
		height = cursor.vmax - cursor.vmin;
	}
	ArchCursor_FormatMetres (width, w, sizeof(w));
	ArchCursor_FormatMetres (height, h, sizeof(h));
	q_snprintf (line, sizeof(line), "BUILD  %s  %s x %s m", ArchWheel_ActionName (current_action), w, h);
	if (gesture.dragging && current_action == ARCH_ACTION_BUMPER)
		q_strlcat (line, va ("  power %d", ArchPads_SpeedFromDrag (gesture.depth)), sizeof(line));
	else if (gesture.dragging && gesture.depth)
	{
		ArchCursor_FormatMetres (abs (gesture.depth), d, sizeof(d));
		q_strlcat (line, va ("  %s %s m", gesture.depth > 0 ? "carve" : "extrude", d), sizeof(line));
	}

	/* pixel canvas, with the 8-pixel font scaled up to stay readable at any resolution */
	scale = glheight / 360;
	if (scale < 1)
		scale = 1;
	GL_SetCanvas (CANVAS_DEFAULT);
	glPushMatrix ();
	glScalef ((float) scale, (float) scale, 1);
	Draw_String ((glwidth / scale - (int) strlen (line) * 8) / 2, 12, line);
	if (wheel.open)
		draw_wheel (scale);
	glPopMatrix ();
}

/* ------------------------------------------------------------ game flow */

/* the main menu's Build Mode entry */
static void ArchBuild_Session_f (void)
{
	build_session = true;
	intro_pending = true;
	intro_until = 0;
	/* run straight away, ahead of anything else queued */
	Cbuf_InsertText ("disconnect\nmaxplayers 1\ndeathmatch 0\ncoop 0\nmap " ARCH_MAPNAME "\n");
}

/* a game started from the menu (new game, load, multiplayer) is a normal game */
void ArchBuild_EndSession (void)
{
	build_session = false;
	intro_pending = false;
	intro_until = 0;
}

static const char *intro_text =
	"This is build mode. You can edit this map or load an existing map through the console, "
	"for example with 'map E1M1'. Maps cannot be saved in this version yet.";
static const char *intro_hint = "Press B to start building.";

/* the welcome text in the cube room, for 20 seconds or until build mode is entered */
void ArchBuild_DrawIntro (void)
{
	char		lines[8][96];
	const char	*s = intro_text;
	int		scale, width, n = 0, i, top, boxw;

	if (intro_pending)
	{
		if (!Arch_Active () || cls.signon != SIGNONS)
			return;
		intro_pending = false;
		intro_until = realtime + 20;
	}
	if (realtime > intro_until)
		return;
	if (!Arch_Active () || build_active || key_dest != key_game)
	{
		if (build_active || !Arch_Active ())
			intro_until = 0;
		return;
	}

	scale = glheight / 360;
	if (scale < 1)
		scale = 1;
	width = glwidth / scale / 8 - 6;
	if (width > 52)
		width = 52;
	/* word wrap */
	while (*s && n < 7)
	{
		int len = (int) strlen (s), cut = len;
		if (len > width)
		{
			for (cut = width; cut > 0 && s[cut] != ' '; cut--)
				;
			if (!cut)
				cut = width;
		}
		q_strlcpy (lines[n], s, cut + 1 < (int) sizeof(lines[n]) ? cut + 1 : (int) sizeof(lines[n]));
		n++;
		s += cut;
		while (*s == ' ')
			s++;
	}

	GL_SetCanvas (CANVAS_DEFAULT);
	glPushMatrix ();
	glScalef ((float) scale, (float) scale, 1);
	boxw = width * 8 + 32;
	top = glheight / scale / 2 + 24;
	Draw_Fill ((glwidth / scale - boxw) / 2, top - 12, boxw, (n + 2) * 12 + 16, 0, 0.6f);
	for (i = 0; i < n; i++)
		Draw_String ((glwidth / scale - (int) strlen (lines[i]) * 8) / 2, top + i * 12, lines[i]);
	{
		/* the hint in the alternate (gold) colour */
		char	hint[64];
		int	k;
		q_strlcpy (hint, intro_hint, sizeof(hint));
		for (k = 0; hint[k]; k++)
			hint[k] |= 0x80;
		Draw_String ((glwidth / scale - (int) strlen (hint) * 8) / 2, top + (n + 1) * 12, hint);
	}
	glPopMatrix ();
}

/* ------------------------------------------------------------------ init */

/* A config.cfg saved by an earlier version starts with "unbindall" and knows
   nothing of commands added since, so their default keys would be lost. When the
   arch map starts, any of these commands that has no key at all gets its default
   key back, unless that key is already used for something else. */
void ArchBuild_NewMap (void)
{
	static const struct { int key; const char *command; } defaults[] =
	{
		{'f', "fly"},
		{'b', "arch_build"},
		{'r', "arch_snap"},
	};
	int	i, k;

	for (i = 0; i < (int) (sizeof(defaults) / sizeof(defaults[0])); i++)
	{
		for (k = 0; k < MAX_KEYS; k++)
			if (keybindings[k] && !q_strcasecmp (keybindings[k], defaults[i].command))
				break;
		if (k == MAX_KEYS && (!keybindings[defaults[i].key] || !keybindings[defaults[i].key][0]))
			Key_SetBinding (defaults[i].key, defaults[i].command);
	}
}

void ArchBuild_Init (void)
{
	Cvar_RegisterVariable (&arch_drag_speed);
	Cmd_AddCommand ("arch_build", ArchBuild_Toggle_f);
	Cmd_AddCommand ("arch_buildmode", ArchBuild_Session_f);
	Cmd_AddCommand ("arch_action", ArchBuild_Action_f);
	Cmd_AddCommand ("arch_snap", ArchBuild_Snap_f);
	Cmd_AddCommand ("+arch_primary", ArchBuild_PrimaryDown_f);
	Cmd_AddCommand ("-arch_primary", ArchBuild_PrimaryUp_f);
	Cmd_AddCommand ("+arch_secondary", ArchBuild_SecondaryDown_f);
	Cmd_AddCommand ("-arch_secondary", ArchBuild_SecondaryUp_f);
	Cmd_AddCommand ("+arch_menu", ArchBuild_MenuDown_f);
	Cmd_AddCommand ("-arch_menu", ArchBuild_MenuUp_f);
	Cmd_AddCommand ("arch_wheel", ArchBuild_Wheel_f);
	Cmd_AddCommand ("arch_wheelmove", ArchBuild_WheelMove_f);
	Cmd_AddCommand ("arch_drag", ArchBuild_Drag_f);
	ArchGesture_Init (&gesture);
	gesture_ready = true;
}
