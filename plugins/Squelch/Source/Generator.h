#pragma once

#include "Sequencer.h"

/**
    Acid pattern generator.

    Every pattern comes from a seed ("Pattern #"), so turning that knob browses an endless,
    repeatable supply of lines. The other controls shape the line without reshuffling it:
    each musical decision (rhythm, pitch, octave, accent, slide, tie) draws from its own
    random stream, so e.g. turning up Accent only adds accents - the melody stays put.

    All of this is allocation-free so it can run on the audio thread (for Evolve).
*/
namespace gen
{
struct Scale
{
    const char* name;
    int size;
    int notes[12];
};

inline const Scale scales[] = {
    { "Minor",             7, { 0, 2, 3, 5, 7, 8, 10 } },
    { "Major",             7, { 0, 2, 4, 5, 7, 9, 11 } },
    { "Dorian",            7, { 0, 2, 3, 5, 7, 9, 10 } },
    { "Phrygian",          7, { 0, 1, 3, 5, 7, 8, 10 } },
    { "Harmonic Minor",    7, { 0, 2, 3, 5, 7, 8, 11 } },
    { "Phrygian Dominant", 7, { 0, 1, 4, 5, 7, 8, 10 } },
    { "Minor Pentatonic",  5, { 0, 3, 5, 7, 10 } },
    { "Major Pentatonic",  5, { 0, 2, 4, 7, 9 } },
    { "Blues",             6, { 0, 3, 5, 6, 7, 10 } },
    { "Chromatic",        12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
};
constexpr int numScales = (int) (sizeof (scales) / sizeof (scales[0]));

inline juce::StringArray scaleNames()
{
    juce::StringArray names;
    for (auto& s : scales)
        names.add (s.name);
    return names;
}

inline juce::StringArray keyNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }

struct Settings
{
    int key = 0;            // 0..11 (root note)
    int scale = 0;          // index into scales
    int seed = 1;
    float density = 0.75f;  // how many steps play a note
    float accent = 0.35f;
    float slide = 0.3f;
    float range = 0.4f;     // octave jumps

