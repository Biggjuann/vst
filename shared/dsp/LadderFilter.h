#pragma once

#include "Oscillator.h"

namespace sdsp
{
/** Rational tanh approximation; accurate enough for saturation, much cheaper than std::tanh. */
inline float fastTanh (float x)
{
    x = std::clamp (x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/**
    Four-pole zero-delay-feedback (TPT) transistor-ladder style filter.

    The feedback path runs through a one-pole high-pass, solved implicitly together
    with the ladder so the loop stays delay-free:
      - set it low (~10 Hz) for a classic ladder that thins out the bass at high resonance
      - set it higher (~150 Hz) for the acid-box character where bass survives resonance
    The input to the ladder is saturated, which gives the "analog" growl when driven.
*/
class LadderFilter
{
public:
    enum Mode { LP24 = 0, LP12, BP12, HP24 };

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        reset();
        setFeedbackHighpass (fbHighpassHz);
        setCutoff (1000.0f);
    }

    void reset()
    {
        for (auto& v : s)
            v = 0.0f;
        hpState = 0.0f;
    }

    void setFeedbackHighpass (float hz)
    {
        fbHighpassHz = hz;
        const float gh = std::tan (pi * std::min (hz, 0.45f * sr) / sr);
        Gh = gh / (1.0f + gh);
    }

    void setCutoff (float hz)
    {
        hz = std::clamp (hz, 10.0f, 0.45f * sr);
        g = std::tan (pi * hz / sr);
        G = g / (1.0f + g);
    }

    /** 0..1, where 1.0 is roughly the self-oscillation point. */
    void setResonance (float r) { k = 4.0f * std::clamp (r, 0.0f, 1.05f); }

    float process (float x, int mode = LP24)
    {
        const float a  = 1.0f / (1.0f + g);
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;

        // Contribution of each stage's state to the 4th-stage output.
        const float S = G3 * s[0] * a + G2 * s[1] * a + G * s[2] * a + s[3] * a;

        // Solve u = x - k * HP(y4), with y4 = G^4 u + S and HP(y) = (1 - Gh)(y - hpState)
        const float kc = k * (1.0f - Gh);
        float u = (x - kc * (S - hpState)) / (1.0f + kc * G4);
        u = fastTanh (u);

        float y = u;
        float ys[4];
        for (int i = 0; i < 4; ++i)
        {
            const float v = (y - s[i]) * G;
            y = v + s[i];
            s[i] = y + v;
            ys[i] = y;
        }

        // Advance the feedback high-pass state (one-pole TPT low-pass of y4).
        const float vh = (ys[3] - hpState) * Gh;
        const float lp = vh + hpState;
        hpState = lp + vh;

        switch (mode)
        {
            case LP12: return ys[1];
            case BP12: return 2.0f * (ys[0] - ys[1]);
            case HP24: return u - 4.0f * ys[0] + 6.0f * ys[1] - 4.0f * ys[2] + ys[3];
            default:   return ys[3];
        }
    }

private:
    float sr = 44100.0f, g = 0.0f, G = 0.0f, Gh = 0.0f, k = 0.0f;
    float fbHighpassHz = 10.0f;
    float s[4] {}, hpState = 0.0f;
};
} // namespace sdsp
