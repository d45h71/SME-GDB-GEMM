# Column-major packing implementations

The common adapter implements the project adapter ABI for endpoint 1 only: serial column-major FP32 accumulation, alpha=1, beta=1. It rejects unsupported endpoints, thread policies, strides, overlapping buffers and invalid extents. The public backend identity is supplied at compilation.

`blocked`, `grouped`, and `online` each contain the packing and main/edge assembly kernels. `common/driver.cpp` provides a shared blocked driver with MC=512, KC=1024, NC=256 and MR=64. `common/compat.S` preserves the Darwin callee-saved register boundary. Offline A packing uses `common/packa.S`.

| Library | `MP5_ABLATION_OFFLINE_PACK` | `MP5_ABLATION_GROUP4` | Backend string |
|---|---:|---:|---|
| `libablation_blocked.dylib` | 1 | 0 | `ablation_blocked` |
| `libablation_grouped.dylib` | 1 | 1 | `ablation_grouped` |
| `libablation_online.dylib` | 0 | unused | `ablation_online` |

Compile the adapter with `MP5_ABLATION_BACKEND_NAME` set to the quoted backend string; compile the driver and compatibility assembly with the offline-pack setting; compile packa only for the first two variants with the grouped-load setting. C++ uses the non-SVE caller flags; assembly uses the SME/SME2 flags declared by the root build.

The online variant preserves the stock packing and kernel instruction sequences. The grouped variant selects the prepacked kernel path. The blocked variant additionally expands grouped loads into individual vector loads. Source digests and packaging-only symbol renames are recorded in `config/ablation.json`.

Build with `make build-ablation`. Qualification and measurement commands are documented in [the experiment README](../../experiments/ablation/README.md). Licensing: [root LICENSE](../../LICENSE).
