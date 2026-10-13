#!/usr/bin/env python3
"""Check semantic export slots and native-resource provenance without running Blender."""
import argparse
import ast
import json
from pathlib import Path
import re
import struct
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent

def check(archive=None):
    slots = json.loads((HERE / 'material-slots.json').read_text())
    for name, entry in slots.items():
        tree = ast.parse((ROOT / entry['exporter']).read_text())
        names = next(ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
                     and any(isinstance(t, ast.Name) and t.id == 'MATERIALS' for t in n.targets))
        expected = entry['slots']
        assert names == list(expected), f'{name}: Blender material slot order changed; review the adapter'
        source = (ROOT / entry['adapter']).read_text()
        table = source.split(entry.get('table_prefix', 'Surfaces={'), 1)[1].split('};', 1)[0]
        actual = re.findall(r'(?:GearGeometry::)?(?:Surface|S)::(\w+)', table)
        assert actual == list(expected.values()), f'{name}: runtime surfaces differ from reviewed slots'
    arpg = ROOT / 'soh/soh/Enhancements/arpg'
    for path in arpg.glob('*'):
        if path.suffix in {'.cpp', '.h'}:
            assert 'ForColor(' not in path.read_text(), f'{path.name}: RGB material inference returned'
    refs = json.loads((HERE / 'resource-provenance.json').read_text())['resources']
    if archive:
        with ZipFile(archive) as z:
            missing = [path for path in refs.values() if path not in z.namelist()]
            assert not missing, f'Missing native resources: {missing}'
            guay = z.read(refs['gGuayBodyTex'])
            assert struct.unpack_from('<4I', guay, 64) == (2, 16, 16, 512), 'Guay feather texture is not RGBA16 16x16'
            leaf = z.read(refs['gDekuNutsLeafTex'])
            assert struct.unpack_from('<4I', leaf, 64) == (2, 32, 32, 2048), 'Mad Scrub leaf texture is not RGBA16 32x32'
            data = z.read(refs['gOctorokRoughSkinTex'])
            assert struct.unpack_from('<4I', data, 64) == (2, 16, 16, 512), 'Skin texture is not RGBA16 16x16'
    print(f'PASS: {len(slots)} semantic material adapters; {len(refs)} native references' +
          (' verified in archive' if archive else ' (archive availability not checked)'))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path)
    args = parser.parse_args()
    check(args.archive)
