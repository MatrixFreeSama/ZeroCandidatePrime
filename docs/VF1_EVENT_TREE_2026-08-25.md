# VF-1 Event-Tree Collapse Attempt, 2026-08-25

## Status

This is a diagnostic experiment. It is not the final verification-free generator and it does not claim causal depth 1.

The purpose is to test whether the sparse plateau/jump geometry observed in the finite-width survivor can be represented as a balanced collision tree rather than as a linear scan over every active dimension.

The diagnostic implementation is `experiments/vf1_event_tree_gmp.c`.

## 1. Collision tree

For a dimension interval `I`, define the product

\[
B_I=\prod_{j\in I}q_j.
\]

For a current survivor candidate `c`,

\[
\gcd(c,B_I)=1
\]

means the whole interval is collision-free. A nontrivial GCD means that at least one dimension in the interval divides `c`.

A balanced binary tree therefore allows an entire collision-free subtree to be skipped with one query, while a hit subtree is recursively split until the first hit dimension is reached.

Given a prebuilt product tree, the number of tree levels is

\[
O(\log K),
\]

rather than a linear `K` scan.

This does not remove the information contained in the prime/dimension basis. The conventional prime fixture and its product tree are deliberately diagnostic and are not admissible as the final generation core.

## 2. Bounded aggregate payload identity

A useful exact identity emerged from this test. The full product itself is not required for a collision predicate once the current candidate `c` is fixed:

\[
\boxed{\gcd(c,B_I)=\gcd(c,B_I\bmod c).}
\]

Define

\[
A_I(c)=B_I\bmod c.
\]

Then

\[
\boxed{\gcd(c,A_I(c))=1}
\]

if and only if no dimension in `I` divides `c`.

The block-combination rule is associative:

\[
A_{I\cup J}(c)=A_I(c)A_J(c)\bmod c
\]

for disjoint adjacent intervals.

Therefore the aggregate collision certificate for an arbitrarily large interval can, in principle, remain bounded by the bit width of `c` rather than growing like an explicit primorial. This removes the *payload-size* objection to the earlier `P_K` formulation. It does not remove the cost of producing the leaves or the dimension information itself.

## 3. Cloud run at the `10^100` target

Environment:

- Linux x86-64 cloud container
- Intel Xeon Platinum 8573C
- single-threaded C
- GCC `-O3 -march=native`
- GMP
- diagnostic prime fixture through `3,000,000`

The fixture contained

```text
216816 prime dimensions
```

and the product-tree root had approximately

```text
4325355 bits
```

The measured build costs were approximately

```text
prime fixture : 11.9 ms
product tree  : 78.5 ms
```

The event queries were:

| current gap | search starts after | event | tree GCD probes | query time |
|---:|---:|---:|---:|---:|
| 4 | prime index 1 | `q = 1,134,709`, active width `K = 88,231` | 39 | 0.99 ms |
| 6 | previous event | `q = 2,391,019`, active width `K = 175,692` | 36 | 1.56 ms |
| 22 | previous event | no event through `3,000,000` | 8 | 0.15 ms |

The first two events agree with the previously measured staircase.

The important structural point is that more than two hundred thousand explicit dimensions were not queried one-by-one. Given the tree, long null plateaus were rejected by subtree certificates.

## 4. Post-hoc probe beyond the 3,000,000 fixture

A separate post-generation factor probe found that

\[
450131585977\mid (10^{100}+267+22).
\]

The factor `450131585977` is prime. Its prime index is

\[
\pi(450131585977)=17453354061.
\]

This is **not** asserted to be the least prime factor of the survivor, because the remaining cofactor has not been fully factored in this experiment. It therefore gives an upper bound on a possible later collision dimension, not a proven next event location.

The result nevertheless shows why an explicit event tree cannot simply be extended until every possible hidden collision appears: the relevant dimension index can jump from roughly `1.8e5` into the multi-billion range.

## 5. What has actually collapsed

The experiments now separate three different notions of collapse.

### A. Large-integer inner arithmetic

The residue-state transform removes repeated use of the full `p` inside fixed-width divisibility dynamics.

### B. R correction representation

The finite-width operator `J_K` absorbs repeated survivor corrections into one mathematical successor map.

### C. K collision-query depth

A balanced event tree changes a linear collision scan into a logarithmic tree query, provided the dimension-block certificates already exist.

Thus a concrete intermediate target is now

\[
\boxed{D_{collision}=O(\log K)}
\]

rather than `O(K)` linear projection depth.

This is still not VF-1.

## 6. Remaining obstruction

The tree exposes the remaining problem sharply: the leaves still encode the dimension sequence. Building or storing billions of leaves simply moves the cost outside the query.

The next admissible target is therefore an **implicit block transfer operator**. For an interval `I`, it should produce the effect of all dimensions in `I` without enumerating its leaves and without returning a hidden factor location.

A candidate algebraic interface is

\[
\mathcal B_I(c,g)\mapsto g',
\]

with composition

\[
\mathcal B_{I\cup J}=\mathcal B_J\circ\mathcal B_I.
\]

The desired properties are:

- no explicit prime/dimension table,
- no factorization output,
- no candidate interval,
- no explicit primorial,
- no dimension-by-dimension scan,
- and a compact representation whose size does not grow linearly with `|I|`.

If such block operators can be composed in a balanced tree, the causal depth target becomes logarithmic in the number of blocks. A further algebraic collapse of that tree would then be the appropriate route toward the conjectural depth-1 operator.

## 7. Main conclusion

The plateau/jump structure is not merely descriptive. A balanced collision tree demonstrates a real reduction in **query depth** over finite K. The identity

\[
\gcd(c,B_I)=\gcd(c,B_I\bmod c)
\]

also shows that the aggregate certificate need not inherit the enormous bit length of an explicit primorial.

What remains unsolved is the generation of those block certificates or block transfer operators without enumerating the dimension leaves. That is now the most precise K-collapse target in the VF-1 branch.
