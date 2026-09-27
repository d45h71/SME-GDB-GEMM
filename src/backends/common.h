#ifndef MP5_PREPARED_COMMON_H
#define MP5_PREPARED_COMMON_H
#include "prepared_api.h"
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <initializer_list>

static bool mul_u64(uint64_t a,uint64_t b,uint64_t *v) {
    if (b && a>UINT64_MAX/b) return false;
    *v=a*b; return true;
}
static bool add_u64(uint64_t a,uint64_t b,uint64_t *v) {
    if (a>UINT64_MAX-b) return false;
    *v=a+b; return true;
}
static bool round_u64(uint64_t a,uint64_t q,uint64_t *v) {
    uint64_t t;
    if (!q || !add_u64(a,q-1,&t)) return false;
    return mul_u64(t/q,q,v);
}
static bool basic_descriptor(const mp5_descriptor *d,bool column,uint32_t element,
                             mp5_requirements *r) {
    if (!d || !r || d->struct_bytes!=sizeof(*d) || d->abi_version!=MP5_ADAPTER_ABI_VERSION ||
        d->thread_policy!=MP5_SERIAL || !d->m || !d->n || !d->k ||
        d->m>INT_MAX || d->n>INT_MAX || d->k>INT_MAX ||
        d->lda!=(column?d->m:d->k) || d->ldb!=(column?d->k:d->n) ||
        d->ldc!=(column?d->m:d->n)) return false;
    uint64_t ak,bk,cn;
    if (!mul_u64(d->m,d->k,&ak) || !mul_u64(d->k,d->n,&bk) ||
        !mul_u64(d->m,d->n,&cn) || ak>INT_MAX || bk>INT_MAX || cn>INT_MAX) return false;
    memset(r,0,sizeof(*r));
    if (!mul_u64(ak,element,&r->a_bytes) || !mul_u64(bk,element,&r->b_bytes) ||
        !mul_u64(cn,4,&r->c_bytes)) return false;
    r->alignment_bytes=128; r->input_element_bytes=element;
    r->output_element_bytes=4; r->requested_workers=1;
    return true;
}
static bool buffers_valid(const mp5_requirements &r,mp5_buffer a,mp5_buffer b,mp5_buffer c) {
    mp5_buffer v[3]={a,b,c}; uint64_t needs[3]={r.a_bytes,r.b_bytes,r.c_bytes};
    uintptr_t lo[3],hi[3];
    for (int i=0;i<3;++i) {
        lo[i]=reinterpret_cast<uintptr_t>(v[i].data);
        if (!lo[i] || lo[i]%128 || v[i].bytes<needs[i] || v[i].bytes>UINTPTR_MAX-lo[i]) return false;
        hi[i]=lo[i]+v[i].bytes;
    }
    for (int i=0;i<3;++i) for (int j=i+1;j<3;++j)
        if (lo[i]<hi[j] && lo[j]<hi[i]) return false;
    return true;
}
static void *allocate_aligned(uint64_t bytes) {
    void *p=nullptr;
    if (!bytes || bytes>SIZE_MAX || posix_memalign(&p,128,static_cast<size_t>(bytes))) return nullptr;
    return p;
}
#endif
