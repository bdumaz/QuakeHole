#include "t.h"

int t_checks, t_failures;

void test_brush (void);
void test_world (void);

int main (void)
{
	test_brush ();
	test_world ();
	printf ("%d checks, %d failures\n", t_checks, t_failures);
	return t_failures != 0;
}
