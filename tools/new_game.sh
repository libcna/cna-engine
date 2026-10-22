#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Starts a new game from templates/game (plan.md GAME-004):
#
#   tools/new_game.sh <directory> "<Game Name>"
#
# writes a working CMake project into <directory> (which must not exist or be
# empty): a first-person game on EngineGame, its assets, a README and a
# browser build script. The game finds this engine checkout through a path
# relative to <directory> when both share a parent tree, else an absolute one.
set -euo pipefail

if [ $# -ne 2 ] || [ -z "$2" ]; then
  echo "usage: $0 <directory> \"<Game Name>\"" >&2
  exit 2
fi
engine=$(cd "$(dirname "$0")/.." && pwd)
template=$engine/templates/game
target_dir=$1
name=$2

case "$name" in
  *'"'*|*'\'*) echo "the name goes into C++ strings: no \" or \\ in it" >&2; exit 2 ;;
esac
if [ -e "$target_dir" ] && [ -n "$(ls -A "$target_dir" 2>/dev/null)" ]; then
  echo "$target_dir exists and is not empty" >&2
  exit 1
fi

# "My Space Game" -> target my-space-game, class MySpaceGame.
target=$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]' | sed -e 's/[^a-z0-9]\{1,\}/-/g' -e 's/^-//' -e 's/-$//')
class=$(printf '%s' "$name" | sed -e 's/[^A-Za-z0-9]\{1,\}/ /g' | awk '{
  out = ""
  for (i = 1; i <= NF; ++i) out = out toupper(substr($i, 1, 1)) substr($i, 2)
  print out
}')
if [ -z "$target" ] || [ -z "$class" ]; then
  echo "the name needs at least one letter or digit" >&2
  exit 2
fi
case "$class" in [0-9]*) class="Game$class" ;; esac
case "$class" in *Game) ;; *) class="${class}Game" ;; esac

mkdir -p "$target_dir"
target_abs=$(cd "$target_dir" && pwd)
# The engine relative to the new project when they share the first path
# component (e.g. both under /home/...), else absolute.
relative=$(realpath --relative-to="$target_abs" "$engine" 2>/dev/null || true)
first() { printf '%s' "$1" | cut -d/ -f2; }
if [ -n "$relative" ] && [ "$(first "$target_abs")" = "$(first "$engine")" ]; then
  engine_root="\${CMAKE_CURRENT_SOURCE_DIR}/$relative"
else
  engine_root=$engine
fi

# sed replacement text: escape \, & and the delimiter |.
escape() { printf '%s' "$1" | sed -e 's/[\\&|]/\\&/g'; }
cp -R "$template/." "$target_abs/"
while IFS= read -r -d '' file; do
  case "$file" in *.png) continue ;; esac
  sed -i \
    -e "s|@GAME_NAME@|$(escape "$name")|g" \
    -e "s|@GAME_TARGET@|$(escape "$target")|g" \
    -e "s|@GAME_CLASS@|$(escape "$class")|g" \
    -e "s|@ENGINE_ROOT@|$(escape "$engine_root")|g" \
    "$file"
done < <(find "$target_abs" -type f -print0)
chmod +x "$target_abs/tools/build_web.sh"

if grep -rIl '@GAME_\|@ENGINE_ROOT@' "$target_abs" > /dev/null; then
  echo "internal error: a template token was left in $(grep -rIl '@GAME_\|@ENGINE_ROOT@' "$target_abs" | head -1)" >&2
  exit 1
fi

cat <<EOF
created $name in $target_abs (target $target, class $class)

  cd $target_dir
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel 8
  ./build/$target

The dependencies are expected beside the engine: $engine/tools/fetch_dependencies.sh
EOF
