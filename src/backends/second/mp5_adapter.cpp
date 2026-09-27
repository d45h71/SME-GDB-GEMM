#include "adapter_abi.h"
#include <cstdlib>
#include <cstring>
#include <climits>
#include <pthread.h>
#if defined(MP5_STOCK_FAMILY)
#include "smegemm.h"
#elif defined(MP5_ACCELERATE)
#ifndef ACCELERATE_NEW_LAPACK
#error adapter requires the new LP64 Accelerate API.
#endif
#include <Accelerate/Accelerate.h>
static_assert(sizeof(__LAPACK_int)==4,"adapter requires LP64 int32 dimensions");
#elif defined(MP5_OPENBLAS)
#include "cblas.h"
static_assert(sizeof(blasint)==4,"adapter requires LP64 int32 dimensions");
#endif

struct mp5_context { mp5_descriptor d; mp5_requirements r; pthread_t owner; };
static const char* names[]={"ROW_FP32_AB","COL_FP32_ACCUMULATE","ROW_FP64_AB","ROW_FP16_FP32_AB","ROW_INT8_INT32_AB","ROW_FP32_ACCUMULATE","COL_FP32_AB","NATIVE_ROW_FP32_AB","NATIVE_COL_FP32_ACCUMULATE","NATIVE_ROW_FP64_AB"};
extern "C" const char *mp5_backend_name(){return MP5_BACKEND_NAME;}
extern "C" const char *mp5_endpoint_name(uint32_t e){return e<10?names[e]:"INVALID";}
static bool column(unsigned e){return e==1||e==6||e==8;}
static bool f64(unsigned e){return e==2||e==9;}
static bool accum(unsigned e){return e==1||e==5||e==8;}
static bool supported(unsigned e,unsigned t){
#if defined(MP5_STOCK_FAMILY)
    return e<=4 && t<=1;
#elif defined(MP5_ACCELERATE)
    return e<=6 && e!=3 && e!=4 && t<=1;
#else
    return e<=9 && e!=3 && e!=4 && t==0;
#endif
}
static int set_threads(unsigned t){
#if defined(MP5_ACCELERATE)
    auto mode=t?BLAS_THREADING_MULTI_THREADED:BLAS_THREADING_SINGLE_THREADED;
    if(BLASSetThreading(mode)!=0 || BLASGetThreading()!=mode)return MP5_THREAD_POLICY_FAILED;
#elif defined(MP5_OPENBLAS)
    (void)t;openblas_set_num_threads(1);
    if(openblas_get_num_threads()!=1)return MP5_THREAD_POLICY_FAILED;
#else
    (void)t;
#endif
    return MP5_OK;
}
extern "C" int mp5_create(const mp5_descriptor*d,mp5_context**out,mp5_requirements*r){
    if(out)*out=nullptr;
    if(!d||!out||!r||d->struct_bytes!=sizeof(*d)||d->abi_version!=MP5_ADAPTER_ABI_VERSION)return MP5_INVALID;
    if(!supported(d->endpoint,d->thread_policy))return MP5_UNSUPPORTED;
    // Stock's C offset arithmetic is signed int. Apply the same conservative range to all.
    if(!d->m||!d->n||!d->k||d->m>INT_MAX-512||d->n>INT_MAX-512||d->k>INT_MAX-2048)return MP5_INVALID;
    if(d->m*d->k>INT_MAX||d->n*d->k>INT_MAX||d->m*d->n>INT_MAX)return MP5_INVALID;
    const bool col=column(d->endpoint);
    if(d->lda!=(col?d->m:d->k)||d->ldb!=(col?d->k:d->n)||d->ldc!=(col?d->m:d->n))return MP5_INVALID;
    unsigned in=f64(d->endpoint)?8:d->endpoint==3?2:d->endpoint==4?1:4;
    unsigned output=f64(d->endpoint)?8:4;
    *r={d->m*d->k*in,d->n*d->k*in,d->m*d->n*output,0,128,in,output,1};
#if defined(MP5_STOCK_FAMILY)
    // Exact largest stock allocation pair: FP64 (512*1024+1024*256)*8.
    r->max_backend_scratch_bytes=6291456ull*(d->thread_policy?4:1);
    r->requested_workers=d->thread_policy?4:1;
#elif defined(MP5_ACCELERATE)
    // External framework scratch is opaque; planning allowance, not measured allocation.
    r->max_backend_scratch_bytes=512ull*1024*1024;
    r->requested_workers=d->thread_policy?0:1;
#else
    r->max_backend_scratch_bytes=64ull*1024*1024;
#endif
    if(r->a_bytes+r->b_bytes+2*r->c_bytes+r->max_backend_scratch_bytes>12ull*1024*1024*1024)return MP5_INVALID;
    int st=set_threads(d->thread_policy);if(st)return st;
    auto c=static_cast<mp5_context*>(std::malloc(sizeof(mp5_context)));
    if(!c)return MP5_ALLOCATION_FAILED;
    *c={*d,*r,pthread_self()};*out=c;return MP5_OK;
}
static bool overlap(mp5_buffer a,mp5_buffer b){
    uintptr_t x=(uintptr_t)a.data,y=(uintptr_t)b.data;
    if(a.bytes>UINTPTR_MAX-x||b.bytes>UINTPTR_MAX-y)return true;
    return x<y+b.bytes&&y<x+a.bytes;
}
extern "C" int mp5_run(mp5_context*c,mp5_buffer a,mp5_buffer b,mp5_buffer out){
    if(!c||!pthread_equal(c->owner,pthread_self()))return MP5_INVALID;
    if(!a.data||!b.data||!out.data||a.bytes<c->r.a_bytes||b.bytes<c->r.b_bytes||out.bytes<c->r.c_bytes)return MP5_INVALID;
    if(((uintptr_t)a.data|(uintptr_t)b.data|(uintptr_t)out.data)&127)return MP5_INVALID;
    if(overlap(a,b)||overlap(a,out)||overlap(b,out))return MP5_INVALID;
    const auto &d=c->d;int m=(int)d.m,n=(int)d.n,k=(int)d.k;
#if defined(MP5_STOCK_FAMILY)
    bool p=d.thread_policy!=0;
    switch(d.endpoint){
    case 0:(p?dispatch_row_sgemm:row_sgemm)(m,n,k,(float*)a.data,(float*)b.data,(float*)out.data);break;
    case 1:(p?dispatch_col_sgemm:col_sgemm)(m,n,k,(float*)a.data,(float*)b.data,(float*)out.data);break;
    case 2:(p?dispatch_row_dgemm:row_dgemm)(m,n,k,(double*)a.data,(double*)b.data,(double*)out.data);break;
    case 3:(p?dispatch_row_shgemm:row_shgemm)(m,n,k,(__fp16*)a.data,(__fp16*)b.data,(float*)out.data);break;
    case 4:(p?dispatch_row_bgemm:row_bgemm)(m,n,k,(int8_t*)a.data,(int8_t*)b.data,(int*)out.data);break;
    default:return MP5_UNSUPPORTED;
    }
#else
#if defined(MP5_ACCELERATE)
    auto mode=d.thread_policy?BLAS_THREADING_MULTI_THREADED:BLAS_THREADING_SINGLE_THREADED;
    if(BLASGetThreading()!=mode)return MP5_THREAD_POLICY_FAILED;
#else
    if(openblas_get_num_threads()!=1)return MP5_THREAD_POLICY_FAILED;
    if(d.endpoint>=7){
        char no='N';bool col=column(d.endpoint);int mm=col?m:n,nn=col?n:m;
        int lda=col?m:n,ldb=k,ldc=col?m:n;
        if(f64(d.endpoint)){double alpha=1,beta=0;dgemm_(&no,&no,&mm,&nn,&k,&alpha,(double*)(col?a.data:b.data),&lda,(double*)(col?b.data:a.data),&ldb,&beta,(double*)out.data,&ldc);}
        else{float alpha=1,beta=accum(d.endpoint)?1:0;sgemm_(&no,&no,&mm,&nn,&k,&alpha,(float*)(col?a.data:b.data),&lda,(float*)(col?b.data:a.data),&ldb,&beta,(float*)out.data,&ldc);}
        return MP5_OK;
    }
#endif
    auto layout=column(d.endpoint)?CblasColMajor:CblasRowMajor;
    if(f64(d.endpoint))cblas_dgemm(layout,CblasNoTrans,CblasNoTrans,m,n,k,1.,(double*)a.data,(int)d.lda,(double*)b.data,(int)d.ldb,0.,(double*)out.data,(int)d.ldc);
    else cblas_sgemm(layout,CblasNoTrans,CblasNoTrans,m,n,k,1.f,(float*)a.data,(int)d.lda,(float*)b.data,(int)d.ldb,accum(d.endpoint)?1.f:0.f,(float*)out.data,(int)d.ldc);
#endif
    return MP5_OK;
}
extern "C" void mp5_destroy(mp5_context*c){std::free(c);}
extern "C" const char *mp5_native_symbol(const mp5_context*c){
    if(!c)return "INVALID";
#if defined(MP5_STOCK_FAMILY)
    static const char* s[]={"row_sgemm","col_sgemm","row_dgemm","row_shgemm","row_bgemm"};
    static const char* p[]={"dispatch_row_sgemm","dispatch_col_sgemm","dispatch_row_dgemm","dispatch_row_shgemm","dispatch_row_bgemm"};
    return (c->d.thread_policy?p:s)[c->d.endpoint];
#elif defined(MP5_ACCELERATE)
    return f64(c->d.endpoint)?"cblas_dgemm$NEWLAPACK":"cblas_sgemm$NEWLAPACK";
#else
    return c->d.endpoint>=7?(f64(c->d.endpoint)?"dgemm_":"sgemm_"):(f64(c->d.endpoint)?"cblas_dgemm":"cblas_sgemm");
#endif
}
extern "C" const void *mp5_native_address(const mp5_context*c){
    if(!c)return nullptr;
#if defined(MP5_STOCK_FAMILY)
    const void*s[]={(void*)row_sgemm,(void*)col_sgemm,(void*)row_dgemm,(void*)row_shgemm,(void*)row_bgemm};
    const void*p[]={(void*)dispatch_row_sgemm,(void*)dispatch_col_sgemm,(void*)dispatch_row_dgemm,(void*)dispatch_row_shgemm,(void*)dispatch_row_bgemm};
    return (c->d.thread_policy?p:s)[c->d.endpoint];
#elif defined(MP5_OPENBLAS)
    if(c->d.endpoint>=7)return f64(c->d.endpoint)?(void*)dgemm_:(void*)sgemm_;
#endif
#if !defined(MP5_STOCK_FAMILY)
    return f64(c->d.endpoint)?(void*)cblas_dgemm:(void*)cblas_sgemm;
#endif
}
extern "C" int mp5_thread_observation(const mp5_context*c){
    if(!c)return -1;
#if defined(MP5_ACCELERATE)
    return (int)BLASGetThreading(); // Enum policy; not measured thread count.
#elif defined(MP5_OPENBLAS)
    return openblas_get_num_threads();
#else
    return c->d.thread_policy?4:1; // Declared maximum tasks; not an observation of placement.
#endif
}
