#include "gpu_solver.h"
#include "gpu_kernel_ptx.h"

typedef int CUresult;
typedef int CUdevice;
typedef void* CUcontext;
typedef void* CUmodule;
typedef void* CUfunction;
typedef void* CUstream;
typedef unsigned long long CUdeviceptr;

#define CUDA_SUCCESS 0
#define GPU_MAX_DIMS 1048576ULL

/* CUDA Driver API is loaded dynamically. No CUDA Toolkit/runtime DLL is required. */
typedef CUresult (WINAPI *PFN_cuInit)(unsigned int);
typedef CUresult (WINAPI *PFN_cuDeviceGetCount)(int*);
typedef CUresult (WINAPI *PFN_cuDeviceGet)(CUdevice*,int);
typedef CUresult (WINAPI *PFN_cuDeviceGetName)(char*,int,CUdevice);
typedef CUresult (WINAPI *PFN_cuDeviceComputeCapability)(int*,int*,CUdevice);
typedef CUresult (WINAPI *PFN_cuCtxCreate)(CUcontext*,unsigned int,CUdevice);
typedef CUresult (WINAPI *PFN_cuCtxDestroy)(CUcontext);
typedef CUresult (WINAPI *PFN_cuModuleLoadDataEx)(CUmodule*,const void*,unsigned int,void*,void*);
typedef CUresult (WINAPI *PFN_cuModuleGetFunction)(CUfunction*,CUmodule,const char*);
typedef CUresult (WINAPI *PFN_cuModuleUnload)(CUmodule);
typedef CUresult (WINAPI *PFN_cuMemAlloc)(CUdeviceptr*,UINT_PTR);
typedef CUresult (WINAPI *PFN_cuMemFree)(CUdeviceptr);
typedef CUresult (WINAPI *PFN_cuMemcpyDtoH)(void*,CUdeviceptr,UINT_PTR);
typedef CUresult (WINAPI *PFN_cuLaunchKernel)(CUfunction,
    unsigned int,unsigned int,unsigned int,
    unsigned int,unsigned int,unsigned int,
    unsigned int,CUstream,void**,void**);
typedef CUresult (WINAPI *PFN_cuCtxSynchronize)(void);

typedef struct CudaApi {
    HINSTANCE dll;
    PFN_cuInit cuInit;
    PFN_cuDeviceGetCount cuDeviceGetCount;
    PFN_cuDeviceGet cuDeviceGet;
    PFN_cuDeviceGetName cuDeviceGetName;
    PFN_cuDeviceComputeCapability cuDeviceComputeCapability;
    PFN_cuCtxCreate cuCtxCreate;
    PFN_cuCtxDestroy cuCtxDestroy;
    PFN_cuModuleLoadDataEx cuModuleLoadDataEx;
    PFN_cuModuleGetFunction cuModuleGetFunction;
    PFN_cuModuleUnload cuModuleUnload;
    PFN_cuMemAlloc cuMemAlloc;
    PFN_cuMemFree cuMemFree;
    PFN_cuMemcpyDtoH cuMemcpyDtoH;
    PFN_cuLaunchKernel cuLaunchKernel;
    PFN_cuCtxSynchronize cuCtxSynchronize;
} CudaApi;

typedef struct GpuMem {
    CUdeviceptr q,count,flvl,fcur,ftotal,fwait,out,stats,gaptemp,mtemp,candidate,hit;
} GpuMem;

static void clear_bytes(void* p, UINT_PTR n){ BYTE* b=(BYTE*)p; while(n--)*b++=0; }
static void copy_ascii(char* d, int cap, const char* s){int i=0;if(cap<=0)return;while(s&&s[i]&&i<cap-1){d[i]=s[i];i++;}d[i]=0;}
static void copy_stats(OwnSolveStats* s,const UINT64* h){if(!s)return;s->layers=h[0];s->dimension_count=h[1];s->dimensions_generated=h[2];s->survivor_hops=h[3];s->divisibility_tests=h[4];}

