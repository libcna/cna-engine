# A 3D game with CNA and cna-engine

This walks from an empty directory to a game you can play on the desktop and
in a browser, then through each part of the engine a game usually needs. Every
step points at code that builds and runs: the project template
(`templates/game`), and three examples that run headless as tests:

| Example | Shows |
| --- | --- |
| `examples/first_person` | walking with collisions, stairs, picking a thing up with a ray, a HUD (strict profile) |
| `examples/third_person` | a glTF character animated and walked, a camera that follows (enhanced) |
| `examples/day_night` | the sky and the light over a day, the sun's shadows, a lamp at night (enhanced) |

*Strict* code uses only XNA 4.0's API (plus Sharp Runtime and the standard
library); *enhanced* code may use CNA's extensions too (PBR, shadows, the
browser's pointer lock). A game picks one; the template is enhanced.

## 1. Get the code

You need CMake 3.21+, Ninja or Make, a C++23 compiler (GCC 13+ or Clang 17+),
and for the browser the Emscripten SDK.

```sh
git clone https://github.com/libcna/cna-engine.git
cna-engine/tools/fetch_dependencies.sh      # CNA, Sharp Runtime, easy-gl, meta-gl beside it
```

The script checks out the revisions the engine is tested with
(`cmake/CnaEngineBaseline.cmake`) and CNA's SDL sources; checkouts that are
already there are reported and left alone. `--dry-run` shows what it would do.

## 2. Start a game

```sh
cna-engine/tools/new_game.sh my-game "My Game"
cd my-game
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8
./build/my-game
```

You walk around a field of crates in the first person: click to look around,
WASD, Shift to run, Space to jump, Escape for the menu. `tools/build_web.sh
--serve` builds the same game for the browser and serves it on
<http://localhost:8080/>.

The project is small on purpose:

- `CMakeLists.txt` includes the engine's `cmake/CnaEngineGame.cmake` and makes
  one `cna_engine_add_game()` call. That call builds the executable, copies
  `assets/` beside it (or preloads it in the browser), and under Emscripten
  writes `web-package/` with the page, the wasm and the data.
- `src/Game.cpp` is the whole game.
- `assets/` holds what the game loads through XNA's content manager.

## 3. The game class

A game derives from `CnaEngine::App::EngineGame`, an XNA `Game` with the
engine already wired: the device, an action map, a fixed-step clock, a camera
kept at the window's aspect, the forward renderer, statistics, screenshots and
the player's preferences. It overrides what it needs:

```cpp
class MyGame final : public CnaEngine::App::EngineGame
{
    void LoadGame() override;                  // the device and assets/ are ready
    void FixedUpdate(double step) override;    // the simulation, in fixed steps
    void UpdateFrame(double seconds) override; // once a frame: camera, animation, sound
    void Submit(CnaEngine::App::Frame& frame) override;  // what to draw
    void DrawOverlay(GraphicsDevice& device) override;   // the HUD, after the world
};

int main(int argc, char** argv) { return CnaEngine::App::RunGame<MyGame>(argc, argv); }
```

Built in: Escape (quit, or release the mouse), F3 (statistics in the title),
F11 (full screen), F12 (screenshot). `--help` lists the command line, among it
the automated runs every engine game has:

```sh
./build/my-game --frames 120 --lockstep --screenshot shot.png
```

which draw 120 frames of exactly one simulation step each, save the last one
and exit. That is how the examples and the template are tested; a game's own
test overrides `ReadInput()` to play a script (see `examples/first_person`).

## 4. Drawing things

```cpp
Graphics::MeshBuilder builder;
builder.AddBox(Vector3(-0.5f, 0, -0.5f), Vector3(0.5f, 1, 0.5f));
box_ = Graphics::GpuMesh::Upload(device, builder.Take(), Graphics::VertexLayout::PositionNormalTexture());
texture_ = std::make_unique<Texture2D>(getContentProperty().Load<Texture2D>("crate"));  // assets/crate.png
material_.baseTexture = texture_.get();

void MyGame::Submit(App::Frame& frame)
{
    frame.Add(box_, material_, Matrix::CreateTranslation(position_));
}
```

Items are culled against the camera and drawn by the forward renderer in
buckets (opaque, alpha-tested, transparent, overlay). `docs/guide-strict.md`
and `docs/guide-enhanced.md` cover materials, instancing, levels of detail
and the renderer's custom passes.

## 5. Input

Actions and axes have names, so players can rebind them:

```cpp
moveForward_ = Actions().AddAxis("MoveForward");
Actions().BindAxis(moveForward_, Input::AxisBinding::ForKeys(Keys::S, Keys::W));
Actions().BindAxis(moveForward_, Input::AxisBinding::ForGamePad(Input::GamePadAxis::LeftStickY));
jump_ = Actions().AddAction("Jump");
Actions().Bind(jump_, Input::Binding::ForKey(Keys::Space));
```

Read them with `Actions().Axis(...)`, `Pressed(...)`, `Held(...)`. Bindings
the player changed are kept in `bindings.ini` beside the preferences.

Mouse look: `CaptureMouse(true)` and `MouseDelta()`. The strict default puts
the cursor back in the middle each frame; a browser needs the pointer lock of
`Enhanced::RelativeMouseCapture`, which the template installs.

## 6. Walking

```cpp
Collision::StaticWorld world_;                    // boxes, triangles, cylinders, a height field
Collision::CharacterController body_;             // an upright capsule

world_.AddBox(BoundingBox(Vector3(-40, -1, -40), Vector3(40, 0, 40)));   // the ground
body_.Teleport(world_, Vector3(0, 0, 2));

void MyGame::FixedUpdate(double step)
{
    const auto input = Graphics::ReadCameraInput(Actions(), axes_, float(step));
    body_.Step(world_, Collision::WishVelocity(input.move, yaw_, 3.5f), jumpQueued_, float(step));
}
```

