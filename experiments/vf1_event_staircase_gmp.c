#define _POSIX_C_SOURCE 200809L
#include <gmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef uint32_t u32;
typedef uint64_t u64;

typedef struct PrimeBlock {
    size_t lo, hi; /* [lo, hi) indices in the diagnostic prime fixture */
    mpz_t product;
} PrimeBlock;

static double now_s(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + 1e-9 * (double)t.tv_nsec;
}

/* DIAGNOSTIC ONLY. This conventional sieve is intentionally outside the
   generator. It exists to measure the staircase/event structure after the
   mathematical recurrence has been specified. */
static u32 *make_primes(u32 limit, size_t *count_out) {
    unsigned char *composite;
    u32 *primes;
    size_t cap = 1024, n = 0;
    u32 i;

    composite = (unsigned char *)calloc((size_t)limit + 1, 1);
    primes = (u32 *)malloc(cap * sizeof(u32));
    if (!composite || !primes) {
        free(composite);
        free(primes);
        return NULL;
    }

    for (i = 2; i <= limit; ++i) {
        if (!composite[i]) {
            if (n == cap) {
                u32 *grown;
                cap *= 2;
                grown = (u32 *)realloc(primes, cap * sizeof(u32));
                if (!grown) {
                    free(composite);
                    free(primes);
                    return NULL;
                }
                primes = grown;
            }
            primes[n++] = i;
            if ((u64)i * (u64)i <= (u64)limit) {
                u64 j;
                for (j = (u64)i * (u64)i; j <= (u64)limit; j += i)
                    composite[(size_t)j] = 1;
            }
        }
    }

    free(composite);
    *count_out = n;
    return primes;
}

static PrimeBlock *make_blocks(const u32 *primes, size_t n, size_t block_size,
                               size_t *block_count_out) {
    size_t block_count = (n + block_size - 1) / block_size;
    PrimeBlock *blocks = (PrimeBlock *)calloc(block_count, sizeof(*blocks));
    size_t b, i;

    if (!blocks)
        return NULL;

    for (b = 0; b < block_count; ++b) {
        blocks[b].lo = b * block_size;
        blocks[b].hi = (b + 1) * block_size;
        if (blocks[b].hi > n)
            blocks[b].hi = n;
        mpz_init_set_ui(blocks[b].product, 1);
        for (i = blocks[b].lo; i < blocks[b].hi; ++i)
            mpz_mul_ui(blocks[b].product, blocks[b].product, primes[i]);
    }

    *block_count_out = block_count;
    return blocks;
}

static void free_blocks(PrimeBlock *blocks, size_t block_count) {
    size_t b;
    if (!blocks)
        return;
    for (b = 0; b < block_count; ++b)
        mpz_clear(blocks[b].product);
    free(blocks);
}

static int survives_width(const u32 *primes, const u32 *residue,
                          size_t width, u64 d, u64 *tests) {
    size_t i;
    for (i = 0; i < width; ++i) {
        if (tests)
            ++*tests;
        if (((u64)residue[i] + (d % primes[i])) % primes[i] == 0)
            return 0;
    }
    return 1;
}

static u64 next_gap_after(const u32 *primes, const u32 *residue,
                          size_t width, u64 after, u64 *tests) {
    u64 d = after;
    for (;;) {
        ++d;
        if (survives_width(primes, residue, width, d, tests))
            return d;
    }
}

/* Find the first diagnostic dimension >= start that divides candidate.

   Full aligned blocks are rejected with one gcd(candidate, block_product).
   A hit block is then resolved locally. This demonstrates plateau skipping,
   but it is NOT an admissible VF-1 generator primitive: the block products are
   built from a conventional prime fixture and the gcd result is a batched
   factor-location oracle. The final project must not disguise this diagnostic
   as verification-free generation. */
static size_t next_collision(const u32 *primes, size_t prime_count,
                             const PrimeBlock *blocks, size_t block_count,
                             size_t block_size, size_t start,
                             const mpz_t candidate,
                             u64 *block_gcd_tests, u64 *local_prime_tests) {
    size_t i = start;
    mpz_t g;
    mpz_init(g);

    while (i < prime_count && (i % block_size) != 0) {
        if (local_prime_tests)
            ++*local_prime_tests;
        if (mpz_divisible_ui_p(candidate, primes[i])) {
            mpz_clear(g);
            return i;
        }
        ++i;
    }

    while (i < prime_count) {
        size_t b = i / block_size;
        size_t j;

        if (b >= block_count)
            break;

        if (blocks[b].lo != i || blocks[b].hi - blocks[b].lo < block_size) {
            for (j = i; j < blocks[b].hi; ++j) {
                if (local_prime_tests)
                    ++*local_prime_tests;
                if (mpz_divisible_ui_p(candidate, primes[j])) {
                    mpz_clear(g);
                    return j;
                }
            }
            i = blocks[b].hi;
            continue;
        }

        if (block_gcd_tests)
            ++*block_gcd_tests;
        mpz_gcd(g, candidate, blocks[b].product);
        if (mpz_cmp_ui(g, 1) != 0) {
            for (j = blocks[b].lo; j < blocks[b].hi; ++j) {
                if (local_prime_tests)
                    ++*local_prime_tests;
                if (mpz_divisible_ui_p(candidate, primes[j])) {
                    mpz_clear(g);
                    return j;
                }
            }
        }
        i = blocks[b].hi;
    }

    mpz_clear(g);
    return prime_count;
}

