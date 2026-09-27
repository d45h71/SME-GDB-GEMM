// Deliberately faulty scalar adapter for explicit harness qualification only.
// This library is never an experiment backend and must never be timed.
#include "adapter_api.hpp"
#include <limits>
#include <new>

struct mp5_context {
    mp5_descriptor descriptor;
    std::string fault;
};

extern "C" const char* mp5_backend_name() { return "harness-test"; }
extern "C" const char* mp5_endpoint_name(uint32_t endpoint) {
    return endpoint == 0 ? "ROW_FP32_AB" : endpoint == 1 ? "COL_FP32_ACCUMULATE" :
           endpoint == 2 ? "ROW_FP64_AB" : endpoint == 3 ? "ROW_FP16_FP32_AB" :
           endpoint == 4 ? "ROW_INT8_INT32_AB" : endpoint == 100 ? "ROW_BF16_FP32_AB" : "UNSUPPORTED";
}

extern "C" int mp5_create(const mp5_descriptor* d, mp5_context** out, mp5_requirements* requirements) {
    if (out) *out = nullptr;
    if (!d || !out || !requirements || d->struct_bytes != sizeof(*d) || d->abi_version != MP5_ADAPTER_ABI_VERSION)
        return MP5_INVALID;
    if ((d->endpoint > 4 && d->endpoint != 100) || d->thread_policy != MP5_SERIAL) return MP5_UNSUPPORTED;
    // A strict bounded test geometry; the worker separately enforces membership.
    if (d->m != 1 || d->n != 256 || d->k != 1152) return MP5_UNSUPPORTED;
    if (d->lda != (column(d->endpoint) ? d->m : d->k) ||
        d->ldb != (column(d->endpoint) ? d->k : d->n) ||
        d->ldc != (column(d->endpoint) ? d->m : d->n)) return MP5_INVALID;
    const char* fault = std::getenv("MP5_TEST_FAULT");
    auto* context = new (std::nothrow) mp5_context{*d, fault ? fault : ""};
    if (!context) return MP5_ALLOCATION_FAILED;
    const unsigned input = input_size(d->endpoint), output = output_size(d->endpoint);
    *requirements = {d->m*d->k*input, d->k*d->n*input, d->m*d->n*output, 0, 128, input, output, 1};
    if (context->fault == "workspace") ++requirements->a_bytes;
    *out = context;
    return MP5_OK;
}

extern "C" int mp5_run(mp5_context* context, mp5_buffer a, mp5_buffer b, mp5_buffer out) {
    if (!context || !a.data || !b.data || !out.data) return MP5_INVALID;
    const auto& d = context->descriptor;
    const auto& fault = context->fault;
    if (a.bytes != d.m*d.k*input_size(d.endpoint) || b.bytes != d.k*d.n*input_size(d.endpoint) || out.bytes != d.m*d.n*output_size(d.endpoint)) return MP5_INVALID;
    for (uint64_t i=0; i<d.m; ++i) {
        for (uint64_t j=0; j<d.n; ++j) {
            double value = (beta1(d.endpoint) ^ (fault == "beta")) ? output_value(d.endpoint, out.data, cindex(d,i,j)) : 0;
            for (uint64_t depth=0; depth<d.k; ++depth) {
                uint64_t b_index = fault == "layout" ? (column(d.endpoint) ? depth*d.n+j : j*d.k+depth) : bindex(d,depth,j);
                value += input_value(d.endpoint,a.data,aindex(d,i,depth)) * input_value(d.endpoint,b.data,b_index);
            }
            set_output(d.endpoint,out.data,cindex(d,i,j),value);
        }
    }
    if (fault == "entry" || fault == "tail") {
        const uint64_t index = fault == "entry" ? 0 : d.m*d.n-1;
        set_output(d.endpoint,out.data,index,output_value(d.endpoint,out.data,index)+100);
    }
    if (fault == "nonfinite") set_output(d.endpoint,out.data,0,std::numeric_limits<double>::quiet_NaN());
    if (fault == "input") static_cast<unsigned char*>(a.data)[0] ^= 1;
    if (fault == "canary") static_cast<unsigned char*>(out.data)[out.bytes] ^= 1;
    return MP5_OK;
}

extern "C" void mp5_destroy(mp5_context* context) { delete context; }
extern "C" const char* mp5_native_symbol(const mp5_context*) { return "explicit_harness_test_scalar"; }
extern "C" const void* mp5_native_address(const mp5_context*) { return reinterpret_cast<const void*>(mp5_run); }
extern "C" int mp5_thread_observation(const mp5_context*) { return 1; }
