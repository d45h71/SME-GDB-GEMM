"""Matrix qualification, bounded batch calibration, and independently checked timing.

Importing this module does not load native libraries or launch workers. The outer
run manager owns prerequisite admission, the measurement lock, and run identity.
"""
from __future__ import annotations

import contextlib
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import selectors
import shlex
import struct
import subprocess
import time
from fractions import Fraction

from matrix_reference import Reference, expected_input_hashes, planned_combined_bytes, sha

CLOCK_SHA256 = "c615a84440f8ae7325e75d236ace6e038838f5cf634f9bba912d2904c99ff2fd"
COLUMN = (1, 6, 8)
BETA_ONE = (1, 5, 8)
LADDER = (1, 8, 64, 1024)
BACKENDS = {"stock", "first", "second", "accelerate", "openblas", "libxsmm", "kleidiai",
            "ablation_blocked", "ablation_grouped", "ablation_online"}


def write(path, value):
    with Path(path).open("x") as stream:
        json.dump(value, stream, indent=2, sort_keys=True, allow_nan=False)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())


def family_key(cell):
    return ":".join(str(cell[x]) for x in ("backend", "endpoint", "thread_policy", "regime"))


def input_bytes(endpoint):
    return 8 if endpoint in (2, 9) else 2 if endpoint in (3, 100) else 1 if endpoint in (4, 101) else 4


def witnesses(cells):
    """Select four distinct members without reference to measured performance."""
    size = input_bytes(cells[0]["endpoint"])
    cap = max(16 * 1024**2, 4 * 1024**2 * size)
    order = lambda c: (c["m"] * c["n"] * c["k"], c["shape_id"])
    bounded = lambda c: size * c["n"] * c["k"] <= cap
    candidates = {
        "smallest": cells,
        "active_row": [c for c in cells if c["m"] == 8 and bounded(c)],
        "k_block": [c for c in cells if c["k"] >= 4096 and bounded(c)],
        "adversarial": [c for c in cells if c["n"] >= 2048 and c["k"] >= 2048 and bounded(c)],
    }
    selected = {}
    used = set()
    for role, candidates_for_role in candidates.items():
        chosen = next((c for c in sorted(candidates_for_role, key=order) if c["shape_id"] not in used), None)
        if chosen is None:
            raise ValueError("workload population lacks required structured witness: " + role)
        selected[role] = chosen
        used.add(chosen["shape_id"])
    return selected


def groups(config):
    result = {}
    identifiers = set()
    for original in config["cells"]:
        cell = dict(original)
        if not isinstance(cell.get("id"), str) or cell["id"] in identifiers:
            raise ValueError("missing or duplicate cell id")
        identifiers.add(cell["id"])
        if cell.get("backend") not in BACKENDS or type(cell.get("endpoint")) is not int or cell["endpoint"] not in (*range(10), 100, 101):
            raise ValueError("unsupported backend or numerical endpoint")
        if cell.get("regime") not in ("ordinary", "prepared") or type(cell.get("thread_policy")) is not int or cell["thread_policy"] not in (0, 1):
            raise ValueError("invalid preparation regime or thread policy")
        if cell["regime"] == "prepared" and cell["backend"] != "libxsmm":
            raise ValueError("prepared entry point is only implemented for libxsmm")
        if any(type(cell[x]) is not int or cell[x] <= 0 for x in ("m", "n", "k")):
            raise ValueError("positive integer dimensions required")
        result.setdefault(family_key(cell), []).append(cell)
    if not result:
        raise ValueError("empty matrix configuration")
    for key, cells in result.items():
        if len(cells) != 160 or len({(c["m"], c["n"], c["k"]) for c in cells}) != 160:
            raise ValueError("every matrix family must cover the complete 160 unique triples: " + key)
    return result


def qualification_schedule(config):
    schedule = []
    for family, cells in groups(config).items():
        selected = witnesses(cells)
        for stride in range(3):
            schedule.append({**selected["smallest"], "kind": "stride_rejection", "short_index": stride})
        for cell in cells:
            for seed in (1701, 1702):
                for c0 in ("zero", "nonzero"):
                    schedule.append({**cell, "kind": "dense", "seed": seed, "pattern": 4, "c0_mode": c0,
                                     "retain_witness": cell["id"] == selected["smallest"]["id"] and seed == 1701})
        for role, cell in selected.items():
            for pattern in (0, 1, 2, 3, 5):
                for c0 in ("zero", "nonzero"):
                    schedule.append({**cell, "kind": "structured", "seed": 1703, "pattern": pattern,
                                     "c0_mode": c0, "witness_role": role,
                                     "retain_witness": pattern == {"smallest": 0, "active_row": 1, "k_block": 1, "adversarial": 3}[role]})
    for index, case in enumerate(schedule):
        case["case_id"] = f"case-{index:06d}"
    return schedule


