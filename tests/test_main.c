#include "t.h"

int t_checks, t_failures;

void test_brush (void);
void test_world (void);
void test_map (void);
void test_cursor (void);
void test_gesture (void);
void test_actions (void);
void test_wheel (void);
void test_pads (void);
void test_review3 (void);
void test_dig (void);
void test_campaign (void);

int main (void)
{
	test_brush ();
	test_world ();
	test_map ();
	test_cursor ();
	test_gesture ();
	test_actions ();
	test_wheel ();
	test_pads ();
	test_review3 ();
	test_dig ();
	test_campaign ();
	printf ("%d checks, %d failures\n", t_checks, t_failures);
	return t_failures != 0;
}
