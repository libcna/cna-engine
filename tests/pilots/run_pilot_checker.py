#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Runs car-simulator's own XNA-only checker over cna-engine's strict sources.

ADR-001 claims the strict engine surface needs exactly one allowance in the
pilot's checker: includes of strict `CnaEngine/<module>/` headers. This script
proves it with the pilot's unmodified checker: it builds a shadow tree in the
given work directory whose `simulator/` folder holds the engine's strict
headers and sources (copies), runs `scripts/check_xna_only.py` from the pilot checkout
against it, and succeeds only if every reported violation is such an include.

Usage: run_pilot_checker.py <car-simulator dir> <engine root> <work dir>
"""
from __future__ import annotations

import pathlib
import re
import shutil
import subprocess
import sys

STRICT_MODULES = ("Core", "Input", "Diagnostics", "Graphics", "Assets")
ALLOWED = re.compile(r"unexpected local include 'CnaEngine/(%s)/[^']+'$" % "|".join(STRICT_MODULES))


def main() -> int:
    if len(sys.argv) != 4:
        print(__doc__, file=sys.stderr)
        return 2
    pilot, engine, work = (pathlib.Path(a).resolve() for a in sys.argv[1:])
    checker = pilot / "scripts" / "check_xna_only.py"
    types = pilot / "scripts" / "xna4_types.txt"
    if not checker.is_file() or not types.is_file():
        print(f"error: pilot checker not found under {pilot}", file=sys.stderr)
        return 2

    root = work / "pilot_checker_root"
    shutil.rmtree(root, ignore_errors=True)
    (root / "simulator" / "include").mkdir(parents=True)
    (root / "simulator" / "src").mkdir(parents=True)
    (root / "scripts").mkdir()
    shutil.copyfile(types, root / "scripts" / "xna4_types.txt")
    for module in STRICT_MODULES:
        for kind in ("include", "src"):
            source = engine / kind / ("CnaEngine" if kind == "include" else "") / module
            if source.is_dir():
                # Copies, not symlinks: the checker's rglob does not follow links.
                shutil.copytree(source, root / "simulator" / kind / module)

    result = subprocess.run([sys.executable, "-I", str(checker), "--root", str(root)],
                            capture_output=True, text=True)
    lines = [line.strip() for line in result.stdout.splitlines() if line.startswith("  ")]
    unexpected = []
    allowed = 0
    for line in lines:
        message = line.split(": ", 1)[1] if ": " in line else line
        if ALLOWED.search(message):
            allowed += 1
        else:
            unexpected.append(line)
    shutil.rmtree(root, ignore_errors=True)

    if result.returncode not in (0, 1):
        print(result.stdout + result.stderr)
        return 1
    if unexpected:
        print(f"car-simulator checker: {len(unexpected)} violation(s) beyond the ADR-001 allowance:")
        for line in unexpected:
            print("  " + line)
        return 1
    scanned = re.search(r"in (\d+) file", result.stdout) or re.search(r"\((\d+) files scanned", result.stdout)
    if allowed == 0 and result.returncode == 0 and "CNA_ENGINE_STRICT_PREFIXES" in checker.read_text():
        # Since car-simulator adopted the engine (2026-10-20) its checker
        # carries the ADR-001 allowance itself, so engine includes are no
        # longer reported: a clean run over the shadow tree is the proof.
        files = int(scanned.group(1)) if scanned else 0
        if files == 0:
            print("car-simulator checker: the shadow tree was not scanned")
            print(result.stdout)
            return 1
        print(f"car-simulator checker: carries the ADR-001 allowance itself and accepts "
              f"the strict engine ({files} files)")
        return 0
    if allowed == 0:
        print("car-simulator checker: no engine include was reported; the shadow tree was not scanned")
        print(result.stdout)
        return 1
    print(f"car-simulator checker: only the ADR-001 allowance is needed "
          f"({allowed} strict engine includes in {scanned.group(1) if scanned else '?'} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