static FARPROC gp(HINSTANCE h,const char* n){return GetProcAddress(h,n);}
static FARPROC gp2(HINSTANCE h,const char* a,const char* b){FARPROC p=gp(h,a);return p?p:gp(h,b);}

static int cuda_load(CudaApi* a){
    clear_bytes(a,sizeof(*a));
    a->dll=LoadLibraryW(L"nvcuda.dll");
    if(!a->dll)return 0;
    a->cuInit=(PFN_cuInit)gp(a->dll,"cuInit");
    a->cuDeviceGetCount=(PFN_cuDeviceGetCount)gp(a->dll,"cuDeviceGetCount");
    a->cuDeviceGet=(PFN_cuDeviceGet)gp(a->dll,"cuDeviceGet");
    a->cuDeviceGetName=(PFN_cuDeviceGetName)gp(a->dll,"cuDeviceGetName");
    a->cuDeviceComputeCapability=(PFN_cuDeviceComputeCapability)gp(a->dll,"cuDeviceComputeCapability");
    a->cuCtxCreate=(PFN_cuCtxCreate)gp2(a->dll,"cuCtxCreate_v2","cuCtxCreate");
    a->cuCtxDestroy=(PFN_cuCtxDestroy)gp2(a->dll,"cuCtxDestroy_v2","cuCtxDestroy");
    a->cuModuleLoadDataEx=(PFN_cuModuleLoadDataEx)gp(a->dll,"cuModuleLoadDataEx");
    a->cuModuleGetFunction=(PFN_cuModuleGetFunction)gp(a->dll,"cuModuleGetFunction");
    a->cuModuleUnload=(PFN_cuModuleUnload)gp(a->dll,"cuModuleUnload");
    a->cuMemAlloc=(PFN_cuMemAlloc)gp2(a->dll,"cuMemAlloc_v2","cuMemAlloc");
    a->cuMemFree=(PFN_cuMemFree)gp2(a->dll,"cuMemFree_v2","cuMemFree");
    a->cuMemcpyDtoH=(PFN_cuMemcpyDtoH)gp2(a->dll,"cuMemcpyDtoH_v2","cuMemcpyDtoH");
    a->cuLaunchKernel=(PFN_cuLaunchKernel)gp(a->dll,"cuLaunchKernel");
    a->cuCtxSynchronize=(PFN_cuCtxSynchronize)gp(a->dll,"cuCtxSynchronize");
    if(!a->cuInit||!a->cuDeviceGetCount||!a->cuDeviceGet||!a->cuDeviceGetName||
       !a->cuDeviceComputeCapability||!a->cuCtxCreate||!a->cuCtxDestroy||
       !a->cuModuleLoadDataEx||!a->cuModuleGetFunction||!a->cuModuleUnload||
       !a->cuMemAlloc||!a->cuMemFree||!a->cuMemcpyDtoH||!a->cuLaunchKernel||
       !a->cuCtxSynchronize){FreeLibrary(a->dll);clear_bytes(a,sizeof(*a));return -1;}
    return 1;
}

static void cuda_unload(CudaApi* a){if(a->dll)FreeLibrary(a->dll);clear_bytes(a,sizeof(*a));}
static void mem_zero(GpuMem* m){clear_bytes(m,sizeof(*m));}
static void mem_free(CudaApi* a,GpuMem* m){
    if(m->hit)a->cuMemFree(m->hit); if(m->candidate)a->cuMemFree(m->candidate);
    if(m->mtemp)a->cuMemFree(m->mtemp); if(m->gaptemp)a->cuMemFree(m->gaptemp);
    if(m->stats)a->cuMemFree(m->stats); if(m->out)a->cuMemFree(m->out);
    if(m->fwait)a->cuMemFree(m->fwait); if(m->ftotal)a->cuMemFree(m->ftotal);
    if(m->fcur)a->cuMemFree(m->fcur); if(m->flvl)a->cuMemFree(m->flvl);
    if(m->count)a->cuMemFree(m->count); if(m->q)a->cuMemFree(m->q);
    mem_zero(m);
}
static int mem_alloc(CudaApi* a,GpuMem* m){
    UINT_PTR big=(UINT_PTR)(GPU_MAX_DIMS*sizeof(UINT64));
    mem_zero(m);
    if(a->cuMemAlloc(&m->q,big)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->count,8)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->flvl,big)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->fcur,big)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->ftotal,big)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->fwait,big)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->out,40)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->stats,40)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->gaptemp,8)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->mtemp,8)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->candidate,8)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->hit,8)!=CUDA_SUCCESS)goto fail;
    return 1;
