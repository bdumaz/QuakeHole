/* Minimal test harness. */
#ifndef T_H
#define T_H
#include <stdio.h>
#include <math.h>

extern int t_checks, t_failures;

#define CHECK(c) do { t_checks++; if (!(c)) { t_failures++; \
	printf ("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); } } while (0)

#define CHECK_NEAR(a, b, eps) do { double t_a = (a), t_b = (b); t_checks++; \
	if (!(fabs (t_a - t_b) <= (eps))) { t_failures++; \
	printf ("%s:%d: %s = %.9g, expected %.9g\n", __FILE__, __LINE__, #a, t_a, t_b); } } while (0)

#endif
