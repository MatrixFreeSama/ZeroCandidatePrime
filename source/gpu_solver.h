#ifndef GPU_SOLVER_H
#define GPU_SOLVER_H
#include "winmini.h"
#include "own_solver.h"

enum {
    GPU_SOLVER_NO_NVIDIA = 0,
    GPU_SOLVER_OK = 1,
    GPU_SOLVER_ERROR = -1
};

typedef struct GpuSolveResult {
    UINT64 p;
    UINT64 gap;
    UINT64 next_p;
    OwnSolveStats stats;
    char device_name[128];
    int cc_major;
    int cc_minor;
    int self_test_pass;
    int error_stage;
} GpuSolveResult;

typedef struct GpuExploreResult {
    UINT64 rank;
    UINT64 p;
    UINT64 gap;
    UINT64 elapsed_ms;
    OwnSolveStats stats;
    char device_name[128];
    int cc_major;
    int cc_minor;
    int self_test_pass;
    int error_stage;
    int end_reason; /* 1 user stop, 2 arithmetic/resource ceiling */
} GpuExploreResult;

typedef void (*GpuExploreProgressFn)(const GpuExploreResult* progress, void* user);

/* mode: 0 = value is current p and solve its gap;
         1 = value is rank n, Self Bootstrap then solve target gap;
         2 = value is rank n, Self Bootstrap only and return p_n. */
int gpu_survivor_solve(int mode, UINT64 value, GpuSolveResult* out);

/* Experimental unverified chain: start at (rank=1,p=2), repeatedly apply only
   the survivor recurrence, preserve the GPU dimension cache between batches,
   and run until cancel or the uint64/resource ceiling. No traditional verifier. */
int gpu_survivor_explore(volatile int* cancel, GpuExploreProgressFn progress,
                         void* user, GpuExploreResult* out);


typedef struct GpuRecordExploreResult {
    OwnU128 rank;
    OwnU128 p;
    UINT64 gap;
    UINT64 elapsed_ms;
    UINT64 causal_depth_cap;
    OwnSolveStats stats;
    char device_name[128];
    int cc_major;
    int cc_minor;
    int self_test_pass;
    int error_stage;
    int end_reason; /* 1 user stop, 2 arithmetic/resource ceiling */
} GpuRecordExploreResult;

typedef void (*GpuRecordExploreProgressFn)(const GpuRecordExploreResult* progress, void* user);

/* Experimental record-seeded 128-bit implicit-dimension chain.
   NVIDIA present: the successor recurrence itself is executed on the GPU.
   No persistent q[]/prime basis is allocated in host or device global memory;
   dimensions exist only in the per-CTA live recursion workspace.
   No NVIDIA device: caller may use the CPU reference fallback. */
int gpu_record_explore(OwnU128 seed_rank, OwnU128 seed_p, UINT64 causal_depth_cap,
                       volatile int* cancel, GpuRecordExploreProgressFn progress,
                       void* user, GpuRecordExploreResult* out);

#endif
