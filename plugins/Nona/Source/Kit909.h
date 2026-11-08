#pragma once

#include "drums/DrumDSP.h"

/**
    Voice models for Nona, following the classic hybrid rhythm composer:

    Analog voices
    - Kick: a triangle VCO, rounded by a waveshaper, with a fast downward pitch sweep.
      Tune sets the sweep, Attack adds the beater click (noise + pulse), Decay the length.
    - Snare: two triangle VCOs with a short pitch drop, plus noise through a Tone low-pass
      and high-pass (Snappy sets the noise level).
    - Toms: triangle VCOs with a pronounced pitch sweep and a little noise; Tune and Decay.
    - Rim shot: short resonator cluster with a sharp click.
    - Clap: band-passed noise with four rapid bursts and a short tail.

    "Sampled" voices
    - The hats, crash and ride were 6-bit samples in the original. Those recordings can't be
      shipped, so they are synthesized from inharmonic square-wave banks and noise, then run
      through a 6-bit quantizer and a ~32 kHz sample-and-hold before the envelope, which gives
      the same gritty, aliased character. Tune changes the "playback rate" (crash, ride).
*/
namespace kit909
{
using namespace drum;

enum Voice { BD, SD, LT, MT, HT, RS, CP, CH, OH, CR, RD, NumVoices };

struct Params
{
    float level[NumVoices] {};
    float bdTune = 0.5f, bdAttack = 0.5f, bdDecay = 0.5f;
    float sdTune = 0.5f, sdTone = 0.5f, sdSnappy = 0.5f;
    float tomTune[3] { 0.5f, 0.5f, 0.5f }, tomDecay[3] { 0.5f, 0.5f, 0.5f };
    float chDecay = 0.3f, ohDecay = 0.4f;
    float crTune = 0.5f, rdTune = 0.5f;
};

inline float tuneRatio (float knob, float range = 0.8f) { return std::exp2 ((knob - 0.5f) * range); }

/** Inharmonic metal source rendered "as a 6-bit sample". */
struct MetalSample
{
    Square osc[6];
    float holdPhase = 0.0f, held = 0.0f;