    bool operator== (const Settings& o) const
    {
        return key == o.key && scale == o.scale && seed == o.seed
            && juce::exactlyEqual (density, o.density) && juce::exactlyEqual (accent, o.accent)
            && juce::exactlyEqual (slide, o.slide) && juce::exactlyEqual (range, o.range);
    }
    bool operator!= (const Settings& o) const { return ! (*this == o); }
};

/** Small, fast, deterministic RNG (splitmix64). */
class Rng
{
public:
    explicit Rng (uint64_t seed) : state (seed) {}
    uint64_t next()
    {
        uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    float uniform() { return (float) (next() >> 40) * (1.0f / 16777216.0f); }

private:
    uint64_t state;
};

/** One independent stream per decision, all derived from the pattern seed. */
inline Rng stream (int seed, uint64_t salt) { return Rng ((uint64_t) (uint32_t) seed * 0x100000001B3ull ^ (salt * 0xC2B2AE3D27D4EB4Full)); }

/** Pick a pitch (semitones above the root) from the scale, favouring root, fifth and octave. */
inline int pickPitch (const Scale& scale, float u)
{
    struct Candidate { int semis; float weight; };
    Candidate c[20];
    int n = 0;
    for (int i = 0; i < scale.size; ++i)
    {
        const int s = scale.notes[i];
        c[n++] = { s, s == 0 ? 3.8f : s == 7 ? 2.2f : (s == 3 || s == 4) ? 1.4f : 1.0f };
    }
    c[n++] = { 12, 1.4f };                                       // octave root
    c[n++] = { scale.notes[scale.size - 1] - 12, 0.7f };         // a step below the root
    if (scale.size > 5)
        c[n++] = { scale.notes[scale.size - 2] - 12, 0.5f };

    float total = 0.0f;
    for (int i = 0; i < n; ++i)
        total += c[i].weight;
    float x = u * total;
    for (int i = 0; i < n; ++i)
    {
        x -= c[i].weight;
        if (x <= 0.0f)
            return c[i].semis;
    }
    return 0;
}

/** Semitones above C2 -> the box's key (0..12) + octave (-1..1). */
inline void encodePitch (int semis, seq::Step& s)
{
    while (semis < -12) semis += 12;
    while (semis > 24)  semis -= 12;
    s.octave = semis < 0 ? -1 : semis > 12 ? 1 : 0;
    s.key = semis - 12 * s.octave;
}

inline std::array<seq::Step, seq::numSteps> generate (const Settings& set)
{
    const auto& scale = scales[juce::jlimit (0, numScales - 1, set.scale)];

    // Per-step random draws, each from its own stream.
    float uRhythm[seq::numSteps], uPitch[seq::numSteps], uOct[seq::numSteps], uOctDir[seq::numSteps];
    float uAccent[seq::numSteps], uSlide[seq::numSteps], uTie[seq::numSteps];
    {
        auto r = stream (set.seed, 1), p = stream (set.seed, 2), o = stream (set.seed, 3), od = stream (set.seed, 4);
        auto a = stream (set.seed, 5), sl = stream (set.seed, 6), t = stream (set.seed, 7);
        for (int i = 0; i < seq::numSteps; ++i)
        {
            uRhythm[i] = r.uniform(); uPitch[i] = p.uniform(); uOct[i] = o.uniform(); uOctDir[i] = od.uniform();
            uAccent[i] = a.uniform(); uSlide[i] = sl.uniform(); uTie[i] = t.uniform();
        }
    }

    // Riff structure: a 4/8/16-step motif, repeated with a few changes and a turnaround.
    auto structure = stream (set.seed, 8);
    const float m = structure.uniform();
    const int motif = m < 0.3f ? 4 : m < 0.8f ? 8 : 16;
    int source[seq::numSteps];
    for (int i = 0; i < seq::numSteps; ++i)
    {
        const float change = structure.uniform();
        const bool turnaround = i >= seq::numSteps - 2;
        source[i] = (i < motif || change < (turnaround ? 0.5f : 0.18f)) ? i : i % motif;
    }

    std::array<seq::Step, seq::numSteps> steps {};
    bool previousWasNote = false;
    for (int i = 0; i < seq::numSteps; ++i)
    {
        const int k = source[i];
        seq::Step s;
        const bool downbeat = (i % 4) == 0;

        // Rhythm: downbeats are more likely to play; step 1 always does.
        const float noteChance = juce::jlimit (0.0f, 1.0f, set.density * (downbeat ? 1.25f : 0.95f));
        if (i > 0 && uRhythm[k] >= noteChance)
        {
            const bool tie = previousWasNote && uTie[k] < 0.15f + 0.25f * (1.0f - set.density);
            s.time = tie ? seq::Tie : seq::Rest;
            s.slide = tie && uSlide[k] < set.slide * 0.3f;
            steps[(size_t) i] = s;
            previousWasNote = tie;
            continue;
        }

        int semis = (i == 0) ? 0 : pickPitch (scale, uPitch[k]);
        if (uOct[k] < set.range * 0.4f)
            semis += uOctDir[k] < 0.65f ? 12 : -12;
        encodePitch (set.key + semis, s);

        s.time = seq::Note;
        s.accent = uAccent[k] < set.accent * (downbeat ? 0.75f : 0.55f);
        s.slide = uSlide[k] < set.slide * 0.5f;
        steps[(size_t) i] = s;
        previousWasNote = true;
    }
    return steps;
}

/** Evolve: blend a fresh pattern into the current one, step by step. */
inline std::array<seq::Step, seq::numSteps> mutate (const std::array<seq::Step, seq::numSteps>& current,
                                                    const std::array<seq::Step, seq::numSteps>& fresh,
                                                    float variation, int salt)
{
    auto r = stream (salt, 99);
    auto out = current;
    for (int i = 0; i < seq::numSteps; ++i)
        if (r.uniform() < variation)
            out[(size_t) i] = fresh[(size_t) i];
    if (out[0].time != seq::Note)
        out[0] = fresh[0].time == seq::Note ? fresh[0] : current[0];
    return out;
}
/** One Evolve step: the pattern after evolution number `evolveIndex` (shared by playback and MIDI export). */
inline std::array<seq::Step, seq::numSteps> evolve (const std::array<seq::Step, seq::numSteps>& current,
                                                    Settings settings, int evolveIndex, float variation)
{
    settings.seed = (int) (((uint32_t) settings.seed * 7919u + (uint32_t) evolveIndex * 104729u) & 0x7fffffffu);
    return mutate (current, generate (settings), variation, settings.seed);
}
} // namespace gen
