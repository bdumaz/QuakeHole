#include "t.h"
#include "arch_gesture.h"

void test_gesture (void)
{
	agesture_t g;
	int depth;

	ArchGesture_Init (&g);
	CHECK (g.width == 160 && g.height == 160 && !g.dragging);

	/* press and release is a click at the default depth */
	depth = 0;
	ArchGesture_PrimaryDown (&g, 1);
	CHECK (ArchGesture_PrimaryUp (&g, &depth) == ARCH_ACT_CLICK);
	CHECK (depth == ARCH_CLICK_DEPTH);

	/* a release with no press does nothing */
	CHECK (ArchGesture_PrimaryUp (&g, &depth) == ARCH_ACT_NONE);

	/* no cursor at the press: nothing on release */
	ArchGesture_PrimaryDown (&g, 0);
	CHECK (ArchGesture_PrimaryUp (&g, &depth) == ARCH_ACT_NONE);

	/* wheel alone scales both */
	ArchGesture_Wheel (&g, 1);
	CHECK (g.width == 176 && g.height == 176);
	ArchGesture_Wheel (&g, -1);
	CHECK (g.width == 160 && g.height == 160);

	/* press, wheel, release: height only, and no action */
	ArchGesture_PrimaryDown (&g, 1);
	ArchGesture_Wheel (&g, 1);
	CHECK (g.width == 160 && g.height == 176);
	CHECK (ArchGesture_PrimaryUp (&g, &depth) == ARCH_ACT_NONE);

	/* press, right press, wheel: width only */
	ArchGesture_PrimaryDown (&g, 1);
	ArchGesture_Secondary (&g, 1);
	ArchGesture_Wheel (&g, 1);
	CHECK (g.width == 176 && g.height == 176);
	ArchGesture_Secondary (&g, 0);
	ArchGesture_Wheel (&g, -1);
	CHECK (g.width == 176 && g.height == 160);
	CHECK (ArchGesture_PrimaryUp (&g, &depth) == ARCH_ACT_NONE);

	/* scaling with the button held works even without a cursor */
	ArchGesture_PrimaryDown (&g, 0);
	ArchGesture_Wheel (&g, 1);
	CHECK (g.height == 176);
	ArchGesture_PrimaryUp (&g, &depth);

	/* right alone, then wheel: same as wheel alone */
	ArchGesture_Init (&g);
	ArchGesture_Secondary (&g, 1);
	ArchGesture_Wheel (&g, 1);
	CHECK (g.width == 176 && g.height == 176);
	ArchGesture_Secondary (&g, 0);

	/* just under the threshold is still a click */
	ArchGesture_Init (&g);
	ArchGesture_PrimaryDown (&g, 1);
	CHECK (ArchGesture_Motion (&g, ARCH_DRAG_THRESHOLD - 1, 1.0) == 0);
	CHECK (!g.dragging);
	CHECK (ArchGesture_PrimaryUp (&g, &depth) == ARCH_ACT_CLICK);

	/* reaching the threshold starts a drag and locks the view; depth counts from there */
	ArchGesture_PrimaryDown (&g, 1);
	CHECK (ArchGesture_Motion (&g, ARCH_DRAG_THRESHOLD - 3, 1.0) == 0);
	CHECK (ArchGesture_Motion (&g, 3, 1.0) == 1);
	CHECK (g.dragging && g.depth == 0);
	CHECK (ArchGesture_Motion (&g, 64, 1.0) == 0);	/* already dragging */
	CHECK (g.depth == 64);
	ArchGesture_Motion (&g, 7, 1.0);		/* 71 counts: rounds to 72 */
	CHECK (g.depth == 72);
	ArchGesture_Wheel (&g, 1);			/* ignored while dragging */
	CHECK (g.width == 160 && g.height == 160);
	depth = 0;
	CHECK (ArchGesture_PrimaryUp (&g, &depth) == ARCH_ACT_DRAG);
	CHECK (depth == 72 && !g.dragging);

	/* backward is negative depth: an extrusion */
	ArchGesture_PrimaryDown (&g, 1);
	ArchGesture_Motion (&g, -(ARCH_DRAG_THRESHOLD + 64), 1.0);
	CHECK (g.dragging && g.depth == -64);
	CHECK (ArchGesture_PrimaryUp (&g, &depth) == ARCH_ACT_DRAG && depth == -64);

	/* speed scales the depth, not the threshold */
	ArchGesture_PrimaryDown (&g, 1);
	ArchGesture_Motion (&g, ARCH_DRAG_THRESHOLD + 16, 2.0);
	CHECK (g.dragging && g.depth == 32);
	ArchGesture_PrimaryUp (&g, &depth);

	/* the limit */
	ArchGesture_PrimaryDown (&g, 1);
	ArchGesture_Motion (&g, 100000, 1.0);
	CHECK (g.depth == ARCH_DRAG_LIMIT);
	ArchGesture_Motion (&g, -300000, 1.0);
	CHECK (g.depth == -ARCH_DRAG_LIMIT);
	ArchGesture_PrimaryUp (&g, &depth);

	/* back to zero: no action, but it was a drag, not a click */
	ArchGesture_PrimaryDown (&g, 1);
	ArchGesture_Motion (&g, ARCH_DRAG_THRESHOLD + 40, 1.0);
	ArchGesture_Motion (&g, -40, 1.0);
	CHECK (g.dragging && g.depth == 0);
	ArchGesture_Motion (&g, -16, 1.0);	/* once dragging there is no dead zone: it crosses straight into an extrusion */
	CHECK (g.depth == -16);
	ArchGesture_Motion (&g, 16, 1.0);
	CHECK (g.depth == 0);
	CHECK (ArchGesture_PrimaryUp (&g, &depth) == ARCH_ACT_NONE);

	/* a scroll before the threshold prevents a drag for the rest of the gesture */
	ArchGesture_PrimaryDown (&g, 1);
	ArchGesture_Motion (&g, 4, 1.0);
	ArchGesture_Wheel (&g, 1);
	CHECK (ArchGesture_Motion (&g, 100, 1.0) == 0);
	CHECK (!g.dragging);
	CHECK (ArchGesture_PrimaryUp (&g, &depth) == ARCH_ACT_NONE);

	/* no cursor: motion never starts a drag */
	ArchGesture_PrimaryDown (&g, 0);
	CHECK (ArchGesture_Motion (&g, 100, 1.0) == 0 && !g.dragging);
	ArchGesture_PrimaryUp (&g, &depth);

	/* motion without the button does nothing */
	CHECK (ArchGesture_Motion (&g, 100, 1.0) == 0 && !g.dragging);

	/* cancel during a drag: unlocked, and the later release does nothing */
	ArchGesture_Init (&g);
	ArchGesture_PrimaryDown (&g, 1);
	ArchGesture_Motion (&g, ARCH_DRAG_THRESHOLD + 64, 1.0);
	ArchGesture_Cancel (&g);
	CHECK (!g.dragging && g.depth == 0);
	CHECK (ArchGesture_PrimaryUp (&g, &depth) == ARCH_ACT_NONE);
	CHECK (g.width == 160 && g.height == 160);	/* cancel keeps the size */
}
