# Numerical and measurement methods

## Workload and backend surface

`config/shapes.json` is the sole matrix authority. Every matrix backend/layout/
precision/thread/preparation family contains all 160 unique triples, retaining
the 148 Primary and 12 Extension populations. Matrix dimensions are never expanded
as a Cartesian product. Qualification regressions use members of this population.
Metadata-only invalid-descriptor tests reject before a matrix call.

FP32 serial families comprise seven ordinary row-major backends (Stock, Second,
First, Accelerate, LIBXSMM, KleidiAI, OpenBLAS), three ordinary column-major
backends (Stock, Accelerate, LIBXSMM), and prepared LIBXSMM row/column endpoints.
Parallel FP32 adds Stock and Accelerate in both layouts. FP64 has Stock and
Accelerate row-major serial/parallel endpoints. Mixed precision has Stock
FP16-to-FP32 and signed INT8-to-INT32 serial/parallel, plus native KleidiAI
BF16-to-FP32. KleidiAI's row FP32 route includes its source-defined format
conversion and packing; it is not relabeled as an identical FP32 arithmetic path.

Stock parallel calls request four Dispatch workers; the recorded value is a
declared maximum task count, not proof of four simultaneously active physical
cores. Accelerate parallel uses the library-managed threading policy. Serial
Accelerate explicitly selects the single-thread API; OpenBLAS is built serial.
No backend substitution or placement claim is made.

Ordinary row-major means alpha=1, beta=0, C=AB and includes mandatory allocation,
packing, conversion and zeroing in the native call. Native column-major means
alpha=1, beta=1, C=C0+AB. Immutable C0 is restored outside every timed call,
including warmups. Column timing uses one call per batch. Prepared LIBXSMM
separates timed setup and first use from the retained steady-call estimator;
JIT code, dispatch and invocation observations are retained.

## Qualification

Harness qualification tests the actual independent checker and worker protocol
using an isolated test adapter on an allowed member. It is a prerequisite for
research-backend qualification and does not qualify a research backend itself.

Each research family has 683 cases: 640 dense cases (160 members × seeds 1701/1702
× zero/nonzero C0), 40 structured cases (four predetermined member witnesses ×
patterns 0/1/2/3/5 × both C0 modes), and three short-stride rejection cases.
The structured seed is 1703. Beta-one calls repeat to check immutable-C0 reset.
No qualification timer is enabled for matrix cases.

The native independent reference executes in the controller process and does not
load a research backend. It uses exact binary-product accumulation, integer
accumulation and analytic structured outputs. Full exact dot evaluation applies
when M×N×(K+beta) ≤ 50,000,000. Larger dense cases use the two endpoints plus up
to 64 fixed SHA256-selected exact dots and sixteen whole-output sign projections.
Every output entry is scanned for nonfinites. Fixed public projection vectors
carry no unconditional floating-point false-accept probability guarantee.

Floating-point bounds include an outward-rounded gamma(2K+1) absolute-sum bound,
expected-value ULP and gradual-underflow term, specialized to the actual datatype.
Signed INT8 uses exact int64 reference accumulation and zero tolerance. Input
magnitudes and registry K values stay within the stated int32 accumulator bound.
No wider-than-double assumption is made about macOS long double. Native saturating
INT8 output is not an admitted workload family; its large-output whole-result
oracle is unavailable and cannot be replaced with linear projections.

Every call checks input hashes, guards, finite outputs, independent numerical
results and outer ABI/state records. Failure, timeout and malformed/truncated
worker protocol produce nonzero exits. Nested-worker guard scope is addressed in
[prerequisites](prerequisites.md).

## Calibration and matrix measurement

Matrix calibration uses each family's smallest predetermined registry member,
three repetitions at each batch rung 1, 8, 64, 1024 (column: 1). Select the first
rung whose arithmetic mean summed native time reaches 20 ms, otherwise the last
rung. The full ladder is retained. No fastest-kernel selection is performed.

