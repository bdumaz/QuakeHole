/*
QuakeArch - startup, cvars, console commands and player rules.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include "quakedef.h"
#include "arch.h"
#include "arch_world.h"
#include "arch_map.h"
#include "arch_pads.h"
#include "arch_dig.h"
#include "arch_bsp.h"

static cvar_t	arch_tex_ground = {"arch_tex_ground", "sfloor4_5", CVAR_ARCHIVE};
static cvar_t	arch_tex_wall = {"arch_tex_wall", "wbrick1_5", CVAR_ARCHIVE};
static cvar_t	arch_tex_ceiling = {"arch_tex_ceiling", "ceiling1_3", CVAR_ARCHIVE};

static cvar_t	arch_campaign = {"arch_campaign", "1", CVAR_ARCHIVE};
static qboolean	campaign_ready;		/* the loaded map has been converted to brushes */

qboolean Arch_Active (void)
{
	return sv.active && !q_strcasecmp (sv.modelname, ARCH_BSPNAME);
}

qboolean Arch_BrushWorld (void)
{
	return sv.active && (campaign_ready || !q_strcasecmp (sv.modelname, ARCH_BSPNAME));
}

static void insert_converted (abrush_t *brush)
{
	ArchWorld_Insert (brush);
}

/* Rebuilds a campaign map's solid space as brushes, from the tree the engine
   uses for point collision (hull 0). */
static void convert_campaign_map (void)
{
	hull_t		*hull = &sv.worldmodel->hulls[0];
	int		numnodes = hull->lastclipnode + 1, numplanes = sv.worldmodel->numplanes;
	abspnode_t	*nodes;
	abspplane_t	*planes;
	int		mins[3], maxs[3], i, k, count, dropped = 0;
	double		start = Sys_DoubleTime ();

	if (numnodes < 1 || numplanes < 1)
		return;
	nodes = (abspnode_t *) malloc (numnodes * sizeof(*nodes));
	planes = (abspplane_t *) malloc (numplanes * sizeof(*planes));
	for (i = 0; i < numnodes; i++)
	{
		nodes[i].planenum = hull->clipnodes[i].planenum;
		nodes[i].children[0] = hull->clipnodes[i].children[0];
		nodes[i].children[1] = hull->clipnodes[i].children[1];
	}
	for (i = 0; i < numplanes; i++)
	{
		for (k = 0; k < 3; k++)
			planes[i].normal[k] = hull->planes[i].normal[k];
		planes[i].dist = hull->planes[i].dist;
	}
	/* room around the map, so there is solid to dig into behind its outer walls */
	for (k = 0; k < 3; k++)
	{
		mins[k] = (int) floor (sv.worldmodel->mins[k]) - 256;
		maxs[k] = (int) ceil (sv.worldmodel->maxs[k]) + 256;
	}
	ArchWorld_BeginCustom (mins, maxs);
	count = ArchBsp_Convert (nodes, planes, hull->firstclipnode, mins, maxs, insert_converted, &dropped);
	free (nodes);
	free (planes);
	if (dropped || !count)
	{
		/* an incomplete conversion would have holes: keep the map's own collision */
		Con_Printf ("arch: %s could not be rebuilt as brushes (%d regions lost); using its own collision\n", sv.name, dropped);
		return;
	}
	campaign_ready = true;
	Con_Printf ("arch: %s rebuilt as %d brushes in %.0f ms\n", sv.name, count, (Sys_DoubleTime () - start) * 1000);
}

void Arch_ServerSpawn (void)
{
	campaign_ready = false;
	if (!q_strcasecmp (sv.modelname, ARCH_BSPNAME))
	{
		ArchWorld_Select (ARCH_WORLD_SANDBOX);	/* the sandbox level is kept as it was left */
		return;
	}
	ArchWorld_Select (ARCH_WORLD_CAMPAIGN);
	if (arch_campaign.value)
		convert_campaign_map ();
}

const char *Arch_Dir (void)
{
	static char	dir[MAX_OSPATH];
	static int	checked;
	int		i, len;

	if (!checked)
	{
		checked = 1;
		i = COM_CheckParm ("-archdir");
		if (i && i < com_argc - 1)
		{
			q_strlcpy (dir, com_argv[i + 1], sizeof(dir));
			for (len = 0; dir[len]; len++)
				if (dir[len] == '\\')
					dir[len] = '/';
			while (len > 1 && dir[len - 1] == '/')
				dir[--len] = 0;
		}
	}
	return dir[0] ? dir : NULL;
}

