#include "common.h"
#include <cmath>
#include <new>
#include <sys/sysctl.h>
#include "kai/kai_common.h"
#include "kai/ukernels/matmul/matmul_clamp_f32_f32p_f32p/kai_matmul_clamp_f32_f32p2vlx1_f32p2vlx1biasf32_sme2_mopa.h"
#include "kai/ukernels/matmul/matmul_clamp_fp32_bf16p_bf16p/kai_matmul_clamp_f32_bf16p2vlx2_bf16p2vlx2_2vlx2vl_sme2_mopa.h"
#include "kai/ukernels/matmul/matmul_clamp_qai8_qai8p_qsi8cxp/kai_matmul_clamp_qai8_qai8p2vlx4_qsi8cxpsb2vlx4_2vlx2vl_sme2_mopa.h"
#include "kai/ukernels/matmul/pack/kai_lhs_pack_f32p2vlx1_f32_sme.h"
#include "kai/ukernels/matmul/pack/kai_rhs_pack_kxn_f32p2vlx1biasf32_f32_f32_sme.h"
#include "kai/ukernels/matmul/pack/kai_lhs_pack_bf16p2vlx2_f32_sme.h"
#include "kai/ukernels/matmul/pack/kai_rhs_pack_kxn_bf16p2vlx2b_f32_x32_sme.h"
#include "kai/ukernels/matmul/pack/kai_lhs_pack_x8p2vlx4_x8_sme.h"
#include "kai/ukernels/matmul/pack/kai_rhs_pack_kxn_qsi8cxp2vlx4sb_qs8cx_f32_i32_sme.h"
/* This declaration allows a nosve caller to use the source-bound assembly
 * rdsvl helper. No SVE instruction is emitted by adapter auto-vectorization. */
