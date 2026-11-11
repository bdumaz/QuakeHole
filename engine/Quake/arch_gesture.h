/*
QuakeHole - the build-mode input gesture: what the left button, right button,
wheel and mouse movement mean together. Also holds the current cursor size.
No engine dependencies.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.
*/
#ifndef ARCH_GESTURE_H
#define ARCH_GESTURE_H

typedef enum { ARCH_ACT_NONE, ARCH_ACT_CLICK, ARCH_ACT_DRAG } aaction_t;

#define ARCH_CLICK_DEPTH	160	/* a plain click carves this deep */
#define ARCH_DRAG_THRESHOLD	24	/* mouse counts of forward/back movement that turn a press into a drag */
#define ARCH_DRAG_LIMIT		2048

typedef struct
{
	int	width, height;		/* cursor size; survives gestures and Cancel */
	int	primary, secondary;	/* buttons currently held */
	int	armed;			/* there was a cursor when primary went down */
	int	scrolled;		/* the wheel moved during this press: it is a scaling gesture */
	int	dragging;		/* the view is locked and depth is live */
	double	travel;			/* forward mouse counts since the press */
	double	origin;			/* travel at which the drag started; depth is measured from here */
	int	depth;			/* > 0 carve, < 0 extrude; multiple of 8 */
} agesture_t;

void      ArchGesture_Init (agesture_t *g);
void      ArchGesture_PrimaryDown (agesture_t *g, int cursor_valid);
aaction_t ArchGesture_PrimaryUp (agesture_t *g, int *depth);	/* what to do, and how deep */
void      ArchGesture_Secondary (agesture_t *g, int down);
void      ArchGesture_Wheel (agesture_t *g, int dir);		/* +1 grow, -1 shrink */
/* forward_counts > 0 is the mouse pushed away from the player. Returns 1 if this call started a drag. */
int       ArchGesture_Motion (agesture_t *g, double forward_counts, double speed);
void      ArchGesture_Cancel (agesture_t *g);			/* drops the gesture; a later release does nothing */

#endif /* ARCH_GESTURE_H */
