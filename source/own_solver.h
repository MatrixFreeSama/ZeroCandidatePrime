#ifndef OWN_SOLVER_H
#define OWN_SOLVER_H
#include "winmini.h"

typedef struct OwnU128 {
    UINT64 lo;
    UINT64 hi;
} OwnU128;

typedef struct OwnSolveStats {
    UINT64 layers;
    UINT64 dimension_count;
    UINT64 dimensions_generated;
    UINT64 survivor_hops;
    UINT64 divisibility_tests;
} OwnSolveStats;

typedef struct OwnMFrame {
    UINT64 level;
    UINT64 cur;
    UINT64 total;
    int waiting;
} OwnMFrame;

typedef struct OwnDimensionCache {
    UINT64* q;
    UINT64 count;
    UINT64 capacity;
    OwnMFrame* frames;
    UINT64 frame_capacity;
} OwnDimensionCache;

int own_cache_init(OwnDimensionCache* c);
void own_cache_free(OwnDimensionCache* c);
int own_next_gap(UINT64 p, OwnDimensionCache* cache, volatile int* cancel,
                 UINT64* gap, OwnSolveStats* stats);
int own_self_bootstrap(UINT64 n, volatile int* cancel, UINT64* p,
                       OwnSolveStats* aggregate);


typedef struct OwnRecordExploreResult {
    OwnU128 rank;
    OwnU128 p;
    UINT64 gap;
    UINT64 elapsed_ms;
    UINT64 causal_depth_cap;
    OwnSolveStats stats;
    int end_reason; /* 1 user stop, 2 arithmetic/resource ceiling */
} OwnRecordExploreResult;

typedef void (*OwnRecordProgressFn)(const OwnRecordExploreResult* progress, void* user);

/* Experimental record-seeded implicit-dimension chain. No prime-dimension
   basis is prebuilt or retained. Dimension values are generated on demand inside
   the local recursion stack and discarded with that successor step. The causal
   depth is deliberately capped, therefore outputs after the exact seed are
   provisional and unverified. */
int own_record_explore(OwnU128 seed_rank, OwnU128 seed_p, UINT64 causal_depth_cap,
                       volatile int* cancel, OwnRecordProgressFn progress, void* user,
                       OwnRecordExploreResult* out);

#endif
