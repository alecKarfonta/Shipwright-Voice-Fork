"""Read the local game's Wolfos assets for the Blender authoring workspace.

This is a deliberately bounded decoder of the commands used by object_wf, not a
general display-list renderer. Unknown geometry commands fail loudly. Original
pixels and mesh data are read from the user's archive and are not checked in.
"""
import hashlib
import math
import re
import struct
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
PREFIX = 'objects/object_wf/'
IDENTITY = [[int(r == c) for c in range(4)] for r in range(4)]


class Reader:
    def __init__(self, data):
        self.data, self.offset = data, 64

    def read(self, fmt):
        result = struct.unpack_from('<' + fmt, self.data, self.offset)
        self.offset += struct.calcsize('<' + fmt)
        return result[0] if len(result) == 1 else result

    def string(self):
        length = self.read('I')
        result = self.data[self.offset:self.offset + length].decode()
        self.offset += length
        return result


def multiply(a, b):
    return [[sum(a[r][k] * b[k][c] for k in range(4)) for c in range(4)] for r in range(4)]


def local_matrix(position, rotation):
    x, y, z = [v * math.pi / 32768 for v in rotation]
    cx, sx, cy, sy, cz, sz = math.cos(x), math.sin(x), math.cos(y), math.sin(y), math.cos(z), math.sin(z)
    return [[cz*cy, cz*sy*sx-sz*cx, cz*sy*cx+sz*sx, position[0]/100],
            [sz*cy, sz*sy*sx+cz*cx, sz*sy*cx-cz*sx, position[1]/100],
            [-sy, cy*sx, cy*cx, position[2]/100], [0, 0, 0, 1]]


