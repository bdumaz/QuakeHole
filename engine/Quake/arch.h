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
qboolean	Arch_Active (void);	/* true while the server is running the arch (sandbox) map */
qboolean	Arch_BrushWorld (void);	/* true while world collision runs on brushes: the sandbox, or a converted campaign map */
void		Arch_ServerSpawn (void);	/* call when a server starts, once its world model is loaded */
const char	*Arch_Dir (void);	/* value of -archdir, forward slashes, no trailing slash; NULL if absent */
void		Arch_TextureNames (const char *names[3]);	/* indexed by ARCH_SLOT_* */
void		ArchGame_PlayerFrame (edict_t *ent);	/* run for each player just before its QuakeC post-think */
void		ArchGame_NewMap (void);			/* call when the arch map starts */
qboolean	ArchGame_CampaignEdit (void *shape, qboolean carve, vec3_t at);	/* shape is an abrush_t */
/* everything the game code writes to a network message passes through these, so that
   explosions and lightning beams can be noticed (weapon digging) */
void		ArchGame_WroteByte (int dest, int value);
void		ArchGame_WroteCoord (int dest, float value);
void		ArchGame_WroteEntity (int dest);
void		ArchGame_Particle (const float *org, int count);	/* the game code asked for a particle burst */

/* arch_build.c */
void		ArchBuild_Init (void);
void		ArchBuild_MapChanged (void);			/* a map is starting: leave build mode */
void		ArchBuild_WorldChanged (void);			/* call after any edit made outside build mode */
void		ArchBuild_NewMap (void);			/* restores default keys that have none */
qboolean	ArchBuild_Active (void);			/* true while in build mode */
void		ArchBuild_PlayerFrame (edict_t *ent);		/* server, once per frame for the player */
qboolean	ArchBuild_KeyEvent (int key, qboolean down);	/* true if build mode used the key */
qboolean	ArchBuild_MouseMove (int dx, int dy);		/* true if the movement must not turn the view */
void		ArchBuild_Draw (void);				/* cursor and preview, after the world */
void		ArchBuild_DrawHUD (void);

/* arch_surf.c: the drawing side of digging in campaign maps */
qboolean	ArchSurf_Dug (void);				/* the current map has been dug since it started */
qboolean	ArchSurf_Built (void);				/* solid has been added to it in build mode */
void		ArchSurf_NoteBuilt (void);
qboolean	ArchSurf_BrushCutsSky (const void *shape);	/* shape is an abrush_t */
void		ArchSurf_DigBrush (const void *shape);
void		ArchSurf_NewMap (void);				/* a map is starting: forget all cuts */
int		ArchSurf_LookFor (vec3_t pos);			/* texture and brightness for a dig at this point */
void		ArchSurf_NoteDig (const float mins[3], const float maxs[3], int look);
qboolean	ArchSurf_BoxCutsSky (const int mins[3], const int maxs[3]);	/* removing this box would open the sky */
void		ArchSurf_Dig (const int mins[3], const int maxs[3]);	/* cut the map's surfaces where this box of solid went */
void		ArchSurf_Commit (void);				/* after the last box of a dig */
int		ArchSurf_VertexCount (void);
unsigned int	ArchSurf_WriteVerts (float *varray, unsigned int index);
void		ArchSurf_Chain (void);
qboolean	ArchSurf_Hidden (const msurface_t *surf);
void		ArchSurf_DrawHoles (void);

/* arch_collide.c */
trace_t		ArchCollide_ClipMove (edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);
int		ArchCollide_PointContents (vec3_t p);	/* CONTENTS_SOLID or CONTENTS_EMPTY */

/* arch_render.c */
void		ArchRender_NewMap (void);	/* loads textures once, rebuilds every mesh */
void		ArchRender_Draw (void);

/* arch_stubbsp.c */
byte		*ArchStubBSP_Build (int *size);	/* minimal BSP file image in a static buffer */

#endif /* ARCH_H */
