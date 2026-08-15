#include "own_solver.h"

/*
  PURE USER GENERATOR ZONE - SURVIVOR RECURSION
  -------------------------------------------------
  No traditional primality test, no factorization routine, no prime table,
  no prime-counting routine and no external next-prime call are available here.

  The q0=2 dimension is evaluated by the same divisibility/survivor rule as
  every later dimension.  There is no parity shortcut and no pre-discard of
  even integers.  Conceptually the pre-dimension successor is x -> x+1; q0
  rejects only when that successor is actually divisible by 2.

  For level k>0, M_k(x) jumps only between states that already survived
  levels 0..k-1.  If the next lower-level survivor is hit by q_k, it keeps
  jumping inside the same level.  Otherwise that survivor distance is returned.

  The same recurrence generates the dimension chain and the requested gap.
*/

static int add_overflow_u64(UINT64 a, UINT64 b, UINT64* out) {
    if (b > (~(UINT64)0) - a) return 1;
    *out = a + b;
    return 0;
}

static int square_gt(UINT64 q, UINT64 x) {
    if (q == 0) return 0;
    return q > x / q;
}

static int base_dimension_u64(UINT64 q, UINT64 x, UINT64* out, OwnSolveStats* stats) {
    UINT64 cur=x,total=0;
    if(!q||!out)return 0;
    for(;;){
        if(cur==(~(UINT64)0))return 0;
        cur++;total++;
        if(stats)stats->divisibility_tests++;
        if((cur%q)!=0){*out=total;return 1;}
        if(stats)stats->survivor_hops++;
    }
}

static void stats_zero(OwnSolveStats* s) {
    if (!s) return;
    s->layers=0; s->dimension_count=0; s->dimensions_generated=0;
    s->survivor_hops=0; s->divisibility_tests=0;
}

static void stats_add(OwnSolveStats* a, const OwnSolveStats* b) {
    if (!a || !b) return;
    a->layers += b->layers;
    if (b->dimension_count > a->dimension_count) a->dimension_count=b->dimension_count;
    a->dimensions_generated += b->dimensions_generated;
    a->survivor_hops += b->survivor_hops;
    a->divisibility_tests += b->divisibility_tests;
}

static int cache_reserve_q(OwnDimensionCache* c, UINT64 need) {
    UINT64 newcap, i;
    UINT64* nq;
    HANDLE heap;
    if (need <= c->capacity) return 1;
    newcap = c->capacity ? c->capacity : 64;
    while (newcap < need) {
        if (newcap > 100000000ULL) return 0;
        newcap *= 2;
    }
    heap=GetProcessHeap();
    nq=(UINT64*)HeapAlloc(heap,0,(UINT_PTR)(newcap*sizeof(UINT64)));
    if(!nq)return 0;
    for(i=0;i<c->count;i++)nq[i]=c->q[i];
    if(c->q)HeapFree(heap,0,c->q);
    c->q=nq;c->capacity=newcap;
    return 1;
}

static int cache_reserve_frames(OwnDimensionCache* c, UINT64 need) {
    UINT64 newcap;
    OwnMFrame* nf;
    HANDLE heap;
    if (need <= c->frame_capacity) return 1;
    newcap=c->frame_capacity?c->frame_capacity:64;
    while(newcap<need){
        if(newcap>100000000ULL)return 0;
        newcap*=2;
    }
    heap=GetProcessHeap();
    nf=(OwnMFrame*)HeapAlloc(heap,0,(UINT_PTR)(newcap*sizeof(OwnMFrame)));
    if(!nf)return 0;
    if(c->frames)HeapFree(heap,0,c->frames);
    c->frames=nf;c->frame_capacity=newcap;
    return 1;
}

int own_cache_init(OwnDimensionCache* c) {
    if(!c)return 0;
    c->q=NULL;c->count=0;c->capacity=0;c->frames=NULL;c->frame_capacity=0;
    if(!cache_reserve_q(c,64))return 0;
    if(!cache_reserve_frames(c,64)){HeapFree(GetProcessHeap(),0,c->q);c->q=NULL;return 0;}
    c->q[0]=2;c->count=1;
    return 1;
}

void own_cache_free(OwnDimensionCache* c) {
    if(!c)return;
    if(c->q)HeapFree(GetProcessHeap(),0,c->q);
    if(c->frames)HeapFree(GetProcessHeap(),0,c->frames);
    c->q=NULL;c->frames=NULL;c->count=0;c->capacity=0;c->frame_capacity=0;
}

