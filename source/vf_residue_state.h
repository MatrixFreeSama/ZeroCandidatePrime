#ifndef VF_RESIDUE_STATE_H
#define VF_RESIDUE_STATE_H
#include "own_solver.h"

typedef struct VfResidueState {
    OwnU128 p;
    UINT64 *q;
    UINT64 *r;
    void *frames;
    UINT64 count;
    UINT64 capacity;
    UINT64 last_gap;
    UINT64 last_correction_rounds;
} VfResidueState;

/* Experimental verification-free residue-state generator.
   No Prime Gate, Miller-Rabin, q^2>x closure, conventional next-prime routine,
   factorization routine, or reference answer is consulted by these functions.

   The dimension width is deliberately finite and therefore outputs are
   PROVISIONAL / UNVERIFIED. The purpose of this module is to test whether the
   large integer p can be removed from the inner survivor dynamics once the
   residue state r_j = p mod q_j has been initialized. */
int vf_residue_init(VfResidueState *s, OwnU128 p, UINT64 dimension_cap);
void vf_residue_free(VfResidueState *s);
int vf_residue_next(VfResidueState *s, volatile int *cancel,
                    UINT64 *gap, UINT64 *correction_rounds,
                    OwnSolveStats *stats);

#endif
