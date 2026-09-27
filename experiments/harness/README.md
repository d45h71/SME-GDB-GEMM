# Harness qualification

Run from the project root:

```sh
make build-matrix
make qualify-harness
```

The build command compiles the matrix worker, exact numerical reference and test adapter. It executes no qualification. `qualify-harness` explicitly runs untimed method checks and is a prerequisite for matrix-backend qualification with the same source, build and configuration.

The complete population is the registry member `(M,N,K)=(1,256,1152)`. Twelve positive cases cover FP32 row/column, FP64, FP16-to-FP32, signed INT8-to-INT32 and BF16-to-FP32, each with zero/nonzero C0, seed 1701 and dense pattern 4. Every positive case calls the scalar test adapter twice, outside timing, and passes through the actual matrix worker and independent numerical reference. Six invalid leading dimensions are rejected before matrix calls. Seven planted faults test the rejection of an incorrect first output, incorrect last output, beta mismatch, layout mismatch, nonfinite output, mutated input and inconsistent workspace requirements. The expected error and relevant raw record must match; an unrelated exception fails qualification.

Separate protocol checks require rejection of truncated input, an unknown command and a nonmember descriptor. The nonmember check is metadata-only, before allocation or a matrix invocation. All positive and fault-test matrix calls use the same allowed registry member. The test adapter is available only to this method qualification and provides no experimental performance result or backend admission.

Options: `BUILD_DIR`, `OUTPUT_DIR`, and `CONFIG` have the same meanings as in the [root README](../../README.md). For example:

```sh
make qualify-harness BUILD_DIR=/absolute/path/build OUTPUT_DIR=/absolute/path/results CONFIG=config/runtime.json
```

Each invocation writes a unique output directory containing the full schedule, worker commands/stdout/stderr, serialized inputs/outputs, numerical checks, seven `expected-fault.json` records, protocol rejection records and terminal summary/status. Failed output is preserved. Success requires every positive case, invalid-stride check, planted fault and protocol check to produce its prescribed outcome. Native qualification has not been performed by project preparation.

License: [root LICENSE](../../LICENSE).
