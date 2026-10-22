#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Puts what a game on cna-engine builds against beside the engine (plan.md
# GAME-002): CNA, Sharp Runtime, easy-gl and meta-gl at the revisions the
# engine is verified with (cmake/CnaEngineBaseline.cmake), and CNA's vendored
# SDL and SDL_mixer (not their nested submodules: CNA's build disables every
# codec under them).
#
#   tools/fetch_dependencies.sh [--dry-run] [--with-extended] [--no-sdl] [DIR]
#
#   DIR              where the checkouts go (default: the directory that holds
#                    this engine checkout, so they end up as its siblings)
#   --dry-run        print what would be done, change nothing
#   --with-extended  also cna-extended (for CnaEngine::ExtendedIntegration)
#   --no-sdl         skip CNA's SDL submodules (when building with
#                    -DCNA_USE_SYSTEM_SDL=ON against an installed SDL3)
#
# A checkout that already exists is never changed: it is reported, with
# whether it contains the pinned revision. Environment: CNA_ENGINE_GIT_BASE
# (default https://github.com/libcna) for mirrors or forks.
set -euo pipefail

engine=$(cd "$(dirname "$0")/.." && pwd)
baseline=$engine/cmake/CnaEngineBaseline.cmake
base=${CNA_ENGINE_GIT_BASE:-https://github.com/libcna}

dry=0
extended=0
sdl=1
dir=""
for argument in "$@"; do
  case "$argument" in
    --dry-run) dry=1 ;;
    --with-extended) extended=1 ;;
    --no-sdl) sdl=0 ;;
    -h|--help) sed -n '4,22p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "unknown option '$argument'" >&2; exit 2 ;;
    *) [ -z "$dir" ] || { echo "only one DIR" >&2; exit 2; }; dir=$argument ;;
  esac
done
dir=${dir:-$(dirname "$engine")}

pin() { # pin NAME -> the revision set(CNA_ENGINE_BASELINE_NAME "...") in the baseline file
  sed -n "s/^set(CNA_ENGINE_BASELINE_$1 \"\([0-9a-f]*\)\")/\1/p" "$baseline"
}

run() {
  echo "+ $*"
  [ "$dry" = 1 ] || "$@"
}

fetch() { # fetch NAME PIN
  local name=$1 revision=$2 target=$dir/$1
  [ -n "$revision" ] || { echo "no pinned revision for $name in $baseline" >&2; exit 1; }
  if [ -e "$target" ]; then
    if git -C "$target" rev-parse --git-dir >/dev/null 2>&1; then
      if git -C "$target" cat-file -e "$revision^{commit}" 2>/dev/null; then
        if git -C "$target" merge-base --is-ancestor "$revision" HEAD 2>/dev/null; then
          echo "$name: present at $target ($(git -C "$target" rev-parse --short HEAD), contains the pin ${revision:0:10}); left as it is"
        else
          echo "$name: present at $target ($(git -C "$target" rev-parse --short HEAD), does not contain the pin ${revision:0:10}); left as it is"
        fi
      else
        echo "$name: present at $target, without the pin ${revision:0:10} (fetch it to check); left as it is"
      fi
    else
      echo "$name: $target exists and is not a git checkout; left as it is" >&2
    fi
    return 0
  fi
  echo "$name: cloning into $target at ${revision:0:10}"
  run git clone --quiet "$base/$name.git" "$target"
  run git -C "$target" -c advice.detachedHead=false checkout --quiet "$revision"
  if [ "$name" = cna ] && [ "$sdl" = 1 ]; then
    run git -C "$target" submodule update --init third_party/SDL third_party/SDL_mixer
  fi
}

[ "$dry" = 1 ] && echo "dry run: nothing is changed"
run mkdir -p "$dir"
fetch cna "$(pin CNA)"
fetch sharp-runtime "$(pin SHARP_RUNTIME)"
fetch easy-gl "$(pin EASYGL)"
fetch meta-gl "$(pin METAGL)"
[ "$extended" = 1 ] && fetch cna-extended "$(pin EXTENDED)"
echo "done: $dir"