fail: mem_free(a,m);return 0;
}

static int launch(CudaApi* a,CUfunction fn,GpuMem* m,unsigned int mode,UINT64 value,
                  UINT64* resolved_p,UINT64* gap,UINT64* next_p,OwnSolveStats* stats){
    UINT64 host_out[4]; UINT64 host_stats[5]; UINT64 maxcount=GPU_MAX_DIMS;
    void* args[15];
    args[0]=&mode; args[1]=&value; args[2]=&m->q; args[3]=&m->count;
    args[4]=&m->flvl; args[5]=&m->fcur; args[6]=&m->ftotal; args[7]=&m->fwait;
    args[8]=&maxcount; args[9]=&m->out; args[10]=&m->stats; args[11]=&m->gaptemp;
    args[12]=&m->mtemp; args[13]=&m->candidate; args[14]=&m->hit;
    if(a->cuLaunchKernel(fn,1,1,1,256,1,1,0,NULL,args,NULL)!=CUDA_SUCCESS)return 0;
    if(a->cuCtxSynchronize()!=CUDA_SUCCESS)return 0;
    if(a->cuMemcpyDtoH(host_out,m->out,sizeof(host_out))!=CUDA_SUCCESS)return 0;
    if(host_out[0]!=1)return 0;
    if(a->cuMemcpyDtoH(host_stats,m->stats,sizeof(host_stats))!=CUDA_SUCCESS)return 0;
    if(resolved_p)*resolved_p=host_out[1];
    if(gap)*gap=host_out[2];
    if(next_p)*next_p=host_out[3];
    copy_stats(stats,host_stats);
    return 1;
}

static int launch_explore(CudaApi* a,CUfunction fn,GpuMem* m,UINT64 p,UINT64 rank,UINT64 steps,
                          UINT64* out_p,UINT64* out_rank,UINT64* out_gap,UINT64* done,
                          OwnSolveStats* stats,int* reached_limit){
    UINT64 host_out[5]; UINT64 host_stats[5]; UINT64 maxcount=GPU_MAX_DIMS;
    void* args[16];
    args[0]=&p;args[1]=&rank;args[2]=&steps;args[3]=&m->q;args[4]=&m->count;
    args[5]=&m->flvl;args[6]=&m->fcur;args[7]=&m->ftotal;args[8]=&m->fwait;
    args[9]=&maxcount;args[10]=&m->out;args[11]=&m->stats;args[12]=&m->gaptemp;
    args[13]=&m->mtemp;args[14]=&m->candidate;args[15]=&m->hit;
    if(a->cuLaunchKernel(fn,1,1,1,256,1,1,0,NULL,args,NULL)!=CUDA_SUCCESS)return 0;
    if(a->cuCtxSynchronize()!=CUDA_SUCCESS)return 0;
    if(a->cuMemcpyDtoH(host_out,m->out,sizeof(host_out))!=CUDA_SUCCESS)return 0;
    if(host_out[0]!=1 && host_out[0]!=3)return 0;
    if(a->cuMemcpyDtoH(host_stats,m->stats,sizeof(host_stats))!=CUDA_SUCCESS)return 0;
    if(out_p)*out_p=host_out[1];if(out_rank)*out_rank=host_out[2];if(out_gap)*out_gap=host_out[3];if(done)*done=host_out[4];
    if(reached_limit)*reached_limit=(host_out[0]==3);
    copy_stats(stats,host_stats);
    return 1;
}