/* Exact explicit-stack evaluation of the nested M_level(x) recurrence. */
static int own_M(OwnDimensionCache* cache, UINT64 level, UINT64 x,
                 volatile int* cancel, UINT64* out, OwnSolveStats* stats) {
    UINT64 depth, ret=0;
    if(!cache || level>=cache->count || !out)return 0;
    if(!cache_reserve_frames(cache,level+1))return 0;
    depth=1;
    cache->frames[0].level=level;
    cache->frames[0].cur=x;
    cache->frames[0].total=0;
    cache->frames[0].waiting=0;
    while(depth){
        OwnMFrame* f;
        if(cancel && *cancel)return 0;
        f=&cache->frames[depth-1];
        if(f->level==0){
            if(!base_dimension_u64(cache->q[0],f->cur,&ret,stats))return 0;
            depth--;
            continue;
        }
        if(!f->waiting){
            f->waiting=1;
            cache->frames[depth].level=f->level-1;
            cache->frames[depth].cur=f->cur;
            cache->frames[depth].total=0;
            cache->frames[depth].waiting=0;
            depth++;
            continue;
        }
        {
            UINT64 nc, nt;
            if(add_overflow_u64(f->total,ret,&nt))return 0;
            if(add_overflow_u64(f->cur,ret,&nc))return 0;
            f->total=nt;f->cur=nc;
            if(stats)stats->survivor_hops++;
            if(stats)stats->divisibility_tests++;
            if((f->cur % cache->q[f->level])!=0){
                ret=f->total;
                depth--;
            } else {
                f->waiting=0;
            }
        }
    }
    *out=ret;
    return 1;
}

/*
  Compute a next-survivor gap using only dimensions already present in cache.
  This is used to grow the dimension chain itself; because the input is the
  current largest generated dimension, lower dimensions are sufficient to
  certify its next survivor before cache exhaustion.
*/
static int gap_existing(UINT64 p, OwnDimensionCache* cache, UINT64 available,
                        volatile int* cancel, UINT64* gap, OwnSolveStats* stats) {
    UINT64 candidate, step, i;
    if(!cache || available<1 || available>cache->count)return 0;
    if(!own_M(cache,0,p,cancel,&step,stats))return 0;
    if(add_overflow_u64(p,step,&candidate))return 0;
    if(square_gt(2,candidate)){*gap=step;return 1;}
    for(i=1;i<available;i++){
        UINT64 q=cache->q[i];
        if(stats)stats->layers++;
        for(;;){
            if(cancel&&*cancel)return 0;
            if(stats)stats->divisibility_tests++;
            if((candidate%q)!=0)break;
            if(!own_M(cache,i-1,candidate,cancel,&step,stats))return 0;
            if(add_overflow_u64(candidate,step,&candidate))return 0;
            if(stats)stats->survivor_hops++;
        }
        if(square_gt(q,candidate)){
            *gap=candidate-p;
            return 1;
        }
    }
    return 0;
}

static int ensure_dimension(OwnDimensionCache* cache, UINT64 index,
                            volatile int* cancel, OwnSolveStats* stats) {
    while(cache->count<=index){
        UINT64 last=cache->q[cache->count-1], g, next;
        OwnSolveStats local;
        stats_zero(&local);
        if(!gap_existing(last,cache,cache->count,cancel,&g,&local))return 0;
        if(add_overflow_u64(last,g,&next))return 0;
        if(!cache_reserve_q(cache,cache->count+1))return 0;
        cache->q[cache->count++]=next;
        local.dimensions_generated++;
        local.dimension_count=cache->count;
        stats_add(stats,&local);
    }
    return 1;
}

int own_next_gap(UINT64 p, OwnDimensionCache* cache, volatile int* cancel,
                 UINT64* gap, OwnSolveStats* stats) {
    UINT64 candidate, step, i;
    OwnSolveStats local;
    if(!cache || !gap || p<2)return 0;
    stats_zero(&local);
    if(!own_M(cache,0,p,cancel,&step,&local))return 0;
    if(add_overflow_u64(p,step,&candidate))return 0;
    if(square_gt(2,candidate)){
        *gap=step;local.dimension_count=cache->count;if(stats)*stats=local;return 1;
    }
    i=1;
    for(;;){
        UINT64 q;
        if(cancel&&*cancel)return 0;
        if(!ensure_dimension(cache,i,cancel,&local))return 0;
        q=cache->q[i];
        local.layers++;
        for(;;){
            if(local.divisibility_tests==(~(UINT64)0))return 0;
            local.divisibility_tests++;
            if((candidate%q)!=0)break;
            if(!own_M(cache,i-1,candidate,cancel,&step,&local))return 0;
            if(add_overflow_u64(candidate,step,&candidate))return 0;
            local.survivor_hops++;
        }
        if(square_gt(q,candidate)){
            *gap=candidate-p;
            local.dimension_count=i+1;
            if(stats)*stats=local;
            return 1;
        }
        i++;
    }
}

