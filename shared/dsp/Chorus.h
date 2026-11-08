#pragma once

#include <vector>

#include "Oscillator.h"

namespace sdsp
{
/**
    Bucket-brigade style stereo chorus in the spirit of the classic 80s poly chorus:
    one triangle LFO modulating a short delay, opposite phase on left and right.
    Modes: 0 = off, 1 = I (slow, wide), 2 = II (faster, deeper), 3 = I+II (fast, subtle shimmer).
*/
class Chorus
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        const int size = (int) (sr * 0.03f) + 4;
        for (auto& b : buffer)
            b.assign ((size_t) size, 0.0f);
        writePos = 0;
        phase = 0.0f;
        // Gentle low-pass on the wet path, like the BBD's anti-aliasing filters.
        lpCoef = 1.0f - std::exp (-twoPi * 7000.0f / sr);
        lpState[0] = lpState[1] = 0.0f;
    }

    void process (float& left, float& right, int mode)
    {
        if (mode <= 0 || buffer[0].empty())
            return;

        float rate = 0.5f, depthMs = 1.85f, centreMs = 3.5f;
        if (mode == 2) { rate = 0.83f; depthMs = 2.2f; }
        if (mode == 3) { rate = 8.0f;  depthMs = 0.35f; centreMs = 3.0f; }

        phase += rate / sr;
        if (phase >= 1.0f)
            phase -= 1.0f;
        const float tri = 1.0f - 4.0f * std::abs (phase - 0.5f);

        const int size = (int) buffer[0].size();
        buffer[0][(size_t) writePos] = left;
        buffer[1][(size_t) writePos] = right;

        const float in[2] = { left, right };
        float out[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            const float lfo = ch == 0 ? tri : -tri;
            const float delaySamples = (centreMs + depthMs * lfo) * 0.001f * sr;
            float readPos = (float) writePos - delaySamples;
            while (readPos < 0.0f)
                readPos += (float) size;
            const int i0 = (int) readPos;
            const int i1 = (i0 + 1) % size;
            const float frac = readPos - (float) i0;
            // Cross-feed: each side reads the mono sum so the stereo image widens.
            const auto& buf = buffer[(size_t) ch];
            const auto& other = buffer[(size_t) (1 - ch)];
            const float a = 0.5f * (buf[(size_t) i0] + other[(size_t) i0]);
            const float b = 0.5f * (buf[(size_t) i1] + other[(size_t) i1]);
            float wet = a + (b - a) * frac;
            lpState[ch] += (wet - lpState[ch]) * lpCoef;
            wet = lpState[ch];
            out[ch] = 0.7f * in[ch] + 0.7f * wet;
        }

        writePos = (writePos + 1) % size;
        left = out[0];
        right = out[1];
    }

private:
    float sr = 44100.0f, phase = 0.0f, lpCoef = 1.0f;
    float lpState[2] {};
    std::vector<float> buffer[2];
    int writePos = 0;
};
} // namespace sdsp
