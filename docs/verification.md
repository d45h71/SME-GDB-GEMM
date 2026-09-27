# Preparation verification

Preparation passed build and tooling checks. It did **not** execute native
qualifications, benchmark warmups, calibration campaigns or measurements.
Project readiness must not be interpreted as experimental results.

| Check | Performed and outcome |
|---|---|
| Full original-location build | All 11 components passed; 17 native artifacts |
| Source-only relocation | Fresh copy under a different absolute temporary path; all components built without the original workspace |
| Build closure | Current source-file sets, source hashes and output hashes reconciled |
| Dynamic dependencies | Local libraries use relocatable paths; remaining dependencies are system libraries |
| Original-workspace references | None in relocated sources, native artifact bytes or dynamic-library references; no source symlinks |
| Lightweight suite | 367 checks passed, including 160 unique shapes, 148/12 split, complete qualification schedules, measurement sessions and target coverage |
| Command inventory | 35 qualification, 34 calibration, 21 measurement targets; five offline analysis targets |
| Failure propagation | Python child exit 7 propagated; logs and process status retained; constructor failure reaped its child |
| Evidence handling | Missing, stale and tampered evidence rejected; exact prerequisite command checked |
| Real runner preflight | Two isolated failed attempts with missing qualification; neither launched a native worker |
| Configuration | Unknown keys, malformed values and inconsistent workload membership rejected |
| Overlap prevention | Second holder of the host lock rejected; lock identity is independent of output directory and TMPDIR |
| Cleanup | Marked build tree removed in a tooling fixture while results remained |
| Timer identity | Current host's 144-byte clock implementation matched the matrix guard allowlist; no timed backend call performed |
| Git | No repository initialized and no commits created |

The verified environment was Xcode 26.6 (17F113), Apple Clang 21.0.0, macOS SDK
26.5 and Python 3.9.6 on Apple arm64 macOS. Native build artifacts and build logs
were removed from the delivered source folder after verification. Build again
with `make build` in your chosen location.

The machine-readable [verification record](verification.json) includes native
artifact hashes and distinguishes all unexecuted scientific operations.

## Concrete limits

- Parallel families remain blocked under the default nested-worker ABI requirement;
  the available guard observes only the outer adapter call. The pending method
  choice is documented in [prerequisites](prerequisites.md).
- Seventeen INT32-input compute cells remain explicitly unsupported by SME MOPA.
- The first-party license requires the owner's choice; third-party licenses remain
  intact.
- Native execution is intentionally unverified. Actual qualification may uncover
  hardware, numerical, resource, clock or backend failures; the runners preserve
  them and return nonzero instead of substituting workloads.
