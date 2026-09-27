# fp32-parallel-row complete independent measurement suite

License: [root LICENSE](../../LICENSE). Third-party source licensing is retained in [the notices](../../THIRD_PARTY_NOTICES.md).

## Exact command

```sh
make measure-fp32-parallel-row
```

## Prerequisites

Run `make build` and `make qualify-harness`, then:

```sh
make qualify-fp32-parallel-row-stock
make calibrate-fp32-parallel-row-stock
make qualify-fp32-parallel-row-accelerate
make calibrate-fp32-parallel-row-accelerate
```

This suite includes 320 cells: every included family retains all 160 unique matrix shapes. It preserves backend rotation within its scope, three sessions, two warmups and five recorded batches. It is independently invocable and does not replace the complete `make measure-fp32` campaign.

## Options, outputs and success

Options are `PYTHON`, `BUILD_DIR`, `OUTPUT_DIR`, and `CONFIG`; see [commands](../../docs/commands.md). No prerequisite runs automatically. Outputs live in unique `OUTPUT_DIR/fp32-parallel-row/measurement/` directories: configuration, identity, status, schedule, per-case raw/numerical/guard records, `sessions.json`, and `measurements.json`.

Success requires all scoped cells and sessions to pass the full numerical/ABI/protocol checks and emit terminal PASS. Failed outputs remain and exit nonzero. Missing or stale evidence prints exact prerequisite commands. A build is not experimental evidence.

The default nested-worker guard prerequisite blocks this suite; see [prerequisites](../../docs/prerequisites.md).
