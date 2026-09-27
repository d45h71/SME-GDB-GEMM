#!/usr/bin/env python3
"""Remove a marked generated build tree, preserving all experiment results."""
import argparse
from pathlib import Path
import shutil
from common import machine_lock, read

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build-dir', required=True)
p.add_argument('--output-dir', required=True)
a = p.parse_args()
build, output = Path(a.build_dir).resolve(), Path(a.output_dir).resolve()
root = Path(__file__).resolve().parents[1]
if not build.exists():
    raise SystemExit(0)
if build in (root, root.parent, Path('/')) or output == build or output.is_relative_to(build) or (build / 'src').exists():
    raise SystemExit('Refusing unsafe clean path: ' + str(build))
manifest = build / 'build-manifest.json'
if not manifest.is_file() or not isinstance(read(manifest).get('components'), dict):
    raise SystemExit('Refusing to delete an unmarked directory: ' + str(build))
with machine_lock():
    shutil.rmtree(build)
print('Removed generated build files:', build)