int gpu_survivor_solve(int mode, UINT64 value, GpuSolveResult* out){
    CudaApi a; CUdevice dev=0; CUcontext ctx=NULL; CUmodule mod=NULL; CUfunction fn=NULL;
    GpuMem mem; int lr,count=0; int result=GPU_SOLVER_ERROR; char name[128]; int maj=0,min=0;
    UINT64 p=0,g=0,next=0; OwnSolveStats st;
    if(!out)return GPU_SOLVER_ERROR;
    clear_bytes(out,sizeof(*out)); clear_bytes(name,sizeof(name)); clear_bytes(&st,sizeof(st)); mem_zero(&mem);
    lr=cuda_load(&a);
    if(lr==0){out->error_stage=1;return GPU_SOLVER_NO_NVIDIA;}
    if(lr<0){out->error_stage=2;return GPU_SOLVER_ERROR;}
    if(a.cuInit(0)!=CUDA_SUCCESS){out->error_stage=3;cuda_unload(&a);return GPU_SOLVER_ERROR;}
    if(a.cuDeviceGetCount(&count)!=CUDA_SUCCESS){out->error_stage=4;cuda_unload(&a);return GPU_SOLVER_ERROR;}
    if(count<=0){out->error_stage=4;cuda_unload(&a);return GPU_SOLVER_NO_NVIDIA;}
    if(a.cuDeviceGet(&dev,0)!=CUDA_SUCCESS){out->error_stage=5;goto done;}
    a.cuDeviceGetName(name,127,dev); name[127]=0;
    a.cuDeviceComputeCapability(&maj,&min,dev);
    copy_ascii(out->device_name,128,name);out->cc_major=maj;out->cc_minor=min;
    if(a.cuCtxCreate(&ctx,0,dev)!=CUDA_SUCCESS){out->error_stage=6;goto done;}
    if(a.cuModuleLoadDataEx(&mod,g_gpu_kernel_ptx,0,NULL,NULL)!=CUDA_SUCCESS){out->error_stage=7;goto done;}
    if(a.cuModuleGetFunction(&fn,mod,"zero_candidate_prime_hd")!=CUDA_SUCCESS){out->error_stage=8;goto done;}
    if(!mem_alloc(&a,&mem)){out->error_stage=9;goto done;}

    /* Runtime JIT + arithmetic self-test. A detected NVIDIA GPU is never silently
       replaced by CPU if this authoritative GPU path is broken. */
    if(!launch(&a,fn,&mem,0,113,&p,&g,&next,&st)||p!=113||g!=14){out->error_stage=10;goto done;}
    if(!launch(&a,fn,&mem,0,839,&p,&g,&next,&st)||p!=839||g!=14){out->error_stage=11;goto done;}
    if(!launch(&a,fn,&mem,0,887,&p,&g,&next,&st)||p!=887||g!=20){out->error_stage=12;goto done;}
    if(!launch(&a,fn,&mem,2,31,&p,&g,&next,&st)||p!=127||g!=0){out->error_stage=13;goto done;}
    out->self_test_pass=1;

    if(!launch(&a,fn,&mem,(unsigned int)mode,value,&p,&g,&next,&st)){out->error_stage=14;goto done;}
    out->p=p;out->gap=g;out->next_p=next;out->stats=st;out->error_stage=0;result=GPU_SOLVER_OK;

done:
    if(mem.q)mem_free(&a,&mem);
    if(mod)a.cuModuleUnload(mod);
    if(ctx)a.cuCtxDestroy(ctx);
    cuda_unload(&a);
    return result;
}