    float next (const float* freqs, float rate, float noiseAmount, float noise, float sr)
    {
        // The "sample" runs at 32 kHz x rate: advance it one frame each time the hold clock ticks,
        // so Tune shifts pitch exactly like changing a sample's playback rate.
        holdPhase += 32000.0f * rate / sr;
        if (holdPhase >= 1.0f)
        {
            holdPhase -= std::floor (holdPhase);
            float m = 0.0f;
            for (int i = 0; i < 6; ++i)
                m += osc[i].next (freqs[i], 32000.0f);
            const float raw = (1.0f - noiseAmount) * m / 6.0f + noiseAmount * noise;
            held = std::round (std::clamp (raw, -1.0f, 1.0f) * 31.0f) / 31.0f; // 6-bit
        }
        return held;
    }
};

class Kit
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        noise.seed (0x909u);
        bdClickLp.set (6000.0f, sr);
        sdNoiseHp.set (500.0f, 0.7f, sr);
        tomNoiseLp.set (2500.0f, sr);
        rsHp.set (400.0f, sr);
        cpBand.set (1150.0f, 1.6f, sr);
        cpHp.set (500.0f, sr);
        hatHp.set (7500.0f, 0.7f, sr);
        crHp.set (3500.0f, 0.7f, sr);
        rdBand.set (4500.0f, 1.2f, sr);
        rdHp.set (2000.0f, 0.7f, sr);
    }

    void setParams (const Params& p)
    {
        params = p;
        sdNoiseLp.set (3000.0f * std::exp2 (p.sdTone * 2.2f), 0.7f, sr);
    }

    void trigger (int v, float level, float accent)
    {
        const float a = level * params.level[v] * (1.0f + 0.8f * accent);
        switch (v)
        {
            case BD:
                bdTime = 0.0f; bdPhase = 0.25f;
                bdAmp.trigger (a); bdAmp.set (0.18f + 1.3f * std::pow (params.bdDecay, 1.5f), sr);
                bdClick.trigger (a * params.bdAttack); bdClick.set (0.006f, sr);
                break;
            case SD:
                sdTime = 0.0f;
                sdBody.trigger (a); sdBody.set (0.17f, sr);
                sdNoise.trigger (a); sdNoise.set (0.26f, sr);
                sdPhase[0] = sdPhase[1] = 0.25f;
                break;
            case LT: case MT: case HT:
            {
                const int i = v - LT;
                tomTime[i] = 0.0f; tomPhase[i] = 0.25f;
                tomAmp[i].trigger (a); tomAmp[i].set (0.15f + 0.9f * params.tomDecay[i], sr);
                tomNoise[i].trigger (a); tomNoise[i].set (0.05f, sr);
                break;
            }
            case RS:
                rsA.trigger (a); rsA.env.set (0.04f, sr);
                rsB.trigger (a); rsB.env.set (0.022f, sr);
                rsC.trigger (a * 0.5f); rsC.env.set (0.015f, sr);
                break;
            case CP:
                cpTime = 0.0f; cpLevel = a; cpBurst.trigger (a); cpBurst.set (0.011f, sr);
                cpBurstsLeft = 3; cpNextBurst = 0.009f;
                cpTail.trigger (a); cpTail.set (0.26f, sr);
                break;
            case CH:
                chEnv.trigger (a); chEnv.set (0.03f + 0.12f * params.chDecay, sr);
                if (ohEnv.active())
                    ohEnv.set (0.008f, sr); // closed hat chokes the open hat
                break;
            case OH:
                ohEnv.trigger (a); ohEnv.set (0.15f + 1.0f * params.ohDecay, sr);
                break;
            case CR:
                crEnv.trigger (a); crEnv.set (1.7f * tuneRatio (params.crTune, -0.6f), sr);
                break;
            case RD:
                rdEnv.trigger (a); rdEnv.set (2.3f * tuneRatio (params.rdTune, -0.6f), sr);
                break;
            default: break;
        }
    }

    float render()
    {
        float sum = 0.0f;

        // Kick: shaped triangle with a fast pitch sweep
        if (bdAmp.active())
        {
            bdTime += 1.0f / sr;
            const float end = 50.0f * tuneRatio (params.bdTune, 0.3f);
            const float start = end * (2.2f + 3.0f * params.bdTune);
            const float hz = end + (start - end) * std::exp (-bdTime / 0.022f);
            bdPhase += hz / sr;
            if (bdPhase >= 1.0f) bdPhase -= 1.0f;
            const float tri = 1.0f - 4.0f * std::abs (bdPhase - 0.5f);
            const float body = softClip (2.0f * tri) * 0.8f * bdAmp.next();
            const float pulse = bdTime < 0.0012f ? 1.0f : 0.0f;
            const float click = bdClickLp.lowpass (0.6f * noise.next() + pulse) * bdClick.next();
            sum += 0.95f * body + 0.8f * click;
        }

        // Snare
        if (sdBody.active() || sdNoise.active())
        {
            sdTime += 1.0f / sr;
            const float drop = 1.0f + 0.5f * std::exp (-sdTime / 0.012f);
            const float tune = tuneRatio (params.sdTune, 0.6f);
            static const float sdHz[] = { 190.0f, 335.0f };
            float body = 0.0f;
            for (int i = 0; i < 2; ++i)
            {
                sdPhase[i] += sdHz[i] * tune * drop / sr;
                if (sdPhase[i] >= 1.0f) sdPhase[i] -= 1.0f;
                body += (i == 0 ? 0.6f : 0.4f) * (1.0f - 4.0f * std::abs (sdPhase[i] - 0.5f));
            }
            const float n = sdNoiseLp.lowpass (sdNoiseHp.highpass (noise.next())) * sdNoise.next();
            sum += 0.6f * softClip (1.3f * body) * sdBody.next() + 1.0f * params.sdSnappy * n;
        }

        // Toms
        static const float tomHz[] = { 90.0f, 128.0f, 180.0f };
        for (int i = 0; i < 3; ++i)
            if (tomAmp[i].active())
            {
                tomTime[i] += 1.0f / sr;
                const float hz = tomHz[i] * tuneRatio (params.tomTune[i]) * (1.0f + 0.55f * std::exp (-tomTime[i] / 0.06f));
                tomPhase[i] += hz / sr;
                if (tomPhase[i] >= 1.0f) tomPhase[i] -= 1.0f;
                const float tri = 1.0f - 4.0f * std::abs (tomPhase[i] - 0.5f);
                sum += 0.7f * softClip (1.6f * tri) * tomAmp[i].next() + 0.12f * tomNoiseLp.lowpass (noise.next()) * tomNoise[i].next();
            }

        // Rim shot
        if (rsA.active())
            sum += 0.55f * softClip (2.0f * rsHp.highpass (rsA.next (480.0f, sr) + 0.8f * rsB.next (1640.0f, sr) + 0.5f * rsC.next (3200.0f, sr)));

        // Clap: four bursts and a tail
        if (cpTail.active())
        {
            cpTime += 1.0f / sr;
            if (cpBurstsLeft > 0 && cpTime >= cpNextBurst)
            {
                cpBurst.trigger (cpLevel);
                --cpBurstsLeft;
                cpNextBurst += 0.009f;
            }
            const float env = std::max (cpBurst.next(), 0.5f * cpTail.next());
            sum += 1.7f * cpHp.highpass (cpBand.bandpass (noise.next())) * env;
        }

        // "Sampled" metal voices
        const float n = noise.next();
        if (chEnv.active() || ohEnv.active())
        {
            static const float hatHz[] = { 263.0f, 400.0f, 421.0f, 474.0f, 587.0f, 845.0f };
            const float s = hatHp.highpass (hats.next (hatHz, 1.0f, 0.35f, n, sr));
            sum += 1.5f * s * (chEnv.next() + ohEnv.next());
        }
        if (crEnv.active())
        {
            static const float crHz[] = { 245.0f, 367.0f, 433.0f, 517.0f, 662.0f, 913.0f };
            const float s = crHp.highpass (crash.next (crHz, tuneRatio (params.crTune), 0.55f, n, sr));
            sum += 1.3f * s * crEnv.next();
        }
        if (rdEnv.active())
        {
            static const float rdHz[] = { 310.0f, 465.0f, 521.0f, 690.0f, 744.0f, 1013.0f };
            const float raw = ride.next (rdHz, tuneRatio (params.rdTune), 0.2f, n, sr);
            const float s = rdHp.highpass (0.7f * rdBand.bandpass (raw) + 0.5f * raw);
            sum += 1.0f * s * rdEnv.next();
        }

        return sum;
    }

private:
    float sr = 44100.0f;
    Params params;
    sdsp::Noise noise;

    Decay bdAmp, bdClick;
    OnePole bdClickLp;
    float bdTime = 0.0f, bdPhase = 0.0f;

    Decay sdBody, sdNoise;
    Svf sdNoiseHp, sdNoiseLp;
    float sdTime = 0.0f, sdPhase[2] {};

    Decay tomAmp[3], tomNoise[3];
    OnePole tomNoiseLp;
    float tomTime[3] {}, tomPhase[3] {};

    Ring rsA, rsB, rsC;
    OnePole rsHp;

    Decay cpBurst, cpTail;
    Svf cpBand;
    OnePole cpHp;
    float cpTime = 0.0f, cpLevel = 0.0f, cpNextBurst = 0.0f;
    int cpBurstsLeft = 0;

    MetalSample hats, crash, ride;
    Svf hatHp, crHp, rdBand, rdHp;
    Decay chEnv, ohEnv, crEnv, rdEnv;
};
} // namespace kit909
