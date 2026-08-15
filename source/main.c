#include "winmini.h"
#include "own_solver.h"
#include "gpu_solver.h"
#include "traditional.h"

void* memset(void* d,int c,UINT_PTR n){BYTE* p=(BYTE*)d;while(n--)*p++=(BYTE)c;return d;}
void* memcpy(void* d,const void* s,UINT_PTR n){BYTE* a=(BYTE*)d;const BYTE* b=(const BYTE*)s;while(n--)*a++=*b++;return d;}

static HINSTANCE g_inst;
static HWND g_main,g_radio_n,g_radio_p,g_edit,g_combo,g_generate,g_explore,g_cancel;
static HWND g_out_p,g_out_gap,g_out_next,g_out_gate,g_out_exact,g_out_meta,g_log;
static HFONT g_font;
static volatile int g_cancel_flag=0;
static volatile int g_busy=0;
static volatile int g_explore_active=0;
static WCHAR g_logbuf[4096];

#define RECORD_SEED_N_HI 82437209ULL
#define RECORD_SEED_N_LO 13140366454229778319ULL
#define RECORD_SEED_P_HI 5421010862ULL
#define RECORD_SEED_P_LO 7886392056514346981ULL
#define RECORD_IMPLICIT_CAUSAL_DEPTH 256ULL

typedef struct Task { int kind; int input_mode; int bootstrap_mode; UINT64 value; } Task;
typedef struct Result {
    int status;
    int task_kind; /* 0 normal, 1 experimental endless self-propelled chain */
    int input_mode;
    int bootstrap_kind; /* 0 direct p, 1 primecount, 2 traditional, 3 self */
    int gate_pass;
    int exact_pass;
    int backend; /* 1 NVIDIA CUDA exact authority, 2 CPU exact fallback, 3 CPU 128-bit implicit fallback, 4 NVIDIA CUDA 128-bit implicit authority */
    int gpu_error_stage;
    int gpu_cc_major,gpu_cc_minor;
    int gpu_self_test;
    int explore_end_reason; /* 1 user stop, 2 arithmetic/resource ceiling */
    char gpu_name[128];
    UINT64 rank,p,gap,next,exact_next;
    OwnU128 rank128,p128;
    UINT64 implicit_depth_cap;
    int wide_experimental;
    UINT64 boot_ms,solve_ms,verify_ms;
    OwnSolveStats solver_stats;
    OwnSolveStats self_stats;
} Result;

typedef struct Progress {
    int backend;
    int gpu_cc_major,gpu_cc_minor;
    char gpu_name[128];
    UINT64 rank,p,gap,elapsed_ms;
    OwnU128 rank128,p128;
    UINT64 implicit_depth_cap;
    int wide_experimental;
    OwnSolveStats stats;
} Progress;

enum {RES_OK=0,RES_BAD_INPUT=1,RES_BOOT_FAIL=2,RES_GATE_FAIL=3,RES_SOLVE_FAIL=4,RES_CANCEL=5,RES_VERIFY_FAIL=6,RES_GPU_FAIL=7,RES_EXPLORE_DONE=8};

static void append(WCHAR* d,UINT_PTR cap,UINT_PTR* pos,LPCWSTR s){while(s&&*s&&*pos+1<cap)d[(*pos)++]=*s++;d[*pos]=0;}
static void append_ascii(WCHAR* d,UINT_PTR cap,UINT_PTR* pos,const char* s){while(s&&*s&&*pos+1<cap)d[(*pos)++]=(WCHAR)(unsigned char)*s++;d[*pos]=0;}
static void u64w(UINT64 v,WCHAR* o,UINT_PTR cap){WCHAR t[32];UINT_PTR n=0,i=0;if(!cap)return;if(!v){o[0]='0';if(cap>1)o[1]=0;return;}while(v&&n<31){t[n++]=(WCHAR)('0'+v%10);v/=10;}while(n&&i+1<cap)o[i++]=t[--n];o[i]=0;}
static void append_u64(WCHAR* d,UINT_PTR cap,UINT_PTR* pos,UINT64 v){WCHAR b[32];u64w(v,b,32);append(d,cap,pos,b);}
static int u128_is_zero(OwnU128 v){return v.lo==0&&v.hi==0;}
static OwnU128 u128_div10(OwnU128 v,UINT64* rem){
    OwnU128 q={0,0};UINT64 r=0;int i;
    for(i=127;i>=0;i--){UINT64 bit=(i>=64)?((v.hi>>(i-64))&1ULL):((v.lo>>i)&1ULL);r=r*2+bit;if(r>=10){r-=10;if(i>=64)q.hi|=(1ULL<<(i-64));else q.lo|=(1ULL<<i);}}
    if(rem)*rem=r;return q;
}
static void u128w(OwnU128 v,WCHAR* o,UINT_PTR cap){WCHAR t[48];UINT_PTR n=0,i=0;if(!cap)return;if(u128_is_zero(v)){o[0]='0';if(cap>1)o[1]=0;return;}while(!u128_is_zero(v)&&n<47){UINT64 r=0;v=u128_div10(v,&r);t[n++]=(WCHAR)('0'+r);}while(n&&i+1<cap)o[i++]=t[--n];o[i]=0;}
static void append_u128(WCHAR* d,UINT_PTR cap,UINT_PTR* pos,OwnU128 v){WCHAR b[48];u128w(v,b,48);append(d,cap,pos,b);}
static int parse_u64(LPCWSTR s,UINT64* v){UINT64 x=0;int seen=0;while(*s==' '||*s=='\t')s++;while(*s>='0'&&*s<='9'){UINT64 d=(UINT64)(*s-'0');if(x>((~(UINT64)0)-d)/10)return 0;x=x*10+d;seen=1;s++;}while(*s==' '||*s=='\t'||*s=='\r'||*s=='\n')s++;if(*s||!seen)return 0;*v=x;return 1;}
static HWND mk(LPCWSTR cls,LPCWSTR txt,DWORD style,DWORD ex,int id){return CreateWindowExW(ex,cls,txt,WS_CHILD|WS_VISIBLE|style,0,0,10,10,g_main,(HMENU)(UINT_PTR)id,g_inst,NULL);}
static void font(HWND h){SendMessageW(h,WM_SETFONT,(WPARAM)g_font,TRUE);}
static int wlen(LPCWSTR s){int n=0;while(s&&s[n])n++;return n;}
static SIZE measure_text(LPCWSTR s){SIZE z={0,0};HDC dc=GetDC(g_main);HGDIOBJ old=NULL;if(!dc)return z;if(g_font)old=SelectObject(dc,(HGDIOBJ)g_font);GetTextExtentPoint32W(dc,s?wlen(s)?s:L" ":L" ",s?wlen(s):1,&z);if(old)SelectObject(dc,old);ReleaseDC(g_main,dc);return z;}
static SIZE measure_control(HWND h){WCHAR b[512];int n=GetWindowTextW(h,b,512);SIZE z;if(n<=0){b[0]=' ';b[1]=0;}z=measure_text(b);return z;}
static int ui_em(void){SIZE z=measure_text(L"Hg");return z.cy>0?(int)z.cy:16;}
static void button_natural(HWND h,int* ow,int* oh){SIZE z=measure_control(h);int em=ui_em();*ow=(int)z.cx+2*em;*oh=(int)z.cy+em;if(*ow<4*em)*ow=4*em;if(*oh<2*em)*oh=2*em;}
static void stats_add_local(OwnSolveStats* a,const OwnSolveStats* b){if(!a||!b)return;a->layers+=b->layers;if(b->dimension_count>a->dimension_count)a->dimension_count=b->dimension_count;a->dimensions_generated+=b->dimensions_generated;a->survivor_hops+=b->survivor_hops;a->divisibility_tests+=b->divisibility_tests;}

