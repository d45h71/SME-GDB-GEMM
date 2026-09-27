#!/usr/bin/env python3
"""Thin experiment entry point: validate current evidence, run once, keep failures."""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import shlex
import sys
import traceback
import uuid
from common import check_manifest, digest, environment, file_manifest, machine_lock, read, sha, utc, write

ROOT = Path(__file__).resolve().parents[1]
PREFIX = {'qualification': 'qualify', 'calibration': 'calibrate', 'measurement': 'measure'}


def inventory():
    return read(ROOT / 'config/inventory.json')


def entry(name):
    found = [x for x in inventory()['experiments'] if x['name'] == name]
    if len(found) != 1:
        raise ValueError('unknown experiment: ' + name)
    return found[0]


def configuration(item, runtime_path):
    runtime = read(runtime_path)
    if set(runtime) - {'command_timeout_seconds', 'reference_timeout_seconds', 'parallel_guard_scope', 'retain_arrays', 'notes'}:
        raise ValueError('unknown runtime configuration key')
    for key in ('command_timeout_seconds', 'reference_timeout_seconds'):
        if type(runtime.get(key)) is not int or not 1 <= runtime[key] <= 86400:
            raise ValueError(key + ' must be an integer from 1 to 86400')
    if runtime.get('parallel_guard_scope') not in ('nested-required', 'outer-call'):
        raise ValueError('parallel_guard_scope must be nested-required or outer-call')
    if 'retain_arrays' in runtime and type(runtime['retain_arrays']) is not bool:
        raise ValueError('retain_arrays must be boolean')
    config = {**read(ROOT / item['config']), **runtime}
    if config.get('name') != item['name']:
        raise ValueError('configuration name does not match the inventory')
    if item['kind'] == 'compute' and item['name'] != 'compute-' + str(config.get('format')):
        raise ValueError('compute datatype does not match the inventory')
    if item['kind'] == 'transfer' and config.get('kind') != ('all' if item['name'] == 'transfer-all' else 'qualification'):
        raise ValueError('transfer configuration does not match the inventory')
    if item['kind'] == 'matrix':
        from matrix import groups
        registry = read(ROOT / 'config/shapes.json')
        expected = {(r['m'], r['n'], r['k'], r['id'], r['study']) for r in registry}
        for cells in groups(config).values():
            if {(c['m'], c['n'], c['k'], c['shape_id'], c['study']) for c in cells} != expected:
                raise ValueError('matrix configuration must preserve the exact 160-member registry')
        if len(config['cells']) != item['cells']:
            raise ValueError('inventory/configuration cell-count mismatch')
    return config


def identity(item, config, build):
    import build as native_build
    try:
        manifest = read(build / 'build-manifest.json')
    except (OSError, ValueError) as e:
        raise RuntimeError('Build manifest missing or invalid. Run exactly:\nmake build BUILD_DIR=' + shlex.quote(str(build))) from e
    components = {}
    for name in item['components']:
        component = manifest['components'].get(name, {})
        if component.get('status') != 'PASS':
            raise RuntimeError('build missing or failed; run make build-' + name + ' BUILD_DIR=' + shlex.quote(str(build)))
        # Enumerate the input closure without instantiating Builder (which writes).
        paths = native_build.Builder.source_paths(None, name)
        if native_build.files_under(paths) != component['source_files']:
            raise RuntimeError('Build source set changed. Run exactly:\nmake build-' + name + ' BUILD_DIR=' + shlex.quote(str(build)))
        try:
            check_manifest(ROOT, component['source_files'])
            check_manifest(build, component['output_files'])
        except RuntimeError as e:
            raise RuntimeError(str(e) + '\nRebuild exactly:\nmake build-' + name + ' BUILD_DIR=' + shlex.quote(str(build))) from e
        components[name] = {'source_files': component['source_files'], 'output_files': component['output_files'],
                            'toolchain': component['toolchain']}
    scripts = {str(p.relative_to(ROOT)): sha(p) for p in sorted((ROOT / 'tools').glob('*.py'))}
    value = {'components': components, 'config': config, 'scripts': scripts,
             'registry': sha(ROOT / 'config/shapes.json'), 'environment': environment()}
    return {'sha256': digest(value), 'details': value}


def prerequisite_command(name, mode, build, output, runtime):
    target = entry(name)['targets'][mode]
    return 'make %s BUILD_DIR=%s OUTPUT_DIR=%s CONFIG=%s' % tuple(
        [target] + [shlex.quote(str(x)) for x in (build, output, runtime)])


def evidence(name, mode, build, output, runtime):
    item = entry(name)
    config = configuration(item, runtime)
    current = identity(item, config, build)
    candidates = sorted((output / name / mode).glob('*/status.json'), reverse=True)
    for status_path in candidates:
        try:
            status = read(status_path)
            if status.get('status') != 'PASS' or status.get('identity_sha256') != current['sha256']:
                continue
            directory = status_path.parent
            contents = read(directory / 'outputs.json')
            if sha(directory / 'outputs.json') != status['outputs_sha256']:
                continue
            check_manifest(directory, contents)
            return directory, read(directory / 'summary.json'), current['sha256']
        except (OSError, ValueError, KeyError, RuntimeError):
            continue
    raise RuntimeError('Missing or stale %s evidence for %s. Run exactly:\n%s' %
                       (mode, name, prerequisite_command(name, mode, build, output, runtime)))