The body slides along walls, climbs steps up to 0.35 m, keeps to slopes up to
45 degrees, falls and jumps; all of it is in `CharacterSettings`. The camera
goes to `body_.Eye()`; `body_.StrideTaken()` is the moment for a footstep and
`body_.HeadBob(0.02f)` a gentle bob. To use something, cast a ray from the eye:

```cpp
const Ray ray(MainCamera().Position(), MainCamera().Forward());
if (const auto hit = world_.Raycast(ray, 3.0f); hit && hit->tag == CrateTag) { ... }
```

## 7. Sky, shadows and lamps

```cpp
sky_ = std::make_unique<Graphics::SkyDome>(device);
sky_->AttachTo(Renderer());                                   // drawn before everything
time_.hours = 16.0;                                           // Graphics::TimeOfDay

void MyGame::UpdateFrame(double seconds)
{
    time_.Advance(seconds, 30.0);                             // a day in 48 minutes
    const auto colors = palette_.At(float(time_.Sun().elevationDegrees));
    sky_->SetColors(colors, time_.DirectionToSun());
    Graphics::ApplySkyLighting(Renderer().Lighting(), colors, time_.DirectionToSun(), 25.0f, 70.0f);
}
```

Shadows of the sun (enhanced; the map is drawn with XNA's effects and received
through CNA's shadow extension):

```cpp
Enhanced::EnableShadows(*this, {.size = 1024, .radius = 25.0f});
```

Lamps: `Enhanced::InstallLocalLights(Renderer())` returns a binder whose
`lights` you fill each frame; every lit item gets the strongest lamp near it.
Strict games draw `Graphics::LightPools` on the ground instead.

## 8. Models and animation

CNA loads `.glb`/`.gltf` (and XNB models) through the content manager at run
time; the engine turns them into render items and animates them:

```cpp
model_ = std::make_unique<Model>(getContentProperty().Load<Model>("figure"));     // assets/figure.gltf
view_ = std::make_unique<Graphics::ModelView>(*model_, Enhanced::PbrMaterialFromEffect);
skeleton_ = Enhanced::SkeletonFromSkinningData(*Enhanced::FindSkinningData(*model_));
clips_ = Enhanced::ClipsFromSkinningData(*Enhanced::FindSkinningData(*model_));
animator_ = std::make_unique<Animation::Animator>(skeleton_);

animator_->Play(walkClip, {.fadeSeconds = 0.25f});            // cross-fades from what played
animator_->Update(seconds);
frame.Add(*view_, Matrix::CreateTranslation(position), &animator_->SkinPalette());
```

PBR parts need `Enhanced::PbrMaterialBinder` as the renderer's binder.
Skeletons and clips can also be built in code (`Animation::Skeleton::FromBindPose`).

## 9. Sound

```cpp
Audio::SoundSystem sound_;      // silent, not failing, without an audio device
Audio::SoundBank steps_;
steps_.Add("ground", *step1);   // SoundEffects loaded from assets/*.wav

if (body_.StrideTaken()) {
    sound_.Play(*steps_.Pick("ground", seed_), {.category = "footsteps", .position = body_.Feet()});
}
sound_.SetListener(Audio::ListenerFrom(MainCamera()));
sound_.Update(seconds);
```

Voices fade with distance and pan by direction; loops wait when there are
more sounds than voices; `SetCategoryVolume` and `SetMasterVolume` mix them.

## 10. HUD and menus

```cpp
font_ = std::make_unique<UI::BuiltInFont>(device);      // ASCII, no file needed
painter_ = std::make_unique<UI::Painter>(device);       // a 720-unit-tall virtual screen

void MyGame::DrawOverlay(GraphicsDevice&)
{
    painter_->Begin();
    painter_->Text(*font_, "Crates: 3", UI::Anchor::TopLeft);
    menus_.Draw(*painter_, *font_);
    painter_->End();
}
```

A menu is a list of buttons, choices and sliders driven by actions
(`UI::MenuActions::Declare`) and the mouse; a `UI::MenuStack` holds the open
ones. `UI::AddQualitySettings` makes settings rows from a `QualitySchema`,
`UI::RebindRows` rows that rebind actions. The template's pause menu uses all
of them. Text beyond ASCII needs a `SpriteFont` from content (`UI::SpriteFontText`).

## 11. After the frame

`EnablePostProcessing()` draws the world off screen and through passes:
`Graphics::ColorGradePass`, `FadePass`, `VignettePass`, or your own effect.

## 12. Saving

Preferences (`Preferences()`, saved by `SavePreferences()`) hold settings.
Game state goes to `SaveSlots`:

```cpp
SaveSlots slots(UserDataPath("my-game") / "saves");
slots.Save("slot-1", stateText, /*version*/ 1, "Level 2, 15 crates");
if (const auto loaded = slots.Load("slot-1")) { ... loaded->content ... }
```

A save is written whole or not at all; a damaged one falls back to the one
before it.

## 13. Particles

With cna-extended (`CNA_ENGINE_WITH_EXTENDED`), `Extended::ParticleBillboards`
draws a `ParticleEffect3DEXT`'s particles as camera-facing quads in the frame.

## Where next

- `docs/api-status.md` — every facility, its status and where it came from;
- `docs/guide-strict.md`, `docs/guide-enhanced.md` — the renderer in depth;
- `docs/game-facilities-survey.md` — how the simulators did each of these
  things before the engine had them;
- `plan.md` Phase 7 — what was built, how it was checked, and what was left out.