static void set_busy(int busy){
    int nmode;g_busy=busy;
    EnableWindow(g_generate,!busy);EnableWindow(g_explore,!busy);EnableWindow(g_edit,!busy);EnableWindow(g_radio_n,!busy);EnableWindow(g_radio_p,!busy);
    nmode=(SendMessageW(g_radio_n,BM_GETCHECK,0,0)==BST_CHECKED);
    EnableWindow(g_combo,(!busy)&&nmode);EnableWindow(g_cancel,busy);
}

static void layout(int w,int h){
    HWND buttons[3]={g_generate,g_explore,g_cancel};
    int em=ui_em(),m=em,gap=em>3?em/2:2,y=m,full=w-2*m;
    int rw1,rh1,rw2,rh2,radio_h,edit_h,combo_h,line_h,i,bx,by,row_h,bw,bh;
    SIZE r1=measure_control(g_radio_n),r2=measure_control(g_radio_p);
    rw1=(int)r1.cx+2*em;rw2=(int)r2.cx+2*em;rh1=(int)r1.cy+em;rh2=(int)r2.cy+em;radio_h=rh1>rh2?rh1:rh2;
    if(rw1+gap+rw2<=full){MoveWindow(g_radio_n,m,y,rw1,radio_h,TRUE);MoveWindow(g_radio_p,m+rw1+gap,y,rw2,radio_h,TRUE);y+=radio_h+gap;}
    else{MoveWindow(g_radio_n,m,y,full,radio_h,TRUE);y+=radio_h+gap;MoveWindow(g_radio_p,m,y,full,radio_h,TRUE);y+=radio_h+gap;}
    edit_h=2*em;combo_h=2*em;
    MoveWindow(g_edit,m,y,full,edit_h,TRUE);y+=edit_h+gap;
    MoveWindow(g_combo,m,y,full,combo_h,TRUE);y+=combo_h+gap;
    bx=m;by=y;row_h=0;
    for(i=0;i<3;i++){
        button_natural(buttons[i],&bw,&bh);
        if(bw>full)bw=full;
        if(bx>m && bx+bw>m+full){by+=row_h+gap;bx=m;row_h=0;}
        MoveWindow(buttons[i],bx,by,bw,bh,TRUE);
        bx+=bw+gap;if(bh>row_h)row_h=bh;
    }
    y=by+row_h+2*gap;
    line_h=ui_em()+gap;
    MoveWindow(g_out_gate,m,y,full,line_h,TRUE);y+=line_h;
    MoveWindow(g_out_p,m,y,full,line_h,TRUE);y+=line_h;
    MoveWindow(g_out_gap,m,y,full,line_h,TRUE);y+=line_h;
    MoveWindow(g_out_next,m,y,full,line_h,TRUE);y+=line_h;
    MoveWindow(g_out_exact,m,y,full,line_h,TRUE);y+=line_h;
    MoveWindow(g_out_meta,m,y,full,line_h,TRUE);y+=line_h+gap;
    if(h-y-m<3*em)y=h-m-3*em;
    MoveWindow(g_log,m,y,full,h-y-m,TRUE);
}

