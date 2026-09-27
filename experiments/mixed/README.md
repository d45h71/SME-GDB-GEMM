# mixed full matched measurement schedule

License: [root license](../../LICENSE); native dependency notices: [third-party notices](../../THIRD_PARTY_NOTICES.md).

## Exact commands

Run from the project root:

```sh
make measure-mixed
```

## Prerequisites

Build the complete project with `make build`, or these components: `make build-kleidiai`, `make build-stock`, `make build-matrix`.
Measurement requires all of the following qualifications and calibrations:

```sh
make qualify-fp16-serial-stock
make calibrate-fp16-serial-stock
make qualify-fp16-parallel-stock
make calibrate-fp16-parallel-stock
make qualify-int8-serial-stock
make calibrate-int8-serial-stock
make qualify-int8-parallel-stock
make calibrate-int8-parallel-stock
make qualify-bf16-serial-kleidiai
make calibrate-bf16-serial-kleidiai
```
Every backend/endpoint/thread/preparation family covers exactly 160 matrix shapes (148 Primary, 12 Extension). No subset or alternate matrix population is admitted.

## Options and outputs

Use `BUILD_DIR`, `OUTPUT_DIR`, `PYTHON`, and `CONFIG` as documented in [the command index](../../docs/commands.md). No command automatically builds or runs a prerequisite.

Outputs use unique directories under `OUTPUT_DIR/mixed/<operation>/`. Each attempt records command, configuration, environment, source/build identity and status. Failed attempts remain. Technical outputs: `schedule.json`, `case directories with raw/oracle/guard records`, `summary.json`, `sessions.json`, `measurements.json`.

## Success criteria

A zero exit and `status.json` with `PASS` require complete configured coverage and all runtime/numerical/protocol checks to pass. Missing/stale prerequisites, unsupported instructions, mismatches, failed children and timeouts exit nonzero. A build or tooling check is not qualification or a measurement result.

See [method details](../../docs/methods.md), [prerequisite matching](../../docs/prerequisites.md), and [`config/mixed.json`](../../config/mixed.json) for the full configuration.

Parallel configurations currently require the unresolved nested-worker ABI prerequisite by default. See [the exact guard limitation](../../docs/prerequisites.md).
