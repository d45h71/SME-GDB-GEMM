#ifndef MP5_PREPARED_EXTENSIONS_H
#define MP5_PREPARED_EXTENSIONS_H
#include "adapter_abi.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Separate identities; never reinterpret FP16 or signed unquantized INT8. */
#define MP5_PREPARED_ROW_BF16_FP32_AB 100u
#define MP5_PREPARED_ROW_NATIVE_INT8 101u
/* This explicitly prepared regime is separate from mp5_run ordinary calls.
 * prepare does not consume A/B data and retains generated code only.
 * Inspect/copy code before destroy; exported copies outlive executable memory. */
int mp5_prepared_prepare(mp5_context*);
int mp5_prepared_run_prepared(mp5_context*,mp5_buffer,mp5_buffer,mp5_buffer);
typedef struct {
    uint64_t generation_requests, cache_lookups, invocations, code_bytes;
    int32_t target_arch_id;
    uint32_t generated, reserved;
} mp5_prepared_jit_info;
int mp5_prepared_jit_observation(const mp5_context*,mp5_prepared_jit_info*);
/* A caller-owned destination receives a snapshot; no executable pointer is
 * returned as a portable identity. NULL destination queries the required size. */
int mp5_prepared_copy_jit_code(const mp5_context*,void*,uint64_t,uint64_t*);
#ifdef __cplusplus
}
#endif
#endif