static void copy_gpu(Result* r,const GpuSolveResult* g){int i;r->gpu_cc_major=g->cc_major;r->gpu_cc_minor=g->cc_minor;r->gpu_self_test=g->self_test_pass;r->gpu_error_stage=g->error_stage;for(i=0;i<127&&g->device_name[i];i++)r->gpu_name[i]=g->device_name[i];r->gpu_name[i]=0;}
static void copy_gpu_explore(Result* r,const GpuExploreResult* g){int i;r->gpu_cc_major=g->cc_major;r->gpu_cc_minor=g->cc_minor;r->gpu_self_test=g->self_test_pass;r->gpu_error_stage=g->error_stage;r->explore_end_reason=g->end_reason;r->rank=g->rank;r->p=g->p;r->gap=g->gap;r->solve_ms=g->elapsed_ms;r->solver_stats=g->stats;for(i=0;i<127&&g->device_name[i];i++)r->gpu_name[i]=g->device_name[i];r->gpu_name[i]=0;}

static void compose_explore_result(const Result* r){
    UINT_PTR p=0;g_logbuf[0]=0;
    append(g_logbuf,4096,&p,L"[EXPERIMENTAL] RECORD-SEEDED IMPLICIT-RECURSION CHAIN\r\n");
    append(g_logbuf,4096,&p,L"Seed rank n = 1520698109714272166094258063\r\n");
    append(g_logbuf,4096,&p,L"Seed p_n = 99999999999999999999999999973 (= 10^29 - 27)\r\n");
    append(g_logbuf,4096,&p,L"The seed is treated as an externally established exact (n,p_n) anchor. The experimental generator does not recompute its history.\r\n");
    append(g_logbuf,4096,&p,L"Prime Gate = disconnected | Exact Pass = disconnected | traditional next-prime / sieve / prime table = not used in generation.\r\n\r\n");
    append(g_logbuf,4096,&p,L"Matrix-Free + implicit-dimension contract = ON\r\n");
    append(g_logbuf,4096,&p,L"No candidate interval, no candidate pool, no dense/global interaction matrix, and no persistent prime-dimension basis are assembled. Dimensions live only inside the recursion state for the current successor.\r\n");
    append(g_logbuf,4096,&p,L"No prime-dimension basis is prebuilt or retained. q_k exists only inside the live recursion stack. The experimental causal-depth cap makes outputs beyond the exact seed PROVISIONAL / UNVERIFIED.\r\n");
    append(g_logbuf,4096,&p,L"causal depth cap = ");append_u64(g_logbuf,4096,&p,r->implicit_depth_cap);append(g_logbuf,4096,&p,L"\r\n\r\n");
    if(r->backend==4){append(g_logbuf,4096,&p,L"backend = NVIDIA CUDA 128-bit implicit-dimension authority\r\ndevice = ");append_ascii(g_logbuf,4096,&p,r->gpu_name);append(g_logbuf,4096,&p,L" | compute capability ");append_u64(g_logbuf,4096,&p,(UINT64)r->gpu_cc_major);append(g_logbuf,4096,&p,L".");append_u64(g_logbuf,4096,&p,(UINT64)r->gpu_cc_minor);append(g_logbuf,4096,&p,L" | GPU self-test PASS\r\n256 CUDA lanes project the live recursion dimensions; q workspace is rebuilt inside each successor and not retained as a prime basis.\r\n");}
    else append(g_logbuf,4096,&p,L"backend = CPU 128-bit implicit-dimension fallback (no NVIDIA CUDA device)\r\n");
    append(g_logbuf,4096,&p,L"last generated rank = ");append_u128(g_logbuf,4096,&p,r->rank128);
    append(g_logbuf,4096,&p,L"\r\nlargest generated p* = ");append_u128(g_logbuf,4096,&p,r->p128);
    append(g_logbuf,4096,&p,L"\r\nlast recurrence gap = ");append_u64(g_logbuf,4096,&p,r->gap);
    append(g_logbuf,4096,&p,L"\r\nelapsed = ");append_u64(g_logbuf,4096,&p,r->solve_ms);append(g_logbuf,4096,&p,L" ms\r\n");
    append(g_logbuf,4096,&p,L"measured peak live recursion depth = ");append_u64(g_logbuf,4096,&p,r->solver_stats.dimension_count);append(g_logbuf,4096,&p,L" | implicit dimensions generated = ");append_u64(g_logbuf,4096,&p,r->solver_stats.dimensions_generated);append(g_logbuf,4096,&p,L" | survivor hops = ");append_u64(g_logbuf,4096,&p,r->solver_stats.survivor_hops);append(g_logbuf,4096,&p,L"\r\n");
    if(r->explore_end_reason==1)append(g_logbuf,4096,&p,L"stop reason = user requested stop\r\n");
    else append(g_logbuf,4096,&p,L"stop reason = 128-bit / causal-depth / arithmetic resource ceiling\r\n");
    append(g_logbuf,4096,&p,L"\r\nImportant: this experiment intentionally does not contain the known external next-prime answer. It is not used as a hidden target or feedback signal.\r\n");
    SetWindowTextW(g_log,g_logbuf);
}