def measurement_schedule(config):
    """Rotate backends within each shape and semantic block in every session."""
    families = groups(config)
    shape_order = [(c["m"], c["n"], c["k"]) for c in next(iter(families.values()))]
    blocks = {}
    for cell in config["cells"]:
        key = (cell["endpoint"], cell["thread_policy"], cell["regime"])
        blocks.setdefault(key, {}).setdefault((cell["m"], cell["n"], cell["k"]), []).append(cell)
    witness_ids = {cell["id"] for cells in families.values() for cell in witnesses(cells).values()}
    result = []
    for session in range(3):
        for shape_index, shape in enumerate(shape_order):
            for block in blocks.values():
                cells = block[shape]
                shift = (session + shape_index) % len(cells)
                for cell in cells[shift:] + cells[:shift]:
                    policy = cell.get("seed_policy", config.get("seed_policy"))
                    if policy is None:
                        if config.get("name") == "ablation":
                            policy = {"base": 21500, "session_stride": 0, "shape_stride": 1}
                        elif cell["endpoint"] in (2, 9):
                            policy = {"base": 21200, "session_stride": 0, "shape_stride": 0}
                        elif cell["endpoint"] in (3, 4, 100, 101):
                            policy = {"base": 21300, "session_stride": 0, "shape_stride": 1}
                        else:
                            policy = {"base": 21100, "session_stride": 1, "shape_stride": 0}
                    seed = policy["base"] + session * policy["session_stride"] + (shape_index + 1) * policy["shape_stride"]
                    result.append({**cell, "kind": "measurement", "case_id": f"session-{session}-cell-{len(result):06d}",
                                   "session": session, "shape_index": shape_index, "seed": seed, "seed_policy": policy,
                                   "pattern": 4, "c0_mode": "nonzero" if cell["endpoint"] in BETA_ONE else "zero",
                                   "retain_witness": session == 0 and cell["id"] in witness_ids})
    return result


