#include "traditional.h"

/* Traditional/external zone. Never called from own_solver.c. */

static UINT64 addmod(UINT64 a, UINT64 b, UINT64 m) {
    if (a >= m) a %= m;
    if (b >= m) b %= m;
    return (a >= m-b) ? (a-(m-b)) : (a+b);
}

static UINT64 mulmod(UINT64 a, UINT64 b, UINT64 m) {
    UINT64 r=0;
    a%=m;
    while (b) {
        if (b&1) r=addmod(r,a,m);
        b>>=1;
        if (b) a=addmod(a,a,m);
    }
    return r;
}

static UINT64 powmod(UINT64 a, UINT64 e, UINT64 m) {
    UINT64 r=1%m;
    a%=m;
    while (e) {
        if (e&1) r=mulmod(r,a,m);
        e>>=1;
        if (e) a=mulmod(a,a,m);
    }
    return r;
}

static int mr_witness(UINT64 a, UINT64 n, UINT64 d, unsigned s) {
    UINT64 x;
    unsigned r;
    if (a%n==0) return 0;
    x=powmod(a,d,n);
    if (x==1 || x==n-1) return 0;
    for (r=1;r<s;r++) {
        x=mulmod(x,x,n);
        if (x==n-1) return 0;
    }
    return 1;
}

int exact_is_prime_u64(UINT64 n) {
    static const UINT64 small[] = {2,3,5,7,11,13,17,19,23,29,31,37};
    static const UINT64 bases[] = {2ULL,325ULL,9375ULL,28178ULL,450775ULL,9780504ULL,1795265022ULL};
    UINT64 d;
    unsigned s=0;
    unsigned i;
    if (n<2) return 0;
    for (i=0;i<sizeof(small)/sizeof(small[0]);i++) {
        if (n==small[i]) return 1;
        if (n%small[i]==0) return 0;
    }
    d=n-1;
    while ((d&1)==0) { d>>=1; s++; }
    for (i=0;i<sizeof(bases)/sizeof(bases[0]);i++) {
        if (mr_witness(bases[i],n,d,s)) return 0;
    }
    return 1;
}

int exact_next_prime_u64(UINT64 p, volatile int* cancel, UINT64* out) {
    UINT64 x;
    if (p>=~(UINT64)0-2) return 0;
    if (p<2) { *out=2; return 1; }
    x=p+1;
    if (x<=2) { *out=2; return 1; }
    if ((x&1)==0) x++;
    for (;;) {
        if (cancel && *cancel) return 0;
        if (exact_is_prime_u64(x)) { *out=x; return 1; }
        if (x>~(UINT64)0-2) return 0;
        x+=2;
    }
}

static void u64_to_ascii(UINT64 v, char* out, int cap) {
    char tmp[32]; int n=0,i;
    if (cap<=0) return;
    if (v==0) { out[0]='0'; if(cap>1)out[1]=0; return; }
    while (v && n<31) { tmp[n++]=(char)('0'+(v%10)); v/=10; }
    i=0;
    while (n && i<cap-1) out[i++]=tmp[--n];
    out[i]=0;
}

static void ascii_to_wide(const char* s, WCHAR* w, int cap) {
    int i=0; if(cap<=0)return;
    while(s[i] && i<cap-1){w[i]=(WCHAR)(unsigned char)s[i];i++;}
    w[i]=0;
}

static UINT64 parse_ascii_first_u64(const char* s, int* ok) {
    UINT64 v=0; int seen=0;
    while (*s) {
        if (*s>='0'&&*s<='9') {
            UINT64 d=(UINT64)(*s-'0');
            if (v>((~(UINT64)0)-d)/10) { *ok=0; return 0; }
            v=v*10+d; seen=1;
        } else if (seen) break;
        s++;
    }
    *ok=seen; return v;
}