static void compose_result(const Result* r){
    UINT_PTR p=0;g_logbuf[0]=0;
    if(r->task_kind==1 && r->status==RES_EXPLORE_DONE){compose_explore_result(r);return;}
    if(r->status==RES_CANCEL){append(g_logbuf,4096,&p,L"已取消。\r\n");SetWindowTextW(g_log,g_logbuf);return;}
    if(r->status!=RES_OK){
        if(r->status==RES_BOOT_FAIL)append(g_logbuf,4096,&p,L"Bootstrap 失败。Fast 模式在未检测到 primecount.exe 时，内置传统回退仅支持 n <= 10,000,000。\r\n");
        else if(r->status==RES_GATE_FAIL)append(g_logbuf,4096,&p,L"Prime Validation Gate: REJECT。输入没有进入目标 gap 求解。\r\n");
        else if(r->status==RES_GPU_FAIL){append(g_logbuf,4096,&p,L"检测到 NVIDIA CUDA 路径，但 GPU 权威求解器初始化、自检或执行失败。按执行合同不静默切回 CPU。\r\nerror stage = ");append_u64(g_logbuf,4096,&p,(UINT64)r->gpu_error_stage);append(g_logbuf,4096,&p,L"\r\n");}
        else if(r->status==RES_SOLVE_FAIL)append(g_logbuf,4096,&p,L"用户生成器未完成（取消、溢出或资源不足）。\r\n");
        else append(g_logbuf,4096,&p,L"Exact Pass 未完成。\r\n");
        SetWindowTextW(g_log,g_logbuf);return;
    }
    append(g_logbuf,4096,&p,L"[1] INPUT / BOOTSTRAP\r\n");
    if(r->input_mode==0){append(g_logbuf,4096,&p,L"排序 n = ");append_u64(g_logbuf,4096,&p,r->rank);append(g_logbuf,4096,&p,L"\r\nBootstrap = ");
        if(r->bootstrap_kind==1)append(g_logbuf,4096,&p,L"primecount --nth-prime (external bootstrap only)\r\n");
        else if(r->bootstrap_kind==2)append(g_logbuf,4096,&p,L"Traditional fallback sieve (bootstrap only)\r\n");
        else append(g_logbuf,4096,&p,L"Self Bootstrap from (1,2), fully implicit per-successor recurrence\r\n");
    }else append(g_logbuf,4096,&p,L"直接输入当前质数 p\r\n");
    append(g_logbuf,4096,&p,L"p_n = ");append_u64(g_logbuf,4096,&p,r->p);
    append(g_logbuf,4096,&p,L"\r\n\r\n[2] PRIME VALIDATION GATE\r\nPASS。仅输出布尔准入，不向目标求解器提供因子、候选区间或下一质数。\r\n\r\n");
    append(g_logbuf,4096,&p,L"[3] YOUR SOLVER - Survivor Recursion\r\nbackend = ");
    if(r->backend==1){append(g_logbuf,4096,&p,L"NVIDIA CUDA high-dimensional exact authority\r\ndevice = ");append_ascii(g_logbuf,4096,&p,r->gpu_name);append(g_logbuf,4096,&p,L" | compute capability ");append_u64(g_logbuf,4096,&p,(UINT64)r->gpu_cc_major);append(g_logbuf,4096,&p,L".");append_u64(g_logbuf,4096,&p,(UINT64)r->gpu_cc_minor);append(g_logbuf,4096,&p,L" | GPU self-test PASS\r\nMatrix-Free operator = ON | 256 CUDA lanes project live prime dimensions directly\r\nDimensions are generated on demand from q0=2; no prime basis survives between Self-Bootstrap successor steps.\r\nNo candidate interval / candidate pool / dense global matrix\r\nTensor Core = not used\r\n");}
    else append(g_logbuf,4096,&p,L"CPU exact fallback (no NVIDIA CUDA device available)\r\n");
    append(g_logbuf,4096,&p,L"gap = ");append_u64(g_logbuf,4096,&p,r->gap);append(g_logbuf,4096,&p,L"\r\np_(n+1) = ");append_u64(g_logbuf,4096,&p,r->next);
    append(g_logbuf,4096,&p,L"\r\nlayers = ");append_u64(g_logbuf,4096,&p,r->solver_stats.layers);append(g_logbuf,4096,&p,L", dimensions = ");append_u64(g_logbuf,4096,&p,r->solver_stats.dimension_count);append(g_logbuf,4096,&p,L", generated dimensions = ");append_u64(g_logbuf,4096,&p,r->solver_stats.dimensions_generated);
    append(g_logbuf,4096,&p,L"\r\nsurvivor hops = ");append_u64(g_logbuf,4096,&p,r->solver_stats.survivor_hops);append(g_logbuf,4096,&p,L", divisibility tests = ");append_u64(g_logbuf,4096,&p,r->solver_stats.divisibility_tests);
    append(g_logbuf,4096,&p,L"\r\nNatural-number candidate interval is not materialized; recursion moves between prior-layer survivors.\r\nVerifier is called only after the result is frozen.\r\n\r\n");
    append(g_logbuf,4096,&p,L"[4] EXACT PASS (traditional, post-solve only)\r\ntraditional next prime = ");append_u64(g_logbuf,4096,&p,r->exact_next);append(g_logbuf,4096,&p,r->exact_pass?L"\r\nEXACT PASS\r\n":L"\r\nMISMATCH\r\n");
    append(g_logbuf,4096,&p,L"\r\nTiming: bootstrap ");append_u64(g_logbuf,4096,&p,r->boot_ms);append(g_logbuf,4096,&p,L" ms | own solver ");append_u64(g_logbuf,4096,&p,r->solve_ms);append(g_logbuf,4096,&p,L" ms | verifier ");append_u64(g_logbuf,4096,&p,r->verify_ms);append(g_logbuf,4096,&p,L" ms\r\n");
    SetWindowTextW(g_log,g_logbuf);
}

static void post_record_progress(const OwnRecordExploreResult* p,void* user){
    Progress* x=(Progress*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(Progress));(void)user;
    if(!x)return;x->backend=3;x->wide_experimental=1;x->rank128=p->rank;x->p128=p->p;x->gap=p->gap;x->elapsed_ms=p->elapsed_ms;x->implicit_depth_cap=p->causal_depth_cap;x->stats=p->stats;
    PostMessageW(g_main,WM_APP_PROGRESS,(WPARAM)x,0);
}

