#include "t.h"

int t_checks, t_failures;

void test_brush (void);
void test_world (void);
void test_map (void);

int main (void)
{
	test_brush ();
	test_world ();
	test_map ();
	printf ("%d checks, %d failures\n", t_checks, t_failures);
	return t_failures != 0;
}
