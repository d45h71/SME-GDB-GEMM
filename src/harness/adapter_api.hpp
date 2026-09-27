#pragma once
#include "adapter_abi.h"
#include "prepared_api.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
constexpr uint64_t MATRIX_CAP=12ull*1024*1024*1024;
inline uint64_t checked_add(uint64_t a,uint64_t b){if(b>MATRIX_CAP||a>MATRIX_CAP-b)throw std::runtime_error("allocation sum exceeds static cap");return a+b;}
inline uint64_t checked_mul(uint64_t a,uint64_t b){if(b&&a>MATRIX_CAP/b)throw std::runtime_error("allocation product exceeds static cap");return a*b;}
inline bool column(uint32_t e){return e==1||e==6||e==8;}
inline bool beta1(uint32_t e){return e==1||e==5||e==8;}
inline unsigned input_size(uint32_t e){if(e>9&&e!=100&&e!=101)throw std::runtime_error("unknown endpoint");return e==2||e==9?8:e==3||e==100?2:e==4||e==101?1:4;}
inline unsigned output_size(uint32_t e){return e==2||e==9?8:e==101?1:4;}
inline uint64_t aindex(const mp5_descriptor& d,uint64_t i,uint64_t t){return column(d.endpoint)?i+t*d.m:i*d.k+t;}
inline uint64_t bindex(const mp5_descriptor& d,uint64_t t,uint64_t j){return column(d.endpoint)?t+j*d.k:t*d.n+j;}
inline uint64_t cindex(const mp5_descriptor& d,uint64_t i,uint64_t j){return column(d.endpoint)?i+j*d.m:i*d.n+j;}
struct GuardReceipt {uint64_t entry_svcr,exit_svcr,entry_fpcr,exit_fpcr,sp_entry,sp_exit,gpr_mask,simd_mask,flags,return_reached,gprs[12],simd[16];};
static_assert(sizeof(GuardReceipt)==304&&offsetof(GuardReceipt,simd)==176);
struct Invocation {void* fn;uint64_t args[8];uint64_t outgoing;uint64_t timed,ticks_start,ticks_end;int64_t result;GuardReceipt* receipt;int64_t failure_fd;uint64_t full_simd;};
static_assert(offsetof(Invocation,receipt)==112&&offsetof(Invocation,timed)==80&&offsetof(Invocation,full_simd)==128);
extern "C" uint64_t matrix_guard_call(Invocation*);
extern "C" void* matrix_clock_pointer;
struct Buffer {
 void* allocation=nullptr;unsigned char* data=nullptr;uint64_t bytes=0;
 explicit Buffer(uint64_t n):bytes(n){auto total=checked_add(n,256);if(posix_memalign(&allocation,128,total))throw std::runtime_error("allocation failure");data=static_cast<unsigned char*>(allocation)+128;std::memset(allocation,0xa5,total);}
 Buffer(const Buffer&)=delete;Buffer& operator=(const Buffer&)=delete;
 ~Buffer(){std::free(allocation);}
 bool guards()const{auto p=static_cast<unsigned char*>(allocation);for(unsigned i=0;i<128;i++)if(p[i]!=0xa5||data[bytes+i]!=0xa5)return false;return true;}
 mp5_buffer view(){return {data,bytes};}
};
uint16_t to_half(float);uint16_t to_bfloat(float);float from_half(uint16_t);float from_bfloat(uint16_t);
void make_inputs(const mp5_descriptor&,uint64_t seed,uint32_t pattern,void* a,void* b,void* c0);
double input_value(uint32_t,const void*,uint64_t);double output_value(uint32_t,const void*,uint64_t);void set_output(uint32_t,void*,uint64_t,double);
extern "C" {
struct matrix_ref_value {double expected,abs_sum;uint64_t terms;uint32_t flags,reserved;};
struct matrix_projection_value {double expected,observed,abs_sum,projection_error;uint64_t terms;uint32_t flags,reserved;};
int matrix_reference_dot(uint32_t,uint64_t,uint64_t,uint64_t,const void*,const void*,const void*,uint64_t,uint64_t,matrix_ref_value*);
int matrix_reference_full(uint32_t,uint64_t,uint64_t,uint64_t,const void*,const void*,const void*,matrix_ref_value*,uint64_t,uint64_t);
int matrix_reference_projection(uint32_t,uint64_t,uint64_t,uint64_t,const void*,const void*,const void*,const void*,const int8_t*,matrix_projection_value*,uint64_t);
}

static_assert(offsetof(Invocation,args)==8&&offsetof(Invocation,outgoing)==72&&offsetof(Invocation,ticks_start)==88&&offsetof(Invocation,ticks_end)==96&&offsetof(Invocation,result)==104&&offsetof(Invocation,failure_fd)==120);
static_assert(offsetof(GuardReceipt,sp_entry)==32&&offsetof(GuardReceipt,sp_exit)==40&&offsetof(GuardReceipt,gpr_mask)==48&&offsetof(GuardReceipt,simd_mask)==56&&offsetof(GuardReceipt,flags)==64&&offsetof(GuardReceipt,return_reached)==72&&offsetof(GuardReceipt,gprs)==80);