static void post_gpu_record_progress(const GpuRecordExploreResult* p,void* user){
    Progress* x=(Progress*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(Progress));int i;(void)user;
    if(!x)return;x->backend=4;x->wide_experimental=1;x->rank128=p->rank;x->p128=p->p;x->gap=p->gap;x->elapsed_ms=p->elapsed_ms;x->implicit_depth_cap=p->causal_depth_cap;x->stats=p->stats;
    x->gpu_cc_major=p->cc_major;x->gpu_cc_minor=p->cc_minor;for(i=0;i<127&&p->device_name[i];i++)x->gpu_name[i]=p->device_name[i];x->gpu_name[i]=0;
    PostMessageW(g_main,WM_APP_PROGRESS,(WPARAM)x,0);
}


static DWORD WINAPI worker(LPVOID vp){
    Task* t=(Task*)vp;Result* r=(Result*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(Result));
    UINT64 t0,t1;int ok=0;int gpu_state;GpuSolveResult gr;OwnDimensionCache cache;
    if(!r){HeapFree(GetProcessHeap(),0,t);PostMessageW(g_main,WM_APP_DONE,0,0);return 0;}
    r->task_kind=t->kind;r->input_mode=t->input_mode;
    if(t->kind==1){
        OwnU128 sr={RECORD_SEED_N_LO,RECORD_SEED_N_HI};OwnU128 sp={RECORD_SEED_P_LO,RECORD_SEED_P_HI};GpuRecordExploreResult gx;
        memset(&gx,0,sizeof(gx));r->wide_experimental=1;r->implicit_depth_cap=RECORD_IMPLICIT_CAUSAL_DEPTH;
        gpu_state=gpu_record_explore(sr,sp,RECORD_IMPLICIT_CAUSAL_DEPTH,&g_cancel_flag,post_gpu_record_progress,NULL,&gx);
        if(gpu_state==GPU_SOLVER_OK){int i;r->backend=4;r->rank128=gx.rank;r->p128=gx.p;r->gap=gx.gap;r->solve_ms=gx.elapsed_ms;r->solver_stats=gx.stats;r->explore_end_reason=gx.end_reason;r->implicit_depth_cap=gx.causal_depth_cap;r->gpu_cc_major=gx.cc_major;r->gpu_cc_minor=gx.cc_minor;r->gpu_self_test=gx.self_test_pass;for(i=0;i<127&&gx.device_name[i];i++)r->gpu_name[i]=gx.device_name[i];r->gpu_name[i]=0;r->status=RES_EXPLORE_DONE;goto done;}
        if(gpu_state==GPU_SOLVER_NO_NVIDIA){OwnRecordExploreResult rx;memset(&rx,0,sizeof(rx));r->backend=3;if(!own_record_explore(sr,sp,RECORD_IMPLICIT_CAUSAL_DEPTH,&g_cancel_flag,post_record_progress,NULL,&rx)){r->status=RES_SOLVE_FAIL;goto done;}r->rank128=rx.rank;r->p128=rx.p;r->gap=rx.gap;r->solve_ms=rx.elapsed_ms;r->solver_stats=rx.stats;r->explore_end_reason=rx.end_reason;r->implicit_depth_cap=rx.causal_depth_cap;r->status=RES_EXPLORE_DONE;goto done;}
        r->gpu_error_stage=gx.error_stage;r->status=RES_GPU_FAIL;goto done;
    }
    if(g_cancel_flag){r->status=RES_CANCEL;goto done;}
    t0=GetTickCount64();
    if(t->input_mode==0){
        r->rank=t->value;
        if(t->bootstrap_mode==1){
            r->bootstrap_kind=3;
            gpu_state=gpu_survivor_solve(2,t->value,&gr);
            if(gpu_state==GPU_SOLVER_OK){r->p=gr.p;copy_gpu(r,&gr);ok=1;}
            else if(gpu_state==GPU_SOLVER_NO_NVIDIA){ok=own_self_bootstrap(t->value,&g_cancel_flag,&r->p,&r->self_stats);}
            else {copy_gpu(r,&gr);r->status=RES_GPU_FAIL;goto done;}
        }else{int usedpc=0;ok=fast_bootstrap_nth(t->value,&g_cancel_flag,&r->p,&usedpc);r->bootstrap_kind=usedpc?1:2;}
        if(!ok){r->status=g_cancel_flag?RES_CANCEL:RES_BOOT_FAIL;goto done;}
    }else{r->p=t->value;r->bootstrap_kind=0;}
    t1=GetTickCount64();r->boot_ms=t1-t0;if(g_cancel_flag){r->status=RES_CANCEL;goto done;}
    if(!exact_is_prime_u64(r->p)){r->status=RES_GATE_FAIL;goto done;}r->gate_pass=1;
    t0=GetTickCount64();gpu_state=gpu_survivor_solve(0,r->p,&gr);
    if(gpu_state==GPU_SOLVER_OK){r->backend=1;copy_gpu(r,&gr);r->gap=gr.gap;r->next=gr.next_p;r->solver_stats=gr.stats;}
    else if(gpu_state==GPU_SOLVER_NO_NVIDIA){r->backend=2;if(!own_cache_init(&cache)){r->status=RES_SOLVE_FAIL;goto done;}ok=own_next_gap(r->p,&cache,&g_cancel_flag,&r->gap,&r->solver_stats);own_cache_free(&cache);if(!ok){r->status=g_cancel_flag?RES_CANCEL:RES_SOLVE_FAIL;goto done;}if(r->gap>(~(UINT64)0)-r->p){r->status=RES_SOLVE_FAIL;goto done;}r->next=r->p+r->gap;}
    else{copy_gpu(r,&gr);r->status=RES_GPU_FAIL;goto done;}
    if(r->next<=r->p){r->status=RES_SOLVE_FAIL;goto done;}t1=GetTickCount64();r->solve_ms=t1-t0;
    t0=GetTickCount64();if(!exact_next_prime_u64(r->p,&g_cancel_flag,&r->exact_next)){r->status=g_cancel_flag?RES_CANCEL:RES_VERIFY_FAIL;goto done;}r->exact_pass=(r->exact_next==r->next);t1=GetTickCount64();r->verify_ms=t1-t0;r->status=RES_OK;
done:
    HeapFree(GetProcessHeap(),0,t);PostMessageW(g_main,WM_APP_DONE,(WPARAM)r,0);return 0;
}

