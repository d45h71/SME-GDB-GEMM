"""Native SME compute and transfer experiments; no matrix substitute workloads."""
from __future__ import annotations
import contextlib
import os
from pathlib import Path
import random
import selectors
import signal
import statistics
import struct
import subprocess
import time
from common import append, command, loads, read, sha, utc, write

LADDER = [1, 8, 64, 1024]
FORMATS = {'int8': ('INT8-to-INT32', 4, 'INT32', 4),
           'int16': ('INT16-to-INT32', 4, 'INT32', 2),
           'int32': ('INT32-to-INT32', 4, 'INT32', None),
           'fp16': ('FP16-to-FP32', 4, 'FP32', 2),
           'bf16': ('BF16-to-FP32', 4, 'FP32', 2),
           'fp32': ('FP32-to-FP32', 4, 'FP32', 1),
           'fp64': ('FP64-to-FP64', 8, 'FP64', 1)}
CONTROLS = ['SME_ONLY', 'CPU_SHARED_READ_ONLY', 'CPU_DISJOINT_READ_ONLY',
            'CPU_WRITE_THEN_SME_READ_HANDOFF']


def compute_cells(token):
    _, tiles, _, _ = FORMATS[token]
    return [{'id': '%s-t%d-w%d' % (token, t, w), 'tiles': t, 'threads': w, 'format': token}
            for t, w in ([(t, 1) for t in range(1, tiles + 1)] + [(tiles, w) for w in range(2, 15)])]


class ComputeWorker:
    def __init__(self, binary, directory, timeout):
        self.directory, self.timeout = Path(directory), timeout
        self.directory.mkdir()
        self.output = self.directory / 'output'
        self.output.mkdir()
        self.err = (self.directory / 'stderr.txt').open('xb')
        self.argv = [str(binary), '--output-dir', str(self.output)]
        self.started = utc()
        self.proc = subprocess.Popen(self.argv, stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=self.err, start_new_session=True)
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.proc.stdout, selectors.EVENT_READ)
        self.buffer = b''
        try:
            self.ready = self.exchange('BEGIN', 'READY')
        except BaseException:
            self.close(False)
            raise

    def exchange(self, text, expected):
        append(self.directory / 'commands.jsonl', {'command': text})
        self.proc.stdin.write((text + '\n').encode())
        self.proc.stdin.flush()
        deadline = time.monotonic() + self.timeout
        while b'\n' not in self.buffer:
            left = deadline - time.monotonic()
            if left <= 0 or not self.selector.select(left):
                raise TimeoutError('compute command timeout: ' + text)
            chunk = os.read(self.proc.stdout.fileno(), 65536)
            if not chunk:
                raise RuntimeError('compute worker exited; see ' + str(self.directory / 'stderr.txt'))
            self.buffer += chunk
            if len(self.buffer) > 1048576:
                raise RuntimeError('compute protocol response too large')
        line, self.buffer = self.buffer.split(b'\n', 1)
        with (self.directory / 'stdout.jsonl').open('ab') as f:
            f.write(line + b'\n')
        response = loads(line)
        if response.get('type') != expected:
            raise RuntimeError('compute response mismatch: ' + repr(response))
        return response

    def close(self, clean):
        error = None
        try:
            if clean:
                self.exchange('STOP', 'STOPPED')
                self.proc.stdin.close()
                if self.proc.wait(timeout=10):
                    raise RuntimeError('compute worker nonzero exit')
                trailing = self.buffer + self.proc.stdout.read()
                if trailing or (self.directory / 'stderr.txt').stat().st_size:
                    raise RuntimeError('compute protocol has trailing output or stderr')
        except BaseException as e:
            error = str(e)
            raise
        finally:
            if self.proc.poll() is None:
                os.killpg(self.proc.pid, signal.SIGKILL)
                self.proc.wait()
            self.selector.close()
            self.err.close()
            with contextlib.suppress(OSError):
                self.proc.stdin.close()
            with contextlib.suppress(OSError):
                self.proc.stdout.close()
            write(self.directory / 'process.json', {'argv': self.argv, 'started': self.started,
                  'ended': utc(), 'returncode': self.proc.returncode, 'reaped': True,
                  'clean_stop_requested': clean, 'cleanup_error': error})