extern "C" uint64_t kai_get_sme_vector_length_u8(void);
#define FP(fn) kai_##fn##_matmul_clamp_f32_f32p2vlx1_f32p2vlx1biasf32_sme2_mopa
#define BF(fn) kai_##fn##_matmul_clamp_f32_bf16p2vlx2_bf16p2vlx2_2vlx2vl_sme2_mopa
#define IQ(fn) kai_##fn##_matmul_clamp_qai8_qai8p2vlx4_qsi8cxpsb2vlx4_2vlx2vl_sme2_mopa
struct mp5_context {
    mp5_descriptor d{};mp5_requirements r{};pthread_t owner{};
    uint64_t q=0,kr=0,lhs=0,rhs=0,bias=0,scale=0,expand_a=0,expand_b=0;
};
static bool feature(const char *name) { int v=0;size_t n=sizeof(v);return !sysctlbyname(name,&v,&n,nullptr,0)&&n==sizeof(v)&&v==1; }
static bool owner(const mp5_context *c) {return c && pthread_equal(c->owner,pthread_self());}
static bool workspace(mp5_context *c) {
    const auto &d=c->d;uint64_t mp,np,kp,t,ep;
    c->kr=d.endpoint==MP5_ROW_FP32_AB?1:d.endpoint==MP5_PREPARED_ROW_BF16_FP32_AB?2:4;
    c->q=2*(kai_get_sme_vector_length_u8()/4);
    if (!c->q || c->q>128 || !round_u64(d.m,c->q,&mp) || !round_u64(d.n,c->q,&np) ||
        !round_u64(d.k,c->kr,&kp)) return false;
    const uint64_t elem=d.endpoint==MP5_ROW_FP32_AB?4:d.endpoint==MP5_PREPARED_ROW_BF16_FP32_AB?2:1;
    const uint64_t trailer=d.endpoint==MP5_PREPARED_ROW_NATIVE_INT8?8:4;
    if (!mul_u64(mp,kp,&t) || !mul_u64(t,elem,&c->lhs) || !mul_u64(kp,elem,&t) ||
        !add_u64(t,trailer,&t) || !mul_u64(np,t,&c->rhs) || !mul_u64(d.n,4,&c->bias)) return false;
    if (d.endpoint==MP5_PREPARED_ROW_NATIVE_INT8) c->scale=c->bias;
    if (d.endpoint==MP5_PREPARED_ROW_BF16_FP32_AB) {
        if (!mul_u64(d.m,d.k,&t) || !mul_u64(t,4,&c->expand_a) ||
            !mul_u64(d.k,d.n,&t) || !mul_u64(t,4,&c->expand_b)) return false;
    }
    /* Sum before calling upstream unchecked size helpers. 64 KiB source stack
     * allowance plus 6*127 alignment slack; allocated payloads are exact. */
    ep=65536+6*127;
    for (auto x:{c->lhs,c->rhs,c->bias,c->scale,c->expand_a,c->expand_b}) if(!add_u64(ep,x,&ep)) return false;
    c->r.max_backend_scratch_bytes=ep;
    for(auto x:{c->r.a_bytes,c->r.b_bytes,c->r.c_bytes}) if(!add_u64(ep,x,&ep)) return false;
    if(ep>12ull*1024*1024*1024) return false;
    uint64_t l=0,r=0,mr=0,nr=0,kr=0,sr=0;
    if(d.endpoint==MP5_ROW_FP32_AB) {
        mr=FP(get_mr)();nr=FP(get_nr)();kr=FP(get_kr)();sr=FP(get_sr)();
        l=kai_get_lhs_packed_size_lhs_pack_f32p2vlx1_f32_sme(d.m,d.k,c->q,1,1);
        r=kai_get_rhs_packed_size_rhs_pack_kxn_f32p2vlx1biasf32_f32_f32_sme(d.n,d.k);
    } else if(d.endpoint==MP5_PREPARED_ROW_BF16_FP32_AB) {
        mr=BF(get_mr)();nr=BF(get_nr)();kr=BF(get_kr)();sr=BF(get_sr)();
        l=kai_get_lhs_packed_size_lhs_pack_bf16p2vlx2_f32_sme(d.m,d.k,c->q,2,1);
        r=kai_get_rhs_packed_size_rhs_pack_kxn_bf16p2vlx2b_f32_x32_sme(d.n,d.k);
    } else {
        mr=IQ(get_mr)();nr=IQ(get_nr)();kr=IQ(get_kr)();sr=IQ(get_sr)();
        l=kai_get_lhs_packed_size_lhs_pack_x8p2vlx4_x8_sme(d.m,d.k,c->q,4,1);
        r=kai_get_rhs_packed_size_rhs_pack_kxn_qsi8cxp2vlx4sb_qs8cx_f32_i32_sme(d.n,d.k);
    }
    return l==c->lhs && r==c->rhs && mr==c->q && nr==c->q && kr==c->kr && sr==1;
}
extern "C" const char *mp5_backend_name(void) {return "kleidiai-sme2-mopa";}
extern "C" const char *mp5_endpoint_name(uint32_t e) {
    return e==0?"ROW_FP32_AB":e==100?"ROW_BF16_FP32_AB":e==101?"ROW_SIGNED_INT8_REQUANTIZED_INT8":"UNSUPPORTED";
}
extern "C" int mp5_create(const mp5_descriptor *d,mp5_context **out,mp5_requirements *r) {
    if(out)*out=nullptr;
    if(!d||!out||!r)return MP5_INVALID;
    if(d->endpoint!=0 && d->endpoint!=100 && d->endpoint!=101)return MP5_UNSUPPORTED;
    if(d->thread_policy!=MP5_SERIAL)return MP5_UNSUPPORTED;
    if(!basic_descriptor(d,false,d->endpoint==0?4:d->endpoint==100?2:1,r))return MP5_INVALID;
    if(d->endpoint==101) {r->output_element_bytes=1;r->c_bytes=d->m*d->n;if(d->k>25600)return MP5_INVALID;}
    if(!feature("hw.optional.arm.FEAT_SME")||!feature("hw.optional.arm.FEAT_SME2"))return MP5_UNSUPPORTED;
    auto *c=new(std::nothrow) mp5_context;
    if(!c)return MP5_ALLOCATION_FAILED;
    c->d=*d;c->r=*r;c->owner=pthread_self();
    if(!workspace(c)){delete c;return MP5_INVALID;}
    *out=c;*r=c->r;return MP5_OK;
}
struct Block {void *p;explicit Block(uint64_t n):p(n?allocate_aligned(n):nullptr){}~Block(){free(p);} };
static void expand_bf16(const void *src,void *dst,uint64_t count) {
    const auto *s=static_cast<const uint16_t*>(src);auto *d=static_cast<float*>(dst);
    for(uint64_t i=0;i<count;++i){uint32_t bits=uint32_t(s[i])<<16;memcpy(d+i,&bits,4);}
}
extern "C" int mp5_run(mp5_context *c,mp5_buffer a,mp5_buffer b,mp5_buffer out) {
    if(!owner(c)||!buffers_valid(c->r,a,b,out))return MP5_INVALID;
    if(2*(kai_get_sme_vector_length_u8()/4)!=c->q)return MP5_UNSUPPORTED;
    Block l(c->lhs),r(c->rhs),bias(c->bias),scale(c->scale),ea(c->expand_a),eb(c->expand_b);
    if(!l.p||!r.p||!bias.p||(c->scale&&!scale.p)||(c->expand_a&&!ea.p)||(c->expand_b&&!eb.p))return MP5_ALLOCATION_FAILED;
    memset(bias.p,0,c->bias);const auto &d=c->d;
    if(d.endpoint==0) {
        kai_run_lhs_pack_f32p2vlx1_f32_sme(d.m,d.k,c->q,1,1,0,a.data,4*d.k,l.p);
        kai_run_rhs_pack_kxn_f32p2vlx1biasf32_f32_f32_sme(1,d.n,d.k,c->q,1,1,4*d.n,b.data,bias.p,nullptr,r.p,0,nullptr);
        FP(run)(d.m,d.n,d.k,l.p,r.p,out.data,4*d.n,4,-INFINITY,INFINITY);
    } else if(d.endpoint==100) {
        expand_bf16(a.data,ea.p,d.m*d.k);expand_bf16(b.data,eb.p,d.k*d.n);
        kai_run_lhs_pack_bf16p2vlx2_f32_sme(d.m,d.k,c->q,2,1,0,ea.p,4*d.k,l.p);
        kai_run_rhs_pack_kxn_bf16p2vlx2b_f32_x32_sme(1,d.n,d.k,c->q,2,1,4*d.n,eb.p,bias.p,nullptr,r.p,0,nullptr);
        BF(run)(d.m,d.n,d.k,l.p,r.p,out.data,4*d.n,4,-INFINITY,INFINITY);
    } else {
        for(uint64_t j=0;j<d.n;++j)static_cast<float*>(scale.p)[j]=1.0f;
        kai_rhs_pack_qsi8cx_params pp{0,1.0f};kai_matmul_requantize32_params rp{-128,127,0};
        kai_run_lhs_pack_x8p2vlx4_x8_sme(d.m,d.k,c->q,4,1,0,a.data,d.k,l.p);
        kai_run_rhs_pack_kxn_qsi8cxp2vlx4sb_qs8cx_f32_i32_sme(1,d.n,d.k,c->q,4,1,d.n,b.data,bias.p,scale.p,r.p,0,&pp);
        IQ(run)(d.m,d.n,d.k,l.p,r.p,out.data,d.n,1,&rp);
    }
    return MP5_OK;
}
extern "C" void mp5_destroy(mp5_context *c){if(owner(c))delete c;}
extern "C" const char *mp5_native_symbol(const mp5_context *c){
    if(!owner(c))return "INVALID";
    return c->d.endpoint==0?"kai_run_matmul_clamp_f32_f32p2vlx1_f32p2vlx1biasf32_sme2_mopa":
        c->d.endpoint==100?"kai_run_matmul_clamp_f32_bf16p2vlx2_bf16p2vlx2_2vlx2vl_sme2_mopa":
        "kai_run_matmul_clamp_qai8_qai8p2vlx4_qsi8cxpsb2vlx4_2vlx2vl_sme2_mopa";
}
extern "C" const void *mp5_native_address(const mp5_context *c){
    if(!owner(c))return nullptr;
    return c->d.endpoint==0?reinterpret_cast<const void*>(&FP(run)):
        c->d.endpoint==100?reinterpret_cast<const void*>(&BF(run)):reinterpret_cast<const void*>(&IQ(run));
}
extern "C" int mp5_thread_observation(const mp5_context *c){return owner(c)?1:0;}
extern "C" int mp5_prepared_prepare(mp5_context*){return MP5_UNSUPPORTED;}
extern "C" int mp5_prepared_run_prepared(mp5_context*,mp5_buffer,mp5_buffer,mp5_buffer){return MP5_UNSUPPORTED;}
extern "C" int mp5_prepared_jit_observation(const mp5_context*,mp5_prepared_jit_info*){return MP5_UNSUPPORTED;}
extern "C" int mp5_prepared_copy_jit_code(const mp5_context*,void*,uint64_t,uint64_t*){return MP5_UNSUPPORTED;}
