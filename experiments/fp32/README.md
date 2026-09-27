# fp32 full matched measurement schedule

License: [root license](../../LICENSE); native dependency notices: [third-party notices](../../THIRD_PARTY_NOTICES.md).

## Exact commands

Run from the project root:

```sh
make measure-fp32
```

## Prerequisites

Build the complete project with `make build`, or these components: `make build-accelerate`, `make build-first`, `make build-kleidiai`, `make build-libxsmm`, `make build-openblas`, `make build-second`, `make build-stock`, `make build-matrix`.
Measurement requires all of the following qualifications and calibrations:

```sh
make qualify-fp32-row-stock
make calibrate-fp32-row-stock
make qualify-fp32-row-second
make calibrate-fp32-row-second
make qualify-fp32-row-first
make calibrate-fp32-row-first
make qualify-fp32-row-accelerate
make calibrate-fp32-row-accelerate
make qualify-fp32-row-libxsmm
make calibrate-fp32-row-libxsmm
make qualify-fp32-row-kleidiai
make calibrate-fp32-row-kleidiai
make qualify-fp32-row-openblas
make calibrate-fp32-row-openblas
make qualify-fp32-column-stock
make calibrate-fp32-column-stock
make qualify-fp32-column-accelerate
make calibrate-fp32-column-accelerate
make qualify-fp32-column-libxsmm
make calibrate-fp32-column-libxsmm
make qualify-fp32-prepared-row-libxsmm
make calibrate-fp32-prepared-row-libxsmm
make qualify-fp32-prepared-column-libxsmm
make calibrate-fp32-prepared-column-libxsmm
make qualify-fp32-parallel-row-stock
make calibrate-fp32-parallel-row-stock
make qualify-fp32-parallel-row-accelerate
make calibrate-fp32-parallel-row-accelerate
make qualify-fp32-parallel-column-stock
make calibrate-fp32-parallel-column-stock
make qualify-fp32-parallel-column-accelerate
make calibrate-fp32-parallel-column-accelerate
```
Every backend/endpoint/thread/preparation family covers exactly 160 matrix shapes (148 Primary, 12 Extension). No subset or alternate matrix population is admitted.

## Options and outputs

Use `BUILD_DIR`, `OUTPUT_DIR`, `PYTHON`, and `CONFIG` as documented in [the command index](../../docs/commands.md). No command automatically builds or runs a prerequisite.

Outputs use unique directories under `OUTPUT_DIR/fp32/<operation>/`. Each attempt records command, configuration, environment, source/build identity and status. Failed attempts remain. Technical outputs: `schedule.json`, `case directories with raw/oracle/guard records`, `summary.json`, `sessions.json`, `measurements.json`.

## Success criteria

A zero exit and `status.json` with `PASS` require complete configured coverage and all runtime/numerical/protocol checks to pass. Missing/stale prerequisites, unsupported instructions, mismatches, failed children and timeouts exit nonzero. A build or tooling check is not qualification or a measurement result.

See [method details](../../docs/methods.md), [prerequisite matching](../../docs/prerequisites.md), and [`config/fp32.json`](../../config/fp32.json) for the full configuration.

Parallel configurations currently require the unresolved nested-worker ABI prerequisite by default. See [the exact guard limitation](../../docs/prerequisites.md).
