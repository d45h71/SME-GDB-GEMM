# Current-build evidence and limits

Build, qualification, calibration and measurement are separate commands. Setup
and builds never run campaigns. No receipt, earlier phase, recovery controller,
previous project result or original workspace is required.

The runner checks the selected components' source hashes, output binary hashes
and successful build manifest. Qualification/calibration identity includes these
bindings, compiler/SDK information, runner source, complete configuration, exact
registry and relevant host/environment. Evidence is accepted only from a successful
unique run with the same identity and a matching retained-file manifest.
Changing a binary, source, configuration or recorded environment invalidates it.
Missing/stale evidence produces the complete `make` command to regenerate it,
including the selected build/output/configuration paths. A native call does not
start until these prerequisites are admitted.

Matrix qualification requires `make qualify-harness`. Calibration requires the
corresponding qualification. Each full measurement requires qualifications and
calibrations for every constituent family. Matched-peak analysis additionally
requires current compute measurements with the same datatype and worker count.
Offline analyses never create missing measurements.

## Parallel guard scope

The available assembly guard verifies the outer adapter call's callee-saved
registers, stack, floating-point control and SME state. It does not observe ABI
state inside Stock Dispatch callbacks or Accelerate's internal worker threads.
The available parallel proof therefore does not satisfy a nested-worker ABI
prerequisite. The default `parallel_guard_scope: "nested-required"` fails before
native execution for all affected families and full measurements, with
`BLOCKED_PARALLEL_ABI` recorded. Complete FP32/FP64/mixed production remains blocked
under that requirement; serial and ablation commands remain independently usable.

Admitting fresh full numerical qualification with the existing outer-call guard
instead is a methodological choice for the owner. If explicitly chosen, use a
runtime configuration with `parallel_guard_scope: "outer-call"`. This setting
records the exact limitation and invalidates earlier evidence; it does not create
or claim nested-worker proof. No such waiver is inferred by setup/build commands.

## Clock and unsupported endpoints

The matrix ABI guard's timer path relies on an inspected integer-only clock leaf.
The worker verifies the 144-byte `mach_absolute_time` implementation hash
`c615a84440f8ae7325e75d236ace6e038838f5cf634f9bba912d2904c99ff2fd`
and nonzero commpage selector before opening a case. A different clock implementation
fails closed and requires reviewing the guard/timer assumptions before changing
that source-level allowlist. A copied hash is not proof for another implementation.

The INT32-input SME MOPA grid has no instruction implementation and is reported
as unsupported, including all seventeen cells. Other SME extensions may be absent
on a particular host; the corresponding native child failure is retained. No
serial fallback, alternate precision, substituted backend or smaller matrix
silently repairs an unsupported required experiment.

The first-party license remains an owner-choice placeholder. Third-party licenses
are preserved and govern their respective sources.
