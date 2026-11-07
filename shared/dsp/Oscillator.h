#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sdsp
{
constexpr float pi    = 3.14159265358979f;
constexpr float twoPi = 2.0f * pi;

/** MIDI note (fractional) -> Hz. */
inline float noteToHz (float note) { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

/** Polynomial band-limited step: removes most of the aliasing from hard edges. */
inline float polyBlep (float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

/** Cheap, fast white noise (xorshift32). */
class Noise
{
public:
    explicit Noise (uint32_t seed = 0x12345678u) : state (seed ? seed : 1u) {}
    void seed (uint32_t s) { state = s ? s : 1u; }

    /** Uniform in [-1, 1). */
    float next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) (int32_t) state * (1.0f / 2147483648.0f);
    }

    /** Uniform in [0, 1). */
    float next01() { return 0.5f * next() + 0.5f; }

private:
    uint32_t state;
};

/** Band-limited virtual-analog oscillator. */
class Oscillator
{
public:
    enum Shape { Saw = 0, Pulse, Triangle, Sine };

    void setPhase (float p) { phase = p - std::floor (p); }
    void setFrequency (float hz, float sampleRate) { dt = std::clamp (hz / sampleRate, 0.0f, 0.49f); }

    float process (int shape, float pulseWidth = 0.5f)
    {
        float out;
        switch (shape)
        {
            case Saw:      out = saw(); break;
            case Pulse:    out = pulse (pulseWidth); break;
            case Triangle: out = 1.0f - 4.0f * std::abs (phase - 0.5f); break;
            default:       out = std::sin (twoPi * phase); break;
        }
        advance();
        return out;
    }

    /** Continuous blend from saw (0) to square (1), as on the classic acid bass box. */
    float processSawToSquare (float morph)
    {
        const float s = saw();
        const float q = pulse (0.5f);
        advance();
        return s + (q - s) * morph;
    }

private:
    float saw() const { return 2.0f * phase - 1.0f - polyBlep (phase, dt); }

    float pulse (float pw) const
    {
        float t2 = phase + 1.0f - pw;
        if (t2 >= 1.0f)
            t2 -= 1.0f;
        return (phase < pw ? 1.0f : -1.0f) + polyBlep (phase, dt) - polyBlep (t2, dt);
    }

    void advance()
    {
        phase += dt;
        if (phase >= 1.0f)
            phase -= 1.0f;
    }

    float phase = 0.0f, dt = 0.0f;
};

/** Global modulation LFO. */
class Lfo
{
public:
    enum Shape { Sine = 0, Triangle, Saw, Square, SampleHold };

    void prepare (double sampleRate) { sr = (float) sampleRate; phase = 0.0f; }

    float process (int shape, float rateHz)
    {
        float out;
        switch (shape)
        {
            case Sine:     out = std::sin (twoPi * phase); break;
            case Triangle: out = 1.0f - 4.0f * std::abs (phase - 0.5f); break;
            case Saw:      out = 2.0f * phase - 1.0f; break;
            case Square:   out = phase < 0.5f ? 1.0f : -1.0f; break;
            default:       out = held; break;
        }
        phase += rateHz / sr;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            held = noise.next();
        }
        return out;
    }

private:
    float sr = 44100.0f, phase = 0.0f, held = 0.0f;
    Noise noise { 0xC0FFEEu };
};
} // namespace sdsp
