#include "vf_residue_state.h"

/*
  VERIFICATION-FREE RESIDUE-STATE EXPERIMENT
  ------------------------------------------
  Fixed-K algebraic state:
      r_j(p) = p mod q_j
      r_j(p + d) = (r_j(p) + d) mod q_j

  After initialization, p is not used by the nested divisibility/survivor
  recursion. Only q_j, r_j and a small travelled offset d participate.

  This does NOT solve the infinite-dimension closure problem. A finite
  dimension_cap can miss a later divisor, so the output is provisional.
*/

static void vf_zero_stats(OwnSolveStats *s) {
    if(!s)return;
    s->layers=0;s->dimension_count=0;s->dimensions_generated=0;
    s->survivor_hops=0;s->divisibility_tests=0;
}

static int vf_add_u64(OwnU128 a, UINT64 b, OwnU128 *out) {
    OwnU128 r;
    r.lo=a.lo+b;
    r.hi=a.hi+(r.lo<a.lo?1ULL:0ULL);
    if(r.hi<a.hi)return 0;
    *out=r;return 1;
}

static UINT64 vf_mod_add(UINT64 a, UINT64 b, UINT64 m) {
    if(m==0)return 0;
    a%=m;b%=m;
    if(a>=m-b)return a-(m-b);
    return a+b;
}

static UINT64 vf_u128_mod_u64(OwnU128 a, UINT64 q) {
    UINT64 r,i,bit;
    if(q==0)return 0;
    r=a.hi%q;
    for(i=0;i<64;i++){
        bit=(a.lo>>(63-i))&1ULL;
        if(r > ((~(UINT64)0)-bit)/2ULL){
            UINT64 t=r;
            if(t>=q-t)t=t-(q-t);else t=t+t;
            r=t+bit;if(r>=q)r-=q;
        }else{
            r=(r<<1)|bit;
            if(r>=q)r-=q;
        }
    }
    return r;
}

/* Existing integer recurrence, used only to generate q_k itself. q generation
   is independent of the large successor state p. */
static UINT64 vf_M_q(const UINT64 *q, UINT64 level, UINT64 x,
                     volatile int *cancel, int *ok, OwnSolveStats *stats) {
    UINT64 cur,total=0,d;
    if(!ok||!*ok||!q){if(ok)*ok=0;return 0;}
    if(cancel&&*cancel){*ok=0;return 0;}
    if(level==0){
        cur=x;
        for(;;){
            if(cur==(~(UINT64)0)){*ok=0;return 0;}
            cur++;total++;
            if(stats)stats->divisibility_tests++;
            if((cur%q[0])!=0)return total;
            if(stats)stats->survivor_hops++;
        }
    }
    cur=x;
    for(;;){
        d=vf_M_q(q,level-1,cur,cancel,ok,stats);
        if(!*ok)return 0;
        if(d>(~(UINT64)0)-cur || d>(~(UINT64)0)-total){*ok=0;return 0;}
        cur+=d;total+=d;
        if(stats){stats->survivor_hops++;stats->divisibility_tests++;}
        if((cur%q[level])!=0)return total;
    }
}

/* Residue-space form of M_level(p+offset). The large integer p is absent.
   For fixed q[0..level] this is algebraically equivalent to the full-integer
   recurrence because divisibility depends only on residues modulo q_j. */
static UINT64 vf_M_residue(const UINT64 *q,const UINT64 *r,UINT64 level,
                           UINT64 offset,volatile int *cancel,int *ok,
                           OwnSolveStats *stats) {
    UINT64 total=0,d,m,child_offset;
    if(!ok||!*ok||!q||!r){if(ok)*ok=0;return 0;}
    if(cancel&&*cancel){*ok=0;return 0;}
    if(level==0){
        for(;;){
            if(total==(~(UINT64)0)){*ok=0;return 0;}
            total++;
            if(stats)stats->divisibility_tests++;
            m=vf_mod_add(offset,total,q[0]);
            if(vf_mod_add(r[0],m,q[0])!=0)return total;
            if(stats)stats->survivor_hops++;
        }
    }
    for(;;){
        if(total>(~(UINT64)0)-offset){*ok=0;return 0;}
        child_offset=offset+total;
        d=vf_M_residue(q,r,level-1,child_offset,cancel,ok,stats);
        if(!*ok)return 0;
        if(d>(~(UINT64)0)-total){*ok=0;return 0;}
        total+=d;
        if(stats){stats->survivor_hops++;stats->divisibility_tests++;}
        m=vf_mod_add(offset,total,q[level]);
        if(vf_mod_add(r[level],m,q[level])!=0)return total;
    }
}

