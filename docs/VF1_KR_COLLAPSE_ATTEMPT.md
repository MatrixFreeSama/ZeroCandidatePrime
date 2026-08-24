# VF-1 K/R Collapse Attempt

## Status

This note records a finite-width algebraic collapse experiment. It is not a proof of VF-1 and it is not a production generator path.

The experiment asks whether the two explicit axes

- dimension coverage `K`, and
- survivor correction rounds `R`

can be compressed into a single finite-width state operator without changing the finite-K survivor result.

## 1. Exact finite-K compression

For active dimensions

\[
q_0,q_1,\ldots,q_{K-1},
\]

define the square-free product

\[
P_K=\prod_{j=0}^{K-1}q_j
\]

and the compressed residue

\[
z_K=p\bmod P_K.
\]

Because the active dimensions are pairwise coprime primes,

\[
\exists j<K:\ q_j\mid(p+d)
\]

if and only if

\[
\gcd(z_K+d,P_K)>1.
\]

Therefore the same finite-width survivor gap can be written as

\[
\boxed{
J_K(z_K)=\min\{d\ge1:\gcd(z_K+d,P_K)=1\}.
}
\]

This is an exact algebraic compression for fixed finite `K`. All active divisibility dimensions are represented by one product residue, and the nested survivor corrections are represented by one next-coprime map `J_K`.

The identity is not the open conjecture. It is simply another representation of the same finite-width survivor set.

## 2. What has actually collapsed

At the representation level,

\[
(\rho_0,\rho_1,\ldots,\rho_{K-1})
\]

is replaced by

\[
z_K=p\bmod P_K.
\]

A single gcd then answers whether a particular offset is hit by any active dimension. This removes the explicit `for j = 0..K-1` divisibility scan from that test.

Likewise, the complete finite-K result can be named by the scalar operator `J_K` instead of exposing the original correction sequence.

This is a useful collapse of the mathematical state graph, but it is not yet a causal-depth-1 implementation.

## 3. Why R has not been computationally eliminated

The diagnostic evaluator of `J_K` deliberately uses

```text
for d = 1, 2, 3, ...:
    if gcd(z_K + d, P_K) == 1:
        return d
```

so the hidden minimization is visible.

That loop is a candidate-offset search. It violates the ZeroCandidatePrime generation contract and therefore remains diagnostic only.

A precomputed wheel table could make `J_K(z_K)` a direct lookup, but its period is `P_K`; the state size grows catastrophically and it would also violate the no-table direction of the project.

A fully parallel batch of offsets could remove the sequential `R` dependency in an idealized machine model, but it would materialize a candidate domain and is therefore rejected for the same reason.

The unresolved target is consequently stronger:

\[
\boxed{
\text{find a compact evaluator of }J_K
\text{ that neither searches }d
\text{ nor stores the period }P_K.
}
\]

## 4. Cloud experiment at the 10^100 scale

Input:

\[
p=10^{100}+267.
\]

The traditional prime fixture used by this isolated diagnostic is outside the generator and is used only to construct the finite-K comparison state. No result from the fixture is fed into the production recurrence.

Single-threaded C + GMP measurements:

| K | largest q | digits in P_K | collapsed finite-K gap | vector finite-K gap | gcd calls |
|---:|---:|---:|---:|---:|---:|
| 256 | 1,619 | 690 | 4 | 4 | 4 |
| 2,048 | 17,863 | 7,687 | 4 | 4 | 4 |
| 88,232 | 1,134,719 | 492,313 | 6 | 6 | 6 |
| 175,692 | 2,391,019 | 1,037,651 | 22 | 22 | 22 |

The collapsed gcd representation and the explicit finite-K residue-vector reference agreed in every tested row.

Measured wall-clock values on the cloud CPU were approximately:

| K | product-tree build | collapsed evaluator | explicit vector evaluator |
|---:|---:|---:|---:|
| 256 | 0.016 ms | 0.020 ms | 0.009 ms |
| 2,048 | 0.136 ms | 0.032 ms | 0.085 ms |
| 88,232 | 15.689 ms | 1.767 ms | 2.460 ms |
| 175,692 | 44.426 ms | 12.870 ms | 5.535 ms |

These times are diagnostic CPU numbers, not NVIDIA/PTX results.

## 5. Exact-successor comparison remains negative

After the generation result is frozen, an independent reference computation gives the immediate successor gap for this input as

\[
682.
\]

Therefore even the `K = 175,692` compressed state still returns only

\[
22,
\]

not the exact successor gap.

A separate coverage diagnostic found that after all prime dimensions through `100,000,000` were considered, twenty offsets below `682` still had no divisor in that covered range; the first uncovered offset was still `22`.

This is strong evidence that the difficult part is not merely the linear scan over the first few hundred thousand dimensions. Some composite states in the interval can hide their first active divisor far beyond that range.

## 6. Asymptotic cost moved into P_K

The product compression does not make dimension information disappear. It moves it into the bit width of `P_K`.

By the prime number theorem for the Chebyshev function,

\[
\log P_K=\vartheta(q_{K-1})\sim q_{K-1}.
\]

So an explicit primorial is not a constant-size representation as `K` grows.

This means the current attempt achieves

\[
\boxed{K\text{-scan collapse}}
\]

but not

\[
\boxed{K\text{-information collapse}}.
\]

Likewise `J_K` achieves an algebraic naming of the full correction closure, but the available evaluator still exposes sequential offset work, so it is not an implementation-level `R=1` result.

## 7. Result of this attempt

The experiment found an exact finite-width identity:

\[
\boxed{
J_K(p\bmod P_K)
=\text{the original finite-K survivor gap}.
}
\]

That identity genuinely merges the explicit `K` dimension vector and the explicit nested correction trajectory into one scalar state map.

However, every straightforward evaluator found so far leaks the hidden complexity back out in one of three forms:

1. iterate offsets, which restores `R` and violates the no-candidate requirement;
2. store a wheel/lookup over `P_K`, which causes state explosion and violates the no-table direction;
3. expand the prime factors or the primorial construction, which restores `K`-dependent work.

Therefore this attempt is retained as a structural reduction and a negative engineering result, not as VF-1 success.

The next valid target is a non-table, non-candidate, non-factor-expansion evaluator for the compressed next-coprime map, or an alternative representation that avoids the primorial state entirely.
