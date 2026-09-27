#include "adapter_abi.h"
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <pthread.h>

#ifndef MP5_ABLATION_BACKEND_NAME
#error MP5_ABLATION_BACKEND_NAME is required
#endif

extern "C" int mp5_ablation_col_sgemm(int, int, int, float*, float*, float*);

struct mp5_context {
    mp5_descriptor descriptor;
    mp5_requirements requirements;
    pthread_t owner;
};

extern "C" const char* mp5_backend_name() { return MP5_ABLATION_BACKEND_NAME; }
extern "C" const char* mp5_endpoint_name(uint32_t endpoint) {
    return endpoint == 1 ? "COL_FP32_ACCUMULATE" : "UNSUPPORTED";
}

extern "C" int mp5_create(const mp5_descriptor* d, mp5_context** out,
                            mp5_requirements* requirements) {
    if (out) *out = nullptr;
    if (!d || !out || !requirements || d->struct_bytes != sizeof(*d) ||
        d->abi_version != MP5_ADAPTER_ABI_VERSION || d->endpoint != 1 ||
        d->thread_policy != 0 || !d->m || !d->n || !d->k ||
        d->m > INT_MAX - 512 || d->n > INT_MAX - 512 || d->k > INT_MAX - 2048) {
        return MP5_INVALID;
    }
    if (d->m > UINT64_MAX / d->k || d->n > UINT64_MAX / d->k ||
        d->m > UINT64_MAX / d->n || d->m * d->k > INT_MAX ||
        d->n * d->k > INT_MAX || d->m * d->n > INT_MAX ||
        d->lda != d->m || d->ldb != d->k || d->ldc != d->m) {
        return MP5_INVALID;
    }
    *requirements = {d->m * d->k * 4, d->k * d->n * 4,
                     d->m * d->n * 4, 6291456, 128, 4, 4, 1};
    auto* context = static_cast<mp5_context*>(std::malloc(sizeof(mp5_context)));
    if (!context) return MP5_ALLOCATION_FAILED;
    *context = {*d, *requirements, pthread_self()};
    *out = context;
    return MP5_OK;
}

static bool overlap(mp5_buffer a, mp5_buffer b) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a.data);
    const uintptr_t y = reinterpret_cast<uintptr_t>(b.data);
    if (a.bytes > UINTPTR_MAX - x || b.bytes > UINTPTR_MAX - y) return true;
    return x < y + b.bytes && y < x + a.bytes;
}

extern "C" int mp5_run(mp5_context* c, mp5_buffer a, mp5_buffer b,
                        mp5_buffer output) {
    if (!c || !pthread_equal(c->owner, pthread_self()) || !a.data || !b.data ||
        !output.data || a.bytes < c->requirements.a_bytes ||
        b.bytes < c->requirements.b_bytes ||
        output.bytes < c->requirements.c_bytes ||
        ((reinterpret_cast<uintptr_t>(a.data) |
          reinterpret_cast<uintptr_t>(b.data) |
          reinterpret_cast<uintptr_t>(output.data)) & 127) ||
        overlap(a, b) || overlap(a, output) || overlap(b, output)) {
        return MP5_INVALID;
    }
    const auto& d = c->descriptor;
    return mp5_ablation_col_sgemm(static_cast<int>(d.m), static_cast<int>(d.n),
                         static_cast<int>(d.k), static_cast<float*>(a.data),
                         static_cast<float*>(b.data),
                         static_cast<float*>(output.data));
}

extern "C" void mp5_destroy(mp5_context* c) { std::free(c); }
extern "C" const char* mp5_native_symbol(const mp5_context*) {
    return "mp5_ablation_col_sgemm_LOCAL_RECONSTRUCTION";
}
extern "C" const void* mp5_native_address(const mp5_context*) {
    return reinterpret_cast<const void*>(mp5_ablation_col_sgemm);
}
extern "C" int mp5_thread_observation(const mp5_context* c) {
    return c ? 1 : -1;
}
