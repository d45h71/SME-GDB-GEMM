#!/usr/bin/env python3
"""Offline reductions of current, complete experiment outputs. Never runs workers."""
from __future__ import annotations
import argparse
import math
from pathlib import Path
import statistics
import sys
import uuid
from common import file_manifest, machine_lock, read, sha, utc, write
from run import ROOT, configuration, entry, evidence


def matrix_data(name, args):
    directory, summary, ident = evidence(name, 'measurement', args.build, args.output, args.runtime)
    config = configuration(entry(name), args.runtime)
    cells = {c['id']: c for c in config['cells']}
    rows = [{**cells[r['cell']], **r} for r in read(directory / 'measurements.json')]
    return rows, {'experiment': name, 'path': str(directory), 'identity': ident,
                  'outputs_sha256': sha(directory / 'outputs.json')}


def comparisons(rows):
    result = []
    for candidate in rows:
        controls = [r for r in rows if (r['shape_id'], r['endpoint'], r['thread_policy'], r['regime']) ==
                    (candidate['shape_id'], candidate['endpoint'], candidate['thread_policy'], candidate['regime'])]
        for control in controls:
            if control['backend'] == candidate['backend']:
                continue
            result.append({'shape_id': candidate['shape_id'], 'study': candidate['study'],
                           'endpoint': candidate['endpoint'], 'thread_policy': candidate['thread_policy'],
                           'regime': candidate['regime'], 'candidate': candidate['backend'], 'baseline': control['backend'],
                           'speedup': control['mean_seconds'] / candidate['mean_seconds'],
                           'session_speedups': [b / c for b, c in zip(control['session_means_seconds'], candidate['session_means_seconds'])],
                           'latency_ratio': candidate['mean_seconds'] / control['mean_seconds']})
    groups = {}
    for r in result:
        key = '|'.join(str(r[k]) for k in ('study', 'endpoint', 'thread_policy', 'regime', 'candidate', 'baseline'))
        groups.setdefault(key, []).append(r['speedup'])
    aggregates = [{'comparison': k, 'count': len(v), 'geometric_mean_speedup': math.exp(statistics.mean(map(math.log, v)))} for k, v in groups.items()]
    return {'ratios': result, 'population_aggregates': aggregates,
            'direction': 'baseline latency / candidate latency; Primary and Extension remain separate'}


def execute(args):
    args.build = Path(args.build_dir).resolve()
    args.output = Path(args.output_dir).resolve()
    args.runtime = Path(args.config).resolve()
    directory = args.output / ('analysis-' + args.kind) / (utc().replace(':', '') + '-' + uuid.uuid4().hex[:8])
    directory.mkdir(parents=True)
    status = {'status': 'RUNNING', 'started': utc(), 'command': [sys.executable, *sys.argv]}
    write(directory / 'status.json', status)
    try:
        with machine_lock():
            name = 'ablation' if args.kind == 'ablation' else 'fp64' if args.kind == 'irregular' else 'fp32'
            rows, origin = matrix_data(name, args)
            origins = [origin]
            if args.kind in ('comparisons', 'ablation'):
                result = comparisons(rows)
            elif args.kind == 'irregular':
                result = {'rows': [r for r in rows if not (r['m'] == r['n'] == r['k'])],
                          'selection': 'exact registry members with unequal dimensions; no new workloads'}
            elif args.kind == 'provenance':
                shapes = read(ROOT / 'config/shapes.json')
                result = {model: [r for r in rows if any(s['id'] == r['shape_id'] and s['study'] == 'Primary' and
                          model.lower() in ' '.join(s['models']).lower() for s in shapes)] for model in ('Qwen', 'SmolLM3')}
            else:
                mixed, source = matrix_data('mixed', args)
                origins.append(source)
                relevant = mixed + [r for r in rows if r['backend'] == 'stock' and r['endpoint'] == 0 and r['thread_policy'] == 1]
                peaks, result = {}, []
                for token in ('fp16', 'int8', 'bf16', 'fp32'):
                    path, summary, ident = evidence('compute-' + token, 'measurement', args.build, args.output, args.runtime)
                    peaks[token] = read(path / 'peaks.json')
                    origins.append({'experiment': 'compute-' + token, 'path': str(path), 'identity': ident})
                for r in relevant:
                    token = {0: 'fp32', 3: 'fp16', 4: 'int8', 100: 'bf16'}[r['endpoint']]
                    workers = 4 if r['thread_policy'] == 1 and r['backend'] == 'stock' else 1
                    rate = 2 * r['m'] * r['n'] * r['k'] / r['mean_seconds']
                    result.append({**r, 'operation_rate': rate, 'compute_peak_rate': peaks[token][str(workers)],
                                   'matched_peak_efficiency': rate / peaks[token][str(workers)],
                                   'format': token, 'requested_workers': workers})
            write(directory / 'inputs.json', origins)
            write(directory / 'analysis.json', result)
            write(directory / 'outputs.json', file_manifest(directory, ('status.json', 'outputs.json')))
            status.update(status='PASS', outputs_sha256=sha(directory / 'outputs.json'))
    except BaseException as e:
        status.update(status='FAILED', error=str(e))
        raise
    finally:
        status['ended'] = utc()
        write(directory / 'status.json', status)
        print('Analysis outputs:', directory)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('kind', choices=['comparisons', 'irregular', 'provenance', 'efficiency', 'ablation'])
    p.add_argument('--build-dir', default=str(ROOT / 'build'))
    p.add_argument('--output-dir', default=str(ROOT / 'results'))
    p.add_argument('--config', default=str(ROOT / 'config/runtime.json'))
    execute(p.parse_args())


if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        print('ERROR:', e, file=sys.stderr)
        sys.exit(1)
