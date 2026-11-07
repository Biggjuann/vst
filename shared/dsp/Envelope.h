#pragma once

#include <algorithm>
#include <cmath>

namespace sdsp
{
/**
    ADSR with RC-capacitor style curves, like analog envelope generators:
    the attack charges toward an overshoot target (so it is slightly convex and hits 1.0
    in the attack time), decay and release are exponential. Retriggering starts from the
    current level, so there are no clicks.
*/
class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        reset();
    }

    void reset()
    {
        stage = Stage::Idle;
        value = 0.0f;
    }

    /** Times in seconds; sustain 0..1. */
    void setParameters (float attack, float decay, float sustainLevel, float release)
    {
        constexpr float ln3    = 1.0986123f;
        constexpr float ln1000 = 6.9077553f;
        attackCoef  = std::exp (-ln3 / (std::max (attack, 0.0005f) * sr));
        decayCoef   = std::exp (-ln1000 / (std::max (decay, 0.001f) * sr));
        releaseCoef = std::exp (-ln1000 / (std::max (release, 0.001f) * sr));
        sustain     = std::clamp (sustainLevel, 0.0f, 1.0f);
    }

    void noteOn()  { stage = Stage::Attack; }
    void noteOff() { if (stage != Stage::Idle) stage = Stage::Release; }

    bool isActive() const { return stage != Stage::Idle; }
    float getValue() const { return value; }

    float next()
    {
        switch (stage)
        {
            case Stage::Attack:
                value = 1.5f + (value - 1.5f) * attackCoef;
                if (value >= 1.0f)
                {
                    value = 1.0f;
                    stage = Stage::Decay;
                }
                break;

            case Stage::Decay:
                value = sustain + (value - sustain) * decayCoef;
                if (value - sustain < 1.0e-4f)
                    stage = Stage::Sustain;
                break;

            case Stage::Sustain:
                // Follow sustain-knob moves smoothly.
                value = sustain + (value - sustain) * decayCoef;
                break;

            case Stage::Release:
                value *= releaseCoef;
                if (value < 1.0e-5f)
                {
                    value = 0.0f;
                    stage = Stage::Idle;
                }
                break;

            case Stage::Idle:
                break;
        }
        return value;
    }

private:
    Stage stage = Stage::Idle;
    float sr = 44100.0f, value = 0.0f, sustain = 1.0f;
    float attackCoef = 0.0f, decayCoef = 0.0f, releaseCoef = 0.0f;
};
} // namespace sdsp
