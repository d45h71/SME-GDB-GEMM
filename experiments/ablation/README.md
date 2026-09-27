# Column-major packing ablations

This experiment compares five implementations over the exact 160-member workload registry (148 Primary, 12 Extension). Every cell computes column-major FP32 `C = C0 + A*B` with alpha=1, beta=1 and one requested worker. It contains 800 cells, with the same shape population for every implementation.

| Backend | Implementation |
|---|---|
| `libxsmm` | Pinned LIBXSMM baseline through its ordinary column-major adapter |
| `ablation_blocked` | Blocked GEMM, offline packing of both operands, single-vector loads |
| `ablation_grouped` | The blocked implementation with grouped vector loads |
| `ablation_online` | Grouped loads with first-round online packing and later packed-buffer reuse |
| `stock` | Unchanged stock column-major implementation, measured as a separate control |

The three local variants are cumulative reconstructions. The baseline and unchanged stock remain separate backends. Allocation, packing and required zeroing occur inside the timed matrix call. The controller restores the same C0 outside every call, including warmups. Each recorded batch contains one individually timed call.

## Commands

Run from the project root:

```sh
make build
make qualify-harness
make qualify-ablation
make calibrate-ablation
make measure-ablation
```

Each command is an independent invocation. Building never qualifies or measures. Ablation qualification requires the current harness qualification. Calibration requires qualification for the current source, build and configuration. Measurement requires both current qualification and calibration. Missing or stale prerequisites cause a nonzero exit with the exact prerequisite command; they are never run implicitly.

`BUILD_DIR` selects the build directory and `OUTPUT_DIR` selects the experiment output directory. Use the same values for the entire set:

```sh
make build BUILD_DIR=/absolute/path/to/build
make qualify-harness BUILD_DIR=/absolute/path/to/build OUTPUT_DIR=/absolute/path/to/results
make qualify-ablation BUILD_DIR=/absolute/path/to/build OUTPUT_DIR=/absolute/path/to/results
make calibrate-ablation BUILD_DIR=/absolute/path/to/build OUTPUT_DIR=/absolute/path/to/results
make measure-ablation BUILD_DIR=/absolute/path/to/build OUTPUT_DIR=/absolute/path/to/results
```

The first command in the custom-path example builds every required component, including the matrix worker, reference, stock control and LIBXSMM baseline. The exact workload and method settings are in [`config/ablation.json`](../../config/ablation.json). Changing a configuration invalidates prerequisite evidence. Full runs do not accept shape truncation, session reduction, or substituted backends.

## Prerequisites and method

Use an Apple Silicon macOS host with the SME/SME2 and FP64 SME capabilities required by the project, a supported Apple Clang toolchain, and the external libraries described in the [root README](../../README.md). Both LIBXSMM and stock must be built as well as the three local variants. `make build-ablation` builds only those three local libraries; `make build` includes the worker, reference and comparison dependencies.

Qualification covers dense inputs with seeds 1701 and 1702 and zero/nonzero C0 for every cell. It also applies all five structured input patterns (0, 1, 2, 3, 5) with seed 1703 to four deterministic member witnesses per implementation, and checks three invalid leading dimensions. Positive beta-one cases repeat the call after restoring C0. Guard/state validation, input invariance, full finite scans, and the independent numerical reference must pass.

Calibration records three single-call observations for the smallest member in each implementation. Column-major beta-one batches remain fixed at one call. Measurement uses three sessions, two single-call warmups, and five recorded single-call repetitions per cell and session. Setup, guards, input restoration, hashing, serialization and reference adjudication remain outside the native timer. The output retains individual observations and session means; the cell latency is the equal-weight mean of the three session means.

## Outputs and success criteria

Each invocation creates a unique run directory beneath `OUTPUT_DIR` (default `results`). The command, configuration, environment, source and build identities, per-cell numerical checks, raw timing records, and terminal status are retained. Failed directories are preserved, and failure never selects a replacement observation or retries automatically. A shared lock prevents overlapping native campaigns.

Qualification succeeds only when every scheduled numerical, input, state and stride check passes. Calibration succeeds only when all five configured families have complete current observations. Measurement succeeds only with all 800 cells and the complete three-session schedule. A successful build or tooling check is not an experimental result.

Licensing: first-party code is subject to the [root LICENSE](../../LICENSE); the LIBXSMM baseline is also subject to its preserved third-party license. See [third-party notices](../../THIRD_PARTY_NOTICES.md).