static void start_task(void){
    WCHAR in[128];UINT64 v;int mode,boot;Task* t;HANDLE th;
    if(g_busy)return;GetWindowTextW(g_edit,in,128);if(!parse_u64(in,&v)||v<1){MessageBoxW(g_main,L"请输入正整数。",L"输入无效",MB_OK|MB_ICONERROR);return;}
    mode=(SendMessageW(g_radio_p,BM_GETCHECK,0,0)==BST_CHECKED)?1:0;if(mode==1&&v<2){MessageBoxW(g_main,L"当前质数必须 >= 2，并先通过 Prime Validation Gate。",L"输入无效",MB_OK|MB_ICONERROR);return;}
    boot=(int)SendMessageW(g_combo,CB_GETCURSEL,0,0);if(boot<0)boot=0;t=(Task*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(Task));if(!t)return;t->kind=0;t->input_mode=mode;t->bootstrap_mode=boot;t->value=v;
    g_cancel_flag=0;g_explore_active=0;set_busy(1);SetWindowTextW(g_out_gate,L"Prime Validation Gate: waiting...");SetWindowTextW(g_out_exact,L"Exact Pass: waiting...");SetWindowTextW(g_out_p,L"Current p: ...");SetWindowTextW(g_out_gap,L"Gap: ...");SetWindowTextW(g_out_next,L"Next prime: ...");SetWindowTextW(g_out_meta,L"Compute authority: detecting NVIDIA CUDA...");
    SetWindowTextW(g_log,L"Matrix-Free operator = ON. Self Bootstrap resets the live prime-dimension state at every successor and regrows dimensions on demand from q0=2. Direct p starts from the same fresh implicit dimension state.\r\nFast Bootstrap remains external/traditional for resolving p_n; the user's successor solver keeps its own execution contract.\r\nNVIDIA present: the user's solver uses the embedded 256-lane CUDA operator path. No NVIDIA CUDA device: the same recurrence falls back to CPU.\r\nPrime Gate and Exact Pass remain isolated observers.");
    th=CreateThread(NULL,0,worker,t,0,NULL);if(th)CloseHandle(th);else{HeapFree(GetProcessHeap(),0,t);set_busy(0);}
}

static void start_explore(void){
    Task* t;HANDLE th;if(g_busy)return;t=(Task*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(Task));if(!t)return;t->kind=1;
    g_cancel_flag=0;g_explore_active=1;set_busy(1);SetWindowTextW(g_out_gate,L"Experimental implicit chain: UNVERIFIED / no Prime Gate");SetWindowTextW(g_out_exact,L"Exact Pass: intentionally disconnected");SetWindowTextW(g_out_p,L"Seed rank n: 1520698109714272166094258063");SetWindowTextW(g_out_gap,L"Last recurrence gap: 0");SetWindowTextW(g_out_next,L"Seed p_n: 99999999999999999999999999973");SetWindowTextW(g_out_meta,L"Detecting NVIDIA CUDA for 128-bit implicit recursion...");
    SetWindowTextW(g_log,L"EXPERIMENTAL RECORD-SEEDED FULLY IMPLICIT RECURSION MODE\r\nExact public anchor: n = 1520698109714272166094258063, p_n = 99999999999999999999999999973.\r\nHistory from 2 is skipped completely. Each experimental output is fed back as the sole next current state.\r\n\r\nMatrix-Free + implicit dimensions = ON: no candidate interval, no candidate pool, no dense/global matrix, and no prebuilt prime-dimension basis are assembled.\r\nNo persistent q[] basis is prepared. On NVIDIA, 256 CUDA lanes project a fixed-size live recursion workspace rebuilt inside each successor step; on systems without NVIDIA the CPU reference recurrence is used. Causal depth is capped at 256 for this experiment, so values after the seed are intentionally UNVERIFIED.\r\nNo external known gap/next-prime answer is embedded into this path.\r\n\r\nPress Stop to freeze the largest provisional value generated so far.");
    th=CreateThread(NULL,0,worker,t,0,NULL);if(th)CloseHandle(th);else{HeapFree(GetProcessHeap(),0,t);g_explore_active=0;set_busy(0);}
}