Production has three sessions. Every exact cell/session starts a fresh worker,
performs two single-call warmups and five recorded batches. Timers use raw
`mach_absolute_time` ticks and `mach_timebase_info`. Python, logging, hashes,
reference checks and C0 restoration are outside the native timer. Retained batch
latency is the sum of individually timed native calls divided by batch length.
The cell estimator is the arithmetic mean of five latencies in each session,
then the equal-weight mean of the three session means. All session means and
their min/max remain available; there is no bootstrap or median substitution.

Backends rotate within each exact shape and endpoint/thread/preparation block
by session and registry position. FP32 input seeds are 21100+session (sessions
0–2); FP64 uses 21200; mixed uses 21300+one-based registry index; ablation uses
21500+one-based registry index. The full matched campaign preserves those orders.
No default smoke population or early-success subset exists.

## Compute microbenchmarks

These experiments are not GEMMs. The exact grid contains 123 cells: INT8, INT16,
INT32, FP16, BF16 and FP32 each have tiles 1–4 serial, and FP64 has tiles 1–8
serial; each format adds full-tile worker counts 2–14. Seventeen INT32-input
cells are explicitly unsupported because SME has no matching MOPA instruction.
Other instructions remain subject to actual host support and fail if unavailable.

Qualification checks rounds 1/2/3, accumulator witness bytes, completion counts
and vector length. Calibration retains all four rungs × three repetitions for
every serial tile count. Select the first arithmetic-mean duration reaching
20 ms, else 1024. Parallel cells use the corresponding full-tile serial selection.
Measurement uses fresh workers per exact cell/session, three sessions, two warmups
and five retained calls. The outer native timer includes Dispatch coordination.
Output witnesses are validated independently in Python.

Operations count multiply and add separately: 2×terms×lanes²×tiles×rounds×workers,
with terms 4 for INT8, 2 for INT16/FP16/BF16, and 1 for FP32/FP64. Compute rate is
the arithmetic mean of per-call operation rates, averaged equally across sessions.
For each datatype and worker count, the peak selector takes the maximum of the
predetermined tile configurations' arithmetic-mean rates, never a fastest call.

## Transfer and interference

Qualification exercises B/H/W/D elements × Z-register groups 1/2/4 at 2 KiB:
alignment, forced wrap, poisoned lanes, load checksum, tagged stores, guards,
planted mismatch rejection and one safe aligned heap/stack pair. The stack path
uses a 1 MiB stack with 64 KiB headroom; there is no large stack sweep.

Calibration uses three fixed loads: B/group1/2 KiB, W/group2/1 MiB and
D/group4/8 MiB. Every rung 1/8/64/1024 has one warmup and three retained calls.
The first rung whose median of the three W/group2/1 MiB raw times reaches 20 ms
is selected, else 1024. The four interference pilots must also pass. Fresh
calibration supplies both iterations and observed streaming vector length.

Measurement covers load/store × B/H/W/D × groups 1/2/4 × footprints 2^11 through
2^29 bytes: 456 cells, followed by four controls per session. A persistent PRNG
seeded 909001 shuffles the joint transfer list in each of three sessions. Each
cell has two warmups and five retained leaf calls. Allocation, deterministic fill,
reference checks, canaries and serialization remain outside the timer.

Controls are SME only, CPU same-buffer read-only, CPU disjoint-buffer read-only,
and synchronized CPU-write/SME-read handoff. Read-only overlap must be positive
in raw ticks; a failed overlap is retained and fails the run. No retry follows.
Handoff is synchronized, not an unsynchronized data race. Capacity, requested
bytes, unique touched bytes and register moves remain separate in raw records;
they do not establish physical DRAM traffic.

## Ablation and offline reductions

Ablation measures all 160 members for a LIBXSMM column baseline, blocked dual
packing, grouped loads, first-round online packing, and unchanged Stock control.
The variants form a cumulative design; they do not identify unique isolated causal
effects. See [variant details](../experiments/ablation/README.md).

Offline comparison uses matched shape/endpoint/thread/preparation/session cells.
Speedup is baseline latency divided by candidate latency. Population summaries
use equal-shape geometric means separately for Primary and Extension. Missing
required cells prevent complete measurement admission. Irregular and model
provenance views select existing registry measurements only. Peak efficiency is
a matched operation-rate ratio, not latency speedup. No existing figures, tables
or experimental observations are shipped with this project.
