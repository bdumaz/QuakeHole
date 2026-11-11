/*
QuakeHole - turns a map's collision tree into convex brushes.
No engine dependencies.

Every path from the root of the tree to a solid child passes a list of planes,
each with a side taken. The region that child stands for is exactly the space
on those sides of those planes, which is convex, so it is one brush.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#include <stdlib.h>
#include <string.h>
#include "arch_bsp.h"

#define MAX_DEPTH	1024

typedef struct
{
	const abspnode_t	*nodes;
	const abspplane_t	*planes;
	aplane_t		*stack;		/* the planes on the path so far, then room for the bounds */
	aplane_t		bounds[6];
	void			(*emit) (abrush_t *brush);
	int			count, dropped;
} convert_t;

static void emit_region (convert_t *c, int depth, int sky)
{
	abrush_t	*b;

	memcpy (&c->stack[depth], c->bounds, sizeof(c->bounds));
	b = ArchBrush_FromPlanes (c->stack, depth + 6);
	if (!b)
	{
		c->dropped++;	/* too thin for the brush code, or outside what it can represent */
		return;
	}
	b->locked = sky;	/* sky is solid for collision but is never dug */
	c->emit (b);
	c->count++;
}

static void walk (convert_t *c, int num, int depth)
{
	const abspnode_t	*node;
	const abspplane_t	*p;
	int			side, k;

	if (num < 0)
	{
		if (num == ARCH_BSP_SOLID || num == ARCH_BSP_SKY)
			emit_region (c, depth, num == ARCH_BSP_SKY);
		return;
	}
	if (depth >= MAX_DEPTH)
	{
		c->dropped++;	/* deeper than any real map; whatever is below is lost */
		return;
	}
	node = &c->nodes[num];
	p = &c->planes[node->planenum];
	for (side = 0; side < 2; side++)
	{
		aplane_t *pl = &c->stack[depth];

		/* On the normal's side the region is n.x >= d, so the brush's outward
		   normal there is -n. On the other side it is n.x <= d. */
		memset (pl, 0, sizeof(*pl));
		for (k = 0; k < 3; k++)
			pl->n[k] = side ? p->normal[k] : -(double) p->normal[k];
		pl->d = side ? p->dist : -(double) p->dist;
		walk (c, node->children[side], depth + 1);
	}
}

int ArchBsp_Convert (const abspnode_t *nodes, const abspplane_t *planes, int head,
		     const int mins[3], const int maxs[3], void (*emit) (abrush_t *brush), int *dropped)
{
	convert_t	c;
	int		k;

	memset (&c, 0, sizeof(c));
	c.nodes = nodes;
	c.planes = planes;
	c.emit = emit;
	c.stack = (aplane_t *) malloc ((MAX_DEPTH + 6) * sizeof(aplane_t));
	for (k = 0; k < 3; k++)
	{
		c.bounds[k * 2].n[k] = -1;
		c.bounds[k * 2].d = -(double) mins[k];
		c.bounds[k * 2 + 1].n[k] = 1;
		c.bounds[k * 2 + 1].d = maxs[k];
	}
	walk (&c, head, 0);
	free (c.stack);
	if (dropped)
		*dropped = c.dropped;
	return c.count;
}
