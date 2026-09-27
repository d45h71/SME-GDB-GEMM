#!/usr/bin/env python3
"""Lightweight deterministic checks; no native worker, GEMM, warmup, or calibration."""
from __future__ import annotations
import ast
import contextlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from unittest import mock
from common import check_manifest, command, digest, file_manifest, machine_lock, read, sha, write
import run
from matrix import groups, measurement_schedule, qualification_schedule
from micro import ComputeWorker, compute_cells

ROOT = Path(__file__).resolve().parents[1]


def check():
    checks = []
    def ok(name, condition):
        if not condition:
            raise AssertionError(name)
        checks.append(name)
    data = run.inventory()
    shapes = read(ROOT / 'config/shapes.json')
    ok('registry exact count', len(shapes) == 160 and len({(x['m'], x['n'], x['k']) for x in shapes}) == 160)
    ok('registry populations', {s: sum(x['study'] == s for x in shapes) for s in ('Primary', 'Extension')} == {'Primary': 148, 'Extension': 12})
    ok('registry content identity', sha(ROOT / 'config/shapes.json') == data['population']['shape_sha256'])
    make = (ROOT / 'Makefile').read_text()
    defined = set(re.findall(r'^([a-z][a-z0-9-]*):', make, re.M))
    targets = set()
    totals = {}
    for item in data['experiments']:
        config = run.configuration(item, ROOT / 'config/runtime.json')
        for mode, target in item['targets'].items():
            ok('target ' + target, target in defined and target not in targets)
            targets.add(target)
            ok('target recipe ' + target, '\t@$(RUN) ' + mode + ' ' + item['name'] in make)
        ok('experiment documentation ' + item['name'], (ROOT / 'experiments' / item['name'] / 'README.md').is_file())
        if item['kind'] == 'matrix':
            families = groups(config)
            ok('matrix cells ' + item['name'], len(config['cells']) == item['cells'])
            if 'qualification' in item['modes']:
                schedule = qualification_schedule(config)
                ok('complete qualification ' + item['name'], len(schedule) == 683 * len(families))
                ok('unique qualification IDs ' + item['name'], len({r['case_id'] for r in schedule}) == len(schedule))
            if 'measurement' in item['modes']:
                schedule = measurement_schedule(config)
                ok('three sessions ' + item['name'], len(schedule) == 3 * item['cells'])
                ok('unique session IDs ' + item['name'], len({r['case_id'] for r in schedule}) == len(schedule))
                if not item.get('supplemental'):
                    totals[item['name']] = item['cells']
        if item['kind'] == 'compute':
            ok('compute grid ' + item['name'], len(compute_cells(config['format'])) == item['cells'])
    ok('production matrix inventory', totals == {'ablation': 800, 'fp32': 2560, 'fp64': 640, 'mixed': 800})
    ok('complete build interface', all('build-' + c in defined for c in ('matrix','stock','first','second','accelerate','libxsmm','kleidiai','openblas','compute','transfer','ablation')))
    for p in (ROOT / 'tools').glob('*.py'):
        ast.parse(p.read_text(), filename=str(p))
    ok('Python syntax', True)
    # Failures below are Python/tooling fixtures, never experimental evidence.
    with tempfile.TemporaryDirectory(prefix='mp5-tooling-validation-') as temp:
        base = Path(temp)
        with machine_lock(base / 'lock'):
            try:
                with machine_lock(base / 'lock'):
                    pass
                raise AssertionError('second lock unexpectedly acquired')
            except RuntimeError:
                ok('overlap rejection', True)
        try:
            command([sys.executable, '-B', '-c', 'import sys; print("preserved"); sys.exit(7)'], base / 'child', 5)
            raise AssertionError('child failure was accepted')
        except RuntimeError:
            ok('child exit propagation', read(base / 'child/command.json')['returncode'] == 7)
            ok('failed stdout preserved', 'preserved' in (base / 'child/stdout.txt').read_text())
        fake = base / 'python-protocol-fixture'
        fake.write_text('#!' + sys.executable + '\nimport sys,json\nfor line in sys.stdin:\n'
                        ' if line.strip()=="BEGIN": print(json.dumps({"type":"READY"}),flush=True)\n'
                        ' elif line.strip()=="STOP":\n  print(json.dumps({"type":"STOPPED"}),flush=True)\n  break\n')
        fake.chmod(0o700)
        worker = ComputeWorker(fake, base / 'protocol-positive', 5)
        worker.close(True)
        ok('protocol process argv and reap', read(base / 'protocol-positive/process.json')['reaped'] is True)
        fake.write_text('#!' + sys.executable + '\nraise SystemExit(7)\n')
        try:
            ComputeWorker(fake, base / 'protocol-negative', 5)
            raise AssertionError('failed READY accepted')
        except RuntimeError:
            process = read(base / 'protocol-negative/process.json')
            ok('constructor failure reaps child', process['reaped'] is True and process['returncode'] == 7)
        fixture = base / 'evidence'; fixture.mkdir()
        (fixture / 'leaf').write_text('valid')
        manifest = file_manifest(fixture)
        check_manifest(fixture, manifest)
        (fixture / 'leaf').write_text('tampered')
        try:
            check_manifest(fixture, manifest)
            raise AssertionError('tamper accepted')
        except RuntimeError:
            ok('tampered evidence rejection', True)
        bad = read(ROOT / 'config/runtime.json'); bad['unknown'] = True
        write(base / 'bad-config.json', bad)
        try:
            run.configuration(run.entry('fp32-row-stock'), base / 'bad-config.json')
            raise AssertionError('unknown configuration accepted')
        except ValueError:
            ok('unknown configuration rejection', True)
        with mock.patch.object(run, 'identity', return_value={'sha256': 'fresh'}):
            try:
                run.evidence('fp32-row-stock', 'qualification', base / 'build', base / 'empty-results', ROOT / 'config/runtime.json')
                raise AssertionError('absent evidence accepted')
            except RuntimeError as e:
                ok('missing evidence exact command', 'make qualify-fp32-row-stock BUILD_DIR=' in str(e))
            ev = base / 'empty-results/fp32-row-stock/qualification/test';ev.mkdir(parents=True)
            write(ev / 'summary.json', {'status':'PASS'})
            write(ev / 'outputs.json', file_manifest(ev))
            write(ev / 'status.json', {'status':'PASS','identity_sha256':'stale','outputs_sha256':sha(ev/'outputs.json')})
            try:
                run.evidence('fp32-row-stock', 'qualification', base / 'build', base / 'empty-results', ROOT / 'config/runtime.json')
                raise AssertionError('stale evidence accepted')
            except RuntimeError:
                ok('stale identity rejection', True)
        build, output = base/'generated', base/'results'; build.mkdir(); output.mkdir()
        write(build/'build-manifest.json', {'components':{}})
        (output/'keep.txt').write_text('results')
        env = {**os.environ, 'TMPDIR': str(base), 'PYTHONDONTWRITEBYTECODE':'1'}
        subprocess.run([sys.executable,'-B',str(ROOT/'tools/clean.py'),'--build-dir',str(build),'--output-dir',str(output)],env=env,check=True,stdout=subprocess.PIPE)
        ok('clean preserves results', not build.exists() and (output/'keep.txt').read_text()=='results')
    print(json.dumps({'status':'PASS','checks':len(checks),'native_workers_launched':0,
                      'matrix_measurement_cells':totals,'commands':len(targets)},indent=2))


if __name__ == '__main__':
    check()
