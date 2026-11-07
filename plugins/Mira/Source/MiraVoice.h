#pragma once

#include "dsp/Envelope.h"
#include "dsp/LadderFilter.h"
#include "dsp/Oscillator.h"

/** Settings shared by all voices, refreshed once per audio block. */
struct MiraVoiceParams
{
    int shape1 = 0, shape2 = 0, filterMode = 0;
    float osc1Offset = 0.0f, osc2Offset = 0.0f; // semitones
    float pulseWidth = 0.5f;
    float level1 = 0.8f, level2 = 0.6f, levelSub = 0.0f, levelNoise = 0.0f;
    float resonance = 0.2f, driveGain = 1.0f, driveComp = 1.0f;
    float keytrack = 0.5f, envAmount = 0.4f, velSens = 0.5f, drift = 0.3f;
    float glideCoef = 1.0f;
    float lfoFilter = 0.0f, lfoPw = 0.0f;
};

/** Per-sample global modulation. */
struct MiraSampleMods
{
    float cutoffHz = 1000.0f;
    float lfo = 0.0f;
    float pitchOffset = 0.0f; // bend + vibrato, semitones
};

class MiraVoice
{
public:
    void prepare (double sampleRate, uint32_t seed)
    {
        sr = (float) sampleRate;
        rng.seed (seed);
        noise.seed (seed * 2654435761u + 1u);
        filter.prepare (sampleRate);
        filter.setFeedbackHighpass (8.0f);
        ampEnv.prepare (sampleRate);
        filterEnv.prepare (sampleRate);
        driftCoef = 1.0f - std::exp (-1.0f / (0.4f * sr));
        staticDetune = rng.next();      // each voice is slightly, permanently "out of tune"
        staticCutoff = rng.next();
        note = -1;
        keyDown = sustained = false;
    }

    void setEnvelopes (float fa, float fd, float fs, float fr, float aa, float ad, float as, float ar)
    {
        filterEnv.setParameters (fa, fd, fs, fr);
        ampEnv.setParameters (aa, ad, as, ar);
    }

    bool isActive() const { return ampEnv.isActive(); }
    float getPitch() const { return pitch; }

    /** glideFrom < 0 means jump straight to the note. */
    void start (int midiNote, float vel, float glideFrom, bool retrigger, uint64_t startOrder, float panPos)
    {
        if (! isActive())
        {
            // Free-running analog oscillators: random phase each time the voice wakes up.
            osc1.setPhase (rng.next01());
            osc2.setPhase (rng.next01());
            sub.setPhase (rng.next01());
        }

        note = midiNote;
        velocity = vel;
        keyDown = true;
        sustained = false;
        order = startOrder;
        targetPitch = (float) midiNote;
        pitch = glideFrom >= 0.0f ? glideFrom : targetPitch;

        const float angle = (panPos + 1.0f) * 0.25f * sdsp::pi;
        panL = std::cos (angle) * 1.41421356f;
        panR = std::sin (angle) * 1.41421356f;

        if (retrigger)
        {
            ampEnv.noteOn();
            filterEnv.noteOn();
        }
    }

    void release()
    {
        ampEnv.noteOff();
        filterEnv.noteOff();
    }

    void render (float& outL, float& outR, const MiraVoiceParams& vp, const MiraSampleMods& m)
    {
        if (! isActive())
            return;

        pitch += (targetPitch - pitch) * vp.glideCoef;

        // Slow random wander of pitch and cutoff, like warm analog circuitry.
        if (--driftCounter <= 0)
        {
            driftCounter = (int) (sr * (0.15f + 0.35f * rng.next01()));
            driftPitchTarget = rng.next();
            driftCutoffTarget = rng.next();
        }
        driftPitch  += (driftPitchTarget - driftPitch) * driftCoef;
        driftCutoff += (driftCutoffTarget - driftCutoff) * driftCoef;

        const float basePitch = pitch + m.pitchOffset + vp.drift * (0.08f * driftPitch + 0.06f * staticDetune);
        const float hz1 = sdsp::noteToHz (basePitch + vp.osc1Offset);
        osc1.setFrequency (hz1, sr);
        osc2.setFrequency (sdsp::noteToHz (basePitch + vp.osc2Offset), sr);
        sub.setFrequency (hz1 * 0.5f, sr);

        const float pw = std::clamp (vp.pulseWidth + m.lfo * vp.lfoPw * 0.45f, 0.05f, 0.95f);
        const float x = osc1.process (vp.shape1, pw) * vp.level1
                      + osc2.process (vp.shape2, pw) * vp.level2
                      + sub.process (sdsp::Oscillator::Pulse) * vp.levelSub
                      + noise.next() * vp.levelNoise;

        const float velScale = 1.0f - vp.velSens + vp.velSens * velocity;
        const float fenv = filterEnv.next();
        const float octaves = vp.envAmount * 6.0f * fenv * velScale
                            + vp.keytrack * (pitch - 60.0f) / 12.0f
                            + m.lfo * vp.lfoFilter * 3.0f
                            + vp.drift * (0.2f * driftCutoff + 0.1f * staticCutoff);

        filter.setCutoff (m.cutoffHz * std::exp2 (octaves));
        filter.setResonance (vp.resonance);
        float y = filter.process (x * vp.driveGain, vp.filterMode) * vp.driveComp;

        y *= ampEnv.next() * velScale;
        outL += y * panL;
        outR += y * panR;
    }

    int note = -1;
    bool keyDown = false, sustained = false;
    uint64_t order = 0;

private:
    float sr = 44100.0f;
    sdsp::Oscillator osc1, osc2, sub;
    sdsp::Noise noise, rng;
    sdsp::LadderFilter filter;
    sdsp::Envelope ampEnv, filterEnv;

    float velocity = 1.0f, pitch = 60.0f, targetPitch = 60.0f;
    float panL = 1.0f, panR = 1.0f;

    int driftCounter = 0;
    float driftCoef = 0.0f, driftPitch = 0.0f, driftCutoff = 0.0f;
    float driftPitchTarget = 0.0f, driftCutoffTarget = 0.0f;
    float staticDetune = 0.0f, staticCutoff = 0.0f;
};