void Arch_TextureNames (const char *names[3])
{
	/* an empty name would write an invalid .map line */
	names[ARCH_SLOT_GROUND] = arch_tex_ground.string[0] ? arch_tex_ground.string : arch_tex_ground.default_string;
	names[ARCH_SLOT_WALL] = arch_tex_wall.string[0] ? arch_tex_wall.string : arch_tex_wall.default_string;
	names[ARCH_SLOT_CEILING] = arch_tex_ceiling.string[0] ? arch_tex_ceiling.string : arch_tex_ceiling.default_string;
}

/* ---------------------------------------------------------- player rules */

static cvar_t	arch_selfdamage = {"arch_selfdamage", "0", CVAR_ARCHIVE};
static cvar_t	arch_falldamage = {"arch_falldamage", "0", CVAR_ARCHIVE};
static cvar_t	arch_infiniteammo = {"arch_infiniteammo", "1", CVAR_ARCHIVE};

#define ARCH_ALL_WEAPONS	(IT_AXE | IT_SHOTGUN | IT_SUPER_SHOTGUN | IT_NAILGUN | IT_SUPER_NAILGUN | \
				 IT_GRENADE_LAUNCHER | IT_ROCKET_LAUNCHER | IT_LIGHTNING)

/* -------------------------------------------------------- weapon digging */

static cvar_t	arch_dig = {"arch_dig", "1", CVAR_ARCHIVE};
static cvar_t	arch_dig_radius = {"arch_dig_radius", "64", CVAR_ARCHIVE};
static cvar_t	arch_dig_time = {"arch_dig_time", "1", CVAR_ARCHIVE};

#define MAX_DIG_EVENTS	64

typedef struct { int kind; float v[6]; } digevent_t;

static adigparse_t	dig_parser;
static digevent_t	dig_events[MAX_DIG_EVENTS];
static int		num_dig_events;
static adwell_t		dig_dwell;
static int		dig_boxes[ARCH_DIG_MAX_BOXES][6];
static double		dig_refused_time = -10;

/* The game code reports explosions and beams from inside its own functions, in
   the middle of physics. They are only noted here and dug the next time the
   player's frame runs. */
static void note_dig_event (int kind, const float *v)
{
	if (kind == ARCH_DIG_NONE || !arch_dig.value || !Arch_BrushWorld () || num_dig_events == MAX_DIG_EVENTS)
		return;
	dig_events[num_dig_events].kind = kind;
	memcpy (dig_events[num_dig_events].v, v, sizeof(dig_events[0].v));
	num_dig_events++;
}

void ArchGame_WroteByte (int dest, int value)
{
	float	out[6];
	note_dig_event (ArchDigParse_Byte (&dig_parser, dest, value, out), out);
}

void ArchGame_WroteCoord (int dest, float value)
{
	float	out[6];
	note_dig_event (ArchDigParse_Coord (&dig_parser, dest, value, out), out);
}

void ArchGame_WroteEntity (int dest)
{
	float	out[6];
	note_dig_event (ArchDigParse_Entity (&dig_parser, dest, out), out);
}

/* an entity resting on ground that is no longer there must start falling again */
static void drop_unsupported_entities (void)
{
	int	i;

	for (i = svs.maxclients + 1; i < sv.num_edicts; i++)
	{
		edict_t	*e = EDICT_NUM (i);
		avec3	below;
		int	movetype = (int) e->v.movetype;

		if (e->free || !((int) e->v.flags & FL_ONGROUND))
			continue;
		if (movetype != MOVETYPE_TOSS && movetype != MOVETYPE_BOUNCE && movetype != MOVETYPE_STEP)
			continue;
		below[0] = e->v.origin[0];
		below[1] = e->v.origin[1];
		below[2] = e->v.origin[2] + e->v.mins[2] - 1;
		if (e->v.groundentity && PROG_TO_EDICT (e->v.groundentity) != sv.edicts)
			continue;	/* standing on a lift or a door, which digging never changes */
		if (!ArchWorld_PointSolid (below))
			e->v.flags = (float) ((int) e->v.flags & ~FL_ONGROUND);
	}
}

