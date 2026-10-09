#pragma once

// Accumulator for a fixed-rate simulation step ("fix your timestep").
//
// Rendering runs at whatever rate the display allows; physics/gameplay should advance in equal
// slices so the result doesn't depend on frame rate. Each frame, feed in the real frame time
// and run the simulation `advance()` times.
class FixedTimestep {
  public:
    // pStep: seconds per simulation tick. pMaxSteps: cap per frame, so a long frame can't
    // trigger ever-more catch-up work ("spiral of death").
    explicit FixedTimestep(float pStep, int pMaxSteps = 5) : stepSeconds(pStep), maxSteps(pMaxSteps) {}

    // Adds the frame's real duration and returns how many fixed ticks to run this frame.
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

    float step() const { return stepSeconds; }

    // How far we are between the last tick and the next, 0..1. Used to interpolate rendering
    // between two simulation states.
    float alpha() const { return accumulator / stepSeconds; }

  private:
    float stepSeconds;
    int   maxSteps;
    float accumulator = 0.0f;
};
