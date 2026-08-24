#define _POSIX_C_SOURCE 200809L
#include <gmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef uint32_t u32;

static double now_s(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC,&t);
    return (double)t.tv_sec + 1e-9*(double)t.tv_nsec;
}

typedef struct PrimeFixture {
    u32 *p;
    size_t n;
} PrimeFixture;

static PrimeFixture build_prime_fixture(u32 limit) {
    unsigned char *composite=(unsigned char*)calloc((size_t)limit+1,1);
    size_t cap=256,n=0;
    u32 *p=(u32*)malloc(cap*sizeof(u32));
    u32 i;
    if(!composite||!p){fprintf(stderr,"allocation failure\n");exit(2);}
    for(i=2;i<=limit;i++) {
        if(!composite[i]) {
            uint64_t j;
            if(n==cap){cap*=2;p=(u32*)realloc(p,cap*sizeof(u32));if(!p)exit(2);}
            p[n++]=i;
            if((uint64_t)i*(uint64_t)i<=limit)
                for(j=(uint64_t)i*(uint64_t)i;j<=limit;j+=i)composite[j]=1;
        }
    }
    free(composite);
    {PrimeFixture r={p,n};return r;}
}

typedef struct ProductTree {
    mpz_t *node;
    size_t size;
    size_t n;
    const u32 *prime;
} ProductTree;

static void product_tree_build(ProductTree *t,const PrimeFixture *pf) {
    size_t i,s=1;
    while(s<pf->n)s<<=1;
    t->size=s;t->n=pf->n;t->prime=pf->p;
    t->node=(mpz_t*)malloc(2*s*sizeof(mpz_t));
    if(!t->node)exit(2);
    for(i=0;i<2*s;i++)mpz_init(t->node[i]);
    for(i=0;i<s;i++) {
        if(i<pf->n)mpz_set_ui(t->node[s+i],pf->p[i]);
        else mpz_set_ui(t->node[s+i],1);
    }
    for(i=s-1;i;i--)mpz_mul(t->node[i],t->node[i<<1],t->node[(i<<1)|1]);
}

static void product_tree_free(ProductTree *t) {
    size_t i;
    if(!t||!t->node)return;
    for(i=0;i<2*t->size;i++)mpz_clear(t->node[i]);
    free(t->node);t->node=NULL;
}

typedef struct EventQuery {
    size_t index;
    unsigned gcd_probes;
    int found;
} EventQuery;

static int find_first_hit_rec(const ProductTree *t,const mpz_t candidate,
                              size_t start,size_t node,size_t left,size_t right,
                              mpz_t scratch,unsigned *probes,size_t *out_index) {
    size_t middle;
    if(right<=start||left>=t->n)return 0;
    if(left>=start) {
        (*probes)++;
        mpz_gcd(scratch,candidate,t->node[node]);
        if(mpz_cmp_ui(scratch,1)==0)return 0;
    }
    if(right-left==1){*out_index=left;return 1;}
    middle=(left+right)>>1;
    if(find_first_hit_rec(t,candidate,start,node<<1,left,middle,scratch,probes,out_index))return 1;
    return find_first_hit_rec(t,candidate,start,(node<<1)|1,middle,right,scratch,probes,out_index);
}

static EventQuery find_first_hit(const ProductTree *t,const mpz_t candidate,size_t start) {
    EventQuery q={0,0,0};
    mpz_t scratch;
    mpz_init(scratch);
    q.found=find_first_hit_rec(t,candidate,start,1,0,t->size,scratch,&q.gcd_probes,&q.index);
    mpz_clear(scratch);
    return q;
}

static void make_candidate(mpz_t candidate,unsigned gap) {
    mpz_ui_pow_ui(candidate,10,100);
    mpz_add_ui(candidate,candidate,267+gap);
}

int main(void) {
    const u32 prime_limit=3000000U;
    const struct {size_t start;unsigned gap;} cases[]={{2,4},{88231,6},{175692,22}};
    PrimeFixture pf;
    ProductTree tree;
    mpz_t candidate,root_mod,g;
    double t0,fixture_s,tree_s,query_s;
    size_t i;

    t0=now_s();pf=build_prime_fixture(prime_limit);fixture_s=now_s()-t0;
    t0=now_s();product_tree_build(&tree,&pf);tree_s=now_s()-t0;

    printf("prime_limit=%u primes=%zu fixture_ms=%.3f tree_ms=%.3f root_bits=%zu\n",
           prime_limit,pf.n,fixture_s*1e3,tree_s*1e3,mpz_sizeinbase(tree.node[1],2));

    mpz_inits(candidate,root_mod,g,NULL);
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        EventQuery q;
        make_candidate(candidate,cases[i].gap);
        t0=now_s();q=find_first_hit(&tree,candidate,cases[i].start);query_s=now_s()-t0;

        /* Exact identity: gcd(c,P)=gcd(c,P mod c).  This shows that the
           aggregate collision certificate itself need never exceed the
           candidate bit width, even though this diagnostic tree stores the
           full products so that arbitrary subranges can be queried. */
        mpz_mod(root_mod,tree.node[1],candidate);
        mpz_gcd(g,candidate,root_mod);

        if(q.found) {
            printf("gap=%u start_index0=%zu hit_index0=%zu active_width_K=%zu q=%u gcd_probes=%u query_ms=%.3f root_gcd=%s\n",
                   cases[i].gap,cases[i].start,q.index,q.index+1,pf.p[q.index],
                   q.gcd_probes,query_s*1e3,mpz_cmp_ui(g,1)==0?"1":">1");
        } else {
            printf("gap=%u start_index0=%zu no_hit_through_%u gcd_probes=%u query_ms=%.3f root_gcd=%s\n",
                   cases[i].gap,cases[i].start,prime_limit,q.gcd_probes,query_s*1e3,
                   mpz_cmp_ui(g,1)==0?"1":">1");
        }
    }

    mpz_clears(candidate,root_mod,g,NULL);
    product_tree_free(&tree);
    free(pf.p);
    return 0;
}
