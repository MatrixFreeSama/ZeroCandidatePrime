# VF-1 Symbolic Collapse Attempt, 2026-08-25

## Status

This note records two results from the attempt to remove both the explicit dimension width K and the bounded offset-mask width W from the VF-1 diagnostic representation.

The first result is a lower bound for any generic OR-monoid compression of the bounded forbidden-offset mask.  The second is an arithmetic-specific K-collapse certificate based on a modular factorial product tree.

Neither result is a final verification-free generator and neither proves causal depth 1.

## 1. Generic W-bit compression has a worst-case lower bound

For a bounded offset horizon U = {1,...,W}, let a forbidden set A be any subset of U.  The finite-window survivor query is

\[
firstzero(A)=\min(U\setminus A).
\]

Suppose a summary map S(A) is required to support exact associative union composition and exact first-zero recovery after arbitrary future unions.  In other words, from S(A) and S(C) one must be able to obtain a summary for A union C, and from the resulting summary recover firstzero(A union C).

Then S must distinguish every pair of distinct masks A and B.

Proof: choose the least offset d where A and B differ.  Without loss of generality d is in A and not in B.  Let C contain every offset smaller than d and not contain d.  Then firstzero(B union C) = d, while firstzero(A union C) is not d.  Therefore S(A) and S(B) cannot be identical, otherwise composition with the same S(C) would give the same answer for both.  Hence S is injective on the 2^W possible masks.

Therefore any generic exact compositional summary needs at least

\[
\boxed{W\text{ bits}}
\]

in the worst case.

This means the W-bit forbidden mask used in the previous OR-monoid experiment is information-theoretically optimal for the unrestricted mask problem.  A true sub-W symbolic collapse must exploit the arithmetic structure of the prime-dimension family rather than compressing an arbitrary OR mask.

## 2. Arithmetic-specific K collapse without a prime table

Let c be the current survivor candidate and let B be a dimension-value bound.  The set of active prime dimensions up to B can be replaced by the factorial identity

\[
\boxed{\gcd(c,B!)>1}
\]

if and only if c has a prime divisor not exceeding B.

This does not require a precomputed prime list.  Every prime q <= B is already a factor of B!, while every composite contribution is redundant for the boolean collision predicate.

The enormous factorial itself need not be materialized.  Define the bounded certificate

\[
\boxed{\Sigma_B(c)=B!\bmod c.}
\]

Then

\[
\boxed{\gcd(c,\Sigma_B(c))=\gcd(c,B!).}
\]

So the root payload is bounded by the bit width of c regardless of how many prime dimensions lie below B.

The experiment `experiments/vf1_factorial_certificate_gmp.c` evaluates Sigma_B(c) with a divide-and-conquer modular product tree.  The checked-in implementation is single-threaded, but the product DAG is balanced: with constant-size leaves its algebraic combination depth is O(log B), while total work still grows with B.

This is a real K-representation collapse:

\[
\text{explicit }q_0,\ldots,q_K
\quad\longrightarrow\quad
\Sigma_B(c)
\]

with no prime table and no explicit primorial payload.

It is not yet an admissible VF-1 generator.  If B is driven to sqrt(c), the boolean collision predicate becomes a primality-style exact closure.  In addition, the modular factorial still performs B-dependent work.

## 3. Cloud measurements at p = 10^100 + 267

Environment:

- Linux x86-64 cloud container
- Intel Xeon Platinum 8573C
- single-threaded C
- GCC -O3 -march=native
- GMP

The target input is

\[
p=10^{100}+267.
\]

Measured modular-factorial collision certificates:

| current gap g | bound B | collision | certificate time |
|---:|---:|---:|---:|
| 4 | 1,000,000 | no | 22.5 ms |
| 4 | 1,134,709 | yes | 31.0 ms |
| 4 | 3,000,000 | yes | 75.2 ms |
| 6 | 2,391,019 | yes | 59.7 ms |
| 6 | 3,000,000 | yes | 70.8 ms |
| 22 | 3,000,000 | no | 70.3 ms |
| 682 | 3,000,000 | no | 108.7 ms |

For the first two known staircase events, a debug build of the diagnostic returned GCD values 1,134,709 and 2,391,019 respectively, matching the independently established collision dimensions.  The normal diagnostic does not expose the factor value; it emits only the collision boolean.

The live certificate payload stayed near the approximately 333-bit size of c instead of growing with an explicit factorial or primorial.

## 4. What has now been learned about K, R, E and W

The experiments distinguish four kinds of collapse:

1. Residue-state collapse removes repeated full-p arithmetic from the finite-K inner recurrence.
2. Event-staircase collapse shows most K increments are null events.
3. OR-mask collapse absorbs finite-K correction rounds R and event sequence E into one first-zero query, but requires W bits.
4. Factorial-certificate collapse removes the explicit prime-dimension list from a single collision query and bounds its root payload by O(log c) bits, but retains B-dependent work and does not produce the next survivor gap by itself.

The generic lower bound explains why the previous request for a summary whose size is independent of both K and W cannot be achieved by treating the forbidden-offset field as an arbitrary OR mask.  Any successful VF-1 symbolic state must use additional number-theoretic structure.

## 5. New target

The remaining admissible target can now be stated more narrowly.

We need an arithmetic-specific transfer object

\[
\boxed{\Theta(p;I)}
\]

that represents the exclusion effect of a large implicit dimension interval I, supports associative or otherwise bounded-depth composition, and lets the next survivor action be extracted without:

- enumerating prime dimensions,
- materializing an offset window,
- locating or returning a factor,
- invoking a conventional primality or next-prime routine,
- or doing work proportional to the raw interval width.

The factorial certificate proves that prime-list information can disappear from a boolean collision query.  The W-bit lower bound proves that a generic offset-mask monoid cannot by itself remove the candidate-offset horizon.  Therefore the next step must fuse these two observations: an arithmetic-specific aggregate that acts directly on the survivor state rather than first constructing a generic forbidden-offset field.

## 6. Current causal-depth boundary

With a preexisting arithmetic interval bound B, the balanced modular product tree provides an intermediate collision-query DAG of

\[
\boxed{D_{collision}=O(\log B)}.
\]

This is not O(1), and the sequential benchmark runtime is not a parallel-depth measurement.  It is nevertheless a stronger structural bound than a dimension-by-dimension K scan because the explicit prime basis is absent from the query representation.

A genuine VF-1 result would require an additional algebraic collapse from this balanced arithmetic aggregate to a scale-independent causal operator.  That remains open.
