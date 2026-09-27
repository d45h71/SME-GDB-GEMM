# SME transfer

License: [root license](../../LICENSE); native dependency notices: [third-party notices](../../THIRD_PARTY_NOTICES.md).

## Exact commands

Run from the project root:

```sh
make qualify-transfer
make calibrate-transfer
```

## Prerequisites

Build the complete project with `make build`, or these components: `make build-transfer`.
`make qualify-transfer` checks all 12 width/group forms at 2 KiB, wrap/canary/poison/content checks and the safe heap/stack pair. `make calibrate-transfer` requires that qualification and runs the fixed three-cell load ladder with interference pilots.

## Options and outputs

Use `BUILD_DIR`, `OUTPUT_DIR`, `PYTHON`, and `CONFIG` as documented in [the command index](../../docs/commands.md). No command automatically builds or runs a prerequisite.

Outputs use unique directories under `OUTPUT_DIR/transfer/<operation>/`. Each attempt records command, configuration, environment, source/build identity and status. Failed attempts remain. Technical outputs: `native.json`, `raw.jsonl`, `sessions.json`.

## Success criteria

A zero exit and `status.json` with `PASS` require complete configured coverage and all runtime/numerical/protocol checks to pass. Missing/stale prerequisites, unsupported instructions, mismatches, failed children and timeouts exit nonzero. A build or tooling check is not qualification or a measurement result.

See [method details](../../docs/methods.md), [prerequisite matching](../../docs/prerequisites.md), and [`config/transfer.json`](../../config/transfer.json) for the full configuration.
