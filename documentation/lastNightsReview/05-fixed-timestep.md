# 5. `FixedTimestep`

Files: `core/include/FixedTimestep.hpp`, the loop in `App::run` (`core/src/App.cpp`),
`App.hpp` (`FIXED_STEP`, `MAX_DT`, `fixedClock`)

## Big picture

Rendering happens as often as the display allows (60, 144, 240 times a second). **Physics and
gameplay** should not: if they advance by "however long the last frame took," the same game plays
differently at different frame rates (and physics can become unstable with big steps).

The fix is to let simulation advance in **equal slices** (here 1/60 s) no matter how frames go:

```
frame time:    4 ms   4 ms   4 ms   4 ms   4 ms   (240 fps)
accumulator:   4      8      12 -> tick, 2 left   ...      one tick every ~4 frames

frame time:    25 ms  (a slow frame)
accumulator:   25 -> tick, tick, 8 left      two ticks to catch up
```

Each real frame adds its duration to an **accumulator**; every time the accumulator holds a whole
step, one simulation tick runs and the step is subtracted.

## Steps

### Step 1: the class

```cpp
class FixedTimestep {
  public:
    explicit FixedTimestep(float pStep, int pMaxSteps = 5)
        : stepSeconds(pStep), maxSteps(pMaxSteps) {}

    int   advance(float pFrameDt);
    float step() const  { return stepSeconds; }
    float alpha() const { return accumulator / stepSeconds; }

  private:
    float stepSeconds;
    int   maxSteps;
    float accumulator = 0.0f;
};
```

It is **header-only and clock-free**: it takes the frame time as an argument and never reads a
clock itself. That is what makes it testable without a window (file 6).

### Step 2: `advance()`

```cpp
int advance(float pFrameDt) {
    accumulator += pFrameDt;
    int steps = 0;
    while (accumulator >= stepSeconds && steps < maxSteps) {
        accumulator -= stepSeconds;
        ++steps;
    }
    if (accumulator >= stepSeconds) {
        accumulator = 0.0f; // hit the cap: drop the backlog instead of chasing it
    }
    return steps;
}
```

Walk-through with `step = 0.01`:

| Call | Accumulator after adding | Ticks | Left over |
|---|---|---|---|
| `advance(0.004)` | 0.004 | 0 | 0.004 |
| `advance(0.0071)` | 0.0111 | 1 | 0.0011 |
| `advance(0.0301)` | 0.0312 | 3 | 0.0012 |

The **remainder is carried**, so no time is lost between frames.

### Step 3: the cap (spiral of death)

If a frame takes 10 seconds (a debugger pause), the naive loop would run 1,000 ticks, which take
even longer, which makes the next frame longer, and so on. The cap does two things:

1. `steps < maxSteps` stops the loop after at most 5 ticks.
2. The `if` afterwards **drops** the remaining backlog, so the next frame starts fresh.

The price: after a huge hitch, the simulation runs slower than real time for that moment rather
than trying to catch up. That is the right trade for games.

### Step 4: `alpha()`

```cpp
float alpha() const { return accumulator / stepSeconds; }   // 0..1
```

How far we are between the last tick and the next. Renderers use it to **interpolate** drawing
between two simulation states, so motion looks smooth even though physics ticks at 60 Hz while the
screen refreshes at 240 Hz. Nothing uses it yet.

### Step 5: how `App` uses it

```cpp
// App.hpp
static constexpr float MAX_DT     = 0.1f;         // seconds; longest frame we will react to
static constexpr float FIXED_STEP = 1.0f / 60.0f; // seconds per fixed simulation tick
FixedTimestep fixedClock{FIXED_STEP};

// App::run
const float dt = std::min(static_cast<float>(now - lastFrameTime), MAX_DT);
...
const int ticks = fixedClock.advance(dt);
for (int i = 0; i < ticks; ++i) {
    fixedUpdate(fixedClock.step());   // currently an empty function
}
cameraController.update(window.handle(), camera, input, dt);
```

There are **two protections** against long frames, at different layers: `MAX_DT` (0.1 s) clamps
the frame time before anything uses it, and the tick cap limits catch-up work.

### Step 6: why the camera is NOT in the fixed step

The camera is sampled **once per rendered frame**, with the real `dt`. If it moved only at 60 Hz
ticks on a 240 Hz display, it would visibly step four frames at a time unless you also
interpolated its position. Input-to-screen latency also matters most for the camera. So:

| System | Rate | Why |
|---|---|---|
| Camera, mouse look | every rendered frame | responsiveness, smoothness |
| Physics, gameplay rules (future) | fixed ticks | determinism, stability |
| Rendering | every frame | display refresh |

`fixedUpdate` is an **empty hook** today. It exists so physics has a place to go.

## Gotchas

- `float` accumulation drifts slightly; fine for this use, but long-running simulations often use
  integer ticks or a `double`.
- Ticks happen **before** the camera update in a frame, but the camera doesn't depend on them.
- The test "same total time split differently gives the same tick count" checks that remainders
  carry correctly.

## Check yourself

1. With `step = 0.01`, what does `advance(0.0301)` return when the accumulator already holds
   `0.0011`, and what is left?
2. Why is the backlog dropped when the cap is hit, instead of kept?
3. Why is `FixedTimestep` given the frame time as an argument and not allowed to read the clock?
4. Name two systems that belong in the fixed step and one that doesn't, and say why.
5. What are the two separate protections against a very long frame, and where does each live?

<details>
<summary>Answer key</summary>

1. The accumulator becomes 0.0312, so it returns 3 ticks and 0.0012 is left over.
2. Keeping it would make later frames run even more catch-up ticks, which take longer, producing
   more backlog: the spiral of death. Dropping it lets the simulation fall slightly behind real
   time once and recover.
3. So it can be tested without a window or real time (tests feed in frame times), and so the same
   class works with any clock.
4. Physics and gameplay rules belong in the fixed step (determinism, stability); the camera does
   not (it must respond every rendered frame).
5. `MAX_DT` clamps the frame time in `App::run`; the `maxSteps` cap and backlog drop live in
   `FixedTimestep::advance`.

</details>

## Rewrite it yourself

Rewrite `advance()` from the table in step 2 and the cap description in step 3. Then run
`./build/engine_tests`: `testFixedTimestep` covers tick counts, the cap and the split-frame case.
