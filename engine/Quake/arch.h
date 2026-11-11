/*
QuakeArch - engine-facing declarations. Include after quakedef.h.
(Not related to arch_def.h, which is QuakeSpasm's CPU architecture header.)

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#ifndef ARCH_H
#define ARCH_H

#define ARCH_MAPNAME	"arch"
#define ARCH_BSPNAME	"maps/arch.bsp"

extern const int arch_spawn_origin[3];	/* player start: feet on the room floor */

/* arch_game.c */
void		Arch_Init (void);	/* builds the world, registers cvars and commands */
qboolean	Arch_Active (void);	/* true while the server is running the arch map */
const char	*Arch_Dir (void);	/* value of -archdir, forward slashes, no trailing slash; NULL if absent */
void		Arch_TextureNames (const char *names[3]);	/* indexed by ARCH_SLOT_* */
void		ArchGame_PlayerFrame (edict_t *ent);	/* run for each player just before its QuakeC post-think */
void		ArchGame_NewMap (void);			/* call when the arch map starts */
/* everything the game code writes to a network message passes through these, so that
   explosions and lightning beams can be noticed (weapon digging) */
void		ArchGame_WroteByte (int dest, int value);
void		ArchGame_WroteCoord (int dest, float value);
void		ArchGame_WroteEntity (int dest);

/* arch_build.c */
void		ArchBuild_Init (void);
void		ArchBuild_WorldChanged (void);			/* call after any edit made outside build mode */
void		ArchBuild_NewMap (void);			/* restores default keys that have none */
qboolean	ArchBuild_Active (void);			/* true while in build mode */
void		ArchBuild_PlayerFrame (edict_t *ent);		/* server, once per frame for the player */
qboolean	ArchBuild_KeyEvent (int key, qboolean down);	/* true if build mode used the key */
qboolean	ArchBuild_MouseMove (int dx, int dy);		/* true if the movement must not turn the view */
void		ArchBuild_Draw (void);				/* cursor and preview, after the world */
void		ArchBuild_DrawHUD (void);

/* arch_collide.c */
trace_t		ArchCollide_ClipMove (edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);
int		ArchCollide_PointContents (vec3_t p);	/* CONTENTS_SOLID or CONTENTS_EMPTY */

/* arch_render.c */
void		ArchRender_NewMap (void);	/* loads textures once, rebuilds every mesh */
void		ArchRender_Draw (void);

/* arch_stubbsp.c */
byte		*ArchStubBSP_Build (int *size);	/* minimal BSP file image in a static buffer */

#endif /* ARCH_H */
