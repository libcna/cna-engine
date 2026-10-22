#!/usr/bin/env bash
# The browser build of @GAME_NAME@: WebAssembly and WebGL 2 through Emscripten.
#
#   tools/build_web.sh            configure (the first time) and build into build-web/,
#                                 the page and its files in build-web/web-package/
#   tools/build_web.sh --serve    ... then serve them on http://localhost:8080/
#
# Environment: EMSDK (default ~/emsdk), BUILD_DIR (default build-web), JOBS
# (default 4), CMAKE_ARGS for further configure options.
set -eu
cd "$(dirname "$0")/.."
emsdk=${EMSDK:-$HOME/emsdk}
[ -f "$emsdk/emsdk_env.sh" ] || { echo "no Emscripten SDK at $emsdk (set EMSDK)" >&2; exit 2; }
# shellcheck disable=SC1091
source "$emsdk/emsdk_env.sh" > /dev/null 2>&1
build=${BUILD_DIR:-build-web}
launchers=()
command -v ccache > /dev/null && launchers=(-DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache)
generator=()
command -v ninja > /dev/null && generator=(-G Ninja)
# shellcheck disable=SC2206
extra=(${CMAKE_ARGS:-})
if [ ! -f "$build/CMakeCache.txt" ]; then
  emcmake cmake -S . -B "$build" "${generator[@]}" "${launchers[@]}" -DCMAKE_BUILD_TYPE=Release "${extra[@]}"
fi
cmake --build "$build" --target @GAME_TARGET@ --parallel "${JOBS:-4}"
echo "browser build: $build/web-package/"
if [ "${1:-}" = "--serve" ]; then
  echo "serving http://localhost:8080/ (Ctrl+C stops)"
  cd "$build/web-package" && exec python3 -m http.server 8080
fi
