"""Print the command surface without importing or executing native code."""
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
inventory = json.loads((root / 'config/inventory.json').read_text())
print('MP5 experiments — standalone Apple arm64 SME experiment suite')
print('make setup | make build | make check | make clean')
print('Build components: matrix stock first second accelerate libxsmm kleidiai openblas compute transfer ablation')
print('Use make build-<component>; setup and build never run experiments.')
print('Options: PYTHON=python3 BUILD_DIR=build OUTPUT_DIR=results CONFIG=config/runtime.json')
print('Every run checks current prerequisites; missing evidence prints the exact command.')
print('Commands and prerequisites: docs/commands.md; methodology: docs/methods.md')
print()
for item in inventory['experiments']:
    print(' | '.join('make ' + t for t in item['targets'].values()))
    if '--inventory' in sys.argv:
        print('  %s; %d cells; %s; build %s' %
              (item['description'], item['cells'], item['config'], ','.join(item['components'])))
print('Offline: make analyze-comparisons | analyze-irregular | analyze-provenance | analyze-efficiency | analyze-ablation')