int gpu_survivor_explore(volatile int* cancel, GpuExploreProgressFn progress,
                         void* user, GpuExploreResult* out){
    CudaApi a;CUdevice dev=0;CUcontext ctx=NULL;CUmodule mod=NULL;CUfunction fn=NULL,fx=NULL;
    GpuMem mem;int lr,count=0,result=GPU_SOLVER_ERROR;char name[128];int maj=0,min=0,limit=0;
    UINT64 p=2,rank=1,g=0,next=0,done=0,t0,last_cb,batch=64,bstart,bms;OwnSolveStats st;GpuExploreResult snap;
    if(!out)return GPU_SOLVER_ERROR;
    clear_bytes(out,sizeof(*out));clear_bytes(&snap,sizeof(snap));clear_bytes(name,sizeof(name));clear_bytes(&st,sizeof(st));mem_zero(&mem);
    lr=cuda_load(&a);
    if(lr==0){out->error_stage=1;return GPU_SOLVER_NO_NVIDIA;}
    if(lr<0){out->error_stage=2;return GPU_SOLVER_ERROR;}
    if(a.cuInit(0)!=CUDA_SUCCESS){out->error_stage=3;cuda_unload(&a);return GPU_SOLVER_ERROR;}
    if(a.cuDeviceGetCount(&count)!=CUDA_SUCCESS){out->error_stage=4;cuda_unload(&a);return GPU_SOLVER_ERROR;}
    if(count<=0){out->error_stage=4;cuda_unload(&a);return GPU_SOLVER_NO_NVIDIA;}
    if(a.cuDeviceGet(&dev,0)!=CUDA_SUCCESS){out->error_stage=5;goto done_all;}
    a.cuDeviceGetName(name,127,dev);name[127]=0;a.cuDeviceComputeCapability(&maj,&min,dev);
    copy_ascii(out->device_name,128,name);out->cc_major=maj;out->cc_minor=min;
    if(a.cuCtxCreate(&ctx,0,dev)!=CUDA_SUCCESS){out->error_stage=6;goto done_all;}
    if(a.cuModuleLoadDataEx(&mod,g_gpu_kernel_ptx,0,NULL,NULL)!=CUDA_SUCCESS){out->error_stage=7;goto done_all;}
    if(a.cuModuleGetFunction(&fn,mod,"zero_candidate_prime_hd")!=CUDA_SUCCESS){out->error_stage=8;goto done_all;}
    if(a.cuModuleGetFunction(&fx,mod,"zero_candidate_prime_explore_hd")!=CUDA_SUCCESS){out->error_stage=9;goto done_all;}
    if(!mem_alloc(&a,&mem)){out->error_stage=10;goto done_all;}
    if(!launch(&a,fn,&mem,0,113,&p,&g,&next,&st)||p!=113||g!=14){out->error_stage=11;goto done_all;}
    if(!launch(&a,fn,&mem,0,839,&p,&g,&next,&st)||p!=839||g!=14){out->error_stage=12;goto done_all;}
    if(!launch(&a,fn,&mem,0,887,&p,&g,&next,&st)||p!=887||g!=20){out->error_stage=13;goto done_all;}
    if(!launch(&a,fn,&mem,2,31,&p,&g,&next,&st)||p!=127||g!=0){out->error_stage=14;goto done_all;}
    out->self_test_pass=1;
    /* Reset the persistent cache to the exact seed (rank=1,p=2). */
    if(!launch(&a,fn,&mem,2,1,&p,&g,&next,&st)||p!=2){out->error_stage=15;goto done_all;}
    rank=1;g=0;t0=GetTickCount64();last_cb=t0;
    for(;;){
        if(cancel&&*cancel){out->end_reason=1;break;}
        bstart=GetTickCount64();
        if(!launch_explore(&a,fx,&mem,p,rank,batch,&p,&rank,&g,&done,&st,&limit)){out->error_stage=16;goto done_all;}
        bms=GetTickCount64()-bstart;
        if(bms>400 && batch>1)batch/=2;else if(bms<60 && batch<256)batch*=2;
        if(limit){out->end_reason=2;break;}
        if(progress && (GetTickCount64()-last_cb>=200)){
            clear_bytes(&snap,sizeof(snap));snap.rank=rank;snap.p=p;snap.gap=g;snap.elapsed_ms=GetTickCount64()-t0;snap.stats=st;
            copy_ascii(snap.device_name,128,name);snap.cc_major=maj;snap.cc_minor=min;snap.self_test_pass=1;
            progress(&snap,user);last_cb=GetTickCount64();
        }
    }
    out->rank=rank;out->p=p;out->gap=g;out->elapsed_ms=GetTickCount64()-t0;out->stats=st;out->error_stage=0;result=GPU_SOLVER_OK;

done_all:
    if(mem.q)mem_free(&a,&mem);if(mod)a.cuModuleUnload(mod);if(ctx)a.cuCtxDestroy(ctx);cuda_unload(&a);return result;
}