class Worker:
    """A single owned process; bounded reads, no retries, durable command log."""
    def __init__(self, argv, env, directory, command_timeout, session_timeout):
        self.directory = directory
        directory.mkdir()
        self.stdout = (directory / "stdout.jsonl").open("xb")
        self.stderr = (directory / "stderr.txt").open("xb")
        self.commands = (directory / "commands.jsonl").open("x")
        self.buffer = b""
        self.timeout = command_timeout
        self.deadline = time.monotonic() + session_timeout
        self.process = None
        self.selector = selectors.DefaultSelector()
        self.meta = {"argv": list(map(str, argv)), "environment": env, "started_at": datetime.datetime.now(datetime.timezone.utc).isoformat()}
        try:
            self.process = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                            stderr=self.stderr, env=env, cwd=directory, bufsize=0)
            self.meta["pid"] = self.process.pid
            self.selector.register(self.process.stdout, selectors.EVENT_READ)
        except BaseException:
            self.close()
            raise

    def send(self, line):
        if time.monotonic() >= self.deadline:
            raise TimeoutError("matrix worker session deadline expired")
        self.commands.write(json.dumps({"command": line, "at_ns": time.time_ns()}) + "\n")
        self.commands.flush()
        self.process.stdin.write((line + "\n").encode())
        self.process.stdin.flush()

    def read(self, kind):
        deadline = min(self.deadline, time.monotonic() + self.timeout)
        while b"\n" not in self.buffer:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not self.selector.select(remaining):
                raise TimeoutError("matrix worker response deadline expired; outputs preserved")
            data = os.read(self.process.stdout.fileno(), 65536)
            if not data:
                raise RuntimeError(f"matrix worker exited before {kind}; inspect {self.directory / 'stderr.txt'}")
            self.stdout.write(data)
            self.stdout.flush()
            self.buffer += data
            if len(self.buffer) > 4 * 1024**2:
                raise ValueError("worker protocol line exceeds 4 MiB")
        line, self.buffer = self.buffer.split(b"\n", 1)
        def pairs(items):
            obj = {}
            for key, value in items:
                if key in obj:
                    raise ValueError("duplicate worker JSON key")
                obj[key] = value
            return obj
        value = json.loads(line, object_pairs_hook=pairs,
                           parse_constant=lambda _: (_ for _ in ()).throw(ValueError("nonfinite JSON")))
        if not isinstance(value, dict) or value.get("type") != kind:
            raise ValueError(f"worker response order: expected {kind}, got {value!r}")
        return value

    def stop(self):
        self.send("STOP")
        self.read("STOPPED")
        self.process.stdin.close()
        code = self.process.wait(timeout=5)
        if code != 0:
            raise RuntimeError(f"matrix worker exited with status {code}")
        # EOF is required, not just a STOPPED claim.
        tail = self.process.stdout.read()
        if self.buffer or tail:
            raise ValueError("unexpected trailing worker output")
        if (self.directory / "stderr.txt").stat().st_size:
            raise ValueError("matrix worker wrote stderr; inspect retained diagnostics")

    def close(self):
        process = self.process
        try:
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=2)
            self.meta["returncode"] = process.returncode if process is not None else None
            self.meta["ended_at"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
            write(self.directory / "process.json", self.meta)
        finally:
            self.selector.close()
            for stream in (self.stdout, self.stderr, self.commands,
                           getattr(process, "stdin", None), getattr(process, "stdout", None)):
                if stream is not None:
                    stream.close()


def check_return(record, directory, timed, expected=0):
    if record.get("returncode") != expected or record.get("guard_mask") != 0 or record.get("timed") is not timed:
        raise ValueError("backend return, ABI guard, or timing mode failed")
    if record["end"] < record["start"] or (not timed and record["end"] != record["start"]):
        raise ValueError("invalid timer interval")
    guard = Path(record["prefix"] + ".guard.bin")
    guard.resolve().relative_to(directory.resolve())
    payload = guard.read_bytes()
    if len(payload) != 304:
        raise ValueError("invalid ABI guard receipt length")
    fields = struct.unpack("<38Q", payload)
    if any(fields[i] for i in (0, 1, 2, 3, 6, 7, 8)) or fields[4] != fields[5] or fields[9] != 1:
        raise ValueError("independent ABI receipt validation failed")
    return {"path": str(guard.relative_to(directory)), "sha256": hashlib.sha256(payload).hexdigest()}


def check_jit(case, record, prefix, call_index=None):
    if case["backend"] != "libxsmm":
        if record is not None:
            raise ValueError("unexpected JIT observation")
        return
    if not isinstance(record, dict) or record.get("generated") != 1 or not (0 < record.get("code_bytes", 0) <= 131072):
        raise ValueError("missing generated SME JIT code evidence")
    code = Path(str(prefix) + ".code.bin")
    if code.stat().st_size != record["code_bytes"] or sha(code) != record["code_sha256"]:
        raise ValueError("generated JIT code identity mismatch")
    if record["generation_requests"] != 1 and case["regime"] == "prepared":
        raise ValueError("prepared regime regenerated its kernel")
    if call_index is not None:
        if case["regime"] == "prepared":
            if record["invocations"] != call_index + 1 or not record["runtime_address"]:
                raise ValueError("prepared JIT invocation or retained address mismatch")
        elif record["generation_requests"] != call_index + 1 or record["invocations"] != call_index + 1 or record["runtime_address"]:
            raise ValueError("ordinary JIT generation/invocation/lifetime mismatch")


def worker_env():
    # Do not inherit loader injection, library-selection, or ambient thread knobs.
    return {"PATH": os.environ.get("PATH", "/usr/bin:/bin"), "LC_ALL": "C", "LANG": "C",
            "OPENBLAS_NUM_THREADS": "1", "OMP_NUM_THREADS": "1", "OMP_DYNAMIC": "FALSE"}


def one_case(root, build, run, case, config, reference, phases):
    directory = run / case["case_id"]
    directory.mkdir()
    write(directory / "configuration.json", {"case": case, "phases": phases})
    allow_timing = any(p[1] for p in phases)
    library = (build / "lib" / ("lib" + case["backend"] + ".dylib")).resolve()
    argv = [str(build / "bin/matrix-worker"), str(library), case["backend"], str(directory / "raw"),
            str(int(allow_timing)), config.get("clock_sha256", CLOCK_SHA256)]
    worker = Worker(argv, worker_env(), directory / "process",
                    config.get("command_timeout_seconds", 600), config.get("case_timeout_seconds", 14400))
    calls = []
    inputs = []
    try:
        ready = worker.read("READY")
        if ready["backend"] != case["backend"] or Path(ready["run_image"]["image"]).resolve() != library:
            raise ValueError("worker loaded unexpected adapter")
        if ready["numer"] <= 0 or ready["denom"] <= 0:
            raise ValueError("invalid native timebase")
        write(directory / "ready.json", ready)
        e, m, n, k = (case[x] for x in ("endpoint", "m", "n", "k"))
        raw = directory / "raw" / case["case_id"]
        if case["kind"] == "stride_rejection":
            strides = [m, k, m] if e in COLUMN else [k, n, n]
            strides[case["short_index"]] -= 1
            worker.send(f"REJECT {case['case_id']} {e} {case['thread_policy']} {m} {n} {k} " + " ".join(map(str, strides)))
            returned = worker.read("RETURN")
            check_return(returned, directory, False, 1)
            rejected = worker.read("REJECTED")
            if rejected.get("context_null") is not True or rejected.get("run_invoked") is not False:
                raise ValueError("malformed-stride rejection violated create-only contract")
            worker.stop()
            result = {"status": "PASS", "case": case, "calls": [], "rejection": rejected}
            write(directory / "result.json", result)
            return result
        worker.send(f"BEGIN2 {case['case_id']} {e} {case['thread_policy']} {m} {n} {k} {case['seed']} {case['pattern']} {case['regime']} {case['c0_mode']}")
        check_return(worker.read("RETURN"), directory, False)
        created = worker.read("CASE_READY")
        if case["thread_policy"] == 0 and (created["requested_workers"] != 1 or created["thread_observation"] != 1):
            raise ValueError("serial backend worker/thread count is not exactly one")
        if case["thread_policy"] == 1:
            expected_policy = (4, 4) if case["backend"] in ("stock", "first", "second") else (0, 2) if case["backend"] == "accelerate" else None
            if expected_policy is None or (created["requested_workers"], created["thread_observation"]) != expected_policy:
                raise ValueError("parallel backend thread-policy observation mismatch")
        planned_combined_bytes(case, created["scratch_allowance_bytes"])
        inputs = [raw / name for name in ("A.bin", "B.bin", "C0.bin")]
        actual = [sha(path) for path in inputs]
        if actual != expected_input_hashes(case) or actual != [created[x] for x in ("a_sha256", "b_sha256", "c0_sha256")]:
            raise ValueError("independent serialized input recipe mismatch")
        write(directory / "inputs.json", {"sha256": actual, "worker": created})
        setup = None
        if case["regime"] == "prepared":
            setup_timed = case["kind"] == "measurement"
            worker.send("PREPARE 0 " + str(int(setup_timed)))
            setup_return = worker.read("RETURN")
            setup_guard = check_return(setup_return, directory, setup_timed)
            prepared = worker.read("PREPARED")
            check_jit(case, prepared["jit"], raw / "setup-0")
            setup_seconds = Fraction((setup_return["end"] - setup_return["start"]) * ready["numer"], ready["denom"] * 1_000_000_000)
            setup = {"timed": setup_timed, "seconds": float(setup_seconds),
                     "seconds_rational": [setup_seconds.numerator, setup_seconds.denominator],
                     "return": setup_return, "guard": setup_guard, "jit": prepared["jit"], "included_in_estimator": False}
            write(directory / "setup.json", setup)
        previous_output = None
        index = 0
        for role, timed, count in phases:
            ticks = 0
            observations = []
            for _ in range(count):
                worker.send(f"CALL {index} {int(timed)}")
                returned = worker.read("RETURN")
                guard = check_return(returned, directory, timed)
                output = worker.read("OUTPUT")
                if returned["index"] != index or output["index"] != index or not output["guards"] or not output["inputs_intact"]:
                    raise ValueError("call identity or buffer integrity mismatch")
                out = raw / f"call-{index}.output.bin"
                output_hash = sha(out)
                if output_hash != output["output_sha256"]:
                    raise ValueError("serialized output hash mismatch")
                if previous_output is not None and output_hash != previous_output:
                    raise ValueError("repeated immutable-input calls changed output; C0 reset or determinism failure")
                previous_output = output_hash
                check_jit(case, output["jit"], raw / f"call-{index}", index)
                reference.lib.matrix_reference_set_budget_ns(int(config.get("reference_timeout_seconds", 600) * 1e9))
                try:
                    numerical = reference.adjudicate({**case, "worker_live_bytes": created["own_buffer_bytes"] + created["scratch_allowance_bytes"]}, inputs, out)
                finally:
                    reference.lib.matrix_reference_clear_budget()
                delta = returned["end"] - returned["start"]
                observation = {"index": index, "role": role, "timed": timed, "ticks": delta,
                               "output_sha256": output_hash, "guard": guard, "jit": output["jit"], "numerical": numerical}
                write(directory / f"call-{index:05d}.json", observation)
                observations.append(observation)
                ticks += delta
                index += 1
            seconds = Fraction(ticks * ready["numer"], ready["denom"] * 1_000_000_000)
            calls.append({"role": role, "timed": timed, "count": count, "ticks": ticks,
                          "seconds": float(seconds), "seconds_rational": [seconds.numerator, seconds.denominator],
                          "observations": [x["index"] for x in observations]})
        worker.send("END")
        ended = worker.read("ENDED")
        if ended["calls"] != index:
            raise ValueError("worker call-count mismatch")
        worker.stop()
        retain = config.get("retain_arrays", False) or case.get("retain_witness", False)
        result = {"status": "PASS", "case": case, "calls": calls, "native_calls": index, "setup": setup,
                  "outputs": "independently adjudicated", "arrays_retained": retain}
        write(directory / "result.json", result)
        if not retain:
            # All checks and durable results precede removal. Failed cases retain
            # every array. Guard/JIT records, hashes, and oracle results stay.
            for path in [*inputs, *raw.glob("*.output.bin")]:
                path.unlink()
        return result
    finally:
        worker.close()


def execute(root: Path, build: Path, run: Path, config: dict, mode: str, calibration: dict | None = None):
    """Execute a full configured matrix family; all prerequisites are external."""
    root, build, run = root.resolve(), build.resolve(), run.resolve()
    if mode not in ("qualification", "calibration", "measurement"):
        raise ValueError("unknown matrix mode: " + mode)
    for key, default in (("command_timeout_seconds", 600), ("case_timeout_seconds", 14400), ("reference_timeout_seconds", 600)):
        value = config.get(key, default)
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or value <= 0:
            raise ValueError("finite positive timeout required: " + key)
    if config.get("reference_timeout_seconds", 600) > 3600:
        raise ValueError("reference_timeout_seconds exceeds oracle maximum of 3600")
    if type(config.get("retain_arrays", False)) is not bool:
        raise ValueError("retain_arrays must be a boolean")
    families = groups(config)
    registry = json.loads((root / "config/shapes.json").read_text())
    allowed = {(row["m"], row["n"], row["k"]): (row["id"], row["study"]) for row in registry}
    if len(allowed) != 160:
        raise ValueError("canonical shape registry must contain 160 unique triples")
    for cell in config["cells"]:
        if allowed.get((cell["m"], cell["n"], cell["k"])) != (cell["shape_id"], cell["study"]):
            raise ValueError("cell differs from canonical shape registry: " + cell["id"])
    if any(c["endpoint"] == 101 for c in config["cells"]):
        raise RuntimeError("native saturating INT8 qualification is blocked: the full required random workload has no admitted whole-output saturation oracle; do not substitute linear projection checks")
    for cell in config["cells"]:
        library = build / "lib" / ("lib" + cell["backend"] + ".dylib")
        if not library.is_file():
            component = "ablation" if cell["backend"].startswith("ablation_") else cell["backend"]
            raise FileNotFoundError(f"missing matrix backend {library}; run make build-{component} BUILD_DIR={shlex.quote(str(build))}")
    reference = Reference(build / "lib/libmatrix-reference.dylib")
    import ctypes
    reference.lib.matrix_reference_set_budget_ns.argtypes = [ctypes.c_uint64]
    reference.lib.matrix_reference_set_budget_ns.restype = None
    reference.lib.matrix_reference_clear_budget.argtypes = []
    reference.lib.matrix_reference_clear_budget.restype = None
    if mode == "qualification":
        schedule = qualification_schedule(config)
        write(run / "schedule.json", schedule)
        count = 0
        for case in schedule:
            phases = [] if case["kind"] == "stride_rejection" else [("qualification", False, 2 if case["endpoint"] in BETA_ONE else 1)]
            result = one_case(root, build, run, case, config, reference, phases)
            count += result.get("native_calls", 0)
        return {"status": "PASS", "mode": mode, "cells": len(config["cells"]), "cases": len(schedule), "native_calls": count,
                "family_keys": list(families), "timed_calls": 0,
                "abi_guard_scope": "outer adapter call; nested backend worker ABI unobserved",
                "thread_observation_scope": "configured policy or declared maximum tasks; physical placement unobserved"}
    if mode == "calibration":
        results = {}
        sample_index = 0
        for key, cells in families.items():
            cell = witnesses(cells)["smallest"]
            rungs = []
            for batch in ((1,) if cell["endpoint"] in COLUMN else LADDER):
                samples = []
                for repetition in range(3):
                    case = {**cell, "kind": "calibration", "case_id": f"sample-{sample_index:05d}", "seed": 1701,
                            "pattern": 4, "c0_mode": "nonzero" if cell["endpoint"] in BETA_ONE else "zero"}
                    phases = ([("first_use", False, 1)] if cell["regime"] == "prepared" else []) + [("calibration", True, batch)]
                    result = one_case(root, build, run, case, config, reference, phases)
                    samples.append(result["calls"][-1]["seconds_rational"])
                    sample_index += 1
                mean = sum(Fraction(*sample) for sample in samples) / 3
                rungs.append({"batch_calls": batch, "samples_seconds_rational": samples,
                              "mean_seconds_rational": [mean.numerator, mean.denominator], "meets_floor": mean >= Fraction(1, 50)})
            selected = next((rung["batch_calls"] for rung in rungs if rung["meets_floor"]), rungs[-1]["batch_calls"])
            results[key] = {"batch_calls": selected, "rungs": rungs, "calibration_cell": cell["id"]}
        return {"status": "PASS", "mode": mode, "families": results, "samples": sample_index}
    if calibration is None or calibration.get("status") != "PASS" or set(calibration.get("families", {})) != set(families):
        raise ValueError("measurement requires complete matching calibration evidence")
    for value in calibration["families"].values():
        if value.get("batch_calls") not in LADDER:
            raise ValueError("calibration selected unsupported batch length")
    session_records = []
    schedule = measurement_schedule(config)
    write(run / "schedule.json", schedule)
    for case in schedule:
        key = family_key(case)
        batch = calibration["families"][key]["batch_calls"]
        if case["endpoint"] in COLUMN and batch != 1:
            raise ValueError("column beta-one timing requires B=1")
        phases = ([("first_use", True, 1)] if case["regime"] == "prepared" else [])
        phases += [("warmup", True, 1)] * 2 + [("recorded", True, batch)] * 5
        result = one_case(root, build, run, case, config, reference, phases)
        observations = [Fraction(*call["seconds_rational"]) / batch for call in result["calls"] if call["role"] == "recorded"]
        mean = sum(observations, Fraction()) / len(observations)
        session_records.append({"session": case["session"], "cell": case["id"], "case": case["case_id"],
                                "seconds_per_call": [float(value) for value in observations], "mean_seconds": float(mean),
                                "mean_seconds_rational": [mean.numerator, mean.denominator],
                                "batch_calls": batch, "family": key, "setup": result["setup"],
                                "first_use_seconds": next((call["seconds"] for call in result["calls"] if call["role"] == "first_use"), None)})
    write(run / "sessions.json", session_records)
    summary = []
    for cell in config["cells"]:
        means = [Fraction(*row["mean_seconds_rational"]) for row in session_records if row["cell"] == cell["id"]]
        mean = sum(means, Fraction()) / len(means)
        summary.append({"cell": cell["id"], "session_means_seconds": list(map(float, means)),
                        "mean_seconds": float(mean), "mean_seconds_rational": [mean.numerator, mean.denominator],
                        "session_min_seconds": float(min(means)), "session_max_seconds": float(max(means))})
    write(run / "measurements.json", summary)
    return {"status": "PASS", "mode": mode, "cells": len(config["cells"]), "sessions": 3,
            "recorded_batches": len(session_records) * 5, "warmup_calls": len(session_records) * 2,
            "estimator": "arithmetic mean of five batch-normalized calls per session; equal-weight mean of three sessions with min/max",
            "abi_guard_scope": "outer adapter call; nested backend worker ABI unobserved"}
