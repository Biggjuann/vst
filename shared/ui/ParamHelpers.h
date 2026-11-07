#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/** Small helpers for declaring parameters with readable value text. */
namespace sparams
{
using Attr = juce::AudioParameterFloatAttributes;

inline Attr percent()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
                 .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; });
}

inline Attr bipolarPercent()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
                 .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; });
}

inline Attr hertz()
{
    return Attr().withStringFromValueFunction ([] (float v, int)
                 {
                     if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                     if (v < 10.0f)    return juce::String (v, 2) + " Hz";
                     return juce::String (juce::roundToInt (v)) + " Hz";
                 })
                 .withValueFromStringFunction ([] (const juce::String& s)
                 {
                     const auto f = s.getFloatValue();
                     return s.containsIgnoreCase ("k") ? f * 1000.0f : f;
                 });
}

inline Attr seconds()
{
    return Attr().withStringFromValueFunction ([] (float v, int)
                 {
                     return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms"
                                     : juce::String (v, 2) + " s";
                 })
                 .withValueFromStringFunction ([] (const juce::String& s)
                 {
                     const auto f = s.getFloatValue();
                     return s.containsIgnoreCase ("ms") ? f / 1000.0f : f;
                 });
}

inline Attr milliseconds()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; })
                 .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
}

inline Attr decibels()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })
                 .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
}

inline Attr semitones()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 2) + " st"; })
                 .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
}

inline Attr cents()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v)) + " ct"; })
                 .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
}

/** Float parameter; `centre` (if > min) sets the knob's midpoint value for a log-like feel. */
inline std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name,
                                                             float min, float max, float def,
                                                             Attr attributes, float centre = 0.0f)
{
    juce::NormalisableRange<float> range (min, max);
    if (centre > min && centre < max)
        range.setSkewForCentre (centre);
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def, attributes);
}

inline std::unique_ptr<juce::AudioParameterInt> makeInt (const juce::String& id, const juce::String& name,
                                                         int min, int max, int def)
{
    return std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, min, max, def);
}

inline std::unique_ptr<juce::AudioParameterChoice> makeChoice (const juce::String& id, const juce::String& name,
                                                               const juce::StringArray& choices, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, def);
}
} // namespace sparams