def check_compute(worker, response, cell, rounds, token):
    label, _, kind, terms = FORMATS[cell['format']]
    svl = response.get('svl_bytes', 0)
    if not isinstance(svl, int) or svl < 16 or svl > 2048 or svl % 8:
        raise RuntimeError('invalid observed streaming vector length')
    if svl != worker.ready.get('svl_bytes') or any(response.get(k) != worker.ready.get(k) for k in ('timebase_numer', 'timebase_denom')):
        raise RuntimeError('compute runtime vector length or timebase changed')
    expected = {'token': token, 'output_kind': kind, 'lanes_per_tile': svl // (8 if kind == 'FP64' else 4), 'format': label, 'tiles': cell['tiles'], 'threads': cell['threads'],
                'mopa_rounds_per_leaf': rounds, 'leaf_invocations': cell['threads'],
                'completed_leaf_invocations': cell['threads'], 'total_mopa_loop_trips': cell['threads'] * rounds,
                'terms_per_accumulator': terms, 'expected_accumulator_units': rounds * terms,
                'outputs_equal_expected_units': True, 'output_bytes': cell['threads'] * cell['tiles'] * svl}
    for k, v in expected.items():
        if response.get(k) != v:
            raise RuntimeError('compute count/output mismatch: ' + k)
    if response.get('output_file') != 'output-' + token + '.bin':
        raise RuntimeError('unsafe or wrong compute output name')
    path = worker.output / response['output_file']
    raw = path.read_bytes()
    value = rounds * terms
    element = struct.pack('<d', value) if kind == 'FP64' else struct.pack('<i' if kind == 'INT32' else '<f', value)
    if len(raw) != expected['output_bytes'] or raw != element * (len(raw) // len(element)):
        raise RuntimeError('independent compute accumulator check failed')
    ticks = response['end_ticks'] - response['start_ticks']
    if ticks < 0 or response['timebase_numer'] <= 0 or response['timebase_denom'] <= 0:
        raise RuntimeError('invalid compute clock')
    seconds = ticks * response['timebase_numer'] / response['timebase_denom'] / 1e9
    lanes = svl // (8 if kind == 'FP64' else 4)
    operations = 2 * terms * lanes * lanes * rounds * cell['tiles'] * cell['threads']
    return {'cell': cell, 'response': response, 'seconds': seconds, 'operations': operations,
            'output_sha256': sha(path), 'independent_check': 'PASS'}


def compute(root, build, run, config, mode, calibration=None):
    token = config['format']
    if token == 'int32':
        write(run / 'unsupported.json', {'cells': compute_cells(token), 'status': 'UNSUPPORTED',
              'reason': 'SME MOPA has no INT32-input to INT32-accumulator instruction; no substitute kernel.'})
        raise RuntimeError('UNSUPPORTED: INT32-input SME MOPA (17 required cells). See unsupported.json')
    cells = compute_cells(token)
    records, selection = [], {}
    sessions = [0] if mode != 'measurement' else [1, 2, 3]
    for session in sessions:
        batches = [[cell] for cell in cells] if mode == 'measurement' else [cells]
        for cell_group in batches:
            worker = None
            clean = False
            try:
                suffix = ('-' + cell_group[0]['id']) if mode == 'measurement' else ''
                worker = ComputeWorker(build / 'bin/compute-worker', run / ('session-' + str(session) + suffix), config['command_timeout_seconds'])
                if worker.ready.get('worker_protocol') != 'SME-COMPUTE-2':
                    raise RuntimeError('wrong compute worker protocol')
                for cell in cell_group:
                    if mode == 'calibration' and cell['threads'] != 1:
                        continue
                    if mode == 'qualification':
                        work = [(r, 'qualification', r) for r in [1, 2, 3]]
                    elif mode == 'calibration':
                        work = [(r, 'calibration', rep) for r in LADDER for rep in [1, 2, 3]]
                    else:
                        key = '%s-t%d-w1' % (token, cell['tiles'])
                        rounds = calibration['selection'][key]
                        work = [(rounds, 'warmup' if rep < 2 else 'recorded', rep) for rep in range(7)]
                    for rounds, disposition, rep in work:
                        ident = 'r%d' % len(records)
                        phase = {'qualification': 'QUALIFY', 'calibration': 'CALIBRATE', 'measurement': 'MEASURE'}[mode]
                        response = worker.exchange('RUN %s %s %d %d %d %s' %
                                                   (ident, FORMATS[token][0], cell['tiles'], cell['threads'], rounds, phase), 'RESULT')
                        if response.get('phase') != phase:
                            raise RuntimeError('compute phase mismatch')
                        record = check_compute(worker, response, cell, rounds, ident)
                        record.update(session=session, disposition=disposition, repetition=rep)
                        records.append(record)
                        append(run / 'raw.jsonl', record)
                clean = True
            finally:
                if worker is not None:
                    worker.close(clean)
    if mode == 'calibration':
        for cell in cells:
            if cell['threads'] != 1:
                continue
            selection[cell['id']] = next((r for r in LADDER if statistics.mean(
                x['seconds'] for x in records if x['cell']['id'] == cell['id'] and
                x['response']['mopa_rounds_per_leaf'] == r) >= .02), LADDER[-1])
    rates = []
    if mode == 'measurement':
        for cell in cells:
            session_means = []
            for session in [1, 2, 3]:
                retained = [r for r in records if r['cell'] == cell and r['session'] == session and r['disposition'] == 'recorded']
                if len(retained) != 5 or any(r['seconds'] <= 0 for r in retained):
                    raise RuntimeError('compute retained-count/clock mismatch')
                session_means.append(statistics.mean(r['operations'] / r['seconds'] for r in retained))
            rates.append({**cell, 'session_mean_operations_per_second': session_means,
                          'operations_per_second': statistics.mean(session_means)})
        write(run / 'rates.json', rates)
        write(run / 'peaks.json', {str(w): max(r['operations_per_second'] for r in rates if r['threads'] == w)
                                  for w in range(1, 15)})
    return {'status': 'PASS', 'cell_count': len(cells), 'records': len(records), 'selection': selection,
            'matrix_invocations': 0, 'rates': rates}


def transfer(root, build, run, config, mode, calibration=None):
    timeout = config['command_timeout_seconds']
    if mode in ('qualification', 'calibration'):
        flag = '--qualification' if mode == 'qualification' else '--pilot'
        response = loads(command([build / 'bin/transfer-qualification', flag], run / 'native', timeout))
        write(run / 'native.json', response)
        if response.get('status') != 'PASS':
            raise RuntimeError('transfer ' + mode + ' failed')
        return {'status': 'PASS', 'native': response, 'matrix_invocations': 0}
    pilot = calibration['native']
    iterations = pilot['future_production_freeze']['fixed_inner_iterations']
    svl = pilot['vector_length_bytes']
    if iterations not in LADDER:
        raise RuntimeError('transfer calibration selected an invalid rung')
    all_cells = [{'kind': kind, 'width': w, 'group': g, 'footprint': 2 ** e}
                 for kind in ['load', 'store'] for w in 'BHWD' for g in [1, 2, 4] for e in range(11, 30)]
    summary = []
    rng = random.Random(909001)
    for session in [1, 2, 3]:
        ordered = list(all_cells)
        rng.shuffle(ordered)
        ordered += [{'kind': 'interference', 'control': control} for control in CONTROLS]
        for i, cell in enumerate(ordered):
            kind = cell['kind']
            argv = [build / 'bin/transfer-worker']
            if kind == 'interference':
                argv += ['--interference-session', '--control', cell['control']]
            else:
                argv += ['--production-session', '--kind', kind, '--width', cell['width'],
                         '--group', str(cell['group']), '--footprint-bytes', str(cell['footprint'])]
            argv += ['--session', str(session), '--iterations', str(iterations), '--seed', '909001',
                     '--vector-length-bytes', str(svl)]
            response = loads(command(argv, run / ('session-%d-cell-%03d' % (session, i)), timeout))
            append(run / 'raw.jsonl', response)
            if response.get('status') != 'PASS' or response.get('recorded_leaf_record_count') != 5 or response.get('warmup_leaf_record_count') != 2:
                raise RuntimeError('transfer validation/overlap failed; output preserved')
            retained = [r for r in response['records'] if r['phase'] == 'recorded']
            if len(retained) != 5 or response.get('session') != session or response.get('runtime_vector_length_bytes') != svl:
                raise RuntimeError('transfer retained count or identity mismatch')
            summary.append({'cell': cell, 'session': session, 'records': retained})
    write(run / 'sessions.json', summary)
    return {'status': 'PASS', 'cell_count': 460, 'session_count': 3, 'matrix_invocations': 0}