static void render_progress(const Progress* x){
    WCHAR b[384];UINT_PTR pos=0;if(!x)return;
    if(x->wide_experimental){
        b[0]=0;append(b,384,&pos,L"Generated rank n*: ");append_u128(b,384,&pos,x->rank128);SetWindowTextW(g_out_p,b);
        pos=0;b[0]=0;append(b,384,&pos,L"Last recurrence gap: ");append_u64(b,384,&pos,x->gap);SetWindowTextW(g_out_gap,b);
        pos=0;b[0]=0;append(b,384,&pos,L"Largest provisional p*: ");append_u128(b,384,&pos,x->p128);SetWindowTextW(g_out_next,b);
        pos=0;b[0]=0;if(x->backend==4){append(b,384,&pos,L"NVIDIA CUDA 128-bit implicit | ");append_ascii(b,384,&pos,x->gpu_name);append(b,384,&pos,L" | ");}else append(b,384,&pos,L"CPU 128-bit implicit fallback | ");append(b,384,&pos,L"depth cap ");append_u64(b,384,&pos,x->implicit_depth_cap);append(b,384,&pos,L" | elapsed ");append_u64(b,384,&pos,x->elapsed_ms);append(b,384,&pos,L" ms");SetWindowTextW(g_out_meta,b);return;
    }
    b[0]=0;append(b,384,&pos,L"Generated rank n: ");append_u64(b,384,&pos,x->rank);SetWindowTextW(g_out_p,b);
    pos=0;b[0]=0;append(b,384,&pos,L"Last recurrence gap: ");append_u64(b,384,&pos,x->gap);SetWindowTextW(g_out_gap,b);
    pos=0;b[0]=0;append(b,384,&pos,L"Largest generated p*: ");append_u64(b,384,&pos,x->p);SetWindowTextW(g_out_next,b);
    pos=0;b[0]=0;if(x->backend==1){append(b,384,&pos,L"Compute authority: NVIDIA CUDA | ");append_ascii(b,384,&pos,x->gpu_name);}else append(b,384,&pos,L"Compute authority: CPU exact fallback");append(b,384,&pos,L" | elapsed ");append_u64(b,384,&pos,x->elapsed_ms);append(b,384,&pos,L" ms");SetWindowTextW(g_out_meta,b);
}