int own_self_bootstrap(UINT64 n, volatile int* cancel, UINT64* p,
                       OwnSolveStats* aggregate) {
    UINT64 rank=1,cur=2;
    OwnSolveStats sum;
    stats_zero(&sum);
    if(n<1 || !p)return 0;
    while(rank<n){
        OwnDimensionCache cache;
        UINT64 g;
        OwnSolveStats st;
        if(cancel&&*cancel)return 0;
        /* Fully implicit Self Bootstrap contract: each successor starts again
           from q0=2.  No prime-dimension basis survives across prime steps. */
        if(!own_cache_init(&cache))return 0;
        if(!own_next_gap(cur,&cache,cancel,&g,&st)){own_cache_free(&cache);return 0;}
        own_cache_free(&cache);
        if(add_overflow_u64(cur,g,&cur))return 0;
        rank++;
        stats_add(&sum,&st);
    }
    *p=cur;
    if(aggregate)*aggregate=sum;
    return 1;
}

/* -------------------------------------------------------------------------
   EXPERIMENTAL 128-BIT FULLY IMPLICIT DIMENSION RECURSION
   -------------------------------------------------------------------------
   This path intentionally contains no prebuilt prime-dimension basis, no q[]
   cache and no candidate interval.  A dimension exists only as the q field of
   a live recursion frame.  q_{k+1}=q_k+M_k(q_k) is generated on demand inside
   the same local frame stack that evaluates the survivor operator.

   IMPORTANT: the causal-depth cap is an experimental resource contract, not a
   proof that exact prime generation has O(1) sequential depth.  Outputs after
   the externally established seed are therefore PROVISIONAL / UNVERIFIED.
*/

typedef struct OwnImplicitFrame {
    UINT64 q;              /* dimension carried by this live recursion frame */
    int q_ready;
} OwnImplicitFrame;

typedef struct OwnImplicitMFrame64 {
    UINT64 level;
    UINT64 cur;
    UINT64 total;
    int waiting;
} OwnImplicitMFrame64;

typedef struct OwnImplicitMFrame128 {
    UINT64 level;
    OwnU128 cur;
    UINT64 total;
    int waiting;
} OwnImplicitMFrame128;

static int u128_add_u64(OwnU128 a, UINT64 b, OwnU128* out) {
    OwnU128 r;
    r.lo=a.lo+b;
    r.hi=a.hi+(r.lo<a.lo?1ULL:0ULL);
    if(r.hi<a.hi)return 0;
    *out=r;return 1;
}

static int u128_inc(OwnU128* a) {
    UINT64 old=a->lo;a->lo++;if(a->lo<old){a->hi++;if(a->hi==0)return 0;}return 1;
}

static UINT64 u128_mod_u64(OwnU128 a, UINT64 q) {
    UINT64 r,i,bit;
    if(q==0)return 0;
    r=a.hi%q;
    for(i=0;i<64;i++){
        bit=(a.lo>>(63-i))&1ULL;
        if(r > ((~(UINT64)0)-bit)/2ULL){
            /* Overflow-free fallback: double modulo q using subtraction. */
            UINT64 t=r;
            if(t>=q-t)t=t-(q-t); else t=t+t;
            r=t+bit; if(r>=q)r-=q;
        } else {
            r=(r<<1)|bit;
            if(r>=q)r-=q;
        }
    }
    return r;
}

static int base_dimension_u128(UINT64 q, OwnU128 x, UINT64* out, OwnSolveStats* stats) {
    OwnU128 cur=x;UINT64 total=0;
    if(!q||!out)return 0;
    for(;;){
        if(!u128_inc(&cur))return 0;
        total++;
        if(stats)stats->divisibility_tests++;
        if(u128_mod_u64(cur,q)!=0){*out=total;return 1;}
        if(stats)stats->survivor_hops++;
    }
}

