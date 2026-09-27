"""Explicit, untimed validation of numerical rejection and worker protocol checks.

The scalar test backend is used only by this qualification. It is never a
performance backend and its outputs never qualify a research implementation.
"""
from __future__ import annotations

import ctypes
from pathlib import Path
from unittest.mock import patch

from common import read, sha, write
import matrix
from matrix_reference import Reference

FAULTS = ('entry', 'tail', 'beta', 'layout', 'nonfinite', 'input', 'workspace')


def transcript(directory):
    path = directory / 'process' / 'stdout.jsonl'
    if not path.exists():
        return []
    from common import loads
    return [loads(line) for line in path.read_text().splitlines() if line]


def expected_failure(fault, error, directory):
    """Recognize the intended failure; unrelated launch/oracle errors never pass."""
    text = str(error)
    output = transcript(directory)
    if fault in ('entry', 'tail', 'beta', 'layout'):
        return (isinstance(error, ValueError) and 'numerical mismatch at ' in text
                and any(x.get('type') == 'OUTPUT' and x.get('inputs_intact') is True
                        and x.get('guards') is True for x in output))
    if fault == 'nonfinite':
        return isinstance(error, ValueError) and text == 'nonfinite output'
    if fault == 'input':
        return (isinstance(error, ValueError) and text == 'call identity or buffer integrity mismatch'
                and any(x.get('type') == 'OUTPUT' and x.get('inputs_intact') is False
                        and x.get('guards') is True for x in output))
    if fault == 'workspace':
        stderr = directory / 'process' / 'stderr.txt'
        process = directory / 'process' / 'process.json'
        return (stderr.is_file() and process.is_file()
                and read(process).get('returncode') == 70
                and 'OBSERVER_OR_BACKEND_FAILURE: workspace requirement mismatch' in stderr.read_text()
                and not any(x.get('type') in ('CASE_READY', 'OUTPUT') for x in output))
    return False


def protocol_rejection(build, run, config, label, line, expected_text):
    directory = run / label
    directory.mkdir()
    worker = matrix.Worker(
        [str(build / 'bin/matrix-worker'), str(build / 'lib/libharness-test.dylib'),
         'harness-test', str(directory / 'raw'), '0', config.get('clock_sha256', matrix.CLOCK_SHA256)],
        matrix.worker_env(), directory / 'process', config['command_timeout_seconds'],
        config['command_timeout_seconds'])
    caught = None
    try:
        ready = worker.read('READY')
        if ready.get('backend') != 'harness-test':
            raise ValueError('unexpected harness protocol backend')
        write(directory / 'ready.json', ready)
        if line is not None:
            worker.send(line)
        worker.process.stdin.close()
        try:
            worker.read('UNREACHABLE_SUCCESS')
        except RuntimeError as error:
            caught = error
        if caught is None:
            raise RuntimeError('invalid protocol was accepted')
        code = worker.process.wait(timeout=10)
        error_text = (directory / 'process/stderr.txt').read_text()
        if code != 70 or ('OBSERVER_OR_BACKEND_FAILURE: ' + expected_text) not in error_text:
            raise RuntimeError('wrong protocol rejection: ' + error_text)
        if any(x.get('type') in ('RETURN', 'CASE_READY', 'OUTPUT') for x in transcript(directory)):
            raise RuntimeError('protocol rejection occurred after backend work')
    finally:
        worker.close()
    result = {'status': 'PASS_EXPECTED_REJECTION', 'test': label, 'expected_error': expected_text,
              'returncode': code, 'native_matrix_calls': 0,
              'stderr_sha256': sha(directory / 'process/stderr.txt')}
    write(directory / 'result.json', result)
    return result