int main(int argc, char **argv) {
    unsigned exponent = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 10) : 100;
    u32 prime_limit = argc > 2 ? (u32)strtoul(argv[2], NULL, 10) : 3000000U;
    size_t block_size = argc > 3 ? (size_t)strtoull(argv[3], NULL, 10) : 512;
    unsigned add = argc > 4 ? (unsigned)strtoul(argv[4], NULL, 10) : 267;

    size_t prime_count = 0, block_count = 0, width = 1, events = 0;
    size_t previous_width = 1;
    u32 *primes, *residue;
    PrimeBlock *blocks;
    mpz_t p, candidate;
    u64 gap;
    u64 block_gcd_tests = 0, local_prime_tests = 0, survivor_tests = 0;
    double t0, sieve_s, blocks_s, residue_s, run_s;
    int postcheck;

    t0 = now_s();
    primes = make_primes(prime_limit, &prime_count);
    sieve_s = now_s() - t0;
    if (!primes || prime_count == 0) {
        fprintf(stderr, "diagnostic prime fixture failed\n");
        return 2;
    }

    t0 = now_s();
    blocks = make_blocks(primes, prime_count, block_size, &block_count);
    blocks_s = now_s() - t0;
    if (!blocks) {
        free(primes);
        return 2;
    }

    mpz_init(p);
    mpz_init(candidate);
    mpz_ui_pow_ui(p, 10, exponent);
    mpz_add_ui(p, p, add);

    residue = (u32 *)malloc(prime_count * sizeof(u32));
    if (!residue) {
        free_blocks(blocks, block_count);
        free(primes);
        mpz_clear(candidate);
        mpz_clear(p);
        return 2;
    }

    t0 = now_s();
    {
        size_t i;
        for (i = 0; i < prime_count; ++i)
            residue[i] = (u32)mpz_fdiv_ui(p, primes[i]);
    }
    residue_s = now_s() - t0;

    gap = next_gap_after(primes, residue, width, 0, &survivor_tests);
    printf("p=10^%u+%u prime_limit=%u primes=%zu block_size=%zu\n",
           exponent, add, prime_limit, prime_count, block_size);
    printf("initial K=%zu qmax=%u gap=%llu\n",
           width, primes[width - 1], (unsigned long long)gap);

    t0 = now_s();
    for (;;) {
        size_t j;
        u64 old_gap;

        mpz_set(candidate, p);
        mpz_add_ui(candidate, candidate, gap);

        j = next_collision(primes, prime_count, blocks, block_count,
                           block_size, width, candidate,
                           &block_gcd_tests, &local_prime_tests);
        if (j == prime_count)
            break;

        old_gap = gap;
        previous_width = width;
        width = j + 1;
        gap = next_gap_after(primes, residue, width, old_gap, &survivor_tests);
        ++events;

        printf("event=%zu K=%zu q=%u gap:%llu->%llu plateau_dimensions=%zu\n",
               events, width, primes[j],
               (unsigned long long)old_gap,
               (unsigned long long)gap,
               j - previous_width);
    }
    run_s = now_s() - t0;

    mpz_set(candidate, p);
    mpz_add_ui(candidate, candidate, gap);
    postcheck = mpz_probab_prime_p(candidate, 25); /* post-hoc only */

    printf("final_within_limit K=%zu qmax=%u gap=%llu postcheck=%s\n",
           width, primes[width - 1], (unsigned long long)gap,
           postcheck ? "probable-prime" : "composite");
    printf("event_count=%zu block_gcd_tests=%llu local_prime_tests=%llu survivor_tests=%llu\n",
           events,
           (unsigned long long)block_gcd_tests,
           (unsigned long long)local_prime_tests,
           (unsigned long long)survivor_tests);
    printf("sieve_ms=%.3f block_build_ms=%.3f residue_init_ms=%.3f event_run_ms=%.3f\n",
           sieve_s * 1e3, blocks_s * 1e3, residue_s * 1e3, run_s * 1e3);

    free(residue);
    free_blocks(blocks, block_count);
    free(primes);
    mpz_clear(candidate);
    mpz_clear(p);
    return 0;
}
