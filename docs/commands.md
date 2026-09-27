# Command index

All commands run from the project root. `make` defaults to `make help`. No build or setup command runs an experiment.

Global options: `PYTHON=python3 BUILD_DIR=build OUTPUT_DIR=results CONFIG=config/runtime.json`. Paths with spaces must be quoted. Configuration changes invalidate prior evidence. See [prerequisites](prerequisites.md).

## Build and tooling

| Command | Purpose |
|---|---|
| `make help` | Print all commands; also the default |
| `make inventory` | Show configuration, implementation and build mapping |
| `make setup` | Check toolchain and vendored dependencies |
| `make build` | Build every component; no experimental calls |
| `make check` | Check inventory and tooling using Python fixtures only |
| `make clean` | Remove the marked build tree; preserve results |
| `make build-matrix` | Build matrix component |
| `make build-stock` | Build stock component |
| `make build-first` | Build first component |
| `make build-second` | Build second component |
| `make build-accelerate` | Build accelerate component |
| `make build-libxsmm` | Build libxsmm component |
| `make build-kleidiai` | Build kleidiai component |
| `make build-openblas` | Build openblas component |
| `make build-compute` | Build compute component |
| `make build-transfer` | Build transfer component |
| `make build-ablation` | Build ablation component |

## Qualifications, calibrations and measurements

The full required grids are listed below. Each command runs only the named operation. Measurements fail with exact prerequisite commands when evidence is absent or stale.

| Experiment | Cells | Qualification | Calibration | Measurement |
|---|---:|---|---|---|
| [harness](../experiments/harness/README.md) | 1 | `make qualify-harness` | — | — |
| [fp32-row-stock](../experiments/fp32-row-stock/README.md) | 160 | `make qualify-fp32-row-stock` | `make calibrate-fp32-row-stock` | — |
| [fp32-row-second](../experiments/fp32-row-second/README.md) | 160 | `make qualify-fp32-row-second` | `make calibrate-fp32-row-second` | — |
| [fp32-row-first](../experiments/fp32-row-first/README.md) | 160 | `make qualify-fp32-row-first` | `make calibrate-fp32-row-first` | — |
| [fp32-row-accelerate](../experiments/fp32-row-accelerate/README.md) | 160 | `make qualify-fp32-row-accelerate` | `make calibrate-fp32-row-accelerate` | — |
| [fp32-row-libxsmm](../experiments/fp32-row-libxsmm/README.md) | 160 | `make qualify-fp32-row-libxsmm` | `make calibrate-fp32-row-libxsmm` | — |
| [fp32-row-kleidiai](../experiments/fp32-row-kleidiai/README.md) | 160 | `make qualify-fp32-row-kleidiai` | `make calibrate-fp32-row-kleidiai` | — |
| [fp32-row-openblas](../experiments/fp32-row-openblas/README.md) | 160 | `make qualify-fp32-row-openblas` | `make calibrate-fp32-row-openblas` | — |
| [fp32-column-stock](../experiments/fp32-column-stock/README.md) | 160 | `make qualify-fp32-column-stock` | `make calibrate-fp32-column-stock` | — |
| [fp32-column-accelerate](../experiments/fp32-column-accelerate/README.md) | 160 | `make qualify-fp32-column-accelerate` | `make calibrate-fp32-column-accelerate` | — |
| [fp32-column-libxsmm](../experiments/fp32-column-libxsmm/README.md) | 160 | `make qualify-fp32-column-libxsmm` | `make calibrate-fp32-column-libxsmm` | — |
| [fp32-prepared-row-libxsmm](../experiments/fp32-prepared-row-libxsmm/README.md) | 160 | `make qualify-fp32-prepared-row-libxsmm` | `make calibrate-fp32-prepared-row-libxsmm` | — |
| [fp32-prepared-column-libxsmm](../experiments/fp32-prepared-column-libxsmm/README.md) | 160 | `make qualify-fp32-prepared-column-libxsmm` | `make calibrate-fp32-prepared-column-libxsmm` | — |
| [fp32-parallel-row-stock](../experiments/fp32-parallel-row-stock/README.md) | 160 | `make qualify-fp32-parallel-row-stock` | `make calibrate-fp32-parallel-row-stock` | — |
| [fp32-parallel-row-accelerate](../experiments/fp32-parallel-row-accelerate/README.md) | 160 | `make qualify-fp32-parallel-row-accelerate` | `make calibrate-fp32-parallel-row-accelerate` | — |
| [fp32-parallel-column-stock](../experiments/fp32-parallel-column-stock/README.md) | 160 | `make qualify-fp32-parallel-column-stock` | `make calibrate-fp32-parallel-column-stock` | — |
| [fp32-parallel-column-accelerate](../experiments/fp32-parallel-column-accelerate/README.md) | 160 | `make qualify-fp32-parallel-column-accelerate` | `make calibrate-fp32-parallel-column-accelerate` | — |
| [fp64-serial-stock](../experiments/fp64-serial-stock/README.md) | 160 | `make qualify-fp64-serial-stock` | `make calibrate-fp64-serial-stock` | — |
| [fp64-serial-accelerate](../experiments/fp64-serial-accelerate/README.md) | 160 | `make qualify-fp64-serial-accelerate` | `make calibrate-fp64-serial-accelerate` | — |
| [fp64-parallel-stock](../experiments/fp64-parallel-stock/README.md) | 160 | `make qualify-fp64-parallel-stock` | `make calibrate-fp64-parallel-stock` | — |
| [fp64-parallel-accelerate](../experiments/fp64-parallel-accelerate/README.md) | 160 | `make qualify-fp64-parallel-accelerate` | `make calibrate-fp64-parallel-accelerate` | — |
| [fp16-serial-stock](../experiments/fp16-serial-stock/README.md) | 160 | `make qualify-fp16-serial-stock` | `make calibrate-fp16-serial-stock` | — |
| [fp16-parallel-stock](../experiments/fp16-parallel-stock/README.md) | 160 | `make qualify-fp16-parallel-stock` | `make calibrate-fp16-parallel-stock` | — |
| [int8-serial-stock](../experiments/int8-serial-stock/README.md) | 160 | `make qualify-int8-serial-stock` | `make calibrate-int8-serial-stock` | — |
| [int8-parallel-stock](../experiments/int8-parallel-stock/README.md) | 160 | `make qualify-int8-parallel-stock` | `make calibrate-int8-parallel-stock` | — |
| [bf16-serial-kleidiai](../experiments/bf16-serial-kleidiai/README.md) | 160 | `make qualify-bf16-serial-kleidiai` | `make calibrate-bf16-serial-kleidiai` | — |
| [ablation](../experiments/ablation/README.md) | 800 | `make qualify-ablation` | `make calibrate-ablation` | `make measure-ablation` |
| [compute-int8](../experiments/compute-int8/README.md) | 17 | `make qualify-compute-int8` | `make calibrate-compute-int8` | `make measure-compute-int8` |
| [compute-int16](../experiments/compute-int16/README.md) | 17 | `make qualify-compute-int16` | `make calibrate-compute-int16` | `make measure-compute-int16` |
| [compute-int32](../experiments/compute-int32/README.md) | 17 | `make qualify-compute-int32` | `make calibrate-compute-int32` | `make measure-compute-int32` |
| [compute-fp16](../experiments/compute-fp16/README.md) | 17 | `make qualify-compute-fp16` | `make calibrate-compute-fp16` | `make measure-compute-fp16` |
| [compute-bf16](../experiments/compute-bf16/README.md) | 17 | `make qualify-compute-bf16` | `make calibrate-compute-bf16` | `make measure-compute-bf16` |
| [compute-fp32](../experiments/compute-fp32/README.md) | 17 | `make qualify-compute-fp32` | `make calibrate-compute-fp32` | `make measure-compute-fp32` |
| [compute-fp64](../experiments/compute-fp64/README.md) | 21 | `make qualify-compute-fp64` | `make calibrate-compute-fp64` | `make measure-compute-fp64` |
| [transfer](../experiments/transfer/README.md) | 12 | `make qualify-transfer` | `make calibrate-transfer` | — |
| [fp32](../experiments/fp32/README.md) | 2560 | — | — | `make measure-fp32` |
| [fp64](../experiments/fp64/README.md) | 640 | — | — | `make measure-fp64` |
| [mixed](../experiments/mixed/README.md) | 800 | — | — | `make measure-mixed` |
| [transfer-all](../experiments/transfer-all/README.md) | 460 | — | — | `make measure-transfer` |

