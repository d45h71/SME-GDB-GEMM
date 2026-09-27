#include "adapter_api.hpp"
#include <cmath>
#include <limits>
uint16_t to_bfloat(float x){uint32_t u;std::memcpy(&u,&x,4);if((u&0x7f800000u)==0x7f800000u)return uint16_t((u>>16)|((u&0x7fffffu)?0x40:0));return uint16_t((u+0x7fff+((u>>16)&1))>>16);}
float from_bfloat(uint16_t h){uint32_t u=uint32_t(h)<<16;float v;std::memcpy(&v,&u,4);return v;}
uint16_t to_half(float x){
 uint32_t u;std::memcpy(&u,&x,4);uint32_t sign=(u>>16)&0x8000,exp=(u>>23)&255,m=u&0x7fffff;
 if(exp==255)return uint16_t(sign|0x7c00|(m?0x200:0));
 int e=int(exp)-127+15;if(e>=31)return uint16_t(sign|0x7c00);
 if(e<=0){if(e<-10)return uint16_t(sign);m|=0x800000;unsigned shift=unsigned(14-e);uint32_t q=m>>shift,r=m&((1u<<shift)-1),half=1u<<(shift-1);return uint16_t(sign+q+(r>half||(r==half&&(q&1))));}
 uint32_t q=m>>13,r=m&8191;return uint16_t(sign+(uint32_t(e)<<10)+q+(r>4096||(r==4096&&(q&1))));
}
float from_half(uint16_t h){unsigned e=(h>>10)&31,m=h&1023;float v=e==31?(m?std::numeric_limits<float>::quiet_NaN():std::numeric_limits<float>::infinity()):e?std::ldexp(float(1024+m),int(e)-25):std::ldexp(float(m),-24);return h&0x8000?-v:v;}
static uint64_t next(uint64_t& x){x+=0x9e3779b97f4a7c15ull;uint64_t z=x;z=(z^(z>>30))*0xbf58476d1ce4e5b9ull;z=(z^(z>>27))*0x94d049bb133111ebull;return z^(z>>31);}
static void put(uint32_t e,void* p,uint64_t i,double v){if(e==2||e==9)static_cast<double*>(p)[i]=v;else if(e==3)static_cast<uint16_t*>(p)[i]=to_half(float(v));else if(e==100)static_cast<uint16_t*>(p)[i]=to_bfloat(float(v));else if(e==4||e==101)static_cast<int8_t*>(p)[i]=int8_t(v);else static_cast<float*>(p)[i]=float(v);}
double input_value(uint32_t e,const void*p,uint64_t i){if(e==2||e==9)return static_cast<const double*>(p)[i];if(e==3)return from_half(static_cast<const uint16_t*>(p)[i]);if(e==100)return from_bfloat(static_cast<const uint16_t*>(p)[i]);if(e==4||e==101)return static_cast<const int8_t*>(p)[i];return static_cast<const float*>(p)[i];}
double output_value(uint32_t e,const void*p,uint64_t i){return e==2||e==9?static_cast<const double*>(p)[i]:e==4?static_cast<const int32_t*>(p)[i]:e==101?static_cast<const int8_t*>(p)[i]:static_cast<const float*>(p)[i];}
void set_output(uint32_t e,void*p,uint64_t i,double v){if(e==2||e==9)static_cast<double*>(p)[i]=v;else if(e==4)static_cast<int32_t*>(p)[i]=int32_t(v);else if(e==101)static_cast<int8_t*>(p)[i]=int8_t(v);else static_cast<float*>(p)[i]=float(v);}
void make_inputs(const mp5_descriptor&d,uint64_t seed,uint32_t pattern,void*a,void*b,void*c0){
 if(pattern>5)throw std::runtime_error("unknown input pattern");bool integer=d.endpoint==4||d.endpoint==101;
 auto value=[&](unsigned matrix,uint64_t i,uint64_t j){int z=int(next(seed)%255)-127;if(pattern==0)return 0.;if(pattern==1)return 1.;if(pattern==2)return i==j?1.:0.;if(pattern==3)return (i+j+matrix)&1?-1.:1.;if(pattern==5)return j==0&&matrix==0?1.:matrix==1?(i==0?double(int(j%7)-3):0.):0.;return integer?double(z):double(z)/128.;};
 for(uint64_t i=0;i<d.m;i++)for(uint64_t t=0;t<d.k;t++)put(d.endpoint,a,aindex(d,i,t),value(0,i,t));
 for(uint64_t t=0;t<d.k;t++)for(uint64_t j=0;j<d.n;j++)put(d.endpoint,b,bindex(d,t,j),value(1,t,j));
 for(uint64_t i=0;i<d.m;i++)for(uint64_t j=0;j<d.n;j++)set_output(d.endpoint,c0,cindex(d,i,j),integer?double(int((i+2*j)%7)-3):double(int((i+2*j)%7)-3)/8.);
}

extern "C" uint16_t matrix_to_half(float v){return to_half(v);}
extern "C" uint16_t matrix_to_bfloat(float v){return to_bfloat(v);}
extern "C" float matrix_from_half(uint16_t v){return from_half(v);}
extern "C" float matrix_from_bfloat(uint16_t v){return from_bfloat(v);}
extern "C" int matrix_allocation_check(uint64_t a,uint64_t b){try{checked_mul(a,b);checked_add(a,b);return 0;}catch(...){return 1;}}
