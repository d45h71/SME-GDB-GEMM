# fp64-serial complete independent measurement suite

License: [root LICENSE](../../LICENSE). Third-party source licensing is retained in [the notices](../../THIRD_PARTY_NOTICES.md).

## Exact command

```sh
make measure-fp64-serial
```

## Prerequisites

Run `make build` and `make qualify-harness`, then:

```sh
make qualify-fp64-serial-stock
make calibrate-fp64-serial-stock
make qualify-fp64-serial-accelerate
make calibrate-fp64-serial-accelerate
```

This suite includes 320 cells: every included family retains all 160 unique matrix shapes. It preserves backend rotation within its scope, three sessions, two warmups and five recorded batches. It is independently invocable and does not replace the complete `make measure-fp64` campaign.

## Options, outputs and success

Options are `PYTHON`, `BUILD_DIR`, `OUTPUT_DIR`, and `CONFIG`; see [commands](../../docs/commands.md). No prerequisite runs automatically. Outputs live in unique `OUTPUT_DIR/fp64-serial/measurement/` directories: configuration, identity, status, schedule, per-case raw/numerical/guard records, `sessions.json`, and `measurements.json`.

Success requires all scoped cells and sessions to pass the full numerical/ABI/protocol checks and emit terminal PASS. Failed outputs remain and exit nonzero. Missing or stale evidence prints exact prerequisite commands. A build is not experimental evidence.
