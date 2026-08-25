#define _POSIX_C_SOURCE 200809L
#include <gmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
  Diagnostic only.

  This experiment compresses a finite active dimension set into a bounded
  forbidden-offset mask over d=1..W:

      F_I(d)=1 iff some q_j in block I divides p+d.

  Block composition is bitwise OR.  The first zero bit is exactly the finite-K
  survivor gap whenever the requested gap lies inside the window.

  The experiment deliberately uses a conventional prime fixture and a bounded
  offset window, so it is NOT admissible as the final ZeroCandidatePrime core.
*/

typedef uint64_t u64;

static double now_s(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + 1e-9 * (double)t.tv_nsec;
}

static int sieve_primes(uint32_t n, uint32_t **out, size_t *count) {
    uint8_t *composite = (uint8_t*)calloc((size_t)n + 1u, 1u);
    uint32_t *primes;
    size_t c = 0, k = 0;
    if (!composite) return 0;

    for (uint32_t i = 2; i <= n; ++i) {
        if (!composite[i]) {
            ++c;
            if ((uint64_t)i * (uint64_t)i <= n) {
                for (uint32_t j = i * i; j <= n; j += i) composite[j] = 1;
            }
        }
    }

    primes = (uint32_t*)malloc(c * sizeof(uint32_t));
    if (!primes) { free(composite); return 0; }
    for (uint32_t i = 2; i <= n; ++i) if (!composite[i]) primes[k++] = i;
    free(composite);
    *out = primes;
    *count = c;
    return 1;
}

static inline void set_bit(u64 *mask, size_t d) {
    mask[(d - 1u) >> 6] |= 1ULL << ((d - 1u) & 63u);
}

static size_t first_zero(const u64 *mask, size_t W) {
    size_t words = (W + 63u) >> 6;
    for (size_t w = 0; w < words; ++w) {
        u64 x = ~mask[w];
        if (w + 1u == words && (W & 63u)) x &= (1ULL << (W & 63u)) - 1ULL;
        if (x) return (w << 6) + (size_t)__builtin_ctzll(x) + 1u;
    }
    return 0;
}

static void dimension_into_mask(u64 *mask, size_t W, const mpz_t p, uint32_t q) {
    unsigned long r = mpz_fdiv_ui(p, q);
    u64 d = ((u64)q - (u64)r) % (u64)q;
    if (d == 0) d = q;
    for (u64 x = d; x <= (u64)W; x += q) set_bit(mask, (size_t)x);
}

static void build_flat(u64 *mask, size_t W, const mpz_t p,
                       const uint32_t *primes, size_t count) {
    size_t words = (W + 63u) >> 6;
    memset(mask, 0, words * sizeof(u64));
    for (size_t i = 0; i < count; ++i) dimension_into_mask(mask, W, p, primes[i]);
}

static size_t compare_shift_prefix(const u64 *old_mask, const u64 *new_mask,
                                   size_t W, size_t g) {
    size_t mismatch = 0;
    for (size_t d = 1; d + g <= W; ++d) {
        size_t aidx = d + g - 1u;
        size_t bidx = d - 1u;
        int a = (int)((old_mask[aidx >> 6] >> (aidx & 63u)) & 1ULL);
        int b = (int)((new_mask[bidx >> 6] >> (bidx & 63u)) & 1ULL);
        if (a != b) ++mismatch;
    }
    return mismatch;
}

int main(int argc, char **argv) {
    uint32_t prime_limit = argc > 1 ? (uint32_t)strtoul(argv[1], 0, 10) : 3000000u;
    size_t W = argc > 2 ? (size_t)strtoull(argv[2], 0, 10) : 4096u;
    size_t block_size = argc > 3 ? (size_t)strtoull(argv[3], 0, 10) : 512u;
    uint32_t *primes = NULL;
    size_t prime_count = 0;
    size_t words, blocks;
    u64 *block_masks, *root, *flat, *shifted_reference;
    mpz_t p, p2;
    double t0, sieve_s, leaf_s, reduce_s, flat_s;
    size_t gap, mismatch;

    t0 = now_s();
    if (!sieve_primes(prime_limit, &primes, &prime_count)) return 2;
    sieve_s = now_s() - t0;

    words = (W + 63u) >> 6;
    blocks = (prime_count + block_size - 1u) / block_size;
    block_masks = (u64*)calloc(blocks * words, sizeof(u64));
    root = (u64*)calloc(words, sizeof(u64));
    flat = (u64*)calloc(words, sizeof(u64));
    shifted_reference = (u64*)calloc(words, sizeof(u64));
    if (!block_masks || !root || !flat || !shifted_reference) return 3;

    mpz_init(p);
    mpz_ui_pow_ui(p, 10, 100);
    mpz_add_ui(p, p, 267);
    mpz_init(p2);

    t0 = now_s();
    for (size_t b = 0; b < blocks; ++b) {
        size_t begin = b * block_size;
        size_t end = begin + block_size;
        u64 *m = block_masks + b * words;
        if (end > prime_count) end = prime_count;
        for (size_t i = begin; i < end; ++i) dimension_into_mask(m, W, p, primes[i]);
    }
    leaf_s = now_s() - t0;

    t0 = now_s();
    for (size_t b = 0; b < blocks; ++b) {
        const u64 *m = block_masks + b * words;
        for (size_t w = 0; w < words; ++w) root[w] |= m[w];
    }
    reduce_s = now_s() - t0;

    gap = first_zero(root, W);

    t0 = now_s();
    build_flat(flat, W, p, primes, prime_count);
    flat_s = now_s() - t0;

    mpz_set(p2, p);
    mpz_add_ui(p2, p2, gap);
    build_flat(shifted_reference, W, p2, primes, prime_count);
    mismatch = compare_shift_prefix(root, shifted_reference, W, gap);

    printf("prime_limit=%u primes=%zu W=%zu blocks=%zu root_bytes=%zu gap=%zu\n",
           prime_limit, prime_count, W, blocks, words * sizeof(u64), gap);
    printf("sieve_ms=%.3f block_leaf_ms=%.3f or_reduce_us=%.3f flat_ms=%.3f equal=%d\n",
           sieve_s * 1e3, leaf_s * 1e3, reduce_s * 1e6, flat_s * 1e3,
           memcmp(root, flat, words * sizeof(u64)) == 0);
    printf("translation_shift_prefix_mismatch=%zu\n", mismatch);

    free(shifted_reference);
    free(flat);
    free(root);
    free(block_masks);
    free(primes);
    mpz_clear(p2);
    mpz_clear(p);
    return 0;
}
