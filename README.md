# Whole-call conditions for eliminating a packed operand in small-row Arm SME GEMM

A standalone source project for the paper Whole-call conditions for eliminating a packed operand in small-row Arm SME GEMM, compute microbenchmarks, transfer/interference experiments and packing ablations. The
matrix population contains exactly 160 shapes: 148 Primary and 12 Extension.

The project builds before Git initialization and needs no files from its source
workspace. Dependencies are vendored as source with pinned revisions and licenses.
No previous experiment outputs are included. Building does not run experiments.

## Setup and build

Required: Apple arm64 macOS, Xcode with an SME2-capable Clang assembler, Python
3.9 or newer, and the standard macOS build tools. Native experiments additionally
require hardware support for the selected SME instruction. The verified toolchain
is Xcode 26.6 (17F113), Apple Clang 21.0.0 and macOS SDK 26.5; SDK and compiler identities are recorded at
build time. Accelerate is supplied by macOS. No Python packages are needed.

Install that Xcode release from Apple and complete its first-launch component
installation. Select its developer directory with
`sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer` if needed.
Python 3.9.6 supplied with the verified developer tools was sufficient. Other
toolchains are recorded distinctly and require fresh qualification.

```sh
cd SME-GDB-GEMM
make                    # help only
make setup              # dependency/toolchain inspection only
make build              # all native components; no worker execution
make check              # Python tooling checks; no native experiment
```

See [all commands](docs/commands.md) and the machine-readable
[inventory](config/inventory.json). `make build-<component>` builds a component;
`make clean` removes the generated build tree while preserving results.

## Run explicitly

Qualification, calibration and measurement are separate invocations. For example:

```sh
make qualify-harness
make qualify-fp32-row-stock
make calibrate-fp32-row-stock
```

Independent complete suites such as `make measure-fp32-row` and
`make measure-fp64-serial` preserve their scoped backend comparisons and require
only their own families. Their prerequisites appear in the command index.

The complete matched FP32 measurement is `make measure-fp32`; it requires current
qualification and calibration for all sixteen FP32 families. If any evidence is
missing or stale, the command exits and prints the exact prerequisite command.
It never starts another campaign automatically. FP64, mixed precision and
ablation use `make measure-fp64`, `make measure-mixed`, and
`make measure-ablation`. Compute commands are per datatype; transfer and
interference share the full `make measure-transfer` schedule.

**Parallel admission is blocked by default:** the available ABI guard observes
the outer adapter call; proof for nested backend workers is unavailable. See
[the prerequisite limitation and explicit configuration choice](docs/prerequisites.md).
The INT32-input compute requirement is explicitly unsupported by SME MOPA;
its targets report the complete 17-cell disposition and exit nonzero.

## Paths, configuration and outputs

```sh
make build BUILD_DIR="/absolute/path/build"
make qualify-harness BUILD_DIR="/absolute/path/build" OUTPUT_DIR="/absolute/path/results"
```

`PYTHON`, `BUILD_DIR`, `OUTPUT_DIR`, and `CONFIG` are configurable. Relative paths
are relative to the project root when invoking Make there. `CONFIG` defaults to
`config/runtime.json`; it accepts command/reference time limits, the explicit
parallel guard scope, and optional `retain_arrays: true`. Scientific grids,
seeds, numerical bounds and estimators are in versioned technical configurations
and runners. Editing them invalidates previous evidence.

Each invocation creates a unique run directory with command, resolved
configuration, selected environment, source/build identities, native logs and
terminal status. Failed outputs remain. Successful nonwitness matrix arrays may
be released only after numerical checking and worker cleanup; predetermined
witness arrays, hashes, guards and numerical records remain. `retain_arrays`
keeps all arrays and can require substantial storage.

One nonblocking host/user lock excludes simultaneous project measurements,
qualifications, builds and cleanup across relocated copies. A conflicting command
fails immediately. The lock does not control unrelated applications; arrange
an otherwise idle machine yourself. There are no retries or resume controllers.

Move the source folder freely, then rebuild in its new location. Generated build
directories are disposable. Results remain tied to their recorded build and host;
they are not silently read from another workspace. Add custom generated paths to
`.gitignore` before initializing Git yourself.

## Technical and licensing references

- [Command index](docs/commands.md), including every qualification and measurement.
- [Numerical and timing methods](docs/methods.md).
- [Evidence matching and admission limits](docs/prerequisites.md).
- [Preparation verification](docs/verification.md).
- [Third-party notices](THIRD_PARTY_NOTICES.md) and [dependency revisions](third_party/dependencies.json).