static int implicit_M64(const OwnImplicitFrame* dims, UINT64 dim_count,
                        OwnImplicitMFrame64* stack, UINT64 stack_cap,
                        UINT64 level, UINT64 x, volatile int* cancel,
                        UINT64* out, OwnSolveStats* stats, UINT64* peak_depth) {
    UINT64 depth=1,ret=0;
    if(!dims||!stack||!out||level>=dim_count||stack_cap<=level)return 0;
    stack[0].level=level;stack[0].cur=x;stack[0].total=0;stack[0].waiting=0;
    if(peak_depth&&*peak_depth<1)*peak_depth=1;
    while(depth){
        OwnImplicitMFrame64* f;
        if(cancel&&*cancel)return 0;
        if(peak_depth&&depth>*peak_depth)*peak_depth=depth;
        f=&stack[depth-1];
        if(f->level==0){if(!base_dimension_u64(dims[0].q,f->cur,&ret,stats))return 0;depth--;continue;}
        if(!f->waiting){
            if(depth>=stack_cap)return 0;
            f->waiting=1;
            stack[depth].level=f->level-1;stack[depth].cur=f->cur;
            stack[depth].total=0;stack[depth].waiting=0;depth++;continue;
        }
        {
            UINT64 nc,nt;
            if(add_overflow_u64(f->total,ret,&nt))return 0;
            if(add_overflow_u64(f->cur,ret,&nc))return 0;
            f->total=nt;f->cur=nc;
            if(stats){stats->survivor_hops++;stats->divisibility_tests++;}
            if((f->cur%dims[f->level].q)!=0){ret=f->total;depth--;}
            else f->waiting=0;
        }
    }
    *out=ret;return 1;
}

static int implicit_M128(const OwnImplicitFrame* dims, UINT64 dim_count,
                         OwnImplicitMFrame128* stack, UINT64 stack_cap,
                         UINT64 level, OwnU128 x, volatile int* cancel,
                         UINT64* out, OwnSolveStats* stats, UINT64* peak_depth) {
    UINT64 depth=1,ret=0;
    if(!dims||!stack||!out||level>=dim_count||stack_cap<=level)return 0;
    stack[0].level=level;stack[0].cur=x;stack[0].total=0;stack[0].waiting=0;
    if(peak_depth&&*peak_depth<1)*peak_depth=1;
    while(depth){
        OwnImplicitMFrame128* f;
        if(cancel&&*cancel)return 0;
        if(peak_depth&&depth>*peak_depth)*peak_depth=depth;
        f=&stack[depth-1];
        if(f->level==0){if(!base_dimension_u128(dims[0].q,f->cur,&ret,stats))return 0;depth--;continue;}
        if(!f->waiting){
            if(depth>=stack_cap)return 0;
            f->waiting=1;
            stack[depth].level=f->level-1;stack[depth].cur=f->cur;
            stack[depth].total=0;stack[depth].waiting=0;depth++;continue;
        }
        {
            OwnU128 nc;UINT64 nt=f->total+ret;
            if(nt<f->total)return 0;
            if(!u128_add_u64(f->cur,ret,&nc))return 0;
            f->total=nt;f->cur=nc;
            if(stats){stats->survivor_hops++;stats->divisibility_tests++;}
            if(u128_mod_u64(f->cur,dims[f->level].q)!=0){ret=f->total;depth--;}
            else f->waiting=0;
        }
    }
    *out=ret;return 1;
}

/* Generate q_level inside the recursion frame stack. Nothing is prepared in
   advance. q values exist only for this one successor operation. */
static int implicit_ensure_dimension(OwnImplicitFrame* dims, UINT64 cap,
                                     OwnImplicitMFrame64* mstack,
                                     UINT64 level, volatile int* cancel,
                                     OwnSolveStats* stats, UINT64* peak_depth) {
    UINT64 i;
    if(!dims||level>=cap)return 0;
    if(!dims[0].q_ready){dims[0].q=2;dims[0].q_ready=1;}
    for(i=1;i<=level;i++){
        UINT64 g,next;
        if(dims[i].q_ready)continue;
        if(!dims[i-1].q_ready)return 0;
        if(!implicit_M64(dims,i,mstack,cap,i-1,dims[i-1].q,cancel,&g,stats,peak_depth))return 0;
        if(add_overflow_u64(dims[i-1].q,g,&next))return 0;
        dims[i].q=next;dims[i].q_ready=1;
        if(stats){stats->dimensions_generated++;stats->dimension_count=i+1;}
    }
    return 1;
}

/* One bounded successor step.  There is no q-array/basis outside this call.
   The only dimension storage is the live recursion-frame stack itself. */
