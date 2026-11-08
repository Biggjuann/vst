#pragma once

#include <algorithm>
#include <cmath>

#include "dsp/Oscillator.h"

/** Small building blocks for analog-style drum voices. */
namespace drum
{
using sdsp::pi;
using sdsp::twoPi;

constexpr float ln1000 = 6.9077553f; // -60 dB

/** Exponential decay envelope; times are "time to fall 60 dB" like a drum ring-out. */
struct Decay
{
    float value = 0.0f, coef = 0.0f;
    void set (float t60Seconds, float sr) { coef = std::exp (-ln1000 / (std::max (t60Seconds, 0.0005f) * sr)); }
    void trigger (float level = 1.0f) { value = level; }
    float next()
    {
        const float out = value;
        value *= coef;
        if (value < 1.0e-6f)
            value = 0.0f;
        return out;
    }
    bool active() const { return value > 0.0f; }
};

/** Zero-delay-feedback state variable filter (LP / BP / HP). */
struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f;
    float lp = 0.0f, bp = 0.0f, hp = 0.0f;

    void set (float hz, float q, float sr)
    {
        const float g = std::tan (pi * std::clamp (hz, 10.0f, 0.48f * sr) / sr);
        k = 1.0f / std::max (q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0.0f; }

    void process (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        bp = v1;
        hp = x - k * v1 - v2;
    }
    float lowpass (float x)  { process (x); return lp; }
    float bandpass (float x) { process (x); return bp; }
    float highpass (float x) { process (x); return hp; }
};

/** One-pole low-pass / high-pass. */
struct OnePole
{
    float z = 0.0f, a = 0.0f;
    void set (float hz, float sr) { a = 1.0f - std::exp (-twoPi * std::clamp (hz, 1.0f, 0.49f * sr) / sr); }
    float lowpass (float x)  { z += (x - z) * a; return z; }
    float highpass (float x) { z += (x - z) * a; return x - z; }
};

/** A damped sine "resonator" - what a bridged-T network does when it's pinged. */
struct Ring
{
    float phase = 0.0f;
    Decay env;
    void trigger (float level, float startPhase = 0.0f) { env.trigger (level); phase = startPhase; }
    float next (float hz, float sr)
    {
        const float out = std::sin (twoPi * phase) * env.next();
        phase += hz / sr;
        if (phase >= 1.0f)
            phase -= 1.0f;
        return out;
    }
    bool active() const { return env.active(); }
};

/** Band-limited square for the metallic oscillator banks. */
struct Square
{
    sdsp::Oscillator osc;
    float next (float hz, float sr)
    {
        osc.setFrequency (hz, sr);
        return osc.process (sdsp::Oscillator::Pulse, 0.5f);
    }
};

inline float softClip (float x)
{
    x = std::clamp (x, -3.0f, 3.0f);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}
} // namespace drum