class NativeWolfos:
    def __init__(self, archive=None, overlay=None):
        self.archive_path = Path(archive or REPO / 'build-cmake/soh/oot.o2r')
        self.archive = zipfile.ZipFile(self.archive_path)
        self.overlay = zipfile.ZipFile(overlay) if overlay else None
        table_source = (REPO / 'libultraship/src/ship/utils/StrHash64.cpp').read_text()
        table_source = table_source.split('CRC64_Table[256] = {')[1].split('};')[0]
        table = [int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]+)', table_source)]
        def crc(name):
            value = (1 << 64) - 1
            for byte in name.encode():
                value = (table[(value >> 56) ^ byte] ^ (value << 8)) & ((1 << 64) - 1)
            return value
        self.names = {crc(n): n for n in self.archive.namelist()}
        self.crc = crc
        if self.overlay:
            self.names.update({crc(n): n for n in self.overlay.namelist()})
        self.hashes = {}
        self.texture_cache = {}
        self.setup_cache = {}
        self.rigs = {s: self.read_rig(s) for s in ('Normal', 'White')}
        self.animations = {}
        for name in sorted(self.archive.namelist()):
            if name.startswith(PREFIX) and name.endswith('Anim'):
                r = Reader(self.read(name))
                assert r.read('I') == 0, 'Expected normal animation'
                frames = r.read('h')
                values = [r.read('h') for _ in range(r.read('I'))]
                indices = [r.read('HHH') for _ in range(r.read('I'))]
                static = r.read('h')
                assert len(indices) == 22
                self.animations[name.removeprefix(PREFIX)] = [
                    [tuple(values[index + (frame if index >= static else 0)] for index in row)
                     for row in indices] for frame in range(frames)]
        assert len(self.animations) == 8

    def read(self, name):
        if self.overlay and 'alt/' + name in self.overlay.namelist():
            data = self.overlay.read('alt/' + name)
        elif self.overlay and name in self.overlay.namelist():
            data = self.overlay.read(name)
        else:
            data = self.archive.read(name)
        self.hashes[name] = hashlib.sha256(data).hexdigest()
        return data

    def read_rig(self, species):
        r = Reader(self.read(PREFIX + 'gWolfos' + species + 'Skel'))
        r.read('bbIIb')
        paths = [r.string() for _ in range(r.read('I'))]
        limbs = []
        for index, path in enumerate(paths):
            r = Reader(self.read(path))
            r.read('bb'); r.string(); r.read('H')
            assert r.read('I') == 0
            r.string(); r.read('fffHHH'); r.string(); r.string()
            dl = r.string(); r.string()
            position, child, sibling = r.read('hhh'), r.read('B'), r.read('B')
            limbs.append(dict(joint=index+1, position=position, child=child, sibling=sibling, dl=dl))
        def visit(index, parent):
            if index == 255:
                return
            limbs[index]['parent'] = parent
            visit(limbs[index]['child'], index+1)
            visit(limbs[index]['sibling'], parent)
        visit(0, 0)
        assert len(limbs) == 21 and all('parent' in limb for limb in limbs)
        return limbs

    def pose(self, animation, frame, species='Normal', head_turn=0):
        joints = self.animations[animation][frame]
        limbs = self.rigs[species]
        world, local = {}, {}
        def visit(index, parent):
            if index == 255:
                return
            limb = limbs[index]
            rotation = list(joints[index+1])
            if index+1 in (17, 18):
                rotation[1] -= head_turn
            local[index+1] = local_matrix(joints[0] if index == 0 else limb['position'], rotation)
            world[index+1] = multiply(parent, local[index+1])
            visit(limb['child'], world[index+1])
            visit(limb['sibling'], parent)
        visit(0, IDENTITY)
        return world, local

    def texture(self, name):
        if name not in self.texture_cache:
            r = Reader(self.read(name))
            version = struct.unpack_from('<I', r.data, 8)[0]
            kind, width, height = r.read('III')
            if version == 1:
                flags, hscale, vscale = r.read('Iff')
                assert flags == 2 and hscale > 0 and vscale > 0
            else:
                assert version == 0
            size = r.read('I')
            if kind == 2:
                assert size == width * height * 2
                words = struct.unpack_from('>' + 'H' * (width * height), r.data, r.offset)
                rgba = [[((v >> 11) & 31)/31, ((v >> 6) & 31)/31, ((v >> 1) & 31)/31, float(v & 1)] for v in words]
            else:
                assert kind == 1 and size == width * height * 4
                rgba = [[v/255 for v in r.data[i:i+4]] for i in range(r.offset, r.offset+size, 4)]
            self.texture_cache[name] = dict(width=width, height=height, rgba=rgba)
        return self.texture_cache[name]

    def material_setups(self, species, joint):
        """Retain this limb's native material/combiner commands for an edited mesh."""
        if (species, joint) in self.setup_cache:
            return self.setup_cache[species, joint]
        dl = self.rigs[species][joint-1]['dl']
        if not dl:
            return {}
        data, offset, commands, setups = self.read(dl), 72, [], {}
        texture, tile = None, (0, 0, 0, 0)
        while offset + 8 <= len(data):
            start = offset
            a, b = struct.unpack_from('<II', data, offset); offset += 8
            opcode = a >> 24
            if opcode in (0x20, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37):
                high, low = struct.unpack_from('<II', data, offset); offset += 8
                resource = self.names[(high << 32) | low]
            if opcode == 0x20:
                texture = resource
            elif opcode == 0xfd:
                assert b == 0x08000001
                texture = PREFIX + 'gWolfos' + species + 'EyeOpenTex'
            elif opcode == 0xf5 and ((b >> 24) & 7) == 0:
                tile = ((b >> 8) & 3, (b >> 18) & 3, (b >> 4) & 15, (b >> 14) & 15)
            if opcode in (5, 6):
                setups.setdefault((texture, tile), b''.join(commands))
            elif opcode == 0xdf:
                break
            elif opcode not in (0x32, 0x33, 0xda):
                assert opcode not in (0x31, 0x34, 0x35, 0x36, 0x37), 'Nested display-list state needs an explicit importer'
                commands.append(data[start:offset])
        self.setup_cache[species, joint] = setups
        return setups

    def mesh(self, species, joint):
        limbs = self.rigs[species]
        dl = limbs[joint-1]['dl']
        if not dl:
            return []
        visible = [limb['joint'] for limb in limbs if limb['dl']]
        data, offset, cache, active_joint = self.read(dl), 72, {}, joint
        texture, tile, triangles = None, (0, 0, 0, 0), []
        # [TIME] | [INFO] | Wolfos.NativeAsset | Geometry/material commands used by this actor are handled explicitly.
        inert = {0x33, 0xe7, 0xe6, 0xe3, 0xe2, 0xf3, 0xfc, 0xd9, 0xfa, 0xf2}
        while offset + 8 <= len(data):
            a, b = struct.unpack_from('<II', data, offset); offset += 8
            opcode, resource = a >> 24, None
            if opcode in (0x20, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37):
                high, low = struct.unpack_from('<II', data, offset); offset += 8
                resource = self.names[(high << 32) | low]
            if opcode == 0x20:
                texture = resource
                self.texture(texture)
            elif opcode == 0xfd:
                assert b == 0x08000001, f'Unexpected segmented texture {b:x}'
                texture = PREFIX + 'gWolfos' + species + 'EyeOpenTex'
                for state in ('Open', 'Half', 'Narrow'):
                    self.texture(texture.replace('Open', state))
            elif opcode == 0xf5:
                if (b >> 24) & 7 == 0:
                    tile = ((b >> 8) & 3, (b >> 18) & 3, (b >> 4) & 15, (b >> 14) & 15)
                    assert ((b >> 10) & 15) == 0 and (b & 15) == 0, 'Shifted UV tile not supported'
            elif opcode == 0xd7:
                assert b == 0xffffffff, 'Unexpected texture scale'
            elif opcode == 0xda:
                assert b >> 24 == 0x0d
                active_joint = visible[(b & 0xffffff) // 64]
            elif opcode == 0x32:
                vertices = self.read(resource)
                assert struct.unpack_from('<I', vertices, 64)[0] == 25, 'Expected vertex array'
                count = (a >> 12) & 255
                start = ((a & 255) // 2) - count
                for i in range(count):
                    values = struct.unpack_from('<hhhHhhbbbB', vertices, 72+b+i*16)
                    cache[start+i] = dict(position=values[:3], uv=values[4:6], normal=values[6:9], joint=active_joint)
            elif opcode in (5, 6):
                assert texture
                for word in ([a] if opcode == 5 else [a, b]):
                    triangles.append(dict(vertices=[cache[((word >> shift) & 255)//2] for shift in (16, 8, 0)],
                                          texture=texture, tile=tile))
            elif opcode == 0xdf:
                break
            else:
                assert opcode in inert, f'Unhandled display command {opcode:x} in {dl}'
        assert triangles
        return triangles

    def validate_bind(self):
        import json
        expected = json.loads((Path(__file__).parent / 'native_pose.json').read_text())
        world, _ = self.pose('gWolfosWaitingAnim', 0)
        error = max(abs(world[int(j)][r][c] - matrix[r][c])
                    for j, matrix in expected.items() for r in range(4) for c in range(4))
        assert error < 1e-6, f'Native bind mismatch {error}'
        return error
