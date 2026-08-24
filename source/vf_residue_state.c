#include "vf_residue_state.h"

typedef struct VfFrame {
    UINT64 level;
    UINT64 cur;
    UINT64 total;
    int waiting;
} VfFrame;

static void vf_zero_stats(OwnSolveStats *s){if(!s)return;s->layers=0;s->dimension_count=0;s->dimensions_generated=0;s->survivor_hops=0;s->divisibility_tests=0;}
static int vf_add_u64(OwnU128 a,UINT64 b,OwnU128*out){OwnU128 r;r.lo=a.lo+b;r.hi=a.hi+(r.lo<a.lo?1ULL:0ULL);if(r.hi<a.hi)return 0;*out=r;return 1;}
static UINT64 vf_mod_add(UINT64 a,UINT64 b,UINT64 m){if(!m)return 0;a%=m;b%=m;if(a>=m-b)return a-(m-b);return a+b;}
static UINT64 vf_u128_mod_u64(OwnU128 a,UINT64 q){UINT64 r,i,bit;if(!q)return 0;r=a.hi%q;for(i=0;i<64;i++){bit=(a.lo>>(63-i))&1ULL;if(r>((~(UINT64)0)-bit)/2ULL){UINT64 t=r;if(t>=q-t)t=t-(q-t);else t=t+t;r=t+bit;if(r>=q)r-=q;}else{r=(r<<1)|bit;if(r>=q)r-=q;}}return r;}

static int vf_base_q(UINT64 q,UINT64 x,UINT64*out,OwnSolveStats*stats){UINT64 cur=x,total=0;if(!q||!out)return 0;for(;;){if(cur==(~(UINT64)0))return 0;cur++;total++;if(stats)stats->divisibility_tests++;if(cur%q!=0){*out=total;return 1;}if(stats)stats->survivor_hops++;}}
static int vf_base_residue(UINT64 q,UINT64 residue,UINT64 offset,UINT64*out,OwnSolveStats*stats){UINT64 d=0;if(!q||!out)return 0;for(;;){if(d==(~(UINT64)0))return 0;d++;if(stats)stats->divisibility_tests++;if(vf_mod_add(residue,vf_mod_add(offset,d,q),q)!=0){*out=d;return 1;}if(stats)stats->survivor_hops++;}}

/* Explicit-stack M for q-chain generation. No C call-stack growth with level. */
static int vf_M_q(VfResidueState*s,UINT64 level,UINT64 x,volatile int*cancel,UINT64*out,OwnSolveStats*stats){
    VfFrame*f;UINT64 depth=1,ret=0;if(!s||!s->frames||level>=s->capacity||!out)return 0;f=(VfFrame*)s->frames;f[0].level=level;f[0].cur=x;f[0].total=0;f[0].waiting=0;
    while(depth){VfFrame*t;if(cancel&&*cancel)return 0;t=&f[depth-1];if(t->level==0){if(!vf_base_q(s->q[0],t->cur,&ret,stats))return 0;depth--;continue;}if(!t->waiting){if(depth>=s->capacity)return 0;t->waiting=1;f[depth].level=t->level-1;f[depth].cur=t->cur;f[depth].total=0;f[depth].waiting=0;depth++;continue;}if(ret>(~(UINT64)0)-t->total||ret>(~(UINT64)0)-t->cur)return 0;t->total+=ret;t->cur+=ret;if(stats){stats->survivor_hops++;stats->divisibility_tests++;}if(t->cur%s->q[t->level]!=0){ret=t->total;depth--;}else t->waiting=0;}
    *out=ret;return 1;
}

