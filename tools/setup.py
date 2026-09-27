#!/usr/bin/env python3
"""Check reproducible build prerequisites; never compile or launch experiments."""
from pathlib import Path
import json, shutil, sys
from build import ROOT, toolchain

def main():
    if sys.version_info<(3,9):raise RuntimeError('Python 3.9 or later is required.')
    for name in ('make','xcrun','install_name_tool','ar','perl','awk','sed'):
        if not shutil.which(name):raise RuntimeError('Missing required build tool: '+name)
    config=toolchain()
    for name in ('libxsmm','kleidiai','openblas'):
        if not (ROOT/'third_party'/name).is_dir():raise RuntimeError('Missing vendored source: '+name)
    print(json.dumps({'status':'READY_TO_BUILD','toolchain':config,'dependencies':json.loads((ROOT/'third_party/dependencies.json').read_text()),'next_command':'make build'},indent=2))
if __name__=='__main__':
    try:main()
    except Exception as e:print('ERROR:',e,file=sys.stderr);sys.exit(1)
