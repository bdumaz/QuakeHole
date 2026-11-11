#include <string.h>
#include "t.h"
#include "arch_wheel.h"

void test_wheel (void)
{
	awheel_t w;

	ArchWheel_Open (&w);
	CHECK (w.open && ArchWheel_Highlight (&w) == -1);

	/* inside the dead zone nothing is highlighted, and closing changes nothing */
	ArchWheel_Move (&w, 0, ARCH_WHEEL_DEADZONE - 1);
	CHECK (ArchWheel_Highlight (&w) == -1);
	CHECK (ArchWheel_Close (&w) == -1 && !w.open);

	/* up is Extrude Quad */
	ArchWheel_Open (&w);
	ArchWheel_Move (&w, 0, 60);
	CHECK (ArchWheel_Highlight (&w) == ARCH_ACTION_QUAD);
	CHECK (ArchWheel_Close (&w) == ARCH_ACTION_QUAD);

	/* lower right is Extrude Arch */
	ArchWheel_Open (&w);
	ArchWheel_Move (&w, 60, -40);
	CHECK (ArchWheel_Highlight (&w) == ARCH_ACTION_ARCH);
	ArchWheel_Close (&w);

	/* lower left is Place Bumper */
	ArchWheel_Open (&w);
	ArchWheel_Move (&w, -60, -40);
	CHECK (ArchWheel_Highlight (&w) == ARCH_ACTION_BUMPER);
	ArchWheel_Close (&w);

	/* straight down sits on the boundary between the two lower segments: either is acceptable, never none */
	ArchWheel_Open (&w);
	ArchWheel_Move (&w, 0, -80);
	CHECK (ArchWheel_Highlight (&w) == ARCH_ACTION_ARCH || ArchWheel_Highlight (&w) == ARCH_ACTION_BUMPER);
	ArchWheel_Close (&w);

	/* segment edges: just above the horizontal on the right is still arch, well above is quad */
	ArchWheel_Open (&w);
	ArchWheel_Move (&w, 100, 20);		/* about 11 degrees */
	CHECK (ArchWheel_Highlight (&w) == ARCH_ACTION_ARCH);
	ArchWheel_Close (&w);
	ArchWheel_Open (&w);
	ArchWheel_Move (&w, 60, 60);		/* 45 degrees */
	CHECK (ArchWheel_Highlight (&w) == ARCH_ACTION_QUAD);
	ArchWheel_Close (&w);

	/* the pointer is held to a fixed radius, so reversing responds at once */
	ArchWheel_Open (&w);
	ArchWheel_Move (&w, 0, 5000);
	CHECK (ArchWheel_Highlight (&w) == ARCH_ACTION_QUAD);
	CHECK (w.y <= ARCH_WHEEL_RADIUS + 1e-9);
	ArchWheel_Move (&w, -200, -260);
	CHECK (ArchWheel_Highlight (&w) == ARCH_ACTION_BUMPER);
	ArchWheel_Close (&w);

	/* movement while closed is ignored; opening starts from the centre */
	ArchWheel_Move (&w, 500, 500);
	ArchWheel_Open (&w);
	CHECK (w.x == 0 && w.y == 0);
	ArchWheel_Close (&w);

	CHECK (!strcmp (ArchWheel_ActionName (ARCH_ACTION_QUAD), "Extrude Quad"));
	CHECK (!strcmp (ArchWheel_ActionName (ARCH_ACTION_ARCH), "Extrude Arch"));
	CHECK (!strcmp (ArchWheel_ActionName (ARCH_ACTION_BUMPER), "Place Bumper"));
}
