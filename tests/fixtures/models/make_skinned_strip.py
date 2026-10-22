#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Writes skinned_strip.gltf: a two-bone skinned strip with two clips, for
cna-engine's model and animation tests (plan.md GAME-010, GAME-011).

A vertical strip 0.5 m wide and 2 m tall facing +Z, rows at y = 0, 0.5, 1,
1.5, 2. Bone "root" sits at the origin, bone "tip" 1 m above it; rows below
0.5 m follow the root, rows from 1.5 m the tip, the row at 1 m half and half.
Clips: "bend" turns the tip about Z by 0, 60, 0 degrees at 0, 1, 2 s;
"sway" turns the root by -20, 20, -20 degrees. Orange PBR material,
double-sided. Everything is embedded (one base64 buffer).

Run: python3 make_skinned_strip.py > skinned_strip.gltf
"""
import base64
import json
import math
import struct
import sys


def rot_z(degrees):
    half = math.radians(degrees) / 2.0
    return [0.0, 0.0, math.sin(half), math.cos(half)]


rows = [0.0, 0.5, 1.0, 1.5, 2.0]
positions, normals, joints, weights = [], [], [], []
for y in rows:
    tip = min(max((y - 0.5) / 1.0, 0.0), 1.0)
    for x in (-0.25, 0.25):
        positions += [x, y, 0.0]
        normals += [0.0, 0.0, 1.0]
        joints += [0, 1, 0, 0]
        weights += [1.0 - tip, tip, 0.0, 0.0]
indices = []
for r in range(len(rows) - 1):
    a, b, c, d = 2 * r, 2 * r + 1, 2 * r + 2, 2 * r + 3
    indices += [a, b, d, a, d, c]
inverse_bind = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
                1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1]
times = [0.0, 1.0, 2.0]
bend = rot_z(0) + rot_z(60) + rot_z(0)
sway = rot_z(-20) + rot_z(20) + rot_z(-20)

blob = bytearray()
views = []
accessors = []


def add(data, fmt, component, count, kind, target=None, minmax=None):
    while len(blob) % 4:
        blob.append(0)
    offset = len(blob)
    blob.extend(struct.pack("<%d%s" % (len(data), fmt), *data))
    view = {"buffer": 0, "byteOffset": offset, "byteLength": len(blob) - offset}
    if target:
        view["target"] = target
    views.append(view)
    accessor = {"bufferView": len(views) - 1, "componentType": component, "count": count, "type": kind}
    if minmax:
        accessor["min"], accessor["max"] = minmax
    accessors.append(accessor)
    return len(accessors) - 1


FLOAT, USHORT = 5126, 5123
ARRAY, ELEMENTS = 34962, 34963
p = add(positions, "f", FLOAT, 10, "VEC3", ARRAY, ([-0.25, 0.0, 0.0], [0.25, 2.0, 0.0]))
n = add(normals, "f", FLOAT, 10, "VEC3", ARRAY)
w = add(weights, "f", FLOAT, 10, "VEC4", ARRAY)
ibm = add(inverse_bind, "f", FLOAT, 2, "MAT4")
t = add(times, "f", FLOAT, 3, "SCALAR", None, ([0.0], [2.0]))
rb = add(bend, "f", FLOAT, 3, "VEC4")
rs = add(sway, "f", FLOAT, 3, "VEC4")
j = add(joints, "H", USHORT, 10, "VEC4", ARRAY)
i = add(indices, "H", USHORT, len(indices), "SCALAR", ELEMENTS)

gltf = {
    "asset": {"version": "2.0", "generator": "cna-engine tests/fixtures/models/make_skinned_strip.py"},
    "scene": 0,
    "scenes": [{"nodes": [0, 1]}],
    "nodes": [
        {"name": "strip", "mesh": 0, "skin": 0},
        {"name": "root", "children": [2]},
        {"name": "tip", "translation": [0.0, 1.0, 0.0]},
    ],
    "meshes": [{"name": "strip", "primitives": [{
        "attributes": {"POSITION": p, "NORMAL": n, "JOINTS_0": j, "WEIGHTS_0": w},
        "indices": i, "material": 0}]}],
    "materials": [{"name": "orange", "doubleSided": True, "pbrMetallicRoughness": {
        "baseColorFactor": [1.0, 0.5, 0.1, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.8}}],
    "skins": [{"name": "rig", "joints": [1, 2], "inverseBindMatrices": ibm, "skeleton": 1}],
    "animations": [
        {"name": "bend", "samplers": [{"input": t, "output": rb, "interpolation": "LINEAR"}],
         "channels": [{"sampler": 0, "target": {"node": 2, "path": "rotation"}}]},
        {"name": "sway", "samplers": [{"input": t, "output": rs, "interpolation": "LINEAR"}],
         "channels": [{"sampler": 0, "target": {"node": 1, "path": "rotation"}}]},
    ],
    "bufferViews": views,
    "accessors": accessors,
    "buffers": [{"byteLength": len(blob),
                 "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(blob)).decode()}],
}
json.dump(gltf, sys.stdout, indent=1)
sys.stdout.write("\n")