static LRESULT CALLBACK wndproc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_CREATE){
        g_main=h;g_font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);g_radio_n=mk(L"BUTTON",L"按质数排序 n",BS_AUTORADIOBUTTON|WS_GROUP|WS_TABSTOP,0,ID_INPUT_N);g_radio_p=mk(L"BUTTON",L"按当前质数 p",BS_AUTORADIOBUTTON|WS_TABSTOP,0,ID_INPUT_P);g_edit=mk(L"EDIT",L"",ES_LEFT|ES_AUTOHSCROLL|WS_BORDER|WS_TABSTOP,WS_EX_CLIENTEDGE,ID_VALUE);g_combo=mk(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,0,ID_BOOTSTRAP);
        g_generate=mk(L"BUTTON",L"Generate",BS_DEFPUSHBUTTON|WS_TABSTOP,0,ID_GENERATE);g_explore=mk(L"BUTTON",L"实验：隐式递归记录锚点推演",BS_PUSHBUTTON|WS_TABSTOP,0,ID_EXPLORE);g_cancel=mk(L"BUTTON",L"停止 / 取消",BS_PUSHBUTTON|WS_TABSTOP,0,ID_CANCEL);
        g_out_gate=mk(L"STATIC",L"Prime Validation Gate: idle",SS_LEFT,0,ID_RESULT_GATE);g_out_p=mk(L"STATIC",L"Current p: -",SS_LEFT,0,ID_RESULT_P);g_out_gap=mk(L"STATIC",L"Gap: -",SS_LEFT,0,ID_RESULT_GAP);g_out_next=mk(L"STATIC",L"Next prime: -",SS_LEFT,0,ID_RESULT_NEXT);g_out_exact=mk(L"STATIC",L"Exact Pass: -",SS_LEFT,0,ID_RESULT_EXACT);g_out_meta=mk(L"STATIC",L"Compute authority: idle",SS_LEFT,0,ID_RESULT_META);g_log=mk(L"EDIT",L"",ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL|WS_BORDER,WS_EX_CLIENTEDGE,ID_LOG);
        {HWND arr[]={g_radio_n,g_radio_p,g_edit,g_combo,g_generate,g_explore,g_cancel,g_out_gate,g_out_p,g_out_gap,g_out_next,g_out_exact,g_out_meta,g_log};int i;for(i=0;i<(int)(sizeof(arr)/sizeof(arr[0]));i++)font(arr[i]);}
        SendMessageW(g_radio_n,BM_SETCHECK,BST_CHECKED,0);SendMessageW(g_edit,EM_SETCUEBANNER,TRUE,(LPARAM)L"输入 n 或当前质数 p");SendMessageW(g_combo,CB_ADDSTRING,0,(LPARAM)L"Fast Bootstrap (primecount if available; traditional fallback)");SendMessageW(g_combo,CB_ADDSTRING,0,(LPARAM)L"Self Bootstrap (from 2; fully implicit per successor)");SendMessageW(g_combo,CB_SETCURSEL,0,0);EnableWindow(g_cancel,FALSE);return 0;
    }
    if(msg==WM_SIZE){layout((int)LOWORD(lp),(int)HIWORD(lp));return 0;}
    if(msg==WM_COMMAND){int id=(int)LOWORD(wp);if(id==ID_GENERATE&&HIWORD(wp)==BN_CLICKED){start_task();return 0;}if(id==ID_EXPLORE&&HIWORD(wp)==BN_CLICKED){start_explore();return 0;}if(id==ID_CANCEL&&HIWORD(wp)==BN_CLICKED){g_cancel_flag=1;SetWindowTextW(g_out_meta,g_explore_active?L"Stop requested. Current batch will return, then the last generated value is frozen.":L"取消请求已发送。GPU kernel 已启动时会在返回后生效。");return 0;}if(id==ID_INPUT_N&&HIWORD(wp)==BN_CLICKED){EnableWindow(g_combo,TRUE);SendMessageW(g_edit,EM_SETCUEBANNER,TRUE,(LPARAM)L"输入质数排序 n");return 0;}if(id==ID_INPUT_P&&HIWORD(wp)==BN_CLICKED){EnableWindow(g_combo,FALSE);SendMessageW(g_edit,EM_SETCUEBANNER,TRUE,(LPARAM)L"输入当前质数 p（先验证）");return 0;}}
    if(msg==WM_APP_PROGRESS){Progress* x=(Progress*)wp;if(x){if(g_busy&&g_explore_active)render_progress(x);HeapFree(GetProcessHeap(),0,x);}return 0;}
    if(msg==WM_APP_DONE){
        Result* r=(Result*)wp;set_busy(0);if(!r){g_explore_active=0;SetWindowTextW(g_log,L"Worker allocation failure.");return 0;}compose_result(r);
        if(r->status==RES_EXPLORE_DONE){WCHAR b[384];UINT_PTR pos=0;SetWindowTextW(g_out_gate,L"Experimental implicit chain: UNVERIFIED / no Prime Gate");SetWindowTextW(g_out_exact,L"Exact Pass: intentionally disconnected");b[0]=0;append(b,384,&pos,L"Generated rank n*: ");append_u128(b,384,&pos,r->rank128);SetWindowTextW(g_out_p,b);pos=0;b[0]=0;append(b,384,&pos,L"Last recurrence gap: ");append_u64(b,384,&pos,r->gap);SetWindowTextW(g_out_gap,b);pos=0;b[0]=0;append(b,384,&pos,L"Largest provisional p*: ");append_u128(b,384,&pos,r->p128);SetWindowTextW(g_out_next,b);pos=0;b[0]=0;if(r->backend==4){append(b,384,&pos,L"NVIDIA CUDA 128-bit implicit | ");append_ascii(b,384,&pos,r->gpu_name);append(b,384,&pos,L" | ");}else append(b,384,&pos,L"CPU 128-bit implicit fallback | ");append(b,384,&pos,L"depth cap ");append_u64(b,384,&pos,r->implicit_depth_cap);append(b,384,&pos,L" | stopped at ");append_u64(b,384,&pos,r->solve_ms);append(b,384,&pos,L" ms");SetWindowTextW(g_out_meta,b);}
        else if(r->status==RES_OK){WCHAR b[256];UINT_PTR pos=0;b[0]=0;append(b,256,&pos,L"Prime Validation Gate: PASS");SetWindowTextW(g_out_gate,b);pos=0;b[0]=0;append(b,256,&pos,L"Current p: ");append_u64(b,256,&pos,r->p);SetWindowTextW(g_out_p,b);pos=0;b[0]=0;append(b,256,&pos,L"Gap: ");append_u64(b,256,&pos,r->gap);SetWindowTextW(g_out_gap,b);pos=0;b[0]=0;append(b,256,&pos,L"Next prime: ");append_u64(b,256,&pos,r->next);SetWindowTextW(g_out_next,b);SetWindowTextW(g_out_exact,r->exact_pass?L"Exact Pass: PASS":L"Exact Pass: MISMATCH");pos=0;b[0]=0;if(r->backend==1){append(b,256,&pos,L"Compute authority: NVIDIA CUDA | ");append_ascii(b,256,&pos,r->gpu_name);}else append(b,256,&pos,L"Compute authority: CPU fallback | no NVIDIA CUDA device");SetWindowTextW(g_out_meta,b);}
        else{SetWindowTextW(g_out_gate,r->status==RES_GATE_FAIL?L"Prime Validation Gate: REJECT":L"Prime Validation Gate: not completed");SetWindowTextW(g_out_exact,L"Exact Pass: not completed");SetWindowTextW(g_out_meta,r->status==RES_GPU_FAIL?L"Compute authority: NVIDIA GPU path rejected; CPU fallback forbidden":L"Compute authority: not completed");}
        g_explore_active=0;HeapFree(GetProcessHeap(),0,r);return 0;
    }
    if(msg==WM_DESTROY){g_cancel_flag=1;PostQuitMessage(0);return 0;}return DefWindowProcW(h,msg,wp,lp);
}

void WINAPI WinMainCRTStartup(void){
    WNDCLASSEXW wc;MSG msg;HWND h;HICON ico;int i;g_inst=GetModuleHandleW(NULL);for(i=0;i<(int)sizeof(wc);i++)((BYTE*)&wc)[i]=0;ico=LoadIconW(g_inst,MAKEINTRESOURCEW(1));if(!ico)ico=LoadIconW(NULL,IDI_APPLICATION);
    wc.cbSize=sizeof(wc);wc.lpfnWndProc=wndproc;wc.hInstance=g_inst;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.hIcon=ico;wc.hIconSm=ico;wc.hbrBackground=(HBRUSH)(UINT_PTR)(COLOR_WINDOW+1);wc.lpszClassName=L"PrimeGapForwardNativeWindow";
    if(!RegisterClassExW(&wc))ExitProcess(2);h=CreateWindowExW(WS_EX_CONTROLPARENT,wc.lpszClassName,L"ζ ZeroCandidatePrime",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,820,650,NULL,NULL,g_inst,NULL);if(!h)ExitProcess(3);ShowWindow(h,SW_SHOWDEFAULT);UpdateWindow(h);while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}ExitProcess(0);
}
