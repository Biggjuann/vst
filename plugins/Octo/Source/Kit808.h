#pragma once

#include "drums/DrumDSP.h"

/**
    Voice models for Octo, following the classic analog rhythm composer's circuits:

    - Kick: a bridged-T resonator (~49 Hz) pinged by a trigger pulse. The pulse briefly
      detunes the resonator upward (the famous "knock" at the start), Decay sets how long it
      rings, Tone low-passes the click.
    - Snare: two resonators (~180 / ~330 Hz, Tone balances them) plus high-passed noise (Snappy).
    - Toms / congas: single resonators with a slight downward pitch bend (toms also add a little
      filtered noise), each with a Tuning control.
    - Rim shot, claves: very short, high resonators; the rim shot is overdriven like the original.
    - Clap: band-passed noise with three quick "sawtooth" bursts, then a reverb-like tail.
    - Maracas: short high-passed noise.
    - Cowbell: two square-wave oscillators (540 / 800 Hz) through a band-pass, two-stage decay.
    - Cymbal and hi-hats: SIX square-wave oscillators at the original's inharmonic frequencies,
      shared by all three, then band-pass/high-pass filtered. Closed hat chokes the open hat.
*/
namespace kit808
{
using namespace drum;

enum Voice { BD, SD, LT, MT, HT, LC, MC, HC, RS, CL, CP, MA, CB, CY, OH, CH, NumVoices };

struct Params
{
    float level[NumVoices] {};
    float bdTone = 0.5f, bdDecay = 0.5f;
    float sdTone = 0.5f, sdSnappy = 0.5f;
    float tune[NumVoices] {};   // LT MT HT LC MC HC
    float cyTone = 0.5f, cyDecay = 0.5f;
    float ohDecay = 0.5f;
};

inline float tuneRatio (float knob) { return std::exp2 ((knob - 0.5f) * 0.8f); } // about +-5 semitones

class Kit
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        noise.seed (0x808u);
        sdNoiseHp.set (1800.0f, 0.7f, sr);
        sdNoiseLp.set (9000.0f, 0.7f, sr);
        tomNoiseLp.set (1200.0f, sr);
        rsHp.set (300.0f, sr);
        cpBand.set (1050.0f, 2.2f, sr);
        maHp.set (6000.0f, 0.7f, sr);
        cbBand.set (2640.0f, 0.9f, sr); // wide, so the 540 / 800 Hz fundamentals still carry the pitch
        hatHp.set (7100.0f, 0.8f, sr);
        hatHp2.set (7100.0f, 0.8f, sr);
        cyLow.set (3440.0f, 2.0f, sr);
        cyHigh.set (7100.0f, 0.8f, sr);
    }

    void setParams (const Params& p)
    {
        params = p;
        bdLp.set (250.0f * std::exp2 (p.bdTone * 4.5f), sr);
        bdAmp.set (0.12f * std::pow (20.0f, p.bdDecay), sr);
    }

    void trigger (int v, float level, float accent)
    {
        const float a = level * params.level[v] * (1.0f + 0.8f * accent);
        switch (v)
        {
            case BD:
                bdTime = 0.0f; bdPhase = 0.0f; bdAccent = accent;
                bdAmp.trigger (a); bdClick.trigger (a); bdClick.set (0.004f, sr);
                break;
            case SD:
                sdLow.trigger (a); sdLow.env.set (0.22f, sr);
                sdHigh.trigger (a); sdHigh.env.set (0.15f, sr);
                sdNoise.trigger (a); sdNoise.set (0.18f + 0.15f * params.sdSnappy, sr);
                break;
            case LT: case MT: case HT:
            {
                const int i = v - LT;
                static const float t60[] = { 0.75f, 0.6f, 0.5f };
                tom[i].trigger (a); tom[i].env.set (t60[i], sr);
                tomTime[i] = 0.0f;
                tomNoise[i].trigger (a); tomNoise[i].set (0.06f, sr);
                break;
            }
            case LC: case MC: case HC:
            {
                const int i = v - LC;
                static const float t60[] = { 0.32f, 0.24f, 0.18f };
                conga[i].trigger (a); conga[i].env.set (t60[i], sr);
                congaTime[i] = 0.0f;
                break;
            }
            case RS:
                rsA.trigger (a); rsA.env.set (0.045f, sr);
                rsB.trigger (a); rsB.env.set (0.028f, sr);
                break;
            case CL:
                cl.trigger (a); cl.env.set (0.07f, sr);
                break;
            case CP:
                cpTime = 0.0f; cpLevel = a; cpBurst.trigger (a); cpBurst.set (0.012f, sr);
                cpBurstsLeft = 2; cpNextBurst = 0.0105f;
                cpTail.trigger (a); cpTail.set (0.38f, sr);
                break;
            case MA:
                maTime = 0.0f; maEnv.trigger (a); maEnv.set (0.06f, sr);
                break;
            case CB:
                cbFast.trigger (a * 0.7f); cbFast.set (0.05f, sr);
                cbSlow.trigger (a * 0.3f); cbSlow.set (0.45f, sr);
                break;
            case CY:
                cyLowEnv.trigger (a); cyLowEnv.set (0.6f + 2.4f * params.cyDecay, sr);
                cyHighEnv.trigger (a); cyHighEnv.set (0.25f + 0.6f * params.cyDecay, sr);
                break;
            case OH:
                ohEnv.trigger (a); ohEnv.set (0.12f + 0.9f * params.ohDecay, sr);
                break;
            case CH:
                chEnv.trigger (a); chEnv.set (0.05f, sr);
                if (ohEnv.active())
                    ohEnv.set (0.008f, sr); // choke the open hat
                break;
            default: break;
        }
    }

    float render()
    {
        float sum = 0.0f;

        // Kick
        if (bdAmp.active())
        {
            bdTime += 1.0f / sr;
            const float sweep = (0.6f * std::exp (-bdTime / 0.004f) + 0.08f * std::exp (-bdTime / 0.05f)) * (1.0f + 0.3f * bdAccent);
            const float hz = 49.0f * (1.0f + sweep);
            bdPhase += hz / sr;
            if (bdPhase >= 1.0f) bdPhase -= 1.0f;
            const float body = softClip (1.25f * std::sin (twoPi * bdPhase)) * bdAmp.next();
            const float click = bdLp.lowpass (bdClick.next() * 1.2f);
            sum += 0.9f * (body + 0.5f * click);
        }

        // Snare
        if (sdLow.active() || sdNoise.active())
        {
            const float lowMix = 1.0f - 0.6f * params.sdTone, highMix = 0.4f + 0.6f * params.sdTone;
            const float body = lowMix * sdLow.next (180.0f, sr) + highMix * sdHigh.next (330.0f, sr);
            const float n = sdNoiseLp.lowpass (sdNoiseHp.highpass (noise.next())) * sdNoise.next();
            sum += 0.55f * body + 1.1f * params.sdSnappy * n;
        }

        // Toms
        static const float tomHz[] = { 82.0f, 120.0f, 172.0f };
        for (int i = 0; i < 3; ++i)
            if (tom[i].active())
            {
                tomTime[i] += 1.0f / sr;
                const float hz = tomHz[i] * tuneRatio (params.tune[LT + i]) * (1.0f + 0.12f * std::exp (-tomTime[i] / 0.04f));
                sum += 0.75f * tom[i].next (hz, sr) + 0.15f * tomNoiseLp.lowpass (noise.next()) * tomNoise[i].next();
            }

        // Congas
        static const float congaHz[] = { 220.0f, 310.0f, 455.0f };
        for (int i = 0; i < 3; ++i)
            if (conga[i].active())
            {
                congaTime[i] += 1.0f / sr;
                const float hz = congaHz[i] * tuneRatio (params.tune[LC + i]) * (1.0f + 0.05f * std::exp (-congaTime[i] / 0.02f));
                sum += 0.7f * conga[i].next (hz, sr);
            }

        // Rim shot (overdriven pair of short resonators) and claves
        if (rsA.active())
            sum += 0.5f * softClip (2.5f * rsHp.highpass (rsA.next (455.0f, sr) + 0.8f * rsB.next (1665.0f, sr)));
        if (cl.active())
            sum += 0.6f * cl.next (2500.0f, sr);

        // Clap: three bursts, then the tail
        if (cpTail.active())
        {
            cpTime += 1.0f / sr;
            if (cpBurstsLeft > 0 && cpTime >= cpNextBurst)
            {
                cpBurst.trigger (cpLevel);
                --cpBurstsLeft;
                cpNextBurst += 0.0105f;
            }
            const float env = std::max (cpBurst.next(), 0.55f * cpTail.next());
            sum += 1.6f * cpBand.bandpass (noise.next()) * env;
        }

        // Maracas: 2 ms attack, short decay
        if (maEnv.active())
        {
            maTime += 1.0f / sr;
            const float attack = std::min (1.0f, maTime / 0.002f);
            sum += 0.7f * maHp.highpass (noise.next()) * maEnv.next() * attack;
        }

        // Cowbell
        if (cbFast.active() || cbSlow.active())
        {
            const float tone = 0.5f * (cbOsc[0].next (540.0f, sr) + cbOsc[1].next (800.0f, sr));
            sum += 0.9f * softClip (1.5f * cbBand.bandpass (tone)) * (cbFast.next() + cbSlow.next());
        }

        // Metal: six square oscillators shared by cymbal and hats
        if (cyLowEnv.active() || cyHighEnv.active() || ohEnv.active() || chEnv.active())
        {
            static const float metalHz[] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
            float metal = 0.0f;
            for (int i = 0; i < 6; ++i)
                metal += metalOsc[i].next (metalHz[i], sr);
            metal *= 1.0f / 6.0f;

            if (cyLowEnv.active() || cyHighEnv.active())
            {
                const float low = cyLow.bandpass (metal) * cyLowEnv.next();
                const float high = cyHigh.highpass (metal) * cyHighEnv.next();
                sum += 1.4f * ((1.0f - 0.7f * params.cyTone) * low + (0.4f + 0.8f * params.cyTone) * high);
            }
            const float hat = hatHp2.highpass (hatHp.highpass (metal));
            sum += 1.6f * hat * (ohEnv.next() + chEnv.next());
        }

        return sum;
    }

