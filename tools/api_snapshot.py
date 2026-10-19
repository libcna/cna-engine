#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""API snapshot of the pilot-validated public headers (PILOT-003, REL-001).

`docs/api-snapshot.txt` lists every tracked header with a fingerprint of its
declarations: comments are removed and whitespace is collapsed, so editing
documentation or formatting never changes the fingerprint, while any change to
a declaration does.

  api_snapshot.py --check   fail (exit 1) when a tracked header changed or a
                            tracked header is missing
  api_snapshot.py --update  rewrite the snapshot after a deliberate API change
                            (record it in CHANGELOG.md)

Which headers are tracked is the list in the snapshot file itself; add a
header there when its facility becomes pilot-validated.
"""
from __future__ import annotations

import argparse
import hashlib
import pathlib
import re
import sys

SNAPSHOT = pathlib.Path("docs/api-snapshot.txt")


def strip_comments(text: str) -> str:
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            j = text.find("\n", i)
            i = n if j == -1 else j
        elif c == "/" and nxt == "*":
            j = text.find("*/", i + 2)
            i = n if j == -1 else j + 2
        elif c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def fingerprint(path: pathlib.Path) -> str:
    normalized = re.sub(r"\s+", " ", strip_comments(path.read_text(encoding="utf-8"))).strip()
    return hashlib.sha256(normalized.encode("utf-8")).hexdigest()[:16]


def read_snapshot(root: pathlib.Path) -> list[tuple[str, str]]:
    entries = []
    for line in (root / SNAPSHOT).read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        header, _, digest = line.partition(" ")
        entries.append((header, digest.strip()))
    return entries


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parent.parent)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--update", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()

    entries = read_snapshot(root)
    problems = []
    current = []
    for header, digest in entries:
        path = root / "include" / header
        if not path.is_file():
            problems.append(f"{header}: tracked header is missing")
            current.append((header, digest))
            continue
        actual = fingerprint(path)
        current.append((header, actual))
        if actual != digest:
            problems.append(f"{header}: declarations changed ({digest} -> {actual})")

    if args.update:
        lines = [
            "# Pilot-validated public API: header and declaration fingerprint.",
            "# Regenerate with tools/api_snapshot.py --update after a deliberate API",
            "# change, and record the change in CHANGELOG.md.",
        ]
        lines += [f"{header} {digest}" for header, digest in current]
        (root / SNAPSHOT).write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"API snapshot: {len(current)} headers recorded")
        return 0

    if problems:
        print(f"API snapshot: {len(problems)} change(s) to pilot-validated API:")
        for problem in problems:
            print("  " + problem)
        print("If deliberate: run tools/api_snapshot.py --update and record it in CHANGELOG.md.")
        return 1
    print(f"API snapshot: OK ({len(entries)} headers unchanged)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
