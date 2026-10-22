#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Installs the engine from build/ and builds/runs the installed-package
# consumer probes (tests/consumer) in build-consumer/ (ENG-012).
#
#   tools/run_consumer_probes.sh [engine-build-dir] [consumer-build-dir]
#
# Uses the shared ccache and at most 3 build jobs (workspace rules).
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Absolute paths: CMAKE_PREFIX_PATH and the install prefix must not depend on
# the directory CMake resolves them from.
engine_build="$(cd "${1:-$root/build}" && pwd)"
mkdir -p "${2:-$root/build-consumer}"
consumer_build="$(cd "${2:-$root/build-consumer}" && pwd)"
prefix="$consumer_build/install"
jobs="${CNA_ENGINE_JOBS:-3}"

# The development machine's one shared cache (CLAUDE.md Rule 1); elsewhere
# (CI) ccache's own defaults apply.
if [ -d /rv/cnaccache ]; then
    export CCACHE_DIR="${CCACHE_DIR:-/rv/cnaccache}"
    export CCACHE_BASEDIR="${CCACHE_BASEDIR:-/rv}"
fi
# Debug builds hash the working directory unless told otherwise, so without
# this no object compiled in build/ could be reused here (upstream-issues.md UP-003).
export CCACHE_NOHASHDIR="${CCACHE_NOHASHDIR:-1}"
launcher=()
if command -v ccache >/dev/null; then
    launcher=(-DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache)
else
    echo "warning: ccache not found; building without it" >&2
fi

dirs() { cmake -L -N "$engine_build" 2>/dev/null | sed -n "s/^$1:PATH=//p"; }
cna_dir="$(dirs CNA_ENGINE_CNA_DIR)"
sharp_dir="$(dirs CNA_ENGINE_SHARP_RUNTIME_DIR)"
extended_dir="$(dirs CNA_ENGINE_EXTENDED_DIR)"

# Regenerate and build first so install rules and libraries are current.
cmake "$engine_build" >/dev/null
cmake --build "$engine_build" --parallel "$jobs"
rm -rf "$prefix"
cmake --install "$engine_build" --prefix "$prefix"

# The consumer must not see the engine's source tree: check the installed
# package before using it.
if grep -rq "$root/include\|$root/src" "$prefix/lib/cmake/CnaEngine"; then
    echo "error: installed package refers to the engine source tree" >&2
    exit 1
fi

# cna-extended 0.2.0 and later install a CMake package: install it beside the
# engine, so the probes take it from there as a binary consumer would.
extended_package=OFF
if [ -f "$extended_dir/cmake/CnaExtendedConfig.cmake.in" ]; then
    extended_build="$consumer_build/extended-package"
    cmake -S "$extended_dir" -B "$extended_build" -G Ninja -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}" \
        -DCNA_EXTENDED_CNA_DIR="$cna_dir" -DCNA_EXTENDED_SHARP_RUNTIME_DIR="$sharp_dir" \
        -DCNA_EXTENDED_BUILD_TESTS=OFF -DCNA_EXTENDED_BUILD_EXAMPLES=OFF \
        -DCNA_ENABLE_SDL=OFF -DCNA_ENABLE_VIDEO=OFF "${launcher[@]}" > /dev/null
    cmake --build "$extended_build" --parallel "$jobs"
    cmake --install "$extended_build" --prefix "$prefix" > /dev/null
    extended_package=ON
fi

cmake -S "$root/tests/consumer" -B "$consumer_build" -G Ninja \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}" \
    -DCMAKE_PREFIX_PATH="$prefix" \
    -DPROBE_CNA_DIR="$cna_dir" -DPROBE_SHARP_RUNTIME_DIR="$sharp_dir" -DPROBE_EXTENDED_DIR="$extended_dir" \
    -DPROBE_EXTENDED_FROM_PACKAGE="$extended_package" \
    "${launcher[@]}"
cmake --build "$consumer_build" --parallel "$jobs"
ctest --test-dir "$consumer_build" --output-on-failure --parallel "$jobs"
python3 "$root/tools/check_strict_boundary.py" --root "$root" "$root/tests/consumer/strict_consumer.cpp"
