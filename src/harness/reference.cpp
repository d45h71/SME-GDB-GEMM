// Source-bound independent numerical oracle.  It retains the exact
// numerical algebra and adds only a cooperative parent-controlled deadline;
// no target code is linked or called. Compile without fast-math. Binary64
// arithmetic is used only for exact power-of-two decoding and final conversion,
// never for dot summation.
#include <algorithm>
#include <array>
#include <cfenv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

extern "C" {
struct matrix_ref_value {
    double expected, abs_sum;
    uint64_t terms;
    uint32_t flags, reserved;
};
struct matrix_projection_value {
    double expected, observed, abs_sum, projection_error;
    uint64_t terms;
    uint32_t flags, reserved;
};
}

namespace {
// Every finite binary64 product is an integer times 2^-2148. Its highest
// possible bit is 4195; 68 limbs leave more than 150 carry bits. The public
// bound of 25600 products (and <=INT_MAX projection terms) needs <=31.
constexpr size_t LIMBS = 68;
constexpr int QUANTUM_EXP = -2148;
constexpr uint64_t MAX_DIM = 2147483647ULL;
constexpr uint64_t MAX_K = 25600;
constexpr uint64_t MAX_REFERENCE_SCRATCH = 4ULL * 1024 * 1024;
enum : uint32_t { UNSUPPORTED=1, NONFINITE=2, ORACLE_OVERFLOW=4,
                  LIMITS=8, INVALID=16, FP_ENVIRONMENT=32, ALLOCATION=64,
                  DEADLINE_EXPIRED=128 };
struct RefError { uint32_t flag; };
using Words = std::array<uint64_t,LIMBS>;

// This deadline is intentionally cooperative, not a claim of an OS-level
// interrupt.  The controller sets it before every adjudication and the
// oracle checks it at most 64 inner-loop iterations apart.
thread_local bool matrix_budget_active=false;
thread_local std::chrono::steady_clock::time_point matrix_budget_deadline{};
inline void matrix_check_budget() {
    if(matrix_budget_active && std::chrono::steady_clock::now()>=matrix_budget_deadline)
        throw RefError{DEADLINE_EXPIRED};
}
inline void matrix_check_budget_periodic(uint64_t iteration) {
    if((iteration&63U)==0)matrix_check_budget();
}

void add_word(Words& x,size_t pos,uint64_t word) {
    while (word) {
        if (pos>=LIMBS) throw RefError{ORACLE_OVERFLOW};
        const unsigned __int128 z=static_cast<unsigned __int128>(x[pos])+word;
        x[pos]=static_cast<uint64_t>(z); word=static_cast<uint64_t>(z>>64); ++pos;
    }
}
void add_shifted(Words& x,unsigned __int128 value,unsigned shift) {
    if (!value) return;
    const size_t pos=shift/64; const unsigned bits=shift%64;
    const uint64_t lo=static_cast<uint64_t>(value), hi=static_cast<uint64_t>(value>>64);
    if (!bits) { add_word(x,pos,lo); add_word(x,pos+1,hi); }
    else {
        add_word(x,pos,lo<<bits);
        add_word(x,pos+1,(hi<<bits)|(lo>>(64-bits)));
        add_word(x,pos+2,hi>>(64-bits));
    }
}
int compare(const Words& a,const Words& b) {
    for (size_t i=LIMBS;i--;) if (a[i]!=b[i]) return a[i]>b[i]?1:-1;
    return 0;
}
Words subtract(const Words& a,const Words& b) {
    Words out{}; uint64_t borrow=0;
    for (size_t i=0;i<LIMBS;++i) {
        const unsigned __int128 rhs=static_cast<unsigned __int128>(b[i])+borrow;
        out[i]=static_cast<uint64_t>(static_cast<unsigned __int128>(a[i])-rhs);
        borrow=static_cast<unsigned __int128>(a[i])<rhs;
    }
    if (borrow) throw RefError{INVALID};
    return out;
}
int highest_bit(const Words& x) {
    for (size_t i=LIMBS;i--;) if (x[i])
        return static_cast<int>(64*i+63-__builtin_clzll(x[i]));
    return -1;
}
bool any_below(const Words& x,unsigned bit_count) {
    const size_t whole=bit_count/64; const unsigned rest=bit_count%64;
    for (size_t i=0;i<whole && i<LIMBS;++i) if(x[i])return true;
    return rest && whole<LIMBS && (x[whole]&((uint64_t{1}<<rest)-1));
}
bool bit(const Words& x,unsigned position) {
    return position/64<LIMBS && ((x[position/64]>>(position%64))&1);
}
uint64_t shifted_low(const Words& x,unsigned shift) {
    const size_t i=shift/64; const unsigned b=shift%64;
    if(i>=LIMBS)return 0;
    uint64_t q=x[i]>>b;
    if(b && i+1<LIMBS)q|=x[i+1]<<(64-b);
    return q;
}
double magnitude_to_double(const Words& x,bool upward) {
    const int high=highest_bit(x); if(high<0)return 0;
    // A normal binary64 keeps 53 bits; a subnormal uses fixed 2^-1074 units.
    const unsigned shift=static_cast<unsigned>(std::max(high-52,1074));
    uint64_t q=shifted_low(x,shift);
    if(upward) { if(any_below(x,shift))++q; }
    else if(bit(x,shift-1) && (any_below(x,shift-1)||(q&1)))++q;
    // Construct the result bits directly, so tiny exact results do not depend
    // on an arithmetic conversion that a flush-to-zero mode could alter.
    if(!q)return 0;
    const int q_high=63-__builtin_clzll(q);
    const int exponent=q_high+static_cast<int>(shift)+QUANTUM_EXP;
    uint64_t result_bits;
    if(exponent>1023)result_bits=0x7ff0000000000000ULL;
    else if(exponent>=-1022){
        const uint64_t significand=q_high<=52?q<<(52-q_high):q>>(q_high-52);
        result_bits=(static_cast<uint64_t>(exponent+1023)<<52)|
                    (significand&0x000fffffffffffffULL);
    } else {
        // Here shift is exactly 1074: q counts binary64 subnormal units.
        result_bits=q;
    }
    double result;std::memcpy(&result,&result_bits,sizeof(result));return result;
}
struct Binary { uint64_t mantissa; int exponent; bool negative; };
Binary decode_double(double value) {
    uint64_t bits; std::memcpy(&bits,&value,sizeof(bits));
    const uint64_t exp=(bits>>52)&2047, frac=bits&0x000fffffffffffffULL;
    if(exp==2047)throw RefError{NONFINITE};
    return {exp?frac|(uint64_t{1}<<52):frac,
            exp?static_cast<int>(exp)-1075:-1074,static_cast<bool>(bits>>63)};
}
class ExactAccumulator {
    Words positive{},negative{};
public:
    void product(double a,double b) {
        const Binary x=decode_double(a),y=decode_double(b);
        const unsigned __int128 v=static_cast<unsigned __int128>(x.mantissa)*y.mantissa;
        if(!v)return;
        const int shift=x.exponent+y.exponent-QUANTUM_EXP;
        if(shift<0)throw RefError{ORACLE_OVERFLOW};
        add_shifted(x.negative!=y.negative?negative:positive,v,static_cast<unsigned>(shift));
    }
    void scalar(double a) { product(a,1.0); }
    double rounded() const {
        const int order=compare(positive,negative);
        if(!order)return 0;
        const double x=magnitude_to_double(order>0?subtract(positive,negative):subtract(negative,positive),false);
        return order>0?x:-x;
    }
    double abs_upper() const {
        const int order=compare(positive,negative);
        return !order?0:magnitude_to_double(order>0?subtract(positive,negative):subtract(negative,positive),true);
    }
    double rounding_error_upper(double rounded_value) const {
        if(!std::isfinite(rounded_value))throw RefError{ORACLE_OVERFLOW};
        ExactAccumulator difference=*this; difference.scalar(-rounded_value);
        return difference.abs_upper();
    }
};
bool is_column(uint32_t e){return e==1||e==6||e==8;}
bool is_beta_one(uint32_t e){return e==1||e==5||e==8;}
bool is_integer(uint32_t e){return e==4||e==101;}
void descriptor(uint32_t endpoint,uint64_t m,uint64_t n,uint64_t k) {
    if(!(endpoint<=9||endpoint==100||endpoint==101))throw RefError{UNSUPPORTED};
    if(!m||!n||!k||m>MAX_DIM||n>MAX_DIM||k>MAX_K||m>MAX_DIM/n||m>MAX_DIM/k||k>MAX_DIM/n)
        throw RefError{LIMITS};
    static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559 &&
                  std::numeric_limits<double>::digits==53,"binary64 required");
    static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559,"binary32 required");
    if(std::fegetround()!=FE_TONEAREST)throw RefError{FP_ENVIRONMENT};
}
template<class T>T load(const void* base,uint64_t index) {
    T out; std::memcpy(&out,static_cast<const unsigned char*>(base)+index*sizeof(T),sizeof(T));return out;
}
double decode_binary32(const void* base,uint64_t index) {
    const uint32_t bits=load<uint32_t>(base,index);
    const uint32_t exponent=(bits>>23)&255, fraction=bits&0x007fffffU;
    if(exponent==255)throw RefError{NONFINITE};
    uint64_t result_bits=static_cast<uint64_t>(bits>>31)<<63;
    if(exponent){
        result_bits|=static_cast<uint64_t>(exponent+896)<<52;
        result_bits|=static_cast<uint64_t>(fraction)<<29;
    }else if(fraction){
        // Every nonzero binary32 subnormal is a normal binary64. Constructing
        // its bits avoids an ARM FPCR.FZ-sensitive float-to-double conversion.
        const unsigned high=31-__builtin_clz(fraction);
        result_bits|=static_cast<uint64_t>(static_cast<int>(high)-149+1023)<<52;
        result_bits|=(static_cast<uint64_t>(fraction)<<(52-high))&0x000fffffffffffffULL;
    }
    double result;std::memcpy(&result,&result_bits,sizeof(result));return result;
}
double input_value(uint32_t e,const void* base,uint64_t index) {
    if(e==2||e==9)return load<double>(base,index);
    if(is_integer(e))return static_cast<double>(load<int8_t>(base,index));
    if(e==3||e==100){
        const uint16_t x=load<uint16_t>(base,index); const bool negative=x>>15;
        const unsigned fraction_bits=e==3?10:7;
        const unsigned exp=(x>>fraction_bits)&(e==3?31:255);
        const unsigned frac=x&((1u<<fraction_bits)-1);
        const unsigned max_exp=e==3?31:255;
        if(exp==max_exp)throw RefError{NONFINITE};
        const int bias=e==3?15:127;
        const double v=std::ldexp(static_cast<double>(exp?frac+(1u<<fraction_bits):frac),
                       (exp?static_cast<int>(exp):1)-bias-static_cast<int>(fraction_bits));
        return negative?-v:v;
    }
    return decode_binary32(base,index);
}
double output_value(uint32_t e,const void* base,uint64_t index) {
    if(e==2||e==9)return load<double>(base,index);
    if(e==4)return static_cast<double>(load<int32_t>(base,index));
    if(e==101)return static_cast<double>(load<int8_t>(base,index));
    return decode_binary32(base,index);
}
uint64_t a_index(uint32_t e,uint64_t m,uint64_t k,uint64_t row,uint64_t depth) {
    return is_column(e)?depth*m+row:row*k+depth;
}
uint64_t b_index(uint32_t e,uint64_t n,uint64_t k,uint64_t depth,uint64_t col) {
    return is_column(e)?col*k+depth:depth*n+col;
}
uint64_t c_index(uint32_t e,uint64_t m,uint64_t n,uint64_t row,uint64_t col) {
    return is_column(e)?col*m+row:row*n+col;
}
matrix_ref_value dot(uint32_t e,uint64_t m,uint64_t n,uint64_t k,
                  const void* a,const void* b,const void* c0,uint64_t row,uint64_t col) {
    matrix_ref_value out{};out.terms=k+is_beta_one(e);
    if(is_integer(e)){
        int64_t sum=0,abs_sum=0;
        for(uint64_t depth=0;depth<k;++depth){
            matrix_check_budget_periodic(depth);
            const int64_t x=load<int8_t>(a,a_index(e,m,k,row,depth));
            const int64_t y=load<int8_t>(b,b_index(e,n,k,depth,col));
            const int64_t p=x*y;sum+=p;abs_sum+=p<0?-p:p;
        }
        if(sum<INT32_MIN||sum>INT32_MAX)throw RefError{ORACLE_OVERFLOW};
        out.expected=static_cast<double>(e==101?std::max<int64_t>(-128,std::min<int64_t>(127,sum)):sum);
        out.abs_sum=static_cast<double>(abs_sum);return out;
    }
    ExactAccumulator sum,abs_sum;
    for(uint64_t depth=0;depth<k;++depth){
        matrix_check_budget_periodic(depth);
        const double x=input_value(e,a,a_index(e,m,k,row,depth));
        const double y=input_value(e,b,b_index(e,n,k,depth,col));
        sum.product(x,y);abs_sum.product(std::abs(x),std::abs(y));
    }
    if(is_beta_one(e)){
        const double initial=output_value(e,c0,c_index(e,m,n,row,col));
        sum.scalar(initial);abs_sum.scalar(std::abs(initial));
    }
    out.expected=sum.rounded();out.abs_sum=abs_sum.abs_upper();
    if(!std::isfinite(out.expected)||!std::isfinite(out.abs_sum))throw RefError{ORACLE_OVERFLOW};
    return out;
}
uint32_t exception_flag() {
    try{throw;}catch(const RefError& e){return e.flag;}
    catch(const std::bad_alloc&){return ALLOCATION;}
    catch(...){return INVALID;}
}
}

