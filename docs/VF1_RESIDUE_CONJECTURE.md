# VF-1 Residue-State Conjecture

## Status

This document defines an experimental, verification-free successor representation. It is a conjectural research branch. It does not claim a proof of exact prime succession, constant causal depth, or asymptotic `O(1)` running time.

The exact 64-bit generator in the main branch remains separate and retains its exact closure. The VF-1 branch deliberately removes that closure from the generation stage so that scale dependence can be studied without feeding any traditional primality or next-prime result back into the generator.

## 1. Existing survivor hierarchy

Let

\[
q_0=2.
\]

Let the level-0 survivor distance be

\[
M_0(x)=\min\{d\ge 1:q_0\nmid x+d\}.
\]

For `k >= 1`, define the lower-level survivor orbit

\[
y_0=x,\qquad y_{r+1}=y_r+M_{k-1}(y_r).
\]

Then

\[
r_k(x)=\min\{r\ge 1:q_k\nmid y_r\},
\]

and

\[
M_k(x)=y_{r_k(x)}-x.
\]

The dimensions are generated recursively by

\[
q_{k+1}=q_k+M_k(q_k).
\]

No prime table is required as generation input.

## 2. Residue-state transform

For a current large state `p`, define the residue field

\[
\rho_j(p)=p\bmod q_j.
\]

For any travelled offset `delta`, translation gives the exact identity

\[
\rho_j(p+\delta)=\bigl(\rho_j(p)+\delta\bigr)\bmod q_j.
\]

Therefore divisibility by every active dimension can be evaluated from the residue field without repeatedly using the large integer `p`:

\[
q_j\mid(p+\delta)
\iff
\bigl(\rho_j(p)+\delta\bigr)\bmod q_j=0.
\]

Define the residue survivor operator `\widetilde M_k` recursively by replacing every divisibility test in `M_k` with the translated residue test above. For a fixed finite active dimension set `q_0,...,q_K`, the branch uses

\[
\boxed{\widetilde M_k(\rho(p),\delta)=M_k(p+\delta)}
\]

as an algebraic equivalence, not as a conjecture. The identity follows by induction on `k` because each recurrence step depends only on divisibility modulo the active `q_j` and on accumulated offsets.

The large integer participates once when the initial residue field is established and once when the final gap is added to the output state. It does not participate in the inner fixed-`K` survivor recursion.

## 3. Verification-free finite-width successor

For a finite dimension width `K`, initialize

\[
\delta_0=\widetilde M_0(\rho(p),0).
\]

For each active dimension, a hit causes a lower-level survivor jump. Define the active-hit set

\[
H_K(\delta)=\{j\in\{1,\ldots,K\}: (\rho_j(p)+\delta)\bmod q_j=0\}.
\]

If `H_K(delta)` is nonempty, select the first active dimension

\[
j^*(\delta)=\min H_K(\delta)
\]

and advance

\[
\delta'=\delta+\widetilde M_{j^*(\delta)-1}(\rho(p),\delta).
\]

The adaptive correction count is

\[
R(p,K)=\min\{r:H_K(\delta_r)=\varnothing\}.
\]

The finite-width experimental output is

\[
\widehat p^+_K=p+\delta_{R(p,K)}.
\]

This output is `PROVISIONAL / UNVERIFIED`. The condition `H_K = empty` certifies only that the finite active dimension set found no hit. It does not imply that no later dimension would hit the same state.

## 4. Chain-resident update

Once a provisional gap `g` has been produced, the residue field advances without recomputing the large integer modulo every dimension:

\[
\boxed{\rho_j(p+g)=\bigl(\rho_j(p)+g\bigr)\bmod q_j.}
\]

Thus a chain-resident state can be written as

\[
(\rho_0,\rho_1,\ldots,\rho_K)\xrightarrow{\mathcal V_K}g\xrightarrow{\text{translation}}(\rho'_0,\rho'_1,\ldots,\rho'_K),
\]

while the large integer itself only receives the final addition

\[
p'=p+g.
\]

This is the specific representation tested by `source/vf_residue_state.c`.

## 5. VF-1 conjecture

The finite-width transform does not remove the dimension-coverage problem. VF-1 is therefore stated at the level of an implicit closure operator rather than a fixed finite `K`.

**VF-1 Conjecture.** There exists an implicit residue-state operator `I` such that, for every prime `p`,

\[
\boxed{\mathfrak I(\rho(p))=p_{next}-p}
\]

without a conventional primality test, conventional next-prime routine, factorization result, candidate interval, or `q^2 > candidate` verification closure participating in generation.

The strongest causal-depth form is

\[
\boxed{D_{causal}(\mathfrak I)=1.}
\]

This is a conjecture. The current code does not prove it. In particular, a finite `dimension_cap`, a fixed GPU launch width, or a fixed recursion cap must not be presented as proof of this statement.

## 6. Complexity separation

The branch separates four quantities:

\[
L=\lceil\log_2 p\rceil
\]

for big-integer bit width,

\[
K
\]

for explicit dimension coverage,

\[
D_{rec}
\]

for nested survivor recursion depth, and

\[
R(p,K)
\]

for adaptive survivor correction rounds.

The fixed-`K` residue transform removes repeated dependence of the inner recurrence on the `L`-bit integer `p`, but it does not by itself make `K`, `D_rec`, or `R` constant. Consequently `D_causal = 1` and `T_bit = O(1)` are different claims. VF-1 concerns the former only.

## 7. Post-generation checking protocol

Generation and checking are deliberately one-way separated:

```text
p -> VF residue generator -> freeze candidate -> independent post-checks
```

A Miller-Rabin post-check may be used after the output is frozen to reject composite outputs. Such a check must not feed a witness, factor, corrected candidate, or next-prime value back into the generator.

A Miller-Rabin pass alone does not prove that a prime output is the immediate successor. When exact successor comparison is desired experimentally, a separate reference next-prime computation may be executed after the VF output has been frozen. Its result is evaluation data, not generation input.
