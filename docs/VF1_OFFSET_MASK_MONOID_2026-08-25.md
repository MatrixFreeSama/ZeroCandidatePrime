# VF-1 Bounded Offset-Mask Monoid Diagnostic, 2026-08-25

## Status

This is a diagnostic representation experiment. It is not the final verification-free generator and it does not claim causal depth 1.

The experiment asks whether the finite-K survivor effect can be compressed into a compact associative block state after the active dimension information has been supplied.

The implementation is `experiments/vf1_offset_mask_monoid_gmp.c`.

## 1. Forbidden-offset indicator

For current state `p`, active dimension block `I`, and bounded horizon `1 <= d <= W`, define

\[
F_I^{(p)}(d)=
\begin{cases}
1,&\exists j\in I:\ q_j\mid p+d,\\
0,&\text{otherwise}.
\end{cases}
\]

The bit mask representing `F_I` has exactly `W` bits. The finite-width successor gap inside this horizon is the first zero bit of the union over all active blocks.

For a single dimension `q_j`, its forbidden offsets are the arithmetic progression

\[
d\equiv -p\pmod{q_j}.
\]

## 2. Exact block composition

For disjoint active blocks `I` and `J`,

\[
\boxed{F_{I\cup J}^{(p)}=F_I^{(p)}\lor F_J^{(p)}}
\]

where `OR` is bitwise OR over the bounded offset mask.

Thus finite blocks form an associative monoid under OR. A balanced reduction over already-constructed block summaries has logarithmic parallel composition depth, while the root payload remains exactly `W` bits regardless of the number of active dimensions.

This is a stronger payload compression than an explicit primorial representation for local survivor transport:

\[
\boxed{\text{summary size}=O(W)\text{ bits, independent of }K.}
\]

It does not imply that the summary can be generated without dimension information.

## 3. R collapse inside the window

Once the root forbidden mask is available, repeated finite-K survivor corrections are not evaluated one event at a time. The finite-K result is simply

\[
\boxed{g_K=\min\{d\in[1,W]:F_{[0,K)}^{(p)}(d)=0\}}
\]

provided such a zero exists in the chosen horizon.

So, inside a fixed finite horizon, the mask absorbs the explicit correction-round sequence into one first-zero operation.

This is an exact finite-K identity. The horizon itself is a bounded candidate-offset materialization, which makes this representation diagnostic only under the current project rules.

## 4. Chain translation identity

The aggregate forbidden state obeys an exact translation law. If a finite-K survivor step advances by `g`, then for every offset still inside the old horizon,

\[
\boxed{F^{(p+g)}(d)=F^{(p)}(d+g).}
\]

Therefore a chain-resident aggregate mask can be shifted after a step. For a sufficiently long precomputed horizon, subsequent finite-K survivor states can be read from the same aggregate collision field without recomputing the large integer or the individual residue vector for the preserved prefix.

The experiment verifies this identity by rebuilding the mask independently at `p+g` and comparing it with the shifted prefix of the old mask.

## 5. Cloud measurements at `p = 10^100 + 267`

Environment:

- Linux x86-64 cloud container
- Intel Xeon Platinum 8573C
- single-threaded C
- GCC `-O3 -march=native`
- GMP only for the diagnostic large integer

For `W=1024` the root mask is only `128 bytes`.

| prime limit | active dimensions | finite-K gap | root payload |
|---:|---:|---:|---:|
| 1,619 | 256 | 4 | 128 B |
| 17,863 | 2,048 | 4 | 128 B |
| 1,134,709 | 88,231 | 6 | 128 B |
| 2,391,019 | 175,692 | 22 | 128 B |
| 3,000,000 | 216,816 | 22 | 128 B |

The gaps exactly match the previously measured finite-K staircase at the corresponding widths.

At prime limit `3,000,000`, block size `512`, and `W=4096`:

```text
active dimensions : 216816
block summaries   : 424
root payload       : 512 bytes
finite-K gap       : 22
block leaf build   : ~4.1 ms
OR reduction       : ~3.6 us
flat rebuild       : ~4.1 ms
block/flat equal   : yes
shift mismatch     : 0
```

For `W=1024`, the same root payload is only `128 bytes`; the OR reduction was about `1.9 us` in the measured run.

The exact timings are machine-specific. The structural observations are the important part:

1. the root summary size depends on `W`, not on `K`;
2. block composition is associative and very cheap once leaf summaries exist;
3. the first-zero query absorbs the finite-K correction sequence;
4. the chain translation law holds exactly on the preserved mask prefix.

## 6. What this does and does not collapse

This experiment collapses three finite-state representation costs:

\[
\text{many block payloads}\to\text{one }W\text{-bit root},
\]

\[
R\text{ correction steps}\to\text{one first-zero query},
\]

and, after a chain step,

\[
F^{(p)}\to\text{shifted }F^{(p+g)}
\]

on the retained horizon.

It does **not** solve the remaining K-information problem. Constructing the leaf masks still requires the active dimension sequence or an equivalent source of its congruence information.

It also materializes a bounded offset window, which is intentionally excluded from the final ZeroCandidatePrime generation core. The mask therefore serves as a structural microscope, not as the final algorithm.

## 7. Stronger target suggested by the experiment

The experiment suggests that the desired implicit block transfer operator should not attempt to preserve every `q_j`. It should instead directly produce the aggregate local exclusion effect of a dimension interval.

An ideal admissible interface would be

\[
\boxed{\mathcal C_I(p)\mapsto \Sigma_I}
\]

where `Sigma_I` is a compact, non-candidate-array summary satisfying an associative composition law

\[
\boxed{\Sigma_{I\cup J}=\Sigma_I\star\Sigma_J}
\]

and from the root summary the next survivor action can be extracted without enumerating offsets.

The offset mask proves that such associative aggregate effects exist for a bounded materialized horizon. The unsolved step is to replace the materialized horizon by a symbolic or implicit summary whose size and construction depth do not grow linearly with either `W` or `K`.

## 8. Main conclusion

The K/R-collapse problem now has a sharper intermediate result:

> For finite K and a bounded local horizon, all active dimension exclusions form an associative OR-monoid whose root state is only W bits, and the entire finite-K correction chain is reduced to one first-zero operation. The aggregate state also translates by a simple shift under a chain step.

This demonstrates that the large explicit dimension list is not intrinsically required in the *query representation* after aggregation. What remains is to generate an equally compact aggregate symbolically, without enumerating the dimension leaves and without materializing the offset window.