/* -------------------------------------------------------------------------
   RECORD-SEEDED 128-BIT IMPLICIT-DIMENSION GPU PATH
   -------------------------------------------------------------------------
   All storage below is a fixed-size live recursion workspace.  q values are
   rebuilt from q0=2 inside every successor step and are never retained as a
   persistent prime basis.  The host never computes a gap on the NVIDIA path.
*/
#define GPU_IMPLICIT_MAX_DEPTH 256ULL

typedef struct GpuWideMem {
    CUdeviceptr q,count,flvl,fcur,ftotal,fwait;
    CUdeviceptr wflvl,wlo,whi,wtotal,wwait;
    CUdeviceptr out,stats,gaptemp,mtemp,candlo,candhi,hit,minlevel;
} GpuWideMem;

static void wide_mem_zero(GpuWideMem* m){clear_bytes(m,sizeof(*m));}
static void wide_mem_free(CudaApi* a,GpuWideMem* m){
    if(m->minlevel)a->cuMemFree(m->minlevel);if(m->hit)a->cuMemFree(m->hit);
    if(m->candhi)a->cuMemFree(m->candhi);if(m->candlo)a->cuMemFree(m->candlo);
    if(m->mtemp)a->cuMemFree(m->mtemp);if(m->gaptemp)a->cuMemFree(m->gaptemp);
    if(m->stats)a->cuMemFree(m->stats);if(m->out)a->cuMemFree(m->out);
    if(m->wwait)a->cuMemFree(m->wwait);if(m->wtotal)a->cuMemFree(m->wtotal);
    if(m->whi)a->cuMemFree(m->whi);if(m->wlo)a->cuMemFree(m->wlo);if(m->wflvl)a->cuMemFree(m->wflvl);
    if(m->fwait)a->cuMemFree(m->fwait);if(m->ftotal)a->cuMemFree(m->ftotal);
    if(m->fcur)a->cuMemFree(m->fcur);if(m->flvl)a->cuMemFree(m->flvl);
    if(m->count)a->cuMemFree(m->count);if(m->q)a->cuMemFree(m->q);wide_mem_zero(m);
}
static int wide_mem_alloc(CudaApi* a,GpuWideMem* m){
    UINT_PTR b=(UINT_PTR)(GPU_IMPLICIT_MAX_DEPTH*sizeof(UINT64));wide_mem_zero(m);
    if(a->cuMemAlloc(&m->q,b)!=CUDA_SUCCESS)goto fail;if(a->cuMemAlloc(&m->count,8)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->flvl,b)!=CUDA_SUCCESS)goto fail;if(a->cuMemAlloc(&m->fcur,b)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->ftotal,b)!=CUDA_SUCCESS)goto fail;if(a->cuMemAlloc(&m->fwait,b)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->wflvl,b)!=CUDA_SUCCESS)goto fail;if(a->cuMemAlloc(&m->wlo,b)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->whi,b)!=CUDA_SUCCESS)goto fail;if(a->cuMemAlloc(&m->wtotal,b)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->wwait,b)!=CUDA_SUCCESS)goto fail;if(a->cuMemAlloc(&m->out,64)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->stats,40)!=CUDA_SUCCESS)goto fail;if(a->cuMemAlloc(&m->gaptemp,8)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->mtemp,8)!=CUDA_SUCCESS)goto fail;if(a->cuMemAlloc(&m->candlo,8)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->candhi,8)!=CUDA_SUCCESS)goto fail;if(a->cuMemAlloc(&m->hit,8)!=CUDA_SUCCESS)goto fail;
    if(a->cuMemAlloc(&m->minlevel,8)!=CUDA_SUCCESS)goto fail;return 1;
