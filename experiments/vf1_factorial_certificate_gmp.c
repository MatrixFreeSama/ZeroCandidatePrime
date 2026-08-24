#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <gmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/*
  Diagnostic only.

  This experiment replaces an explicit prime-dimension fixture up to a bound B
  by the exact identity

      gcd(c, B!) > 1

  iff c has a prime divisor <= B.

  The modular factorial certificate is evaluated without materializing B!:

      Sigma_B(c) = B! mod c.

  Then

      gcd(c, Sigma_B(c)) = gcd(c, B!).

  The implementation uses a bounded-leaf divide-and-conquer product tree.  It
  does not build a prime table.  It is not an admissible final generator because
  the operation becomes a primality-style closure when B approaches sqrt(c),
  and the total work still grows with B.  The purpose is to isolate K-information
  collapse and payload size.
*/

typedef unsigned long long u64;

static double now_s(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + 1e-9 * (double)t.tv_nsec;
}

static void range_product_mod(mpz_t out, u64 lo, u64 hi, const mpz_t mod) {
    if (lo > hi) {
        mpz_set_ui(out, 1);
        return;
    }

    /* Constant-size leaf.  Reductions keep the live payload bounded. */
    if (hi - lo <= 255ULL) {
        unsigned batch = 0;
        mpz_set_ui(out, 1);
        for (u64 i = lo; i <= hi; ++i) {
            mpz_mul_ui(out, out, (unsigned long)i);
            if (++batch == 16U) {
                mpz_mod(out, out, mod);
                batch = 0;
            }
        }
        mpz_mod(out, out, mod);
        return;
    }

    {
        u64 mid = lo + (hi - lo) / 2ULL;
        mpz_t left, right;
        mpz_inits(left, right, NULL);
        range_product_mod(left, lo, mid, mod);
        range_product_mod(right, mid + 1ULL, hi, mod);
        mpz_mul(out, left, right);
        mpz_mod(out, out, mod);
        mpz_clears(left, right, NULL);
    }
}

static int factorial_collision_certificate(mpz_t sigma, const mpz_t c, u64 B) {
    mpz_t g;
    int hit;
    mpz_init(g);
    range_product_mod(sigma, 2ULL, B, c);
    mpz_gcd(g, c, sigma);
    hit = mpz_cmp_ui(g, 1UL) != 0;
#ifdef VF1_DEBUG_FACTOR
    gmp_printf("debug_gcd=%Zd\n", g);
#endif
    mpz_clear(g);
    return hit;
}

int main(int argc, char **argv) {
    u64 B = argc > 1 ? strtoull(argv[1], 0, 10) : 3000000ULL;
    unsigned gap = argc > 2 ? (unsigned)strtoul(argv[2], 0, 10) : 4U;
    mpz_t p, c, sigma;
    double t0, elapsed;
    int hit;

    mpz_inits(p, c, sigma, NULL);
    mpz_ui_pow_ui(p, 10, 100);
    mpz_add_ui(p, p, 267UL);
    mpz_set(c, p);
    mpz_add_ui(c, c, gap);

    t0 = now_s();
    hit = factorial_collision_certificate(sigma, c, B);
    elapsed = now_s() - t0;

    printf("B=%llu gap=%u collision=%d sigma_bits=%zu time_ms=%.3f\n",
           B, gap, hit, mpz_sizeinbase(sigma, 2), elapsed * 1e3);

    mpz_clears(p, c, sigma, NULL);
    return 0;
}
