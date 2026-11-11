/*
QuakeArch - writes the brush world as standard Quake .map text.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#ifndef ARCH_MAP_H
#define ARCH_MAP_H

#include <stdio.h>
#include "arch_world.h"

/* Writes the whole world. texnames[slot] is the texture for that slot.
   Returns 1 on success, 0 on a write error. */
int ArchMap_Write (FILE *f, const char *const texnames[ARCH_NUM_SLOTS], const int spawn[3]);

#endif /* ARCH_MAP_H */
