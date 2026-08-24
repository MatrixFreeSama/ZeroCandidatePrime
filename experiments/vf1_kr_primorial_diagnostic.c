#define _POSIX_C_SOURCE 200809L
#include <gmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/*
  DIAGNOSTIC ONLY. DO NOT LINK INTO THE GENERATOR.

  This file studies an exact finite-K algebraic compression of the survivor
  state. The prime fixture below intentionally uses a conventional sieve only
  to isolate the K/R representation experiment. It is not generation input for
  ZeroCandidatePrime and is not part of the Windows build.

      P_K = product_{j < K} q_j
      z   = p mod P_K

  For fixed finite K:

      exists j < K : q_j divides p+d
      iff gcd(z+d, P_K) > 1

  Therefore the finite-width survivor gap can be written as the scalar map

      J_K(z) = min { d >= 1 : gcd(z+d, P_K) = 1 }.

  This collapses K and the nested correction sequence algebraically, but the
  reference evaluator below still advances d one value at a time. Therefore it
  is NOT a successful zero-candidate implementation and must remain diagnostic.
*/

typedef uint64_t u64;

static double now_s(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + 1e-9 * (double)t.tv_nsec;
}

static size_t estimate_nth_bound(size_t k) {
    double n, b;
    if (k < 6) return 16;
    n = (double)k;
    b = n * (log(n) + log(log(n))) + 32.0;
    return (size_t)b + 1;
}

/* Traditional fixture generation for this isolated diagnostic only. */
static int first_k_primes(size_t k, u64 **out) {
    size_t bound, i, j, n;
    unsigned char *composite;
    u64 *q;
    if (k == 0 || !out) return 0;
    bound = estimate_nth_bound(k);
    for (;;) {
        composite = (unsigned char *)calloc(bound + 1, 1);
        q = (u64 *)malloc(k * sizeof(u64));
        if (!composite || !q) {
            free(composite);
            free(q);
            return 0;
        }
        n = 0;
        for (i = 2; i <= bound && n < k; ++i) {
            if (!composite[i]) {
                q[n++] = (u64)i;
                if (i <= bound / i) {
                    for (j = i * i; j <= bound; j += i) composite[j] = 1;
                }
            }
        }
        free(composite);
        if (n == k) {
            *out = q;
            return 1;
        }
        free(q);
        bound *= 2;
    }
}

/* Balanced product construction. This changes construction depth from a long
   left-associated multiply chain to a binary product tree. */
static void product_tree(mpz_t out, const u64 *q, size_t lo, size_t hi) {
    size_t mid;
    mpz_t a, b;
    if (hi - lo == 1) {
        mpz_set_ui(out, (unsigned long)q[lo]);
        return;
    }
    mid = lo + (hi - lo) / 2;
    mpz_inits(a, b, NULL);
    product_tree(a, q, lo, mid);
    product_tree(b, q, mid, hi);
    mpz_mul(out, a, b);
    mpz_clears(a, b, NULL);
}

/* Vector reference for the same finite K. It verifies the algebraic collapse;
   it is not a production path. */
static u64 vector_gap(const u64 *q, size_t k, const mpz_t p, u64 max_d) {
    size_t j;
    u64 d, m;
    u64 *r = (u64 *)malloc(k * sizeof(u64));
    if (!r) return 0;
    for (j = 0; j < k; ++j) r[j] = (u64)mpz_fdiv_ui(p, (unsigned long)q[j]);
    for (d = 1; d <= max_d; ++d) {
        int clean = 1;
        for (j = 0; j < k; ++j) {
            m = (r[j] + (d % q[j])) % q[j];
            if (m == 0) {
                clean = 0;
                break;
            }
        }
        if (clean) {
            free(r);
            return d;
        }
    }
    free(r);
    return 0;
}

/* Scalar evaluator of J_K. The d loop is intentionally visible so that an
   algebraic collapse cannot be mistaken for a causal-depth-1 implementation. */
static u64 collapsed_gcd_gap(const mpz_t primorial, const mpz_t z,
                             u64 max_d, u64 *gcd_calls) {
    u64 d;
    mpz_t x, g;
    mpz_inits(x, g, NULL);
    for (d = 1; d <= max_d; ++d) {
        mpz_add_ui(x, z, (unsigned long)d);
        if (mpz_cmp(x, primorial) >= 0) mpz_mod(x, x, primorial);
        mpz_gcd(g, x, primorial);
        ++(*gcd_calls);
        if (mpz_cmp_ui(g, 1) == 0) {
            mpz_clears(x, g, NULL);
            return d;
        }
    }
    mpz_clears(x, g, NULL);
    return 0;
}

int main(int argc, char **argv) {
    size_t k = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 256;
    unsigned exponent = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 10) : 100;
    u64 max_d = argc > 3 ? (u64)strtoull(argv[3], NULL, 10) : 4096;
    u64 *q = NULL, g_collapsed, g_vector, gcd_calls = 0;
    double t0, t_fixture, t_product, t_residue, t_collapsed, t_vector;
    mpz_t p, primorial, z;

    if (k == 0) return 2;
    mpz_inits(p, primorial, z, NULL);
    mpz_ui_pow_ui(p, 10, exponent);
    mpz_add_ui(p, p, 267);

    t0 = now_s();
    if (!first_k_primes(k, &q)) return 3;
    t_fixture = now_s() - t0;

    t0 = now_s();
    product_tree(primorial, q, 0, k);
    t_product = now_s() - t0;

    t0 = now_s();
    mpz_mod(z, p, primorial);
    t_residue = now_s() - t0;

    t0 = now_s();
    g_collapsed = collapsed_gcd_gap(primorial, z, max_d, &gcd_calls);
    t_collapsed = now_s() - t0;

    t0 = now_s();
    g_vector = vector_gap(q, k, p, max_d);
    t_vector = now_s() - t0;

    printf("K=%zu qmax=%llu exp10=%u P_bits=%zu P_digits=%zu\n",
           k, (unsigned long long)q[k - 1], exponent,
           mpz_sizeinbase(primorial, 2), mpz_sizeinbase(primorial, 10));
    printf("collapsed_gap=%llu vector_gap=%llu gcd_calls=%llu equal=%s\n",
           (unsigned long long)g_collapsed,
           (unsigned long long)g_vector,
           (unsigned long long)gcd_calls,
           g_collapsed == g_vector ? "yes" : "NO");
    printf("fixture_ms=%.3f product_ms=%.3f residue_us=%.3f collapsed_ms=%.3f vector_ms=%.3f\n",
           t_fixture * 1e3, t_product * 1e3, t_residue * 1e6,
           t_collapsed * 1e3, t_vector * 1e3);

    free(q);
    mpz_clears(p, primorial, z, NULL);
    return g_collapsed == g_vector ? 0 : 4;
}
