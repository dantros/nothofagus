## Sprite Animations

> The Animations subsection in [CLAUDE.md](../CLAUDE.md) is the quick reference
> for the API surface. **This page is the deep dive** — full extended examples,
> methods tables, edge cases, and gotchas. Start here when you need more than
> the cheat sheet.

Nothofagus supports frame-by-frame sprite animation through multi-layer textures and an optional state machine. Each layer of an `IndirectTexture` holds one animation frame. The `AnimationStateMachine` advances the active frame over time and writes the result into `bellota.currentLayer()`.

---

### Multi-layer textures

`IndirectTexture` accepts an optional third argument — the number of layers:

```cpp
// 5-layer 4×4 texture (5 animation frames)
Nothofagus::IndirectTexture tex({4, 4}, glm::vec4(0, 0, 0, 1), 5);
tex.setPallete(palette)
   .setPixels({ /* frame 0 indices */ }, 0)
   .setPixels({ /* frame 1 indices */ }, 1)
   // ...
   .setPixels({ /* frame 4 indices */ }, 4);

Nothofagus::TextureId texId = canvas.addTexture(tex);
```

Add a bellota using that texture normally. The `depthOffset` (third argument to `Bellota`) is unrelated to layer count:

```cpp
// depthOffset = 1, texture has 5 layers — independent values
Nothofagus::BellotaId id = canvas.addBellota({{{x, y}}, texId, /*depthOffset=*/1});
```

---

### AnimationState

`AnimationState` describes a looping frame sequence — which layers to show and for how long each one.

```cpp
#include <animation_state.h>

AnimationState(const std::vector<int>& layers,
               const std::vector<float>& times,
               const std::string& name);
```

- `layers` — texture layer indices to cycle through (in order)
- `times` — duration in ms for each layer; must be the same length as `layers`
- `name` — string identifier (must be unique within a state machine)

The sequence loops: after the last layer the state resets to layer 0 automatically.

```cpp
// Cycle through layers 0→1→2→0→1→2… at 500 ms per frame
Nothofagus::AnimationState walkState({0, 1, 2}, {500.0f, 500.0f, 500.0f}, "walk");
```

Per-frame durations are independent — the `times` vector is parallel to `layers`, not a single global frame rate. Mix-and-match for things like a slow wind-up + quick swing:

```cpp
// Hold layer 0 for 200 ms, flash layer 1 for 50 ms, settle on layer 2 for 400 ms.
Nothofagus::AnimationState attack({0, 1, 2}, {200.0f, 50.0f, 400.0f}, "attack");
```

**Methods:**

| Method | Description |
|---|---|
| `update(float dt)` | Advances the frame timer; call once per frame |
| `int getCurrentLayer() const` | Returns the layer index currently active |
| `std::string getName() const` | Returns the state name |
| `reset()` | Jumps back to the first layer and resets the timer |

---

### AnimationStateMachine

`AnimationStateMachine` manages one or more `AnimationState` objects, drives their timers each frame, and writes the active layer into the bound `Bellota`.

```cpp
#include <animation_state_machine.h>

AnimationStateMachine(Nothofagus::Bellota& bellota);
```

The machine holds a reference to the `Bellota` — its `currentLayer()` is updated automatically by `update()`.

> **Tile-map textures opt out.** If the bellota's texture is in tile-map mode (`IndirectTexture::setMap(mapSize)`), per-cell layer choice is driven by the cell grid and `bellota.currentLayer()` is ignored at draw time. Wire state machines to plain or animation-mode `IndirectTexture`s only.

**Lifetime of `AnimationState` objects.** The machine stores raw `AnimationState*` pointers passed to `addState` — it does **not** own them. They must remain alive *and at the same address* for the entire lifetime of the machine: do not move, reassign, destroy, or reallocate the vector that holds them. The safe pattern is to declare each state as a member variable on the owning object, or as locals in a scope that outlives the machine.

**Methods:**

| Method | Description |
|---|---|
| `addState(name, AnimationState*)` | Register a state |
| `setState(name)` | Set the initial state — **must be called before the first `update()`** |
| `newAnimationTransition(from, transitionName, to)` | Define a named edge in the transition graph |
| `transition(transitionName)` | Fire a named edge from the current state. **Caller must ensure an edge `(currentState, transitionName)` was previously registered via `newAnimationTransition`** — see safety note below |
| `goToState(name)` | Jump directly to a state, bypassing the transition graph; asserts the target state exists and calls `reset()` on it |
| `update(float dt)` | Advance the current state's timer and sync `bellota.currentLayer()` |
| `int getCurrentLayer() const` | Returns the current texture layer |

> **`transition()` safety contract.** The implementation asserts (debugCheck) only that the *current* state exists in the machine, then resolves the new state via `transitions[(currentState, name)]`. If the edge is not registered for the current state, `std::map::operator[]` returns a default-constructed empty string and the next dereference hits a null `AnimationState*` — **undefined behaviour, will crash in debug, likely segfault in release**. If you cannot guarantee the edge exists from the current state at call time, use `goToState(name)` for an unconditional jump that only requires the destination to be registered.