fail:wide_mem_free(a,m);return 0;
}

static int wide_launch(CudaApi* a,CUfunction fn,GpuWideMem* m,
                       OwnU128 rank,OwnU128 p,UINT64 steps,UINT64 depth_cap,
                       OwnU128* out_rank,OwnU128* out_p,UINT64* out_gap,UINT64* done,
                       OwnSolveStats* stats,int* reached_limit){
    UINT64 h[8];UINT64 hs[5];UINT64 rlo=rank.lo,rhi=rank.hi,plo=p.lo,phi=p.hi;
    void* args[25];
    args[0]=&rlo;args[1]=&rhi;args[2]=&plo;args[3]=&phi;args[4]=&steps;args[5]=&depth_cap;
    args[6]=&m->q;args[7]=&m->count;args[8]=&m->flvl;args[9]=&m->fcur;args[10]=&m->ftotal;args[11]=&m->fwait;
    args[12]=&m->wflvl;args[13]=&m->wlo;args[14]=&m->whi;args[15]=&m->wtotal;args[16]=&m->wwait;
    args[17]=&m->out;args[18]=&m->stats;args[19]=&m->gaptemp;args[20]=&m->mtemp;
    args[21]=&m->candlo;args[22]=&m->candhi;args[23]=&m->hit;args[24]=&m->minlevel;
    if(a->cuLaunchKernel(fn,1,1,1,256,1,1,0,NULL,args,NULL)!=CUDA_SUCCESS)return 0;
    if(a->cuCtxSynchronize()!=CUDA_SUCCESS)return 0;
    if(a->cuMemcpyDtoH(h,m->out,sizeof(h))!=CUDA_SUCCESS)return 0;if(h[0]!=1&&h[0]!=3)return 0;
    if(a->cuMemcpyDtoH(hs,m->stats,sizeof(hs))!=CUDA_SUCCESS)return 0;
    if(out_rank){out_rank->lo=h[1];out_rank->hi=h[2];}if(out_p){out_p->lo=h[3];out_p->hi=h[4];}
    if(out_gap)*out_gap=h[5];if(done)*done=h[6];if(reached_limit)*reached_limit=(h[0]==3);copy_stats(stats,hs);return 1;
}

