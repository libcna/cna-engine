#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Strict-XNA boundary check for cna-engine (ADR-001, plan item ENG-011).

The strict targets (CnaEngine::Core, CnaEngine::Graphics and CnaEngine::App) may depend only on

  * XNA 4.0-compatible CNA headers (``Microsoft/Xna/Framework/...`` whose type
    is listed in ``tools/xna4_types.txt``),
  * Sharp Runtime (``System/...``, ``SharpRuntime/...``),
  * the C++ standard library,
  * strict cna-engine headers (``CnaEngine/<strict module>/...``).

The rules mirror car-simulator's ``scripts/check_xna_only.py`` (MIT, same
owner), which is the strict pilot this surface must pass unchanged, plus one
engine-specific rule: strict files must not include the enhanced
``CnaEngine/Extended/`` headers.

Usage:
  check_strict_boundary.py [--root DIR] [PATH ...]

Without paths, the strict include and source directories of the repository are
scanned. Exit code 0 means clean; 1 means violations were printed; 2 means a
usage error.
"""
from __future__ import annotations

import argparse
import pathlib
import re
import sys

STRICT_MODULES = ("Core", "Input", "Diagnostics", "Graphics", "Assets", "App", "Collision")
SOURCE_SUFFIXES = {".hpp", ".cpp", ".h", ".c", ".inl", ".ipp"}

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]', re.MULTILINE)

FORBIDDEN_INCLUDE_PATTERNS = [
    re.compile(p, re.IGNORECASE)
    for p in (
        r"(^|/)CNA/",
        r"/Internal/",
        r"CnaExt|CNAEXT",
        r"(^|/)SDL",
        r"(^|/)GL/|(^|/)GLES",
        r"vulkan",
        r"d3d|dxgi",
        r"easygl|metagl|rlgl",
        r"glad|glew|epoxy",
    )
]

FORBIDDEN_IDENTIFIERS = [
    re.compile(r"\bCNAEXT\b"),
    re.compile(r"\bCnaExt\b"),
    re.compile(r"\bCNA\s*::"),
    re.compile(r"\b\w+EXT\b"),
    re.compile(r"\bShaderEffect\b"),
    re.compile(r"\b(Skinned)?PbrEffect\b"),
    re.compile(r"\bColorMatrixEffect\b"),
    re.compile(r"\bAnimationPlayer\b"),
    re.compile(r"\bGetGraphicsRenderer(Name|Type)\b"),
    re.compile(r"\bSupportsCapability\b"),
    re.compile(r"\bGraphicsCapability\b"),
    re.compile(r"\bRendererCapabilityProfile\b"),
    re.compile(r"\bGetMaxTextureDimension\b"),
    re.compile(r"\bSetDepthTestEnabled\b|\bSetBlendEnabled\b|\bSetDepthWriteEnabled\b"),
    re.compile(r"\bSetCurrentEffect\b"),
    re.compile(r"\b(Easy|Meta)GL\b|\bRlgl\b|\bRLGL\b|\bVulkanRenderer\b|\bSdlRenderer\b"),
    re.compile(r"\bSDL_\w+"),
    re.compile(r"\bgl[A-Z]\w*\s*\("),
    re.compile(r"\bvk[A-Z]\w*\s*\("),
]

STRICT_ENGINE_INCLUDE_RE = re.compile(r"^CnaEngine/(%s)/" % "|".join(STRICT_MODULES))


def strip_comments_and_strings(text: str) -> str:
    """Blank out comments and string literals so documentation cannot trip rules."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            j = text.find("\n", i)
            j = n if j == -1 else j
            out.append(" " * (j - i))
            i = j
        elif c == "/" and nxt == "*":
            j = text.find("*/", i + 2)
            j = n if j == -1 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif c == '"' or c == "'":
            quote = c
            j = i + 1
            while j < n and text[j] != quote:
                if text[j] == "\\":
                    j += 1
                if j < n and text[j] == "\n":
                    break
                j += 1
            j = min(j + 1, n)
            out.append(quote + " " * (j - i - 2) + quote if j - i >= 2 else text[i:j])
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def load_type_list(path: pathlib.Path) -> set[str]:
    types = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            types.add(line)
    return types


def header_to_type(include_path: str) -> str:
    return include_path.rsplit(".", 1)[0].replace("/", ".")


def check_file(path: pathlib.Path, display: str, xna_types: set[str], violations: list[str]) -> None:
    raw = path.read_text(encoding="utf-8", errors="replace")
    code = strip_comments_and_strings(raw)

    for match in INCLUDE_RE.finditer(raw):
        opener, inc = match.group(1), match.group(2)
        line_no = raw.count("\n", 0, match.start()) + 1
        where = f"{display}:{line_no}"
        if any(p.search(inc) for p in FORBIDDEN_INCLUDE_PATTERNS):
            violations.append(f"{where}: forbidden include '{inc}'")
        elif inc.startswith("Microsoft/Xna/Framework/"):
            if header_to_type(inc) not in xna_types:
                violations.append(f"{where}: '{inc}' is not an XNA 4.0 type (not in tools/xna4_types.txt)")
        elif inc.startswith("Microsoft/"):
            violations.append(f"{where}: unexpected Microsoft header '{inc}'")
        elif inc.startswith("CnaEngine/"):
            if not STRICT_ENGINE_INCLUDE_RE.match(inc):
                violations.append(f"{where}: strict code includes non-strict engine header '{inc}'")
        elif opener == '"' and not (inc.startswith("System/") or inc.startswith("SharpRuntime/")):
            violations.append(f"{where}: unexpected local include '{inc}'")

    for line_no, line in enumerate(code.splitlines(), start=1):
        for pat in FORBIDDEN_IDENTIFIERS:
            m = pat.search(line)
            if m:
                violations.append(f"{display}:{line_no}: forbidden identifier '{m.group(0)}'")


def default_paths(root: pathlib.Path) -> list[pathlib.Path]:
    paths = []
    for module in STRICT_MODULES:
        paths.append(root / "include" / "CnaEngine" / module)
        paths.append(root / "src" / module)
    return paths


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", default=pathlib.Path(__file__).resolve().parent.parent, type=pathlib.Path)
    parser.add_argument("--types", default=None, type=pathlib.Path)
    parser.add_argument("paths", nargs="*", type=pathlib.Path)
    args = parser.parse_args()

    root = args.root.resolve()
    types_path = args.types or root / "tools" / "xna4_types.txt"
    if not types_path.exists():
        print(f"error: type list not found: {types_path}", file=sys.stderr)
        return 2
    xna_types = load_type_list(types_path)

    targets = args.paths or default_paths(root)
    files: list[pathlib.Path] = []
    for target in targets:
        target = target if target.is_absolute() else (pathlib.Path.cwd() / target)
        if target.is_file():
            files.append(target)
        elif target.is_dir():
            files.extend(p for p in target.rglob("*") if p.suffix in SOURCE_SUFFIXES and p.is_file())
        elif args.paths:
            print(f"error: no such path: {target}", file=sys.stderr)
            return 2
    files = sorted(set(files))
    if not files:
        print("error: no files to scan", file=sys.stderr)
        return 2

    violations: list[str] = []
    for f in files:
        try:
            display = str(f.resolve().relative_to(root))
        except ValueError:
            display = str(f)
        check_file(f, display, xna_types, violations)

    if violations:
        print(f"Strict boundary check: {len(violations)} violation(s) in {len(files)} file(s):")
        for v in violations:
            print("  " + v)
        return 1
    print(f"Strict boundary check: OK ({len(files)} files scanned, {len(xna_types)} XNA 4.0 types known)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
