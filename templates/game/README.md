# @GAME_NAME@

A 3D game on [CNA](https://github.com/libcna/cna) (XNA 4.0 in C++) and
[cna-engine](https://github.com/libcna/cna-engine), started from the
engine's game template.

## Build and run

CNA, Sharp Runtime and easy-gl are expected beside cna-engine; the engine's
`tools/fetch_dependencies.sh` puts them there at the revisions it is tested
with. You need CMake 3.21+ and a C++23 compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8
./build/@GAME_TARGET@
```

`--help` lists the options every engine game has. An automated run draws a
number of frames, saves the last one and exits:

```sh
./build/@GAME_TARGET@ --frames 120 --lockstep --screenshot shot.png
```

### In a browser

```sh
tools/build_web.sh --serve    # needs the Emscripten SDK (~/emsdk or $EMSDK)
```

then open <http://localhost:8080/>. Desktop options go in the address:
`?arg=--width&arg=800`.

## Controls

| Input | Action |
| --- | --- |
| Click | Capture the mouse for looking around |
| Mouse, right stick | Look |
| W A S D, left stick | Walk |
| Shift | Run |
| Space, A button | Jump |
| Esc | Release the mouse; again: quit (not in a browser) |
| F3 | Statistics in the window's title |
| F11 | Full screen |
| F12 | Screenshot |

Key bindings can be changed in `bindings.ini` beside the preferences
(`~/.config/@GAME_TARGET@/` on Linux).

## Where things are

- `src/Game.cpp` — the game: input, the world (each block is a box to
  collide with and a cube to draw), the sky and the sun's shadows over the
  day, the body (`CharacterController`: walls, stairs, gravity, jumps),
  footsteps (`SoundSystem`), drawing.
- `assets/` — copied beside the executable (preloaded in the browser);
  loaded through XNA's content manager, e.g. `Load<Texture2D>("crate")`.
- `CMakeLists.txt` — one `cna_engine_add_game()` call.

The engine's `docs/tutorial.md` continues from here.