## Offline reductions

These commands read current successful measurements and never launch a campaign.

| Command | Required measurements | Output |
|---|---|---|
| `make analyze-comparisons` | `measure-fp32` | Matched seven-backend speedups and separate population geometric means |
| `make analyze-irregular` | `measure-fp64` | Unequal-dimension registry view |
| `make analyze-provenance` | `measure-fp32` | Qwen and SmolLM3 provenance views of existing Primary members |
| `make analyze-efficiency` | `measure-fp32`, `measure-mixed`, `measure-compute-fp32`, `measure-compute-fp16`, `measure-compute-int8`, `measure-compute-bf16` | Format/worker-matched compute efficiency |
| `make analyze-ablation` | `measure-ablation` | Matched configuration latency and throughput ratios |

For a read-only resolved schedule/configuration, use `python3 -B tools/run.py --plan measurement fp32` (substitute any inventory operation/name). It launches no native worker.

## Independent measurement suites

These commands preserve full 160-member backend families and the matched order within each named suite. They require only their own qualifications/calibrations. Their outputs are separate runs; they are not silently combined or substituted for the full FP32/FP64/mixed comparison campaigns.

| Command | Cells | Prerequisite family configurations |
|---|---:|---|
| [`make measure-fp32-row`](../experiments/fp32-row/README.md) | 1120 | `fp32-row-stock`, `fp32-row-second`, `fp32-row-first`, `fp32-row-accelerate`, `fp32-row-libxsmm`, `fp32-row-kleidiai`, `fp32-row-openblas` |
| [`make measure-fp32-column`](../experiments/fp32-column/README.md) | 480 | `fp32-column-stock`, `fp32-column-accelerate`, `fp32-column-libxsmm` |
| [`make measure-fp32-prepared`](../experiments/fp32-prepared/README.md) | 320 | `fp32-prepared-row-libxsmm`, `fp32-prepared-column-libxsmm` |
| [`make measure-fp32-parallel-row`](../experiments/fp32-parallel-row/README.md) | 320 | `fp32-parallel-row-stock`, `fp32-parallel-row-accelerate` |
| [`make measure-fp32-parallel-column`](../experiments/fp32-parallel-column/README.md) | 320 | `fp32-parallel-column-stock`, `fp32-parallel-column-accelerate` |
| [`make measure-fp64-serial`](../experiments/fp64-serial/README.md) | 320 | `fp64-serial-stock`, `fp64-serial-accelerate` |
| [`make measure-fp64-parallel`](../experiments/fp64-parallel/README.md) | 320 | `fp64-parallel-stock`, `fp64-parallel-accelerate` |
| [`make measure-mixed-serial`](../experiments/mixed-serial/README.md) | 480 | `fp16-serial-stock`, `int8-serial-stock`, `bf16-serial-kleidiai` |
| [`make measure-mixed-parallel`](../experiments/mixed-parallel/README.md) | 320 | `fp16-parallel-stock`, `int8-parallel-stock` |
