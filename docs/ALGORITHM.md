# Algorithm

## 1. Successor problem

For consecutive primes

```text
p_n < p_(n+1),
```

define the gap

```text
g_n = p_(n+1) - p_n.
```

The generator is organized around the map

```text
p_n -> g_n -> p_(n+1)
```

rather than around a preconstructed interval of integers that is scanned until a prime is found.

The core generator is isolated from conventional bootstrap and verification code. In particular, no prime table, prime-counting routine, factorization routine, conventional primality search, or external `nextprime` result is supplied to the recurrence as generation input.

## 2. Survivor maps

Set

```text
q_0 = 2.
```

The base map returns the distance to the first state that survives divisibility by `q_0`:

```text
M_0(x) = min { d >= 1 : q_0 does not divide x + d }.
```

In the implementation this is evaluated as the same advance-and-test rule used by the recurrence. There is no dedicated `x += 2` or parity formula inside the generator.

Assume `M_k` and `q_k` are defined. The next dimension is generated from the existing survivor map:

```text
q_(k+1) = q_k + M_k(q_k).
```

To evaluate `M_(k+1)(x)`, let

```text
y_0 = x,
y_(r+1) = y_r + M_k(y_r).
```

The sequence `y_1, y_2, ...` therefore contains only states that have already survived levels `0..k`. The next-level map stops at the first lower-level survivor not divisible by `q_(k+1)`:

```text
r* = min { r >= 1 : q_(k+1) does not divide y_r }
M_(k+1)(x) = y_(r*) - x.
```

Equivalently, each hit by the new dimension causes another jump through the previous survivor map; a miss closes that level.

## 3. Why this is not an interval array

A conventional interval-oriented search may represent

```text
p+1, p+2, p+3, ...
```

and then mark or test those integers. This implementation does not allocate such an interval for the generator.

At level `k+1`, the recurrence moves only through states produced by `M_k`. Intermediate integers that do not belong to that lower-level survivor stream are not represented as candidate records.

The implementation may keep a live workspace for the nested recurrence and for the prime dimensions needed by the active successor transaction. This is working state for evaluating the operator, not a natural-number candidate interval and not a persistent external prime table. Self Bootstrap discards that transaction-local dimension state before the next prime-to-prime step.

## 4. Exact closure in the 64-bit path

Suppose the active candidate has survived every generated prime dimension below the next required factor bound. If the next prime dimension `q` satisfies

```text
q * q > candidate,
```

then a composite candidate cannot have an undiscovered prime factor smaller than or equal to its square root. The 64-bit exact path uses this condition to close the successor transaction.

This exact closure is distinct from the record-seeded 128-bit experiment, whose recursion workspace is deliberately bounded. The latter is labelled provisional beyond its exact seed.

## 5. Separation of generation and validation

The executable has three deliberately separate roles:

```text
bootstrap / input resolution -> generator -> post-solve verification
```

The Prime Gate may reject an invalid direct input before generation. It exports admission only; it does not export factors, candidate intervals, or a next-prime value to the generator.

After the generator freezes `g_n` and `p_(n+1)`, Exact Pass may independently compute a traditional reference result and compare it. That result is not fed back into the recurrence.

Fast Bootstrap follows the same separation. It may use an external or traditional method to resolve the starting `p_n`, after which the generator receives the resolved starting state rather than the bootstrap algorithm's internal search state.

## 6. Self Bootstrap

Self Bootstrap begins with

```text
(n, p_n) = (1, 2)
```

and repeatedly applies the successor transaction:

```text
p_1 -> p_2 -> p_3 -> ...
```

The prime-to-prime chain is causal. The dimension workspace used to obtain one successor is not retained as a hidden basis for the next successor. Each transaction begins from `q_0 = 2` and regenerates the dimensions demanded by that transaction.

## 7. NVIDIA execution

The NVIDIA implementation is split into host and device code.

`gpu_solver.c` dynamically loads the CUDA Driver API from `nvcuda.dll`, creates the context, JIT-loads the embedded PTX module, allocates device workspaces, launches kernels, and performs startup self-tests.

`gpu_kernel.ptx` contains the device recurrence. The normal path launches one 256-thread CTA so that live prime dimensions can be projected across CUDA lanes. The record-seeded 128-bit experiment also uses a 256-thread CTA with a fixed live-recursion workspace.

The device recurrence uses integer arithmetic and does not use Tensor Cores.

## 8. Complexity status

The project distinguishes total work from sequential or causal depth.

Matrix-Free representation means the generator does not need to assemble a global matrix or natural-number candidate array. CUDA execution can move independent dimension tests into spatial parallel work. Neither property alone proves

```text
sequential depth = O(1).
```

The record-seeded path intentionally exposes a fixed engineering depth cap so that scale-independent causal-depth behavior can be investigated. The cap is not a theorem. A proof would have to show that the required causal recursion depth is bounded independently of the numerical scale of `p_n`.
