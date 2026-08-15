# Architecture

ZeroCandidatePrime separates four concerns that are allowed to use different algorithms: input/bootstrap, prime admission, successor generation, and post-solve verification.

## Execution pipeline

```text
input or bootstrap
        |
        v
Prime Gate
        |
        v
survivor-recurrence generator
        |
        v
frozen (gap, next prime)
        |
        v
Exact Pass
```

Only the generator determines the gap. Bootstrap and verification code may use conventional methods, but their search state is not visible to the generator.

## Fast Bootstrap

Fast Bootstrap is an external starting-state resolver. It first attempts `primecount --nth-prime` when that executable is available and otherwise uses the built-in traditional fallback supported by `traditional.c`.

This path is intentionally not rewritten to imitate the survivor recurrence. Its only product for the generator is the resolved starting prime.

## Prime Gate

A directly supplied `p_n` is validated before it enters the generator. The gate exports a Boolean admission decision. It does not provide factors, a candidate interval, a prime table, or a precomputed successor.

## Successor generator

The generator is implemented independently in `own_solver.c` and in the NVIDIA device program `gpu_kernel.ptx`.

Its state is based on the nested survivor maps described in [`ALGORITHM.md`](ALGORITHM.md). Prime dimensions are generated from `q_0 = 2` by the recurrence itself. The base dimension uses the same advance-and-divisibility rule as later levels; there is no generator-specific odd-only shortcut.

For the exact 64-bit path, dimensions needed by an active successor transaction may occupy a live workspace because nested `M_k` evaluation must refer to lower levels. This workspace belongs only to that transaction. Self Bootstrap discards it before beginning the next prime-to-prime step.

The generator does not allocate a natural-number candidate interval or candidate-prime pool, and it does not assemble a dense/global matrix.

## Direct p

Direct mode performs one successor transaction after the Prime Gate accepts the supplied `p_n`. The transaction starts from fresh generator state.

## Self Bootstrap

Self Bootstrap begins at `(1, 2)` and repeatedly invokes the same successor transaction. The prime chain is causal, but dimension state from one transaction is not retained as hidden input to the next.

## Record-seeded experiment

The record experiment uses 128-bit rank and prime state and a fixed live-recursion depth cap. It begins from the exact anchor embedded in `main.c` and skips reconstruction of the history before that anchor.

The cap is an engineering boundary. Outputs after the exact seed are labelled `PROVISIONAL / UNVERIFIED`, and the experiment does not claim an asymptotic proof.

## NVIDIA numerical path

The executable does not link against the CUDA Runtime. `gpu_solver.c` loads `nvcuda.dll` dynamically and calls the CUDA Driver API directly.

The PTX module contains the GPU recurrence and is JIT-loaded through the driver. The normal 64-bit path and the 128-bit record experiment use a 256-thread CTA for live-dimension projection.

The recurrence is integer-based and does not use Tensor Cores.

When an NVIDIA device is detected, initialization and arithmetic self-tests must pass before GPU results are accepted. A detected but broken NVIDIA authority path is reported as an error instead of being silently replaced by a CPU answer. The CPU implementation is used when no NVIDIA CUDA device is present.

## Exact Pass

Exact Pass runs only after the generator result has been frozen. It is allowed to use a conventional next-prime calculation because its role is comparison, not generation. A mismatch is reported rather than used as a correction signal.

## Build boundary

The application is a freestanding native Windows x64 program built from C plus the checked-in PTX device program. `embed_ptx.c` is a host build utility that converts `gpu_kernel.ptx` into the generated `gpu_kernel_ptx.h` string header consumed by `gpu_solver.c`.

Binary resources such as the application icon, Windows resource object, import libraries, and final executable are kept separate from the text-source layer.