static int implicit128_next(OwnU128 p, UINT64 depth_cap, volatile int* cancel,
                            UINT64* gap, OwnSolveStats* stats, UINT64* peak_depth) {
    OwnImplicitFrame* dims=NULL;
    OwnImplicitMFrame64* s64=NULL;
    OwnImplicitMFrame128* s128=NULL;
    HANDLE heap=GetProcessHeap();
    OwnU128 candidate;UINT64 step,level;
    if(!gap||depth_cap<2)return 0;
    dims=(OwnImplicitFrame*)HeapAlloc(heap,HEAP_ZERO_MEMORY,(UINT_PTR)(depth_cap*sizeof(OwnImplicitFrame)));
    s64=(OwnImplicitMFrame64*)HeapAlloc(heap,HEAP_ZERO_MEMORY,(UINT_PTR)(depth_cap*sizeof(OwnImplicitMFrame64)));
    s128=(OwnImplicitMFrame128*)HeapAlloc(heap,HEAP_ZERO_MEMORY,(UINT_PTR)(depth_cap*sizeof(OwnImplicitMFrame128)));
    if(!dims||!s64||!s128)goto fail;
    dims[0].q=2;dims[0].q_ready=1;
    if(stats){stats->dimension_count=1;}
    if(!implicit_M128(dims,1,s128,depth_cap,0,p,cancel,&step,stats,peak_depth))goto fail;
    if(!u128_add_u64(p,step,&candidate))goto fail;
    for(level=1;level<depth_cap;level++){
        UINT64 q;
        if(cancel&&*cancel)goto fail;
        if(!implicit_ensure_dimension(dims,depth_cap,s64,level,cancel,stats,peak_depth))goto fail;
        q=dims[level].q;
        if(stats){stats->layers++;stats->divisibility_tests++;}
        while(u128_mod_u64(candidate,q)==0){
            if(!implicit_M128(dims,level,s128,depth_cap,level-1,candidate,cancel,&step,stats,peak_depth))goto fail;
            if(!u128_add_u64(candidate,step,&candidate))goto fail;
            if(stats){stats->survivor_hops++;stats->divisibility_tests++;}
        }
    }
    {
        UINT64 lo=candidate.lo-p.lo;
        UINT64 borrow=(candidate.lo<p.lo)?1ULL:0ULL;
        UINT64 dhi=candidate.hi-p.hi-borrow;
        if(dhi!=0)goto fail;
        *gap=lo;
    }
    HeapFree(heap,0,s128);HeapFree(heap,0,s64);HeapFree(heap,0,dims);return 1;
fail:
    if(s128)HeapFree(heap,0,s128);if(s64)HeapFree(heap,0,s64);if(dims)HeapFree(heap,0,dims);return 0;
}

int own_record_explore(OwnU128 seed_rank, OwnU128 seed_p, UINT64 causal_depth_cap,
                       volatile int* cancel, OwnRecordProgressFn progress, void* user,
                       OwnRecordExploreResult* out) {
    OwnSolveStats sum,st;OwnU128 rank=seed_rank,p=seed_p,next;
    UINT64 g=0,t0,last,step_count=0,peak=0;
    if(!out)return 0;
    stats_zero(&sum);
    if(causal_depth_cap<4)causal_depth_cap=4;
    if(causal_depth_cap>256)causal_depth_cap=256;
    t0=GetTickCount64();last=t0;
    for(;;){
        OwnRecordExploreResult snap;UINT64 local_peak=0;
        if(cancel&&*cancel){out->end_reason=1;break;}
        stats_zero(&st);
        if(!implicit128_next(p,causal_depth_cap,cancel,&g,&st,&local_peak)){out->end_reason=2;break;}
        if(!u128_add_u64(p,g,&next)){out->end_reason=2;break;}
        p=next;if(!u128_inc(&rank)){out->end_reason=2;break;}
        stats_add(&sum,&st);if(local_peak>peak)peak=local_peak;step_count++;
        if(progress&&(GetTickCount64()-last>=200)){
            snap.rank=rank;snap.p=p;snap.gap=g;snap.elapsed_ms=GetTickCount64()-t0;
            snap.causal_depth_cap=causal_depth_cap;snap.stats=sum;snap.end_reason=0;
            /* dimension_count is used here as measured peak live recursive depth. */
            snap.stats.dimension_count=peak;
            progress(&snap,user);last=GetTickCount64();
        }
        if(step_count==(~(UINT64)0)){out->end_reason=2;break;}
    }
    out->rank=rank;out->p=p;out->gap=g;out->elapsed_ms=GetTickCount64()-t0;
    out->causal_depth_cap=causal_depth_cap;out->stats=sum;out->stats.dimension_count=peak;
    return 1;
}