static int vf_build_dimensions(VfResidueState *s, UINT64 cap) {
    UINT64 k,g,next;
    int ok=1;
    if(!s||cap<2)return 0;
    s->q[0]=2;
    for(k=0;k+1<cap;k++){
        g=vf_M_q(s->q,k,s->q[k],NULL,&ok,NULL);
        if(!ok || g>(~(UINT64)0)-s->q[k])return 0;
        next=s->q[k]+g;
        if(next<=s->q[k])return 0;
        s->q[k+1]=next;
    }
    s->count=cap;
    return 1;
}

int vf_residue_init(VfResidueState *s, OwnU128 p, UINT64 dimension_cap) {
    HANDLE heap;UINT64 i;
    if(!s||dimension_cap<2||dimension_cap>1048576ULL)return 0;
    s->p=p;s->q=NULL;s->r=NULL;s->count=0;s->capacity=0;
    s->last_gap=0;s->last_correction_rounds=0;
    heap=GetProcessHeap();
    s->q=(UINT64*)HeapAlloc(heap,HEAP_ZERO_MEMORY,(UINT_PTR)(dimension_cap*sizeof(UINT64)));
    s->r=(UINT64*)HeapAlloc(heap,HEAP_ZERO_MEMORY,(UINT_PTR)(dimension_cap*sizeof(UINT64)));
    if(!s->q||!s->r){vf_residue_free(s);return 0;}
    s->capacity=dimension_cap;
    if(!vf_build_dimensions(s,dimension_cap)){vf_residue_free(s);return 0;}
    for(i=0;i<s->count;i++)s->r[i]=vf_u128_mod_u64(p,s->q[i]);
    return 1;
}

void vf_residue_free(VfResidueState *s) {
    HANDLE heap;
    if(!s)return;
    heap=GetProcessHeap();
    if(s->q)HeapFree(heap,0,s->q);
    if(s->r)HeapFree(heap,0,s->r);
    s->q=NULL;s->r=NULL;s->count=0;s->capacity=0;
    s->last_gap=0;s->last_correction_rounds=0;
}

int vf_residue_next(VfResidueState *s, volatile int *cancel,
                    UINT64 *gap, UINT64 *correction_rounds,
                    OwnSolveStats *stats) {
    UINT64 off,level,d,m,i,rounds=0;
    OwnU128 nextp;int ok=1;
    if(!s||!s->q||!s->r||s->count<2||!gap)return 0;
    vf_zero_stats(stats);
    if(stats)stats->dimension_count=s->count;

    off=vf_M_residue(s->q,s->r,0,0,cancel,&ok,stats);
    if(!ok)return 0;
    for(level=1;level<s->count;level++){
        if(cancel&&*cancel)return 0;
        if(stats)stats->layers++;
        for(;;){
            if(stats)stats->divisibility_tests++;
            m=off%s->q[level];
            if(vf_mod_add(s->r[level],m,s->q[level])!=0)break;
            d=vf_M_residue(s->q,s->r,level-1,off,cancel,&ok,stats);
            if(!ok || d>(~(UINT64)0)-off)return 0;
            off+=d;rounds++;
            if(stats)stats->survivor_hops++;
        }
    }

    /* Translation update. All future divisibility state is updated with the
       small gap only; the large p participates only in this final output add. */
    for(i=0;i<s->count;i++)s->r[i]=vf_mod_add(s->r[i],off,s->q[i]);
    if(!vf_add_u64(s->p,off,&nextp))return 0;
    s->p=nextp;s->last_gap=off;s->last_correction_rounds=rounds;
    *gap=off;if(correction_rounds)*correction_rounds=rounds;
    return 1;
}