/* Explicit-stack M in residue space. The large p is absent. */
static int vf_M_residue(VfResidueState*s,UINT64 level,UINT64 offset,volatile int*cancel,UINT64*out,OwnSolveStats*stats){
    VfFrame*f;UINT64 depth=1,ret=0;if(!s||!s->frames||!s->r||level>=s->count||!out)return 0;f=(VfFrame*)s->frames;f[0].level=level;f[0].cur=offset;f[0].total=0;f[0].waiting=0;
    while(depth){VfFrame*t;UINT64 m;if(cancel&&*cancel)return 0;t=&f[depth-1];if(t->level==0){if(!vf_base_residue(s->q[0],s->r[0],t->cur,&ret,stats))return 0;depth--;continue;}if(!t->waiting){if(depth>=s->capacity)return 0;t->waiting=1;f[depth].level=t->level-1;f[depth].cur=t->cur;f[depth].total=0;f[depth].waiting=0;depth++;continue;}if(ret>(~(UINT64)0)-t->total||ret>(~(UINT64)0)-t->cur)return 0;t->total+=ret;t->cur+=ret;if(stats){stats->survivor_hops++;stats->divisibility_tests++;}m=t->cur%s->q[t->level];if(vf_mod_add(s->r[t->level],m,s->q[t->level])!=0){ret=t->total;depth--;}else t->waiting=0;}
    *out=ret;return 1;
}

static int vf_build_dimensions(VfResidueState*s,UINT64 cap){UINT64 k,g,next;if(!s||cap<2)return 0;s->q[0]=2;for(k=0;k+1<cap;k++){s->count=k+1;if(!vf_M_q(s,k,s->q[k],NULL,&g,NULL))return 0;if(g>(~(UINT64)0)-s->q[k])return 0;next=s->q[k]+g;if(next<=s->q[k])return 0;s->q[k+1]=next;}s->count=cap;return 1;}

int vf_residue_init(VfResidueState*s,OwnU128 p,UINT64 dimension_cap){HANDLE heap;UINT64 i;if(!s||dimension_cap<2||dimension_cap>1048576ULL)return 0;s->p=p;s->q=NULL;s->r=NULL;s->frames=NULL;s->count=0;s->capacity=0;s->last_gap=0;s->last_correction_rounds=0;heap=GetProcessHeap();s->q=(UINT64*)HeapAlloc(heap,HEAP_ZERO_MEMORY,(UINT_PTR)(dimension_cap*sizeof(UINT64)));s->r=(UINT64*)HeapAlloc(heap,HEAP_ZERO_MEMORY,(UINT_PTR)(dimension_cap*sizeof(UINT64)));s->frames=HeapAlloc(heap,HEAP_ZERO_MEMORY,(UINT_PTR)(dimension_cap*sizeof(VfFrame)));if(!s->q||!s->r||!s->frames){vf_residue_free(s);return 0;}s->capacity=dimension_cap;if(!vf_build_dimensions(s,dimension_cap)){vf_residue_free(s);return 0;}for(i=0;i<s->count;i++)s->r[i]=vf_u128_mod_u64(p,s->q[i]);return 1;}
void vf_residue_free(VfResidueState*s){HANDLE heap;if(!s)return;heap=GetProcessHeap();if(s->q)HeapFree(heap,0,s->q);if(s->r)HeapFree(heap,0,s->r);if(s->frames)HeapFree(heap,0,s->frames);s->q=NULL;s->r=NULL;s->frames=NULL;s->count=0;s->capacity=0;s->last_gap=0;s->last_correction_rounds=0;}
int vf_residue_next(VfResidueState*s,volatile int*cancel,UINT64*gap,UINT64*correction_rounds,OwnSolveStats*stats){UINT64 off,level,d,m,i,rounds=0;OwnU128 nextp;if(!s||!s->q||!s->r||!s->frames||s->count<2||!gap)return 0;vf_zero_stats(stats);if(stats)stats->dimension_count=s->count;if(!vf_M_residue(s,0,0,cancel,&off,stats))return 0;for(level=1;level<s->count;level++){if(cancel&&*cancel)return 0;if(stats)stats->layers++;for(;;){if(stats)stats->divisibility_tests++;m=off%s->q[level];if(vf_mod_add(s->r[level],m,s->q[level])!=0)break;if(!vf_M_residue(s,level-1,off,cancel,&d,stats))return 0;if(d>(~(UINT64)0)-off)return 0;off+=d;rounds++;if(stats)stats->survivor_hops++;}}for(i=0;i<s->count;i++)s->r[i]=vf_mod_add(s->r[i],off,s->q[i]);if(!vf_add_u64(s->p,off,&nextp))return 0;s->p=nextp;s->last_gap=off;s->last_correction_rounds=rounds;*gap=off;if(correction_rounds)*correction_rounds=rounds;return 1;}
