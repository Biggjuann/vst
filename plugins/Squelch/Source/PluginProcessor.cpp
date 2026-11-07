#include "PluginProcessor.h"

#include "ui/KnobPanelEditor.h"
#include "ui/ParamHelpers.h"

namespace ids
{
static const juce::String tune      { "tune" };
static const juce::String wave      { "wave" };
static const juce::String cutoff    { "cutoff" };
static const juce::String resonance { "resonance" };
static const juce::String envMod    { "envmod" };
static const juce::String decay     { "decay" };
static const juce::String accent    { "accent" };
static const juce::String slide     { "slide" };
static const juce::String accentVel { "accentvel" };
static const juce::String drive     { "drive" };
static const juce::String volume    { "volume" };
} // namespace ids

namespace
{
constexpr float accentDecaySeconds = 0.2f;    // accented notes use a short, fixed filter decay
constexpr float envModOctaves      = 4.5f;    // max filter sweep from the envelope
constexpr float accentOctaves      = 2.5f;    // extra sweep from the accent circuit
constexpr float feedbackHighpassHz = 150.0f;  // keeps the bass when resonance is high
} // namespace

SquelchAudioProcessor::SquelchAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    p.tune      = apvts.getRawParameterValue (ids::tune);
    p.wave      = apvts.getRawParameterValue (ids::wave);
    p.cutoff    = apvts.getRawParameterValue (ids::cutoff);
    p.resonance = apvts.getRawParameterValue (ids::resonance);
    p.envMod    = apvts.getRawParameterValue (ids::envMod);
    p.decay     = apvts.getRawParameterValue (ids::decay);
    p.accent    = apvts.getRawParameterValue (ids::accent);
    p.slide     = apvts.getRawParameterValue (ids::slide);
    p.accentVel = apvts.getRawParameterValue (ids::accentVel);
    p.drive     = apvts.getRawParameterValue (ids::drive);
    p.volume    = apvts.getRawParameterValue (ids::volume);
}

juce::AudioProcessorValueTreeState::ParameterLayout SquelchAudioProcessor::createParameterLayout()
{
    using namespace sparams;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeFloat (ids::tune,      "Tune",       -12.0f, 12.0f, 0.0f, semitones()));
    layout.add (makeFloat (ids::wave,      "Saw/Square", 0.0f, 1.0f, 0.0f, percent()));
    layout.add (makeFloat (ids::cutoff,    "Cutoff",     30.0f, 8000.0f, 400.0f, hertz(), 600.0f));
    layout.add (makeFloat (ids::resonance, "Resonance",  0.0f, 1.0f, 0.65f, percent()));
    layout.add (makeFloat (ids::envMod,    "Env Mod",    0.0f, 1.0f, 0.5f, percent()));
    layout.add (makeFloat (ids::decay,     "Decay",      0.05f, 2.5f, 0.5f, seconds(), 0.5f));
    layout.add (makeFloat (ids::accent,    "Accent",     0.0f, 1.0f, 0.6f, percent()));
    layout.add (makeFloat (ids::slide,     "Slide Time", 10.0f, 500.0f, 60.0f, milliseconds(), 80.0f));
    layout.add (makeInt   (ids::accentVel, "Accent Vel", 1, 127, 100));
    layout.add (makeFloat (ids::drive,     "Drive",      0.0f, 1.0f, 0.15f, percent()));
    layout.add (makeFloat (ids::volume,    "Volume",     -36.0f, 6.0f, -6.0f, decibels()));

    return layout;
}

bool SquelchAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void SquelchAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = (float) sampleRate;

    filter.prepare (sampleRate);
    filter.setFeedbackHighpass (feedbackHighpassHz);

    ampEnv.prepare (sampleRate);
    ampEnv.setParameters (0.002f, 4.0f, 0.0f, 0.008f);
    filterEnv.prepare (sampleRate);

    heldNotes.clear();
    heldNotes.reserve (128);
    sliding = accented = false;
    accentSweep = 0.0f;
    accentGain = 1.0f;

    cutoffSmooth.reset (sampleRate, 0.02);
    cutoffSmooth.setCurrentAndTargetValue (p.cutoff->load());
    for (auto* s : { &resSmooth, &envModSmooth, &waveSmooth, &driveSmooth, &gainSmooth })
        s->reset (sampleRate, 0.02);
    resSmooth.setCurrentAndTargetValue (p.resonance->load());
    envModSmooth.setCurrentAndTargetValue (p.envMod->load());
    waveSmooth.setCurrentAndTargetValue (p.wave->load());
    driveSmooth.setCurrentAndTargetValue (p.drive->load());
    gainSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.volume->load()));
}

void SquelchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    int position = 0;

    for (const auto meta : midi)
    {
        const int eventPos = juce::jlimit (0, numSamples, meta.samplePosition);
        if (eventPos > position)
        {
            render (buffer, position, eventPos - position);
            position = eventPos;
        }
        handleMidi (meta.getMessage());
    }

    if (position < numSamples)
        render (buffer, position, numSamples - position);
}

void SquelchAudioProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getVelocity());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        heldNotes.clear();
        ampEnv.noteOff();
    }
}

void SquelchAudioProcessor::noteOn (int note, int velocity)
{
    const bool legato = ! heldNotes.empty();
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
    heldNotes.push_back (note);

    accented = velocity >= (int) p.accentVel->load();

    if (legato)
    {
        // Slide: glide to the new pitch, don't retrigger the envelopes.
        targetPitch = (float) note;
        sliding = true;
    }
    else
    {
        pitch = targetPitch = (float) note;
        sliding = false;
        filterEnv.noteOn();
        ampEnv.noteOn();
    }
}

void SquelchAudioProcessor::noteOff (int note)
{
    auto it = std::find (heldNotes.begin(), heldNotes.end(), note);
    if (it == heldNotes.end())
        return;

    const bool wasSounding = (note == heldNotes.back());
    heldNotes.erase (it);

    if (heldNotes.empty())
        ampEnv.noteOff();
    else if (wasSounding)
    {
        targetPitch = (float) heldNotes.back();
        sliding = true;
    }
}

void SquelchAudioProcessor::render (juce::AudioBuffer<float>& buffer, int start, int numSamples)
{
    cutoffSmooth.setTargetValue (p.cutoff->load());
    resSmooth.setTargetValue (p.resonance->load());
    envModSmooth.setTargetValue (p.envMod->load());
    waveSmooth.setTargetValue (p.wave->load());
    driveSmooth.setTargetValue (p.drive->load());
    gainSmooth.setTargetValue (juce::Decibels::decibelsToGain (p.volume->load()));

    const float tune = p.tune->load();
    const float accentAmount = p.accent->load();
    const float decay = accented ? accentDecaySeconds : p.decay->load();
    filterEnv.setParameters (0.001f, decay, 0.0f, decay);

    // Slide is an RC glide that gets ~95% of the way there in "Slide Time".
    const float slideSeconds = juce::jmax (0.001f, p.slide->load() * 0.001f);
    const float glideCoef = 1.0f - std::exp (-3.0f / (slideSeconds * sr));

    // The accent "wow" capacitor charges slower when resonance is up, like the original circuit.
    const float accentCoef = 1.0f - std::exp (-1.0f / ((0.03f + 0.12f * p.resonance->load()) * sr));
    const float accentGainTarget = accented ? 1.0f + 0.8f * accentAmount : 1.0f;
    const float gainCoef = 1.0f - std::exp (-1.0f / (0.003f * sr));

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = start; i < start + numSamples; ++i)
    {
        if (sliding)
        {
            pitch += (targetPitch - pitch) * glideCoef;
            if (std::abs (targetPitch - pitch) < 1.0e-4f)
            {
                pitch = targetPitch;
                sliding = false;
            }
        }

        osc.setFrequency (sdsp::noteToHz (pitch + tune), sr);
        const float x = osc.processSawToSquare (waveSmooth.getNextValue());

        const float env = filterEnv.next();
        accentSweep += ((accented ? env : 0.0f) - accentSweep) * accentCoef;

        const float octaves = envModSmooth.getNextValue() * envModOctaves * env
                            + accentAmount * accentSweep * accentOctaves;
        filter.setCutoff (cutoffSmooth.getNextValue() * std::exp2 (octaves));
        filter.setResonance (resSmooth.getNextValue() * 0.98f);

        float y = filter.process (x, sdsp::LadderFilter::LP24);

        accentGain += (accentGainTarget - accentGain) * gainCoef;
        y *= ampEnv.next() * accentGain;

        // Saturation; the 1/(1+drive) keeps loudness roughly constant across the knob.
        const float drive = driveSmooth.getNextValue();
        y = sdsp::fastTanh (y * (1.0f + drive * 15.0f)) / (1.0f + drive);

        y *= gainSmooth.getNextValue();

        left[i] = y;
        if (right != nullptr)
            right[i] = y;
    }
}

juce::AudioProcessorEditor* SquelchAudioProcessor::createEditor()
{
    using namespace sui;
    std::vector<Row> rows {
        { { "Oscillator", { ids::tune, ids::wave } },
          { "Filter",     { ids::cutoff, ids::resonance, ids::envMod, ids::decay, ids::accent } } },
        { { "Performance", { ids::slide, ids::accentVel } },
          { "Output",      { ids::drive, ids::volume } } },
    };
    return new KnobPanelEditor (*this, apvts, "SQUELCH  /  acid bass", std::move (rows), juce::Colour (0xffffa62b));
}

void SquelchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SquelchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SquelchAudioProcessor();
}
