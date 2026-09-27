#ifndef MP5_ADAPTER_ABI_H
#define MP5_ADAPTER_ABI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MP5_ADAPTER_ABI_VERSION 1u
typedef struct mp5_context mp5_context;
typedef enum {
    MP5_ROW_FP32_AB=0, MP5_COL_FP32_ACCUMULATE=1, MP5_ROW_FP64_AB=2,
    MP5_ROW_FP16_FP32_AB=3, MP5_ROW_INT8_INT32_AB=4,
    MP5_ROW_FP32_ACCUMULATE=5, MP5_COL_FP32_AB=6,
    MP5_NATIVE_ROW_FP32_AB=7, MP5_NATIVE_COL_FP32_ACCUMULATE=8,
    MP5_NATIVE_ROW_FP64_AB=9
} mp5_endpoint;
typedef enum { MP5_SERIAL=0, MP5_PARALLEL_LOCAL=1 } mp5_thread_policy;
typedef enum { MP5_OK=0, MP5_INVALID=1, MP5_UNSUPPORTED=2,
               MP5_ALLOCATION_FAILED=3, MP5_THREAD_POLICY_FAILED=4 } mp5_status;
typedef struct {
    uint32_t struct_bytes, abi_version;
    uint32_t endpoint, thread_policy;
    uint64_t m,n,k,lda,ldb,ldc;
} mp5_descriptor;
typedef struct { void *data; uint64_t bytes; } mp5_buffer;
typedef struct {
    /* Scratch is an exact conservative bound for stock; a prospective allowance
     * for opaque BLAS internals, not an enforced or measured external maximum. */
    uint64_t a_bytes,b_bytes,c_bytes,max_backend_scratch_bytes;
    uint32_t alignment_bytes, input_element_bytes, output_element_bytes;
    uint32_t requested_workers; /* 0: library managed; 4: stock maximum tasks. */
} mp5_requirements;
/* Dense, nontransposed, disjoint, 128-byte aligned arrays only. Positive dimensions.
 * Alpha is exactly one. Beta is one only for *_ACCUMULATE; caller restores immutable
 * C0 outside every individually timed native beta=1 call. No retained packing/JIT.
 * create/destroy and input generation/reference are outside ordinary-call timing.
 * run includes buffer checks, thread-policy query, backend allocation/free/packing/
 * conversion and GEMM, plus ABI repair where applicable.
 * Context is thread-affine: create and every run occur on the same worker thread.
 * Normal nonstreaming caller with no live ZA only. Never call raw SME exports.
 * All returned strings are immutable static storage owned by this library. */
const char *mp5_backend_name(void);
const char *mp5_endpoint_name(uint32_t endpoint);
int mp5_create(const mp5_descriptor*,mp5_context**,mp5_requirements*);
int mp5_run(mp5_context*,mp5_buffer a,mp5_buffer b,mp5_buffer c);
void mp5_destroy(mp5_context*);
const char *mp5_native_symbol(const mp5_context*);
const void *mp5_native_address(const mp5_context*);
int mp5_thread_observation(const mp5_context*);
#ifdef __cplusplus
}
#endif
#endif