> **Type note.** `Bellota::currentLayer()` is `std::size_t&`, but `AnimationState::getCurrentLayer()` and `AnimationStateMachine::getCurrentLayer()` return `int`. The state machine bridges the two via implicit conversion inside `update()` — user code that reads `getCurrentLayer()` gets an `int`, user code that mutates the bellota's layer directly should treat it as `std::size_t`.

---

### Example: single looping animation

Pared-down version of [examples/hello_animation.cpp](../examples/hello_animation.cpp) — the working example also passes a `depthOffset` and tweaks the bellota scale, omitted here for clarity.

```cpp
// 5-layer 4×4 texture
Nothofagus::IndirectTexture tex({4, 4}, glm::vec4(0,0,0,1), 5);
tex.setPallete(palette)
   .setPixels({/* layer 0 */}, 0)
   .setPixels({/* layer 1 */}, 1)
   .setPixels({/* layer 2 */}, 2)
   .setPixels({/* layer 3 */}, 3)
   .setPixels({/* layer 4 */}, 4);

Nothofagus::TextureId texId = canvas.addTexture(tex);
Nothofagus::BellotaId id    = canvas.addBellota({{{75.0f, 50.0f}}, texId});

// Animation state: all 5 layers, 500 ms each
Nothofagus::AnimationState anim({0, 1, 2, 3, 4},
                                {500.0f, 500.0f, 500.0f, 500.0f, 500.0f},
                                "example");

// State machine bound to the bellota
Nothofagus::AnimationStateMachine machine(canvas.bellota(id));
machine.addState("example", &anim);
machine.setState("example");

canvas.run([&](float dt) {
    canvas.bellota(id).transform().scale() = {10.0f, 10.0f};
    machine.update(dt);
});
```

---

### Example: state machine with transitions

Mirrors `examples/hello_animation_state_machine.cpp`. States represent a character facing up/down with optional left/right lean.

```cpp
// 10-layer texture (frames for W, S, Wleft, Wright, Sleft, Sright — 2 frames each)
Nothofagus::IndirectTexture tex({4, 4}, glm::vec4(0,0,0,1), 10);
// ... setPallete + setPixels for layers 0–9 ...

Nothofagus::TextureId texId = canvas.addTexture(tex);
Nothofagus::BellotaId id    = canvas.addBellota({{{75.0f, 50.0f}}, texId});

// Define states
Nothofagus::AnimationState stateW      ({0},    {500.0f},         "W");
Nothofagus::AnimationState stateS      ({1},    {500.0f},         "S");
Nothofagus::AnimationState stateWleft  ({2, 3}, {500.0f, 500.0f}, "Wleft");
Nothofagus::AnimationState stateWright ({4, 5}, {500.0f, 500.0f}, "Wright");
Nothofagus::AnimationState stateSleft  ({6, 7}, {500.0f, 500.0f}, "Sleft");
Nothofagus::AnimationState stateSright ({8, 9}, {500.0f, 500.0f}, "Sright");

Nothofagus::AnimationStateMachine machine(canvas.bellota(id));
machine.addState("W",      &stateW);
machine.addState("S",      &stateS);
machine.addState("Wleft",  &stateWleft);
machine.addState("Wright", &stateWright);
machine.addState("Sleft",  &stateSleft);
machine.addState("Sright", &stateSright);

// "right" transition — from any facing, lean right
machine.newAnimationTransition("W",      "right", "Wright");
machine.newAnimationTransition("Wleft",  "right", "Wright");
machine.newAnimationTransition("Wright", "right", "Wright");
machine.newAnimationTransition("S",      "right", "Sright");
machine.newAnimationTransition("Sleft",  "right", "Sright");
machine.newAnimationTransition("Sright", "right", "Sright");

// "left" transition
machine.newAnimationTransition("W",      "left", "Wleft");
machine.newAnimationTransition("Wright", "left", "Wleft");
machine.newAnimationTransition("Wleft",  "left", "Wleft");
machine.newAnimationTransition("S",      "left", "Sleft");
machine.newAnimationTransition("Sright", "left", "Sleft");
machine.newAnimationTransition("Sleft",  "left", "Sleft");

machine.setState("W");  // initial state

Nothofagus::Controller controller;
// goToState: direct jump, ignores transition graph
controller.registerAction({Nothofagus::Key::W, Nothofagus::DiscreteTrigger::Press},
    [&]() { machine.goToState("W"); });
controller.registerAction({Nothofagus::Key::S, Nothofagus::DiscreteTrigger::Press},
    [&]() { machine.goToState("S"); });
// transition: fires named edge from current state
controller.registerAction({Nothofagus::Key::A, Nothofagus::DiscreteTrigger::Press},
    [&]() { machine.transition("left"); });
controller.registerAction({Nothofagus::Key::D, Nothofagus::DiscreteTrigger::Press},
    [&]() { machine.transition("right"); });

canvas.run([&](float dt) {
    canvas.bellota(id).transform().scale() = {10.0f, 10.0f};
    machine.update(dt);
}, controller);
```

---

### Manual layer control

For cases that don't need a full state machine, set `bellota.currentLayer()` directly:

```cpp
Nothofagus::Bellota& b = canvas.bellota(id);
b.currentLayer() = 2;  // show layer 2 immediately
```

This is useful for UI elements or objects that switch appearance based on game state rather than time.
