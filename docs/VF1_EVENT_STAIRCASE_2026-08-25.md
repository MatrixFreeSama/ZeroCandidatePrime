# VF-1 Event Staircase Diagnostic, 2026-08-25

## Status

This document studies the observed plateau-and-jump structure of the finite-width survivor output. It does not claim an exact verification-free next-prime generator and does not claim causal depth 1.

The diagnostic code is `experiments/vf1_event_staircase_gmp.c`. It deliberately uses a conventional prime fixture and batched GCD products to measure the event structure. Those mechanisms are diagnostic only and are not admissible as the final ZeroCandidatePrime generation core.

## 1. Finite-width staircase

Let

\[
P_K=\prod_{j=0}^{K-1}q_j
\]

and define

\[
g_K(p)=\min\{d\ge1:\gcd(p+d,P_K)=1\}.
\]

For increasing dimension width,

\[
g_{K+1}(p)\ge g_K(p).
\]

Therefore `g_K` is a monotone nondecreasing staircase in `K`.

The plateau rule is exact:

\[
q_K\nmid p+g_K\quad\Longrightarrow\quad g_{K+1}=g_K.
\]

A jump can occur only when the newly admitted dimension hits the current survivor:

\[
q_K\mid p+g_K.
\]

If the dimension sequence is the consecutive-prime sequence and the current survivor `c=p+g_K` is composite, then the next jump dimension is the index of the least prime factor of `c` that is not already active.

This statement is useful as a post-hoc characterization. It is not an admissible generation rule because directly computing that least prime factor would insert factor location into the generator.

## 2. Event representation

Instead of treating every dimension as a causal event, define the jump set

\[
\mathcal E(p)=\{K:g_K(p)>g_{K-1}(p)\}.
\]

Let its ordered elements be

\[
K_1<K_2<\cdots.
\]

Then the finite-width evolution can be written as

\[
(K_0,g_0)\to(K_1,g_1)\to(K_2,g_2)\to\cdots
\]

with all dimensions between `K_t` and `K_(t+1)` belonging to a plateau and therefore producing no change in the survivor gap.

This absorbs the distinction between an explicit dimension scan and a survivor correction round into one event sequence. The new diagnostic quantity is

\[
E(p;K_{max})=|\mathcal E(p)\cap[1,K_{max}]|.
\]

A small `E` does not prove constant causal depth, but it is the appropriate quantity for testing jump-style collapse.

## 3. Target case near 10^100

The target input is

```text
p = 10^100 + 267
```

which was established independently before the experiment as prime. Traditional information is not fed into the recurrence during the finite-width measurements.

The staircase observed through the first 175,692 prime dimensions is:

| active width K | new collision q | gap before | gap after |
|---:|---:|---:|---:|
| 1 | 2 | start | 2 |
| 2 | 3 | 2 | 4 |
| 88,231 | 1,134,709 | 4 | 6 |
| 175,692 | 2,391,019 | 6 | 22 |

Thus the two large plateaus are approximately

```text
g = 4 : K = 2 ... 88,230
g = 6 : K = 88,231 ... 175,691
```

and after the collision at `q=2,391,019` the finite-width survivor becomes `g=22`.

Scanning the diagnostic prime fixture further to `3,000,000` finds no additional jump. The state `p+22` remains composite under an independent post-check, so the next event lies beyond this diagnostic limit.

The independently established exact successor gap for this input remains

\[
g_{exact}=682.
\]

Consequently the sparse event staircase is real, but the observed event sequence has not yet reached exact closure.

## 4. Batched plateau skipping experiment

The diagnostic groups consecutive prime dimensions into blocks. For a current composite survivor `c`, a block product is

\[
B=\prod_{q_j\in\text{block}}q_j.
\]

One test

\[
\gcd(c,B)=1
\]

rejects the entire block as collision-free. A nontrivial GCD marks a hit block, which the diagnostic then resolves locally.

With prime limit `3,000,000`, there are 216,816 prime dimensions. Using block size 512 on the cloud CPU produced the three jump events above while using approximately:

```text
422 block GCD tests
1775 local prime tests
```

for collision location. The event-run portion was about `3.2 ms` on the test machine after the conventional diagnostic fixture had been built.

This demonstrates that long K plateaus can be skipped in large chunks once block products exist.

However this is not a valid VF-1 solution. The block products encode an explicit conventional prime basis, and a nontrivial block GCD is a batched factor-location oracle. The experiment is therefore evidence for the staircase geometry, not a replacement generator.

## 5. R collapse and the remaining obstruction

The previous K/R primorial formulation defines

\[
J_K(z)=\min\{d\ge1:\gcd(z+d,P_K)=1\}.
\]

This absorbs all finite-K survivor correction rounds into one mathematical operator. The event staircase now adds a second reduction: only those K values that invalidate the current `J_K` result are structurally active.

The combined idealized event operator is

\[
\mathcal T_p:(K_t,g_t)\mapsto(K_{t+1},g_{t+1}).
\]

A genuine jump-style VF-1 collapse would require computing this map without:

- enumerating the skipped dimensions,
- locating a prime factor of the current survivor,
- materializing a candidate interval,
- using a precomputed prime or wheel table,
- or feeding a primality/factorization/next-prime result into generation.

The current code does not yet have such an operator.

## 6. Main conclusion

The experiments support the following structural statement:

> Finite-width survivor evolution is sparse in K: most newly admitted dimensions are null events, and the output changes only at collision dimensions. The observed trajectory is therefore better represented as a jump staircase than as a uniform level-by-level evolution.

The unresolved problem is no longer merely to make `K` larger. It is to predict or absorb the next collision event without computing the hidden least-prime-factor information that characterizes that event after the fact.
