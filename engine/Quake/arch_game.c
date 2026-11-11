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

static cvar_t	arch_tex_ground = {"arch_tex_ground", "sfloor4_5", CVAR_ARCHIVE};
static cvar_t	arch_tex_wall = {"arch_tex_wall", "wbrick1_5", CVAR_ARCHIVE};
static cvar_t	arch_tex_ceiling = {"arch_tex_ceiling", "ceiling1_3", CVAR_ARCHIVE};

qboolean Arch_Active (void)
{
	return sv.active && !q_strcasecmp (sv.modelname, ARCH_BSPNAME);
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

void ArchGame_PlayerFrame (edict_t *ent)
{
	eval_t	*val;

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

static void Arch_Carve_f (void)
{
	abrush_t *box = box_from_args ();

	if (!box)
		return;
	if (!ArchWorld_Carve (box))
		Con_Printf ("arch: brush limit reached, edit refused\n");
	ArchBrush_Free (box);
}

static void Arch_Add_f (void)
{
	abrush_t *box = box_from_args ();

	if (!box)
		return;
	if (!ArchWorld_Add (box))
		Con_Printf ("arch: brush limit reached, edit refused\n");
	ArchBrush_Free (box);
}

static void Arch_Reset_f (void)
{
	ArchWorld_Reset ();
}

static void Arch_Stats_f (void)
{
	Con_Printf ("arch: %d brushes, volume %.0f\n", ArchWorld_BrushCount (), ArchWorld_SolidVolume ());
}

static void Arch_Export_f (void)
{
	const char	*names[3], *name;
	char		path[MAX_OSPATH];
	FILE		*f;
	int		ok;

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

	Cmd_AddCommand ("arch_carve", Arch_Carve_f);
	Cmd_AddCommand ("arch_add", Arch_Add_f);
	Cmd_AddCommand ("arch_reset", Arch_Reset_f);
	Cmd_AddCommand ("arch_stats", Arch_Stats_f);
	Cmd_AddCommand ("arch_export", Arch_Export_f);

	/* built once: the level must survive map restarts and respawns */
	ArchWorld_Reset ();
}