/* forget anything left over from before a map start: time may have gone backwards */
void ArchGame_NewMap (void)
{
	ArchDigParse_Reset (&dig_parser);
	ArchDig_DwellReset (&dig_dwell);
	num_dig_events = 0;
	dig_refused_time = -10;
}

/* removes one box of solid; false if the brush limit refused it. In a campaign
   map, look says what the inside of the hole should look like. */
static qboolean dig_box (const int mins[3], const int maxs[3], int look)
{
	abrush_t	*box = ArchBrush_Box (mins, maxs);
	int		ok, i, revision = ArchWorld_Revision ();

	if (!box)
		return true;
	for (i = 0; i < box->nfaces; i++)
		box->faces[i].plane.tag = look;
	ok = ArchWorld_Carve (box);
	ArchBrush_Free (box);
	if (ok && !Arch_Active () && ArchWorld_Revision () != revision)
		ArchSurf_Dig (mins, maxs);	/* the map's own surfaces lose what stood in front of this box */
	if (!ok && sv.time - dig_refused_time > 1)
	{
		dig_refused_time = sv.time;
		Con_Printf ("arch: brush limit reached, edit refused\n");
	}
	return ok != 0;
}

/* an explosion: remove the grid cubes around it */
static void dig_blast (const float *pos)
{
	avec3	centre;
	vec3_t	at, lo, hi;
	double	radius = arch_dig_radius.value;
	int	n, i, k, look = 0;

	if (radius < ARCH_DIG_CELL) radius = ARCH_DIG_CELL;
	if (radius > ARCH_DIG_MAX_RADIUS) radius = ARCH_DIG_MAX_RADIUS;
	for (k = 0; k < 3; k++)
	{
		centre[k] = at[k] = pos[k];
		lo[k] = pos[k] - (float) radius - ARCH_DIG_CELL;
		hi[k] = pos[k] + (float) radius + ARCH_DIG_CELL;
	}
	if (!Arch_Active ())
	{
		look = ArchSurf_LookFor (at);
		ArchSurf_NoteDig (lo, hi, look);
	}
	n = ArchDig_Crater (centre, radius, dig_boxes, ARCH_DIG_MAX_BOXES);
	for (i = 0; i < n; i++)
		if (!dig_box (dig_boxes[i], dig_boxes[i] + 3, look))
			break;
}

/* a lightning beam report: time how long it stays on one cube, and remove the cube when the time is up */
static void dig_beam (const float *v)
{
	avec3	beam_end, beam_dir;
	vec3_t	dir, end, centre;
	double	hold = arch_dig_time.value;
	int	cell[3], mins[3], maxs[3], k, revision, look = 0;

	if (hold < 0.1) hold = 0.1;
	if (hold > 10) hold = 10;
	for (k = 0; k < 3; k++)
	{
		dir[k] = v[3 + k] - v[k];
		end[k] = v[3 + k];
	}
	if (VectorNormalize (dir) < 1)
	{
		ArchDig_DwellReset (&dig_dwell);
		return;
	}
	for (k = 0; k < 3; k++)
	{
		beam_end[k] = end[k];
		beam_dir[k] = dir[k];
	}
	if (!ArchDig_BeamCell (beam_end, beam_dir, ArchWorld_PointSolid, cell))
	{
		ArchDig_DwellReset (&dig_dwell);	/* the beam ended in open air */
		return;
	}
	if (!ArchDig_Dwell (&dig_dwell, cell, sv.time, hold))
	{
		SV_StartParticle (end, vec3_origin, 111, 4);	/* sparks while it works */
		return;
	}
	ArchDig_CellBox (cell, mins, maxs);
	revision = ArchWorld_Revision ();
	if (!Arch_Active ())
	{
		vec3_t lo, hi;
		for (k = 0; k < 3; k++)
		{
			lo[k] = (float) mins[k];
			hi[k] = (float) maxs[k];
		}
		look = ArchSurf_LookFor (end);
		ArchSurf_NoteDig (lo, hi, look);
	}
	if (!dig_box (mins, maxs, look) || ArchWorld_Revision () == revision)
		return;		/* refused, or nothing there could be removed (sky, or the world's outer shell) */
	for (k = 0; k < 3; k++)
		centre[k] = (mins[k] + maxs[k]) * 0.5f;
	/* the cube bursts: grey chips and a brighter flash */
	SV_StartParticle (centre, vec3_origin, 4, 60);
	SV_StartParticle (centre, vec3_origin, 111, 30);
}