extern "C" void matrix_reference_set_budget_ns(uint64_t budget_ns) {
    if(!budget_ns){matrix_budget_active=false;return;}
    constexpr uint64_t MAX_BUDGET_NS=3600ULL*1000ULL*1000ULL*1000ULL;
    const uint64_t capped=std::min(budget_ns,MAX_BUDGET_NS);
    matrix_budget_deadline=std::chrono::steady_clock::now()+std::chrono::nanoseconds(capped);
    matrix_budget_active=true;
}
extern "C" void matrix_reference_clear_budget() { matrix_budget_active=false; }

extern "C" int matrix_reference_dot(uint32_t endpoint,uint64_t m,uint64_t n,uint64_t k,
 const void* a,const void* b,const void* c0,uint64_t row,uint64_t col,matrix_ref_value* out) {
    if(!out)return INVALID;*out={};
    try{
        descriptor(endpoint,m,n,k);
        matrix_check_budget();
        if(!a||!b||(is_beta_one(endpoint)&&!c0)||row>=m||col>=n)throw RefError{INVALID};
        *out=dot(endpoint,m,n,k,a,b,c0,row,col);return 0;
    }catch(...){out->flags=exception_flag();return out->flags;}
}
extern "C" int matrix_reference_full(uint32_t endpoint,uint64_t m,uint64_t n,uint64_t k,
 const void* a,const void* b,const void* c0,matrix_ref_value* out,uint64_t output_count,uint64_t max_terms) {
    try{
        descriptor(endpoint,m,n,k);
        matrix_check_budget();
        if(!a||!b||!out||(is_beta_one(endpoint)&&!c0))throw RefError{INVALID};
        const uint64_t cells=m*n,terms=k+is_beta_one(endpoint);
        if(output_count!=cells||cells>max_terms/terms)throw RefError{LIMITS};
        for(uint64_t row=0;row<m;++row){
            matrix_check_budget();
            for(uint64_t col=0;col<n;++col){
                matrix_check_budget_periodic(col);
                out[row*n+col]=dot(endpoint,m,n,k,a,b,c0,row,col);
            }
        }
        return 0;
    }catch(...){const uint32_t flags=exception_flag();if(out&&output_count)out[0]={0,0,0,flags,0};return flags;}
}
extern "C" int matrix_reference_projection(uint32_t endpoint,uint64_t m,uint64_t n,uint64_t k,
 const void* a,const void* b,const void* c0,const void* c,const int8_t* signs,
 matrix_projection_value* out,uint64_t output_count) {
    try{
        descriptor(endpoint,m,n,k);
        matrix_check_budget();
        // Saturation is nonlinear. Never apply the linear projection identity to
        // endpoint 101. Its independently decoded full/sampled exact dots remain available.
        if(endpoint==101)throw RefError{UNSUPPORTED};
        if(!a||!b||!c||!signs||!out||(is_beta_one(endpoint)&&!c0))throw RefError{INVALID};
        if(output_count!=m||k>MAX_REFERENCE_SCRATCH/(3*sizeof(double)))throw RefError{LIMITS};
        for(uint64_t col=0;col<n;++col){
            matrix_check_budget_periodic(col);
            if(signs[col]!=-1&&signs[col]!=1)throw RefError{INVALID};
        }
        std::vector<double> br(k),br_error(k),b_abs(k);
        for(uint64_t depth=0;depth<k;++depth){
            matrix_check_budget_periodic(depth);
            ExactAccumulator projected,absolute;
            for(uint64_t col=0;col<n;++col){
                matrix_check_budget_periodic(col);
                const double x=input_value(endpoint,b,b_index(endpoint,n,k,depth,col));
                projected.product(x,static_cast<double>(signs[col]));absolute.scalar(std::abs(x));
            }
            br[depth]=projected.rounded();br_error[depth]=projected.rounding_error_upper(br[depth]);
            b_abs[depth]=absolute.abs_upper();
            if(!std::isfinite(b_abs[depth]))throw RefError{ORACLE_OVERFLOW};
        }
        for(uint64_t row=0;row<m;++row){
            matrix_check_budget_periodic(row);
            ExactAccumulator expected,observed,absolute,rounding;
            for(uint64_t depth=0;depth<k;++depth){
                matrix_check_budget_periodic(depth);
                const double x=input_value(endpoint,a,a_index(endpoint,m,k,row,depth));
                expected.product(x,br[depth]);absolute.product(std::abs(x),b_abs[depth]);
                rounding.product(std::abs(x),br_error[depth]);
            }
            for(uint64_t col=0;col<n;++col){
                matrix_check_budget_periodic(col);
                const uint64_t idx=c_index(endpoint,m,n,row,col);
                observed.product(output_value(endpoint,c,idx),static_cast<double>(signs[col]));
                if(is_beta_one(endpoint)){
                    const double initial=output_value(endpoint,c0,idx);
                    expected.product(initial,static_cast<double>(signs[col]));absolute.scalar(std::abs(initial));
                }
            }
            matrix_projection_value item{};item.expected=expected.rounded();item.observed=observed.rounded();
            rounding.scalar(expected.rounding_error_upper(item.expected));
            rounding.scalar(observed.rounding_error_upper(item.observed));
            item.abs_sum=absolute.abs_upper();item.projection_error=rounding.abs_upper();
            item.terms=k+is_beta_one(endpoint);
            if(!std::isfinite(item.abs_sum)||!std::isfinite(item.projection_error))throw RefError{ORACLE_OVERFLOW};
            out[row]=item;
        }
        return 0;
    }catch(...){const uint32_t flags=exception_flag();if(out&&output_count)out[0]={0,0,0,0,0,flags,0};return flags;}
}