private:
    float sr = 44100.0f;
    Params params;
    sdsp::Noise noise;

    // Kick
    Decay bdAmp, bdClick;
    OnePole bdLp;
    float bdTime = 0.0f, bdPhase = 0.0f, bdAccent = 0.0f;
    // Snare
    Ring sdLow, sdHigh;
    Decay sdNoise;
    Svf sdNoiseHp, sdNoiseLp;
    // Toms & congas
    Ring tom[3], conga[3];
    Decay tomNoise[3];
    OnePole tomNoiseLp;
    float tomTime[3] {}, congaTime[3] {};
    // Rim, claves
    Ring rsA, rsB, cl;
    OnePole rsHp;
    // Clap
    Decay cpBurst, cpTail;
    Svf cpBand;
    float cpTime = 0.0f, cpLevel = 0.0f, cpNextBurst = 0.0f;
    int cpBurstsLeft = 0;
    // Maracas
    Decay maEnv;
    Svf maHp;
    float maTime = 0.0f;
    // Cowbell
    Square cbOsc[2];
    Svf cbBand;
    Decay cbFast, cbSlow;
    // Metal
    Square metalOsc[6];
    Svf hatHp, hatHp2, cyLow, cyHigh;
    Decay cyLowEnv, cyHighEnv, ohEnv, chEnv;
};
} // namespace kit808