/* Optional adapter: uses primecount.exe if already available on PATH/current folder. */
static int try_primecount(UINT64 n, UINT64* p) {
    HANDLE r=NULL,w=NULL;
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    char nascii[32]; WCHAR nw[32]; WCHAR cmd[160];
    char buf[256]; DWORD got=0; int ok=0;
    int pos=0,i;
    sa.nLength=sizeof(sa); sa.lpSecurityDescriptor=NULL; sa.bInheritHandle=TRUE;
    if(!CreatePipe(&r,&w,&sa,0)) return 0;
    SetHandleInformation(r,HANDLE_FLAG_INHERIT,0);
    for(i=0;i<(int)sizeof(si);i++) ((BYTE*)&si)[i]=0;
    for(i=0;i<(int)sizeof(pi);i++) ((BYTE*)&pi)[i]=0;
    si.cb=sizeof(si); si.dwFlags=STARTF_USESTDHANDLES; si.hStdOutput=w; si.hStdError=w; si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
    u64_to_ascii(n,nascii,32); ascii_to_wide(nascii,nw,32);
    {
        const WCHAR a[]=L"primecount.exe "; const WCHAR b[]=L" --nth-prime";
        for(i=0;a[i]&&pos<150;i++)cmd[pos++]=a[i];
        for(i=0;nw[i]&&pos<150;i++)cmd[pos++]=nw[i];
        for(i=0;b[i]&&pos<150;i++)cmd[pos++]=b[i];
        cmd[pos]=0;
    }
    if(!CreateProcessW(NULL,cmd,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)) {
        CloseHandle(r); CloseHandle(w); return 0;
    }
    CloseHandle(w); w=NULL;
    WaitForSingleObject(pi.hProcess,INFINITE);
    if(ReadFile(r,buf,255,&got,NULL) && got>0) {
        buf[got<255?got:255]=0;
        *p=parse_ascii_first_u64(buf,&ok);
    }
    CloseHandle(r); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return ok && *p>=2;
}

static unsigned bitlen64(UINT64 n) {
    unsigned b=0; while(n){b++;n>>=1;} return b?b:1;
}
static int bit_get(const BYTE* a, UINT64 i){ return (a[i>>3]>>(i&7))&1; }
static void bit_set(BYTE* a, UINT64 i){ a[i>>3]|=(BYTE)(1u<<(i&7)); }

/* Portable traditional fallback, deliberately outside the user's solver. */
static int nth_prime_sieve(UINT64 n, volatile int* cancel, UINT64* out) {
    UINT64 upper, odd_count, bytes, i,p,m,count;
    BYTE* comp;
    HANDLE heap=GetProcessHeap();
    static const UINT64 first[]={0,2,3,5,7,11,13};
    if(n<1)return 0;
    if(n<=6){*out=first[n];return 1;}
    if(n>10000000ULL) return 0; /* fallback memory/runtime guard */
    {
        UINT64 mult=(UINT64)(2*bitlen64(n)+2);
        if(n>(~(UINT64)0)/mult)return 0;
        upper=n*mult+100;
    }
    odd_count=(upper>>1)+1;
    bytes=(odd_count+7)>>3;
    comp=(BYTE*)HeapAlloc(heap,HEAP_ZERO_MEMORY,(UINT_PTR)bytes);
    if(!comp)return 0;
    for(p=3; p<=upper/p; p+=2) {
        UINT64 idx=p>>1;
        if(cancel&&*cancel){HeapFree(heap,0,comp);return 0;}
        if(!bit_get(comp,idx)) {
            UINT64 step=p<<1;
            for(m=p*p;m<=upper;) {
                bit_set(comp,m>>1);
                if(m>upper-step)break;
                m+=step;
            }
        }
    }
    count=1;
    for(i=3;i<=upper;i+=2) {
        if(cancel&&*cancel){HeapFree(heap,0,comp);return 0;}
        if(!bit_get(comp,i>>1)) {
            count++;
            if(count==n){*out=i;HeapFree(heap,0,comp);return 1;}
        }
    }
    HeapFree(heap,0,comp); return 0;
}

int fast_bootstrap_nth(UINT64 n, volatile int* cancel, UINT64* p, int* used_primecount) {
    if(used_primecount)*used_primecount=0;
    if(cancel&&*cancel)return 0;
    if(try_primecount(n,p)) { if(used_primecount)*used_primecount=1; return 1; }
    return nth_prime_sieve(n,cancel,p);
}
