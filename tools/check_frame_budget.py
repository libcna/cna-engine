#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run the two Release scene comparisons five times and enforce REL-003."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys


VIEW = re.compile(r"^--- view: ([^(]+?)(?:\s+\(|$)")
FRAME = re.compile(r"^cpu frame:\s+(car-simulator|street) ([0-9.]+) ms, engine ([0-9.]+) ms")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--car", type=Path, required=True)
    parser.add_argument("--street", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--max-ratio", type=float, default=1.15)
    parser.add_argument("--max-load-per-cpu", type=float, default=0.35)
    return parser.parse_args()


def run_comparison(executable: Path, output: Path) -> dict[str, tuple[float, float]]:
    result = subprocess.run(
        [str(executable), str(output)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(f"{executable.name} failed with {result.returncode}:\n{result.stdout}")

    current_view: str | None = None
    samples: dict[str, tuple[float, float]] = {}
    for line in result.stdout.splitlines():
        view = VIEW.match(line)
        if view:
            current_view = view.group(1).strip()
            continue
        frame = FRAME.match(line)
        if frame and current_view:
            key = f"{frame.group(1)}/{current_view}"
            samples[key] = (float(frame.group(2)), float(frame.group(3)))
            current_view = None
    if not samples:
        raise RuntimeError(f"{executable.name} produced no frame timings")
    return samples


def main() -> int:
    args = parse_args()
    if args.runs < 1 or args.max_ratio <= 0.0:
        raise ValueError("--runs and --max-ratio must be positive")

    cpu_count = os.cpu_count() or 1
    load = os.getloadavg()[0]
    load_per_cpu = load / cpu_count
    print(f"reference start: load {load:.2f}, {cpu_count} CPUs, {load_per_cpu:.3f} per CPU")
    if load_per_cpu > args.max_load_per_cpu:
        print(
            f"refusing performance measurement: load per CPU {load_per_cpu:.3f} exceeds "
            f"{args.max_load_per_cpu:.3f}",
            file=sys.stderr,
        )
        return 2

    args.output.mkdir(parents=True, exist_ok=True)
    all_samples: dict[str, list[tuple[float, float]]] = {}
    programs = ((args.car, args.output / "car"), (args.street, args.output / "street"))
    for run in range(args.runs):
        # Alternate which large scene goes first so a slow drift does not
        # systematically favour one pilot.
        ordered = programs if run % 2 == 0 else tuple(reversed(programs))
        print(f"run {run + 1}/{args.runs}")
        for executable, output in ordered:
            for key, sample in run_comparison(executable, output).items():
                all_samples.setdefault(key, []).append(sample)

    failed = False
    print("view | pilot best ms | engine best ms | ratio | result")
    for key in sorted(all_samples):
        samples = all_samples[key]
        if len(samples) != args.runs:
            raise RuntimeError(f"{key}: expected {args.runs} samples, got {len(samples)}")
        pilot_best = min(sample[0] for sample in samples)
        engine_best = min(sample[1] for sample in samples)
        ratio = engine_best / pilot_best
        passed = ratio <= args.max_ratio
        failed |= not passed
        print(
            f"{key} | {pilot_best:.1f} | {engine_best:.1f} | {ratio:.3f} | "
            f"{'PASS' if passed else 'FAIL'}"
        )

    if failed:
        print(f"frame budget exceeded: engine must be <= {args.max_ratio:.2f}x pilot", file=sys.stderr)
        return 1
    print(f"frame budget satisfied: every engine best is <= {args.max_ratio:.2f}x pilot best")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError) as error:
        print(f"frame budget check: {error}", file=sys.stderr)
        raise SystemExit(2)
