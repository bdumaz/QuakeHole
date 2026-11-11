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

/* arch_stubbsp.c */
byte		*ArchStubBSP_Build (int *size);	/* minimal BSP file image in a static buffer */

#endif /* ARCH_H */
