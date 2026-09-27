#include "common.h"
#include "libxsmm.h"
#include <new>
#include <vector>

extern "C" void mp5_prepared_preserved_call(libxsmm_gemmfunction,const libxsmm_gemm_param*);
static constexpr uint64_t code_limit=128ull*1024;
struct mp5_context {
    mp5_descriptor d{};
    mp5_requirements r{};
    pthread_t owner{};
    libxsmm_gemmfunction prepared=nullptr;
    std::vector<unsigned char> snapshot;
    mp5_prepared_jit_info observation{};
};
/* The frozen module has single-context, single-thread ownership. Deregistration
 * and prepared lifetime cannot invalidate another context's generated code. */
static mp5_context *live_context=nullptr;
static bool on_owner(const mp5_context *c) { return c && c==live_context && pthread_equal(c->owner,pthread_self()); }
extern "C" const char *mp5_backend_name(void) { return "libxsmm-sme"; }
extern "C" const char *mp5_endpoint_name(uint32_t e) {
    return e==MP5_ROW_FP32_AB?"ROW_FP32_AB":e==MP5_COL_FP32_ACCUMULATE?"COL_FP32_ACCUMULATE":"UNSUPPORTED";
}
extern "C" int mp5_create(const mp5_descriptor *d,mp5_context **out,mp5_requirements *r) {
    if (out) *out=nullptr;
    if (!out || !d || !r) return MP5_INVALID;
    if (d->endpoint!=MP5_ROW_FP32_AB && d->endpoint!=MP5_COL_FP32_ACCUMULATE) return MP5_UNSUPPORTED;
    if (d->thread_policy!=MP5_SERIAL) return MP5_UNSUPPORTED;
    if (live_context || d->k>25600 || !basic_descriptor(d,d->endpoint==MP5_COL_FP32_ACCUMULATE,4,r)) return MP5_INVALID;
    auto *c=new(std::nothrow) mp5_context;
    if (!c) return MP5_ALLOCATION_FAILED;
    c->d=*d; c->r=*r; c->owner=pthread_self();
    /* 512 MiB is an opaque native-library planning allowance, not a measured
     * or enforced peak. Snapshot storage is separately bounded at 128 KiB.
     * The source-default 128 KiB JIT generator stack is also budgeted. */
    c->r.max_backend_scratch_bytes=512ull*1024*1024+2*code_limit+256*d->k+672;
    uint64_t total=c->r.max_backend_scratch_bytes;
    for(auto x:{r->a_bytes,r->b_bytes,r->c_bytes})
        if(!add_u64(total,x,&total)){delete c;return MP5_INVALID;}
    if(total>12ull*1024*1024*1024){delete c;return MP5_INVALID;}
    *r=c->r; *out=c; live_context=c; return MP5_OK;
}
static libxsmm_gemm_shape shape(const mp5_context *c) {
    const auto &d=c->d;
    const bool col=d.endpoint==MP5_COL_FP32_ACCUMULATE;
    return libxsmm_create_gemm_shape(col?d.m:d.n,col?d.n:d.m,d.k,
        col?d.m:d.n,d.k,col?d.m:d.n,
        LIBXSMM_DATATYPE_F32,LIBXSMM_DATATYPE_F32,LIBXSMM_DATATYPE_F32,LIBXSMM_DATATYPE_F32);
}
static int generate(mp5_context *c,libxsmm_gemmfunction *fn) {
    libxsmm_init();
    const int actual=libxsmm_cpuid(nullptr);
    if (actual!=LIBXSMM_AARCH64_APPL_M4) return MP5_UNSUPPORTED;
    libxsmm_set_target_archid(LIBXSMM_AARCH64_APPL_M4);
    if(libxsmm_get_target_archid()!=LIBXSMM_AARCH64_APPL_M4)return MP5_UNSUPPORTED;
    libxsmm_registry_info before{},after{};
    if (libxsmm_get_registry_info(&before)) return MP5_UNSUPPORTED;
    const libxsmm_bitfield flags=c->d.endpoint==MP5_ROW_FP32_AB?LIBXSMM_GEMM_FLAG_BETA_0:0;
    *fn=libxsmm_dispatch_gemm(shape(c),flags,LIBXSMM_GEMM_PREFETCH_NONE);
    if (!*fn || libxsmm_get_registry_info(&after)) return MP5_UNSUPPORTED;
    if (after.size==before.size+1) ++c->observation.generation_requests;
    else if (after.size==before.size) ++c->observation.cache_lookups;
    else return MP5_UNSUPPORTED;
    libxsmm_kernel_info ki{};
    libxsmm_xmmfunction u{};u.gemm=*fn;
    libxsmm_mmkernel_info mi{};
    if (libxsmm_get_kernel_info(reinterpret_cast<const void*>(*fn),&ki) ||
        libxsmm_get_mmkernel_info(u,&mi) || ki.kind!=LIBXSMM_KERNEL_KIND_MATMUL ||
        ki.is_reference_kernel || !ki.code_size || ki.code_size>code_limit) return MP5_UNSUPPORTED;
    const auto s=shape(c);
    if (mi.m!=s.m || mi.n!=s.n || mi.k!=s.k || mi.lda!=s.lda || mi.ldb!=s.ldb || mi.ldc!=s.ldc ||
        mi.iprecision!=LIBXSMM_DATATYPE_F32 || mi.oprecision!=LIBXSMM_DATATYPE_F32 ||
        mi.flags!=(flags|LIBXSMM_GEMM_FLAG_USE_XGEMM_ABI) || mi.prefetch!=LIBXSMM_GEMM_PREFETCH_NONE) return MP5_UNSUPPORTED;
    try {
        c->snapshot.resize(ki.code_size);
        memcpy(c->snapshot.data(),reinterpret_cast<const void*>(*fn),ki.code_size);
    } catch (const std::bad_alloc&) { return MP5_ALLOCATION_FAILED; }
    c->observation.code_bytes=ki.code_size;c->observation.target_arch_id=actual;c->observation.generated=1;
    return MP5_OK;
}
static void release(libxsmm_gemmfunction f) {
    if (f) libxsmm_release_kernel(reinterpret_cast<const void*>(f));
}
static int invoke(mp5_context *c,libxsmm_gemmfunction f,mp5_buffer a,mp5_buffer b,mp5_buffer out) {
    if (!buffers_valid(c->r,a,b,out)) return MP5_INVALID;
    libxsmm_gemm_param p{};
    const bool col=c->d.endpoint==MP5_COL_FP32_ACCUMULATE;
    p.a.primary=col?a.data:b.data;p.b.primary=col?b.data:a.data;p.c.primary=out.data;
    mp5_prepared_preserved_call(f,&p);
    ++c->observation.invocations;return MP5_OK;
}
extern "C" int mp5_run(mp5_context *c,mp5_buffer a,mp5_buffer b,mp5_buffer out) {
    if (!on_owner(c) || c->prepared || !buffers_valid(c->r,a,b,out)) return MP5_INVALID;
    libxsmm_gemmfunction f=nullptr;int status=generate(c,&f);
    if (status==MP5_OK) status=invoke(c,f,a,b,out);
    release(f);
    libxsmm_registry_info info{};
    if (libxsmm_get_registry_info(&info) || info.size!=0) return MP5_UNSUPPORTED;
    return status;
}
extern "C" int mp5_prepared_prepare(mp5_context *c) {
    if (!on_owner(c)) return MP5_INVALID;
    libxsmm_gemmfunction f=nullptr;int s=generate(c,&f);
    if (s!=MP5_OK) { if (f!=c->prepared) release(f);return s; }
    if (c->prepared && f!=c->prepared) {release(f);return MP5_UNSUPPORTED;}
    c->prepared=f;return MP5_OK;
}
extern "C" int mp5_prepared_run_prepared(mp5_context *c,mp5_buffer a,mp5_buffer b,mp5_buffer out) {
    if (!on_owner(c) || !c->prepared) return MP5_INVALID;
    return invoke(c,c->prepared,a,b,out);
}
extern "C" int mp5_prepared_jit_observation(const mp5_context *c,mp5_prepared_jit_info *o) {
    if (!on_owner(c) || !o) return MP5_INVALID;
    *o=c->observation;return MP5_OK;
}
extern "C" int mp5_prepared_copy_jit_code(const mp5_context *c,void *dest,uint64_t cap,uint64_t *n) {
    if (!on_owner(c) || !n || c->snapshot.empty()) return MP5_INVALID;
    *n=c->snapshot.size();if (!dest) return MP5_OK;
    if (cap<*n) return MP5_INVALID;memcpy(dest,c->snapshot.data(),*n);return MP5_OK;
}
extern "C" void mp5_destroy(mp5_context *c) {
    if (!on_owner(c)) return;
    release(c->prepared);libxsmm_finalize();live_context=nullptr;delete c;
}
extern "C" const char *mp5_native_symbol(const mp5_context *c) {
    return on_owner(c)?"libxsmm_dispatch_gemm:actual generated instance saved separately":"INVALID";
}
extern "C" const void *mp5_native_address(const mp5_context *c) {
    return on_owner(c)?reinterpret_cast<const void*>(c->prepared):nullptr;
}
extern "C" int mp5_thread_observation(const mp5_context *c) { return on_owner(c)?1:0; }
