#ifndef TRADITIONAL_H
#define TRADITIONAL_H
#include "winmini.h"

int exact_is_prime_u64(UINT64 n);
int exact_next_prime_u64(UINT64 p, volatile int* cancel, UINT64* out);
int fast_bootstrap_nth(UINT64 n, volatile int* cancel, UINT64* p, int* used_primecount);

#endif