int gpu_record_explore(OwnU128 seed_rank, OwnU128 seed_p, UINT64 causal_depth_cap,
                       volatile int* cancel, GpuRecordExploreProgressFn progress,
                       void* user, GpuRecordExploreResult* out){
    CudaApi a;CUdevice dev=0;CUcontext ctx=NULL;CUmodule mod=NULL;CUfunction fn=NULL;GpuWideMem mem;
    int lr,count=0,result=GPU_SOLVER_ERROR,limit=0;char name[128];int maj=0,min=0;
    OwnU128 rank=seed_rank,p=seed_p,tr,tp;UINT64 g=0,done=0,t0,last,batch=16,bstart,bms;
    OwnSolveStats st;GpuRecordExploreResult snap;
    if(!out)return GPU_SOLVER_ERROR;clear_bytes(out,sizeof(*out));clear_bytes(&snap,sizeof(snap));
    clear_bytes(name,sizeof(name));clear_bytes(&st,sizeof(st));wide_mem_zero(&mem);
    if(causal_depth_cap<4)causal_depth_cap=4;if(causal_depth_cap>GPU_IMPLICIT_MAX_DEPTH)causal_depth_cap=GPU_IMPLICIT_MAX_DEPTH;
    lr=cuda_load(&a);if(lr==0){out->error_stage=1;return GPU_SOLVER_NO_NVIDIA;}if(lr<0){out->error_stage=2;return GPU_SOLVER_ERROR;}
    if(a.cuInit(0)!=CUDA_SUCCESS){out->error_stage=3;cuda_unload(&a);return GPU_SOLVER_ERROR;}
    if(a.cuDeviceGetCount(&count)!=CUDA_SUCCESS){out->error_stage=4;cuda_unload(&a);return GPU_SOLVER_ERROR;}
    if(count<=0){out->error_stage=4;cuda_unload(&a);return GPU_SOLVER_NO_NVIDIA;}
    if(a.cuDeviceGet(&dev,0)!=CUDA_SUCCESS){out->error_stage=5;goto done_all;}a.cuDeviceGetName(name,127,dev);name[127]=0;
    a.cuDeviceComputeCapability(&maj,&min,dev);copy_ascii(out->device_name,128,name);out->cc_major=maj;out->cc_minor=min;
    if(a.cuCtxCreate(&ctx,0,dev)!=CUDA_SUCCESS){out->error_stage=6;goto done_all;}
    if(a.cuModuleLoadDataEx(&mod,g_gpu_kernel_ptx,0,NULL,NULL)!=CUDA_SUCCESS){out->error_stage=7;goto done_all;}
    if(a.cuModuleGetFunction(&fn,mod,"zero_candidate_prime_record_implicit")!=CUDA_SUCCESS){out->error_stage=8;goto done_all;}
    if(!wide_mem_alloc(&a,&mem)){out->error_stage=9;goto done_all;}

    tr.lo=30;tr.hi=0;tp.lo=113;tp.hi=0;
    if(!wide_launch(&a,fn,&mem,tr,tp,1,5,&tr,&tp,&g,&done,&st,&limit)||limit||g!=14||tp.hi||tp.lo!=127){out->error_stage=10;goto done_all;}
    tr.lo=146;tr.hi=0;tp.lo=839;tp.hi=0;
    if(!wide_launch(&a,fn,&mem,tr,tp,1,10,&tr,&tp,&g,&done,&st,&limit)||limit||g!=14||tp.hi||tp.lo!=853){out->error_stage=11;goto done_all;}
    tr.lo=154;tr.hi=0;tp.lo=887;tp.hi=0;
    if(!wide_launch(&a,fn,&mem,tr,tp,1,10,&tr,&tp,&g,&done,&st,&limit)||limit||g!=20||tp.hi||tp.lo!=907){out->error_stage=12;goto done_all;}
    out->self_test_pass=1;t0=GetTickCount64();last=t0;
    for(;;){
        if(cancel&&*cancel){out->end_reason=1;break;}bstart=GetTickCount64();
        if(!wide_launch(&a,fn,&mem,rank,p,batch,causal_depth_cap,&rank,&p,&g,&done,&st,&limit)){out->error_stage=13;goto done_all;}
        bms=GetTickCount64()-bstart;if(limit){out->end_reason=2;break;}
        if(bms>450&&batch>1)batch/=2;else if(bms<80&&batch<128)batch*=2;
        if(progress&&(GetTickCount64()-last>=200)){clear_bytes(&snap,sizeof(snap));snap.rank=rank;snap.p=p;snap.gap=g;
            snap.elapsed_ms=GetTickCount64()-t0;snap.causal_depth_cap=causal_depth_cap;snap.stats=st;snap.cc_major=maj;snap.cc_minor=min;
            snap.self_test_pass=1;copy_ascii(snap.device_name,128,name);progress(&snap,user);last=GetTickCount64();}
    }
    out->rank=rank;out->p=p;out->gap=g;out->elapsed_ms=GetTickCount64()-t0;out->causal_depth_cap=causal_depth_cap;
    out->stats=st;out->error_stage=0;result=GPU_SOLVER_OK;
done_all:
    if(mem.q)wide_mem_free(&a,&mem);if(mod)a.cuModuleUnload(mod);if(ctx)a.cuCtxDestroy(ctx);cuda_unload(&a);return result;
}