def execute(root, build, run, config, mode, calibration=None):
    if mode != 'qualification':
        raise ValueError('harness checks support qualification only')
    if config.get('positive_endpoints') != [0, 1, 2, 3, 4, 100]:
        raise ValueError('harness qualification must retain all six precision/layout branches')
    if tuple(config.get('faults', ())) != FAULTS:
        raise ValueError('harness qualification must retain all seven fault checks')
    root, build, run = Path(root).resolve(), Path(build).resolve(), Path(run).resolve()
    rows = read(root / 'config/shapes.json')
    member = min(rows, key=lambda x: (x['m'] * x['n'] * x['k'], x['id']))
    if (member['m'], member['n'], member['k']) != (1, 256, 1152):
        raise ValueError('harness member disagrees with frozen workload population')
    reference = Reference(build / 'lib/libmatrix-reference.dylib')
    reference.lib.matrix_reference_set_budget_ns.argtypes = [ctypes.c_uint64]
    reference.lib.matrix_reference_set_budget_ns.restype = None
    reference.lib.matrix_reference_clear_budget.argtypes = []
    reference.lib.matrix_reference_clear_budget.restype = None
    options = {**config, 'retain_arrays': True}
    base = {'id': 'harness-member', 'backend': 'harness-test', 'endpoint': 0,
            'thread_policy': 0, 'regime': 'ordinary', 'm': member['m'], 'n': member['n'],
            'k': member['k'], 'shape_id': member['id'], 'study': member['study'],
            'kind': 'dense', 'seed': 1701, 'pattern': 4, 'c0_mode': 'nonzero'}
    schedule = []
    for endpoint in config.get('positive_endpoints', [0, 1]):
        for c0 in ('zero', 'nonzero'):
            schedule.append({**base, 'endpoint': endpoint, 'c0_mode': c0,
                             'case_id': f'positive-e{endpoint}-{c0}'})
    for endpoint in (0, 1):
        for stride in range(3):
            schedule.append({**base, 'endpoint': endpoint, 'kind': 'stride_rejection',
                             'short_index': stride, 'case_id': f'stride-e{endpoint}-{stride}'})
    write(run / 'schedule.json', {'positive_cases': schedule, 'faults': list(FAULTS),
          'protocol_checks': ['truncated', 'unknown-command', 'nonmember-metadata'],
          'scope': 'test harness only; no research-backend qualification or performance evidence'})
    positive = []
    for case in schedule:
        phases = [] if case['kind'] == 'stride_rejection' else [('qualification', False, 2)]
        result = matrix.one_case(root, build, run, case, options, reference, phases)
        positive.append({'case': case['case_id'], 'native_calls': result.get('native_calls', 0),
                         'status': result['status']})
    faults = []
    original_environment = matrix.worker_env
    for fault in FAULTS:
        case = {**base, 'case_id': 'fault-' + fault, 'endpoint': 1 if fault == 'beta' else 0}
        caught = None
        with patch.object(matrix, 'worker_env', lambda: {**original_environment(), 'MP5_TEST_FAULT': fault}):
            try:
                matrix.one_case(root, build, run, case, options, reference, [('qualification', False, 1)])
            except Exception as error:
                caught = error
        if caught is None:
            raise RuntimeError('planted ' + fault + ' fault was not rejected')
        directory = run / case['case_id']
        if not expected_failure(fault, caught, directory):
            raise RuntimeError('planted ' + fault + ' failed for an unrelated reason: ' + repr(caught)) from caught
        result = {'fault': fault, 'status': 'PASS_EXPECTED_FAULT_DETECTED', 'error': str(caught),
                  'research_qualification': False}
        write(directory / 'expected-fault.json', result)
        faults.append(result)
    protocol = [
        protocol_rejection(build, run, config, 'truncated', None, 'truncated protocol: missing STOP'),
        protocol_rejection(build, run, config, 'unknown-command', 'INVALID', 'unknown command'),
        protocol_rejection(build, run, config, 'nonmember-metadata',
                           'BEGIN2 rejected 0 0 1 1 1 1701 4 ordinary zero', 'MATRIX_NONMEMBER'),
    ]
    return {'status': 'PASS', 'positive_cases': positive, 'faults': faults, 'protocol_checks': protocol,
            'timed_calls': 0, 'research_backends_executed': [],
            'scope': 'untimed member-based method qualification; not experiment results'}
