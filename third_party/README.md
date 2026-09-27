# Vendored build dependencies

Required implementation sources are already included; setup does not download code or run experiments. Exact upstream revisions, license choices, and local compatibility changes are declared in `dependencies.json` and [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

Run `make setup` to check the Apple arm64 macOS/Xcode build tools and Python 3.9 or later. Run `make build` to compile all components. Configure `CC`, `CXX`, `SDKROOT`, `PYTHON`, and `BUILD_DIR` as documented in the root README. The tested compiler is Apple Clang 21.0.0 with macOS SDK 26.5; compiler bytes, version, SDK version, commands, source files, and outputs are recorded for each build.

OpenBLAS uses a fixed VORTEXM4 serial FP32/FP64 build with LAPACK and Fortran disabled. LIBXSMM uses a serial static host library with JIT enabled, deregistration enabled, and dispatch cache disabled; JIT-generated kernels still belong to the measured adapter endpoint. KleidiAI compiles only the selected FP32, BF16, and requantized INT8 SME packers/kernels and their required headers. These settings are encoded in `tools/build.py`.

The sources in this directory are governed by their individual license notices, not the project's first-party license placeholder.
