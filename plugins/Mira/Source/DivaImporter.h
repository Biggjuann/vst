#pragma once

#include <juce_core/juce_core.h>

#include <map>

/**
    Reads u-he Diva presets (.h2p) and translates them into Mira parameter values.

    Diva has far more going on than Mira (5 oscillator models, 5 filter models, two LFOs,
    a mod matrix, 2 FX slots), so this is a "closest match" translation, not a 1:1 clone:
    oscillator waveforms, tuning, levels, filter type/cutoff/resonance/keytrack/env amount,
    both envelopes, the most-used LFO, glide, voice mode, drift, PWM, pan spread and chorus
    carry over; everything else is dropped.
*/
namespace diva
{
using Values = std::vector<std::pair<juce::String, float>>;

/** Raw contents of an .h2p file: section -> key -> value text. */
struct H2P
{
    std::map<juce::String, std::map<juce::String, juce::String>> sections;
    std::map<juce::String, juce::String> globals; // "#key=value" lines outside sections
    juce::String author;

    bool has (const juce::String& section) const { return sections.count (section) > 0; }
    float num (const juce::String& section, const juce::String& key, float fallback) const;
    juce::String str (const juce::String& section, const juce::String& key) const;
};

H2P parse (const juce::String& text);

struct Converted
{
    bool ok = false;
    juce::String error;
    juce::String name, category, author;
    Values values;
};

/** Convert parsed Diva data to Mira values. */
Converted convert (const H2P& preset, const juce::String& name);

/** Read and convert a file. */
Converted convertFile (const juce::File& file);

/** Guess Bass/Lead/Pad/Keys/... from a preset name like "BS Rubber" or "PAD - Haze". */
juce::String guessCategory (const juce::String& name);
} // namespace diva
