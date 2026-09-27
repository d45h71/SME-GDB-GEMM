#include <algorithm>
#include <cstdlib>
#include <cstring>

#ifndef MP5_ABLATION_OFFLINE_PACK
#error MP5_ABLATION_OFFLINE_PACK is required
#endif

namespace {
constexpr int COL_MC = 512;
constexpr int COL_KC = 1024;
constexpr int COL_NC = 256;
constexpr int COL_MR = 64;
constexpr int SME_CACHELINE_SIZE = 128;

extern "C" void mp5_ablation_safe_col_packb(int, int, float*, int, float*);
extern "C" void mp5_ablation_safe_col_kernel_64x16(int, int, int, float*, float*,
                                           float*, float*, int, int, int);
extern "C" void mp5_ablation_safe_col_kernel_16x64(int, int, int, float*, float*,
                                           float*, float*, int, int, int);
#if MP5_ABLATION_OFFLINE_PACK
extern "C" void mp5_ablation_safe_packa_64(int, int, float*, int, float*);
extern "C" void mp5_ablation_safe_packa_16(int, int, float*, int, float*);
#endif
}

extern "C" int mp5_ablation_col_sgemm(int m, int n, int k, float* a, float* b,
                              float* c) {
    float* packed_b = nullptr;
    float* packed_a = nullptr;
    if (posix_memalign(reinterpret_cast<void**>(&packed_b), SME_CACHELINE_SIZE,
                       COL_KC * COL_NC * sizeof(float)) ||
        posix_memalign(reinterpret_cast<void**>(&packed_a), SME_CACHELINE_SIZE,
                       COL_MC * COL_KC * sizeof(float))) {
        std::free(packed_b);
        std::free(packed_a);
        return 3;
    }

    const int full_m = (m / COL_MC) * COL_MC;
    const int m_tail = m - full_m;
    const int main_tail = (m_tail / COL_MR) * COL_MR;
    const int edge_tail = m_tail - main_tail;

    for (int jc = 0; jc < n; jc += COL_NC) {
        const int jb = std::min(COL_NC, n - jc);
        for (int pc = 0; pc < k; pc += COL_KC) {
            const int pb = std::min(COL_KC, k - pc);
            mp5_ablation_safe_col_packb(pb, jb, &b[pc + jc * k], k, packed_b);

            for (int ic = 0; ic < full_m; ic += COL_MC) {
#if MP5_ABLATION_OFFLINE_PACK
                std::memset(packed_a, 0, COL_MC * COL_KC * sizeof(float));
                mp5_ablation_safe_packa_64(COL_MC, pb, &a[ic + pc * m], m, packed_a);
#endif
                mp5_ablation_safe_col_kernel_64x16(COL_MC, pb, jb, &a[ic + pc * m],
                                           packed_a, packed_b, &c[ic + jc * m],
                                           COL_KC, m, pc);
            }
            if (m_tail) {
#if MP5_ABLATION_OFFLINE_PACK
                std::memset(packed_a, 0, COL_MC * COL_KC * sizeof(float));
                if (main_tail)
                    mp5_ablation_safe_packa_64(main_tail, pb, &a[full_m + pc * m], m,
                                      packed_a);
                if (edge_tail)
                    mp5_ablation_safe_packa_16(
                        edge_tail, pb, &a[full_m + main_tail + pc * m], m,
                        packed_a + static_cast<size_t>(main_tail) * pb);
#endif
                if (main_tail)
                    mp5_ablation_safe_col_kernel_64x16(
                        main_tail, pb, jb, &a[full_m + pc * m], packed_a,
                        packed_b, &c[full_m + jc * m], COL_KC, m, pc);
                if (edge_tail)
                    mp5_ablation_safe_col_kernel_16x64(
                        edge_tail, pb, jb, &a[full_m + main_tail + pc * m],
                        packed_a + static_cast<size_t>(main_tail) * pb,
                        packed_b, &c[full_m + main_tail + jc * m], COL_KC, m,
                        pc);
            }
        }
    }
    std::free(packed_b);
    std::free(packed_a);
    return 0;
}
