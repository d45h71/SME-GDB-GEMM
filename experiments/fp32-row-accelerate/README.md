# FP32 row-major C=AB

License: [root license](../../LICENSE); native dependency notices: [third-party notices](../../THIRD_PARTY_NOTICES.md).

## Exact commands

Run from the project root:

```sh
make qualify-fp32-row-accelerate
make calibrate-fp32-row-accelerate
```

## Prerequisites

Build the complete project with `make build`, or these components: `make build-matrix`, `make build-accelerate`.
Run `make qualify-harness` before this numerical qualification.
Calibration requires a successful current `make qualify-fp32-row-accelerate`.
Every backend/endpoint/thread/preparation family covers exactly 160 matrix shapes (148 Primary, 12 Extension). No subset or alternate matrix population is admitted.

## Options and outputs

Use `BUILD_DIR`, `OUTPUT_DIR`, `PYTHON`, and `CONFIG` as documented in [the command index](../../docs/commands.md). No command automatically builds or runs a prerequisite.

Outputs use unique directories under `OUTPUT_DIR/fp32-row-accelerate/<operation>/`. Each attempt records command, configuration, environment, source/build identity and status. Failed attempts remain. Technical outputs: `schedule.json`, `case directories with raw/oracle/guard records`, `summary.json`.

## Success criteria

A zero exit and `status.json` with `PASS` require complete configured coverage and all runtime/numerical/protocol checks to pass. Missing/stale prerequisites, unsupported instructions, mismatches, failed children and timeouts exit nonzero. A build or tooling check is not qualification or a measurement result.

See [method details](../../docs/methods.md), [prerequisite matching](../../docs/prerequisites.md), and [`config/fp32-row-accelerate.json`](../../config/fp32-row-accelerate.json) for the full configuration.
