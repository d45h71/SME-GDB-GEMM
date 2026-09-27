# BF16 SME tile/thread compute grid

License: [root license](../../LICENSE); native dependency notices: [third-party notices](../../THIRD_PARTY_NOTICES.md).

## Exact commands

Run from the project root:

```sh
make qualify-compute-bf16
make calibrate-compute-bf16
make measure-compute-bf16
```

## Prerequisites

Build the complete project with `make build`, or these components: `make build-compute`.
Calibration requires the corresponding qualification. Measurement requires both current qualification and calibration. The grid contains serial tile counts 1–4 plus full-tile worker counts 2–14. All commands are non-GEMM.

## Options and outputs

Use `BUILD_DIR`, `OUTPUT_DIR`, `PYTHON`, and `CONFIG` as documented in [the command index](../../docs/commands.md). No command automatically builds or runs a prerequisite.

Outputs use unique directories under `OUTPUT_DIR/compute-bf16/<operation>/`. Each attempt records command, configuration, environment, source/build identity and status. Failed attempts remain. Technical outputs: `raw.jsonl`, `rates.json`, `peaks.json`.

## Success criteria

A zero exit and `status.json` with `PASS` require complete configured coverage and all runtime/numerical/protocol checks to pass. Missing/stale prerequisites, unsupported instructions, mismatches, failed children and timeouts exit nonzero. A build or tooling check is not qualification or a measurement result.

See [method details](../../docs/methods.md), [prerequisite matching](../../docs/prerequisites.md), and [`config/compute-bf16.json`](../../config/compute-bf16.json) for the full configuration.
