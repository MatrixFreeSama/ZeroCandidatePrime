# ZeroCandidatePrime architecture

This document describes the current source tree only. It is not a proof of an asymptotic complexity claim.

## Mode separation

### Fast Bootstrap

Fast Bootstrap is intentionally external/traditional. It may use `primecount` or the built-in traditional fallback to resolve a starting `(n, p_n)`. Its internal optimizations are not required to imitate the user generator.

### Self Bootstrap

Self Bootstrap starts from `(1, 2)` and repeatedly applies the user's successor recurrence. For each `p_k -> p_(k+1)` transaction, the live prime-dimension state begins again from `q0=2`; the previous successor's dimension state is discarded rather than reused as hidden historical input.

When a working NVIDIA CUDA device is available, the normal 64-bit successor path uses 256 CUDA lanes to project live prime dimensions. If no NVIDIA CUDA device is available, the CPU implementation is the exact fallback. If NVIDIA is detected but the GPU authority path fails initialization, JIT, self-test, or execution, the application reports failure instead of silently changing numerical authority.

### Direct p

After Prime Gate validates the supplied `p_n`, Direct mode performs one fresh successor transaction through the same user-generator core. The post-solve Exact Pass is validation only and does not feed a candidate, factor, or next-prime result back into generation.

### Record Experiment

The record-seeded experiment starts from the exact external anchor stored in `main.c` and uses 128-bit state. Its causal-depth cap is currently **256**. The experiment is explicitly bounded, so states produced beyond the exact seed are labelled `PROVISIONAL / UNVERIFIED`.

## Matrix-Free and live implicit state

The user generator does not construct a natural-number candidate interval, candidate-prime pool, or dense/global interaction matrix.

The recurrence generates prime-dimension values internally from `q0=2`. The exact 64-bit implementation may hold the dimensions required by the current successor in a live workspace because nested lower-level `M` evaluation needs them. That workspace is not an external prime table and is not retained across Self-Bootstrap successor steps.

The 128-bit record path likewise rebuilds its bounded live dimension workspace for each successor transaction. The fixed workspace is an engineering depth bound, not a proof that the mathematical recursion has constant sequential depth.

## q0 = 2

Inside the user generator, `q0=2` is not implemented as an odd-only shortcut. The base dimension advances by one and applies the same divisibility/survivor rule used by the recurrence. Its natural consequence is that states divisible by 2 do not survive that dimension.

Traditional code in `traditional.c` may still use ordinary odd-only optimizations. Those routines belong to Fast Bootstrap, Prime Gate, and Exact Pass, and their internal state does not feed back into the user generator.

## GPU scope

- NVIDIA Driver API is loaded dynamically.
- Normal 64-bit GPU projection uses 256 CUDA lanes.
- The record-seeded 128-bit experiment also launches a 256-thread CTA.
- Tensor Core is not used.
- Prime-to-prime successor chaining remains causal.
- GPU parallelism does not by itself prove scale-independent sequential depth.

## Generated and binary files

`source/embed_ptx.py` generates `source/gpu_kernel_ptx.h` from `source/gpu_kernel.ptx` during the build.

Binary support and release files can be tracked separately, including the application icon/resource, import libraries, and distributable EXE.
