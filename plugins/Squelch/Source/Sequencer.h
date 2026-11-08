#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <random>

#include <juce_core/juce_core.h>

/**
    The step data for Squelch's built-in sequencer, modelled on the classic silver bass box:
    16 steps per pattern, and each step has
      - a pitch from the 13-key one-octave keyboard (C .. C')
      - octave DOWN / UP
      - ACCENT and SLIDE
      - a TIME value: Note, Tie (extend the previous note) or Rest
*/
namespace seq
{
constexpr int numSteps = 16;
constexpr int numPatterns = 8;
constexpr int baseNote = 36; // the "C" key with no octave transpose = C2 (MIDI 36)

enum Time : uint8_t { Note = 0, Tie = 1, Rest = 2 };

struct Step
{
    int key = 0;        // 0..12 (C .. C')
    int octave = 0;     // -1, 0, +1
    bool accent = false;
    bool slide = false;
    Time time = Note;

    uint32_t pack() const
    {
        return (uint32_t) juce::jlimit (0, 12, key)
             | ((uint32_t) (juce::jlimit (-1, 1, octave) + 1) << 4)
             | ((uint32_t) accent << 6)
             | ((uint32_t) slide << 7)
             | ((uint32_t) time << 8);
    }

    static Step unpack (uint32_t v)
    {
        Step s;
        s.key = juce::jlimit (0, 12, (int) (v & 15u));
        s.octave = juce::jlimit (-1, 1, (int) ((v >> 4) & 3u) - 1);
        s.accent = ((v >> 6) & 1u) != 0;
        s.slide = ((v >> 7) & 1u) != 0;
        s.time = (Time) juce::jlimit (0, 2, (int) ((v >> 8) & 3u));
        return s;
    }

    int midiNote (int transpose) const { return juce::jlimit (0, 127, baseNote + key + 12 * octave + transpose); }
};

inline juce::String keyName (int key)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C'" };
    return names[juce::jlimit (0, 12, key)];
}

/**
    Parse a compact pattern string, one token per step:
      key name (C, C#, ... B, C'), then optional modifiers:  ^ up   v down   ! accent   ~ slide
      "-" = tie, "." = rest.      e.g. "C! C C^~ D# . -"
*/
inline std::array<Step, numSteps> parsePattern (const juce::String& text)
{
    std::array<Step, numSteps> steps {};
    auto tokens = juce::StringArray::fromTokens (text, " ", "");
    tokens.removeEmptyStrings();
    for (int i = 0; i < numSteps && i < tokens.size(); ++i)
    {
        auto t = tokens[i];
        Step s;
        if (t.startsWith ("-")) { s.time = Tie; }
        else if (t.startsWith (".")) { s.time = Rest; }
        else
        {
            static const juce::StringArray keys { "C'", "C#", "D#", "F#", "G#", "A#", "C", "D", "E", "F", "G", "A", "B" };
            static const int values[] = { 12, 1, 3, 6, 8, 10, 0, 2, 4, 5, 7, 9, 11 };
            for (int k = 0; k < keys.size(); ++k)
                if (t.startsWith (keys[k])) { s.key = values[k]; t = t.substring (keys[k].length()); break; }
            s.octave = t.containsChar ('^') ? 1 : t.containsChar ('v') ? -1 : 0;
        }
        s.accent = t.containsChar ('!');
        s.slide = t.containsChar ('~');
        steps[(size_t) i] = s;
    }
    return steps;
}

/** A random but musical acid line: minor-ish scale, some accents, slides, rests and ties. */
inline std::array<Step, numSteps> randomPattern (uint32_t seed)
{
    std::mt19937 rng (seed);
    auto chance = [&rng] (float p) { return std::uniform_real_distribution<float> (0.0f, 1.0f) (rng) < p; };
    static const int scale[] = { 0, 0, 0, 3, 5, 7, 7, 10, 12, 1, 6 }; // weighted: root-heavy minor with b2/b5 spice

    std::array<Step, numSteps> steps {};
    for (int i = 0; i < numSteps; ++i)
    {
        Step s;
        s.key = scale[std::uniform_int_distribution<int> (0, 10) (rng)];
        s.octave = chance (0.18f) ? 1 : chance (0.15f) ? -1 : 0;
        s.accent = chance (0.28f);
        s.slide = chance (0.22f);
        s.time = (i > 0 && chance (0.08f)) ? Tie : chance (0.14f) ? Rest : Note;
        steps[(size_t) i] = s;
    }
    steps[0].time = Note; // start on a note so the pattern has a clear downbeat
    return steps;
}

/** Thread-safe pattern storage: the UI writes, the audio thread reads. */
class PatternBank
{
public:
    PatternBank()
    {
        static const char* demos[] = {
            "C! C C^~ C D#! C~ Gv A#v C! . C^ C~ F G# C^! -",
            "Cv~ Cv C C! Cv~ D# Cv F! Cv Cv~ C G#v A#v! C . Cv",
            "C . C! . D#~ F . C^! C . A#v~ C . G~ G# .",
            "C~ D#~ G~ C^! G~ D# C~ Gv C~ D#~ G~ A#! G~ F D#~ C",
        };
        for (int p = 0; p < numPatterns; ++p)
            setPattern (p, p < 4 ? parsePattern (demos[p]) : randomPattern ((uint32_t) (1303 + p)));
    }

    Step get (int pattern, int step) const
    {
        return Step::unpack (data[(size_t) wrapPattern (pattern)][(size_t) (step & (numSteps - 1))].load (std::memory_order_relaxed));
    }

    void set (int pattern, int step, const Step& s)
    {
        data[(size_t) wrapPattern (pattern)][(size_t) (step & (numSteps - 1))].store (s.pack(), std::memory_order_relaxed);
        ++version;
    }

    void setPattern (int pattern, const std::array<Step, numSteps>& steps)
    {
        for (int i = 0; i < numSteps; ++i)
            set (pattern, i, steps[(size_t) i]);
    }

    /** Rotate a pattern by +-1 step. */
    void shift (int pattern, int delta)
    {
        std::array<Step, numSteps> old {};
        for (int i = 0; i < numSteps; ++i)
            old[(size_t) i] = get (pattern, i);
        for (int i = 0; i < numSteps; ++i)
            set (pattern, (i + delta + numSteps) % numSteps, old[(size_t) i]);
    }

    /** "1,2,3,..." per pattern, for saving in the plugin state. */
    juce::String serialise (int pattern) const
    {
        juce::StringArray parts;
        for (int i = 0; i < numSteps; ++i)
            parts.add (juce::String (get (pattern, i).pack()));
        return parts.joinIntoString (",");
    }

    void deserialise (int pattern, const juce::String& text)
    {
        auto parts = juce::StringArray::fromTokens (text, ",", "");
        if (parts.size() != numSteps)
            return;
        for (int i = 0; i < numSteps; ++i)
            set (pattern, i, Step::unpack ((uint32_t) parts[i].getLargeIntValue()));
    }

    /** Bumped on every edit so the UI knows when to repaint. */
    std::atomic<uint32_t> version { 0 };

private:
    static int wrapPattern (int p) { return ((p % numPatterns) + numPatterns) % numPatterns; }
    std::array<std::array<std::atomic<uint32_t>, numSteps>, numPatterns> data {};
};
} // namespace seq