static void run_dig_events (void)
{
	int	i, revision = ArchWorld_Revision ();

	for (i = 0; i < num_dig_events; i++)
	{
		if (dig_events[i].kind == ARCH_DIG_BLAST)
			dig_blast (dig_events[i].v);
		else
			dig_beam (dig_events[i].v);
	}
	num_dig_events = 0;
	if (ArchWorld_Revision () != revision)
	{
		if (Arch_Active ())
		{
			ArchPads_Prune (ArchWorld_PointSolid);
			ArchBuild_WorldChanged ();
		}
		else
			ArchSurf_Commit ();
		drop_unsupported_entities ();
	}
}

/* jump pads: launch the player away from any pad they are touching */
static void touch_pads (edict_t *ent)
{
	avec3	absmin, absmax;
	double	velocity[3];
	int	k;

	if (!ArchPads_Count () || ent->v.health <= 0)
		return;
	for (k = 0; k < 3; k++)
	{
		absmin[k] = ent->v.origin[k] + ent->v.mins[k];
		absmax[k] = ent->v.origin[k] + ent->v.maxs[k];
		velocity[k] = ent->v.velocity[k];
	}
	if (!ArchPads_Touch (absmin, absmax, velocity))
		return;
	for (k = 0; k < 3; k++)
		ent->v.velocity[k] = (float) velocity[k];
	/* lift off: a player standing on a floor pad would otherwise stay grounded */
	ent->v.flags = (float) ((int) ent->v.flags & ~FL_ONGROUND);
}

void ArchGame_PlayerFrame (edict_t *ent)
{
	eval_t	*val;

	run_dig_events ();
	if (!Arch_Active ())
		return;		/* a campaign map: digging only, none of the sandbox's rules */
	ArchBuild_PlayerFrame (ent);
	if (!ArchBuild_Active ())
		touch_pads (ent);

	ent->v.items = (float) ((int) ent->v.items | ARCH_ALL_WEAPONS);

	if (arch_infiniteammo.value)
	{
		ent->v.ammo_shells = 100;
		ent->v.ammo_nails = 200;
		ent->v.ammo_rockets = 100;
		ent->v.ammo_cells = 100;

		/* the big HUD counter follows the weapon in hand */
		switch ((int) ent->v.weapon)
		{
		case IT_SHOTGUN:
		case IT_SUPER_SHOTGUN:		ent->v.currentammo = ent->v.ammo_shells; break;
		case IT_NAILGUN:
		case IT_SUPER_NAILGUN:		ent->v.currentammo = ent->v.ammo_nails; break;
		case IT_GRENADE_LAUNCHER:
		case IT_ROCKET_LAUNCHER:	ent->v.currentammo = ent->v.ammo_rockets; break;
		case IT_LIGHTNING:		ent->v.currentammo = ent->v.ammo_cells; break;
		default:			break;
		}
	}

	/* god mode blocks damage in the game code but still applies knockback, so rocket jumps work */
	if (arch_selfdamage.value)
		ent->v.flags = (float) ((int) ent->v.flags & ~FL_GODMODE);
	else
		ent->v.flags = (float) ((int) ent->v.flags | FL_GODMODE);

	/* the game code reads jump_flag in post-think to detect a hard landing */
	if (!arch_falldamage.value)
	{
		val = GetEdictFieldValue (ent, "jump_flag");
		if (val)
			val->_float = 0;
	}
}

/* ------------------------------------------------------------- commands */

/* parses "x1 y1 z1 x2 y2 z2" into a box brush; prints usage and returns NULL on bad input */
static abrush_t *box_from_args (void)
{
	int	c[6], mins[3], maxs[3], i;
	qboolean valid = Cmd_Argc () == 7;

	for (i = 0; i < 6 && valid; i++)
	{
		char	*end;
		long	v = strtol (Cmd_Argv (1 + i), &end, 10);

		if (end == Cmd_Argv (1 + i) || *end)
			valid = false;	/* not a whole number */
		/* nothing outside the build volume matters, and huge values would not make a valid brush */
		c[i] = v < -ARCH_HALF ? -ARCH_HALF : v > ARCH_HALF ? ARCH_HALF : (int) v;
	}
	if (valid && ArchWorld_BoxFromCorners (c, c + 3, mins, maxs))
	{
		abrush_t *box = ArchBrush_Box (mins, maxs);
		if (box)
			return box;
	}
	Con_Printf ("usage: %s x1 y1 z1 x2 y2 z2\n", Cmd_Argv (0));
	return NULL;
}