def execute(args):
    build = Path(args.build_dir).expanduser().resolve()
    output = Path(args.output_dir).expanduser().resolve()
    runtime = Path(args.config).expanduser().resolve()
    if output == build or output.is_relative_to(build) or build.is_relative_to(output):
        raise ValueError('BUILD_DIR and OUTPUT_DIR must be separate directories; clean must preserve results')
    item = entry(args.experiment)
    if args.mode not in item['modes']:
        raise ValueError('this experiment does not define mode ' + args.mode)
    if args.plan:
        config = configuration(item, runtime)
        print(__import__('json').dumps({'experiment': item, 'config': config}, indent=2))
        return
    run = output / item['name'] / args.mode / (utc().replace(':', '').replace('+', '-') + '-' + uuid.uuid4().hex[:8])
    run.mkdir(parents=True, exist_ok=False)
    status = {'status': 'RUNNING', 'experiment': item['name'], 'mode': args.mode, 'started': utc(),
              'command': [sys.executable, *sys.argv], 'cwd': str(Path.cwd()), 'build': str(build)}
    write(run / 'status.json', status)
    try:
        with machine_lock():
            if os.environ.get('DYLD_INSERT_LIBRARIES') or os.environ.get('DYLD_LIBRARY_PATH'):
                raise RuntimeError('unset DYLD_INSERT_LIBRARIES and DYLD_LIBRARY_PATH before running experiments')
            config = configuration(item, runtime)
            write(run / 'configuration.json', config)
            write(run / 'environment.json', environment())
            ident = identity(item, config, build)
            status['identity_sha256'] = ident['sha256']
            write(run / 'identity.json', ident)
            if item.get('parallel') and config['parallel_guard_scope'] == 'nested-required':
                raise RuntimeError('BLOCKED_PARALLEL_ABI: available guard verifies the outer adapter call; nested backend-worker ABI proof is unavailable. See docs/prerequisites.md. No native call was launched.')
            prerequisites = []
            calibration = None
            names = item.get('prerequisite_families', [item['name']])
            if item['kind'] == 'matrix' and args.mode == 'qualification':
                path, summary, key = evidence('harness', 'qualification', build, output, runtime)
                prerequisites.append({'mode': 'qualification', 'path': str(path), 'identity': key})
            if args.mode in ('calibration', 'measurement'):
                for name in names:
                    path, summary, key = evidence(name, 'qualification', build, output, runtime)
                    prerequisites.append({'mode': 'qualification', 'path': str(path), 'identity': key})
            if args.mode == 'measurement':
                combined = {}
                for name in names:
                    path, summary, key = evidence(name, 'calibration', build, output, runtime)
                    prerequisites.append({'mode': 'calibration', 'path': str(path), 'identity': key})
                    if item['kind'] == 'matrix':
                        combined.update(summary['families'])
                    else:
                        calibration = summary
                if item['kind'] == 'matrix':
                    calibration = {'status': 'PASS', 'families': combined}
            write(run / 'prerequisites.json', prerequisites)
            if item['kind'] == 'harness':
                from harness_checks import execute as native
            elif item['kind'] == 'matrix':
                from matrix import execute as native
            else:
                from micro import compute, transfer
                native = compute if item['kind'] == 'compute' else transfer
            summary = native(ROOT, build, run, config, args.mode, calibration)
            if summary.get('status') != 'PASS':
                raise RuntimeError('experiment did not pass: ' + repr(summary))
            if identity(item, config, build)['sha256'] != ident['sha256']:
                raise RuntimeError('source, build, configuration, or environment changed while running')
            write(run / 'summary.json', summary)
            write(run / 'outputs.json', file_manifest(run, ('status.json', 'outputs.json')))
            status.update(status='PASS', outputs_sha256=sha(run / 'outputs.json'))
    except BaseException as e:
        status.update(status='FAILED', error=str(e))
        (run / 'failure.txt').write_text(traceback.format_exc())
        raise
    finally:
        status['ended'] = utc()
        write(run / 'status.json', status)
        print('Run outputs:', run, flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build-dir', default=str(ROOT / 'build'))
    p.add_argument('--output-dir', default=str(ROOT / 'results'))
    p.add_argument('--config', default=str(ROOT / 'config/runtime.json'))
    p.add_argument('--plan', action='store_true', help='print full resolved configuration without building or launching workers')
    p.add_argument('mode', choices=tuple(PREFIX))
    p.add_argument('experiment')
    execute(p.parse_args())


if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        print('ERROR:', e, file=sys.stderr)
        sys.exit(1)
