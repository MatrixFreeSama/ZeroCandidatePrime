#define _POSIX_C_SOURCE 200809L
#include <gmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef unsigned long long u64;

typedef struct QChain {
    u64 *q;
    size_t n;
} QChain;

static double now_s(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC,&t);
    return (double)t.tv_sec + 1e-9*(double)t.tv_nsec;
}

static u64 M_u64(const u64 *q,size_t level,u64 x) {
    if(level==0) {
        u64 d=0,cur=x;
        do { cur++; d++; } while(cur%q[0]==0);
        return d;
    }
    {
        u64 total=0,cur=x;
        for(;;) {
            u64 d=M_u64(q,level-1,cur);
            cur+=d; total+=d;
            if(cur%q[level]!=0) return total;
        }
    }
}

static void gen_q(QChain *qc,size_t K) {
    size_t k;
    qc->q=(u64*)calloc(K,sizeof(u64));
    qc->n=K;
    qc->q[0]=2;
    for(k=0;k+1<K;k++) qc->q[k+1]=qc->q[k]+M_u64(qc->q,k,qc->q[k]);
}

static u64 M_res(const u64 *q,const u64 *r,size_t level,u64 off) {
    if(level==0) {
        u64 d=0;
        do { d++; } while((r[0]+((off+d)%q[0]))%q[0]==0);
        return d;
    }
    {
        u64 total=0;
        for(;;) {
            u64 d=M_res(q,r,level-1,off+total);
            total+=d;
            if((r[level]+((off+total)%q[level]))%q[level]!=0) return total;
        }
    }
}

static u64 next_gap_res(const u64 *q,const u64 *r,size_t K) {
    size_t level;
    u64 off=M_res(q,r,0,0);
    for(level=1;level<K;level++) {
        while((r[level]+(off%q[level]))%q[level]==0)
            off+=M_res(q,r,level-1,off);
    }
    return off;
}

static u64 M_mpz(const u64 *q,size_t level,const mpz_t x) {
    if(level==0) {
        mpz_t cur;
        u64 d=0;
        mpz_init_set(cur,x);
        do { mpz_add_ui(cur,cur,1); d++; } while(mpz_divisible_ui_p(cur,q[0]));
        mpz_clear(cur);
        return d;
    }
    {
        mpz_t cur;
        u64 total=0;
        mpz_init_set(cur,x);
        for(;;) {
            u64 d=M_mpz(q,level-1,cur);
            mpz_add_ui(cur,cur,d);
            total+=d;
            if(!mpz_divisible_ui_p(cur,q[level])) {
                mpz_clear(cur);
                return total;
            }
        }
    }
}

static u64 next_gap_mpz(const u64 *q,size_t K,const mpz_t p) {
    size_t level;
    mpz_t candidate;
    u64 off;
    mpz_init_set(candidate,p);
    off=M_mpz(q,0,p);
    mpz_add_ui(candidate,candidate,off);
    for(level=1;level<K;level++) {
        while(mpz_divisible_ui_p(candidate,q[level])) {
            u64 d=M_mpz(q,level-1,candidate);
            mpz_add_ui(candidate,candidate,d);
            off+=d;
        }
    }
    mpz_clear(candidate);
    return off;
}

static void init_res(u64 *r,const u64 *q,size_t K,const mpz_t p) {
    size_t i;
    for(i=0;i<K;i++) r[i]=mpz_fdiv_ui(p,q[i]);
}

static void advance_res(u64 *r,const u64 *q,size_t K,u64 g) {
    size_t i;
    for(i=0;i<K;i++) r[i]=(r[i]+(g%q[i]))%q[i];
}

static void make_p(mpz_t p,unsigned exponent) {
    mpz_ui_pow_ui(p,10,exponent);
    mpz_add_ui(p,p,267);
}

int main(int argc,char **argv) {
    size_t K=argc>1?(size_t)strtoull(argv[1],0,10):256;
    unsigned exponent=argc>2?(unsigned)strtoul(argv[2],0,10):100;
    size_t reps=argc>3?(size_t)strtoull(argv[3],0,10):1000;
    QChain qc;
    mpz_t p,chainp;
    u64 *r,gr,gm,checksum=0,same_checksum=0;
    double t0,tq,ti,tr,tm,ts,trr;
    size_t it;

    t0=now_s();
    gen_q(&qc,K);
    tq=now_s()-t0;

    mpz_init(p);
    make_p(p,exponent);
    r=(u64*)calloc(K,sizeof(u64));

    t0=now_s();
    init_res(r,qc.q,K,p);
    ti=now_s()-t0;

    t0=now_s();
    gr=next_gap_res(qc.q,r,K);
    tr=now_s()-t0;

    t0=now_s();
    gm=next_gap_mpz(qc.q,K,p);
    tm=now_s()-t0;

    printf("K=%zu exp10=%u qmax=%llu gap_res=%llu gap_mpz=%llu\n",
           K,exponent,qc.q[K-1],gr,gm);
    printf("qgen_us=%.3f init_res_us=%.3f res_one_us=%.3f mpz_one_us=%.3f\n",
           tq*1e6,ti*1e6,tr*1e6,tm*1e6);

    t0=now_s();
    for(it=0;it<reps;it++) same_checksum^=next_gap_res(qc.q,r,K);
    trr=now_s()-t0;
    printf("same_state_reps=%zu per_call_us=%.3f checksum=%llu\n",
           reps,trr*1e6/(double)reps,same_checksum);

    mpz_init_set(chainp,p);
    t0=now_s();
    for(it=0;it<reps;it++) {
        u64 g=next_gap_res(qc.q,r,K);
        checksum^=g;
        advance_res(r,qc.q,K,g);
        mpz_add_ui(chainp,chainp,g);
    }
    ts=now_s()-t0;
    printf("steady_reps=%zu total_ms=%.3f per_step_us=%.3f checksum=%llu\n",
           reps,ts*1e3,ts*1e6/(double)reps,checksum);

    free(r);
    free(qc.q);
    mpz_clear(chainp);
    mpz_clear(p);
    return gr==gm?0:2;
}