/* the editing commands only make sense in the sandbox map */
static qboolean in_sandbox (void)
{
	if (Arch_Active ())
		return true;
	Con_Printf ("arch: not in the arch map\n");
	return false;
}

static void Arch_Carve_f (void)
{
	abrush_t *box = in_sandbox () ? box_from_args () : NULL;

	if (!box)
		return;
	if (!ArchWorld_Carve (box))
		Con_Printf ("arch: brush limit reached, edit refused\n");
	ArchBrush_Free (box);
	ArchPads_Prune (ArchWorld_PointSolid);
	ArchBuild_WorldChanged ();
}

static void Arch_Add_f (void)
{
	abrush_t *box = in_sandbox () ? box_from_args () : NULL;

	if (!box)
		return;
	if (!ArchWorld_Add (box))
		Con_Printf ("arch: brush limit reached, edit refused\n");
	ArchBrush_Free (box);
	ArchPads_Prune (ArchWorld_PointSolid);
	ArchBuild_WorldChanged ();
}

static void Arch_Reset_f (void)
{
	if (!in_sandbox ())
		return;
	ArchWorld_Reset ();
	ArchPads_Clear ();
	ArchBuild_WorldChanged ();
}

static void Arch_Stats_f (void)
{
	if (Arch_Active () || !sv.active)
		Con_Printf ("arch: %d brushes, volume %.0f, %d pads\n", ArchWorld_BrushCount (), ArchWorld_SolidVolume (), ArchPads_Count ());
	else
		Con_Printf ("arch: this map as brushes: %d, volume %.0f\n", ArchWorld_BrushCount (), ArchWorld_SolidVolume ());
}

static void Arch_Export_f (void)
{
	const char	*names[3], *name;
	char		path[MAX_OSPATH];
	FILE		*f;
	int		ok;

	if (!in_sandbox ())
		return;
	name = Cmd_Argv (1);
	if (Cmd_Argc () != 2 || !*name || strstr (name, "..") || strchr (name, '/') || strchr (name, '\\') || strchr (name, ':'))
	{
		Con_Printf ("usage: arch_export name\n");
		return;
	}
	q_snprintf (path, sizeof(path), "%s/maps", com_gamedir);
	Sys_mkdir (path);
	q_snprintf (path, sizeof(path), "%s/maps/%s.map", com_gamedir, name);

	Arch_TextureNames (names);
	f = fopen (path, "w");
	ok = f && ArchMap_Write (f, names, arch_spawn_origin);
	if (f && fclose (f) != 0)
		ok = 0;
	if (ok)
		Con_Printf ("arch: wrote %s\n", path);
	else
		Con_Printf ("arch: could not write %s\n", path);
}

void Arch_Init (void)
{
	Cvar_RegisterVariable (&arch_tex_ground);
	Cvar_RegisterVariable (&arch_tex_wall);
	Cvar_RegisterVariable (&arch_tex_ceiling);
	Cvar_RegisterVariable (&arch_selfdamage);
	Cvar_RegisterVariable (&arch_falldamage);
	Cvar_RegisterVariable (&arch_infiniteammo);
	Cvar_RegisterVariable (&arch_campaign);
	Cvar_RegisterVariable (&arch_dig);
	Cvar_RegisterVariable (&arch_dig_radius);
	Cvar_RegisterVariable (&arch_dig_time);

	Cmd_AddCommand ("arch_carve", Arch_Carve_f);
	Cmd_AddCommand ("arch_add", Arch_Add_f);
	Cmd_AddCommand ("arch_reset", Arch_Reset_f);
	Cmd_AddCommand ("arch_stats", Arch_Stats_f);
	Cmd_AddCommand ("arch_export", Arch_Export_f);

	ArchBuild_Init ();

	/* built once: the level must survive map restarts and respawns */
	ArchWorld_Reset ();
}
