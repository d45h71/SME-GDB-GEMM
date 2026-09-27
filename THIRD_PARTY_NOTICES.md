# Third-party implementation notices

These sources are redistributed under their original licenses. The root license placeholder applies only to project-owned additions and does not replace these licenses.

| Component | Source revision | License and attribution |
| --- | --- | --- |
| MPGEMM stock and derived First/Second kernels | Source identities recorded by the build manifest | [MIT](src/backends/stock/LICENSE), copyright 2026 ChenchengDeng / NUDT. Identical license texts are retained beside First and Second. |
| LIBXSMM | `55a8fa6a1e479dec1f5ddbe20684c1cdc0ff7eb1` | [BSD 3-Clause](third_party/libxsmm/LICENSE.md): Intel Corporation, Friedrich Schiller University Jena, Technische Universitaet Muenchen, Google Inc. |
| KleidiAI | `74b1a12d3620c89dae4766de640e064952000f4d` | Arm Limited and contributors; source SPDX annotations specify [Apache 2.0](third_party/kleidiai/LICENSES/Apache-2.0.txt) or [BSD 3-Clause](third_party/kleidiai/LICENSES/BSD-3-Clause.txt). |
| OpenBLAS | `e0166008be8e466242aa76b2ff75ce3f0fbf574a` (0.3.34) | [BSD 3-Clause](third_party/openblas/LICENSE); inherited GotoBLAS notice retained in [GotoBLAS_00License.txt](third_party/openblas/GotoBLAS_00License.txt). |
| Apple Accelerate | Supplied by the selected macOS SDK and operating system | Apple system framework; not redistributed. Installation and use remain subject to Apple's applicable terms. |

The source closures contain the implementation, headers, build definitions, and license notices required by the delivered backend configurations. They exclude upstream benchmark programs and results. The machine-readable revisions are in [third_party/dependencies.json](third_party/dependencies.json).

The LIBXSMM closure includes Darwin compatibility changes: include the pthread declaration for serial JIT write protection; retain packed descriptors and compile-time descriptor bounds for Apple ARM64 Clang; place compiler probes in the configured build temporary directory. Kernel generation behavior is otherwise retained. Stock-family ABI wrapper names and local adapter extension identifiers were renamed for standalone use; their numerical behavior is unchanged. The ablation sources derive from the licensed stock implementation and retain its attribution by reference here.
