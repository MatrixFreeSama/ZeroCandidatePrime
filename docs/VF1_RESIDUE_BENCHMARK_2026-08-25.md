# VF-1 Residue-State Cloud Benchmark, 2026-08-25

## Purpose

This benchmark isolates one specific claim: after a residue field `r_j = p mod q_j` has been initialized, can a fixed-width survivor recurrence stop repeatedly using the large integer `p` in its inner divisibility dynamics?

It is not an exact-prime benchmark and is not evidence that VF-1 is proved.

## Environment

- Linux x86-64 cloud container
- 5 online CPUs
- Intel Xeon Platinum 8573C
- GCC 14.2.0
- C with GMP only for arbitrary-precision benchmark state
- single-threaded benchmark code
- `-O3 -march=native`
- active dimension width: `K = 256`
- largest generated dimension at this width: `q_255 = 1619`

The checked-in Windows application does not acquire a GMP dependency. GMP was used only in this cloud benchmark so the same experiment could be extended far beyond 128 bits.

## Compared inner representations

**Full-big-integer fixed-K recurrence**

Every divisibility test operates on the current arbitrary-precision integer state.

**Residue-state fixed-K recurrence**

The initial state computes

\[
r_j=p\bmod q_j,
\]

then all inner divisibility tests use

\[
(r_j+\delta)\bmod q_j.
\]

The two implementations were required to return the same finite-width gap before timing comparisons were accepted.

## Single-state results

| Input scale | Residue gap | Full-big-int gap | Residue inner call | Full-big-int inner call |
|---|---:|---:|---:|---:|
| `10^100 + 267` | 4 | 4 | 1.86 us | 5.90 us |
| `10^1000 + 267` | 4 | 4 | 1.79 us | 17.92 us |
| `10^10000 + 267` | 34 | 34 | 3.33 us | 296.28 us |
| `10^100000 + 267` | 10 | 10 | 2.14 us | 1386.27 us |
| `10^1000000 + 267` | 12 | 12 | 3.31 us | 36422.87 us |

Repeated evaluation of the unchanged residue state produced approximately:

| Input scale | Residue call latency |
|---|---:|
| `10^100` | 1.77 us |
| `10^1000` | 1.72 us |
| `10^10000` | 2.76 us |
| `10^100000` | 2.01 us |
| `10^1000000` | 2.86 us |

The gap itself changes the path length, so these values should not be interpreted as a proof of perfectly constant latency. The important observation is that increasing the large integer from roughly 333 bits to roughly 3.32 million bits did not produce the growth seen when the arbitrary-precision integer remained inside every divisibility operation.

## Initialization cost

The large integer has not disappeared from the entire computation. Establishing the residue field still costs `K` big-integer reductions. Measured initialization time at `K=256` was approximately:

| Input scale | Residue initialization |
|---|---:|
| `10^100` | 8.35 us |
| `10^1000` | 19.39 us |
| `10^10000` | 67.80 us |
| `10^100000` | 629.58 us |
| `10^1000000` | 5947.17 us |

This cost is paid once for a chain-resident residue state. Subsequent state updates use

\[
r'_j=(r_j+g)\bmod q_j
\]

and only the final externally visible integer state requires `p <- p + g`.

## Width scaling at `10^10000`

| K | q_max | Residue inner call | Full-big-int inner call |
|---:|---:|---:|---:|
| 256 | 1619 | 4.45 us | 407.28 us |
| 512 | 3671 | 8.33 us | 2113.09 us |
| 1024 | 8161 | 18.13 us | 5104.62 us |
| 2048 | 17863 | 22.69 us | 4823.01 us |

The residue transform therefore attacks bit-width participation, not the dimension-coverage problem. `K` remains an explicit source of cost.

## 128-bit equivalence check against the current experiment

Using the repository's record anchor

```text
p = 10^29 - 27
```

and `K=256`, the residue-state C module returned

```text
gap = 18
```

matching the existing fixed-depth 128-bit survivor experiment for the same finite dimension width. The repository implementation was additionally rewritten to use an explicit work stack rather than C recursion and still returned `gap = 18`, `q_max = 1619` for this check.

This checks the intended algebraic equivalence of the two fixed-K representations; it does not make `18` the exact next-prime gap.

## Interpretation

The benchmark supports a narrow representation claim:

> Once the residue field is available, the large integer can be removed from the inner fixed-width survivor divisibility loop.

At `K=256`, the full-big-integer inner call grew from about 5.90 us near `10^100` to about 36.4 ms near `10^1000000`, while the residue-only inner call remained in the low-single-digit microsecond range for these tested states. Initialization still scales with bit width and explicit dimension width.

It does not establish:

- exact next-prime generation without closure,
- bounded required dimension width,
- bounded adaptive correction rounds,
- or causal depth `1`.

Those remain the open VF-1 questions.
