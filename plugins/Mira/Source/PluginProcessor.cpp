#include "PluginProcessor.h"

#include "ui/KnobPanelEditor.h"
#include "ui/ParamHelpers.h"

namespace ids
{
static const juce::String osc1Shape { "osc1shape" }, osc1Oct { "osc1oct" }, pulseWidth { "pw" };
static const juce::String osc2Shape { "osc2shape" }, osc2Oct { "osc2oct" }, osc2Semi { "osc2semi" }, osc2Fine { "osc2fine" };
static const juce::String osc1Level { "osc1lvl" }, osc2Level { "osc2lvl" }, subLevel { "sublvl" }, noiseLevel { "noiselvl" };
static const juce::String filterMode { "fmode" }, cutoff { "cutoff" }, resonance { "res" }, drive { "drive" };
static const juce::String keytrack { "keytrack" }, envAmount { "envamt" };
static const juce::String fAttack { "fatt" }, fDecay { "fdec" }, fSustain { "fsus" }, fRelease { "frel" };
static const juce::String aAttack { "aatt" }, aDecay { "adec" }, aSustain { "asus" }, aRelease { "arel" };
static const juce::String lfoShape { "lfoshape" }, lfoRate { "lforate" }, lfoPitch { "lfopitch" }, lfoFilter { "lfofilter" }, lfoPw { "lfopw" };
static const juce::String voiceMode { "voicemode" }, glide { "glide" }, drift { "drift" }, velSens { "velsens" };
static const juce::String spread { "spread" }, volume { "volume" };
} // namespace ids

MiraAudioProcessor::MiraAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MiraAudioProcessor::createParameterLayout()
{
    using namespace sparams;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    const juce::StringArray shapes { "Saw", "Pulse", "Triangle", "Sine" };

    l.add (makeChoice (ids::osc1Shape, "Osc 1 Wave", shapes, 0));
    l.add (makeInt    (ids::osc1Oct,   "Osc 1 Oct", -2, 2, 0));
    l.add (makeFloat  (ids::pulseWidth, "Pulse Width", 0.05f, 0.95f, 0.5f, percent()));

    l.add (makeChoice (ids::osc2Shape, "Osc 2 Wave", shapes, 0));
    l.add (makeInt    (ids::osc2Oct,   "Osc 2 Oct", -2, 2, 0));
    l.add (makeInt    (ids::osc2Semi,  "Osc 2 Semi", -12, 12, 0));
    l.add (makeFloat  (ids::osc2Fine,  "Osc 2 Fine", -50.0f, 50.0f, 7.0f, cents()));

    l.add (makeFloat  (ids::osc1Level,  "Osc 1", 0.0f, 1.0f, 0.8f, percent()));
    l.add (makeFloat  (ids::osc2Level,  "Osc 2", 0.0f, 1.0f, 0.6f, percent()));
    l.add (makeFloat  (ids::subLevel,   "Sub", 0.0f, 1.0f, 0.0f, percent()));
    l.add (makeFloat  (ids::noiseLevel, "Noise", 0.0f, 1.0f, 0.0f, percent()));

    l.add (makeChoice (ids::filterMode, "Mode", { "LP 24", "LP 12", "BP 12", "HP 24" }, 0));
    l.add (makeFloat  (ids::cutoff,     "Cutoff", 20.0f, 20000.0f, 1500.0f, hertz(), 1000.0f));
    l.add (makeFloat  (ids::resonance,  "Resonance", 0.0f, 1.0f, 0.25f, percent()));
    l.add (makeFloat  (ids::drive,      "Drive", 0.0f, 1.0f, 0.2f, percent()));
    l.add (makeFloat  (ids::keytrack,   "Key Track", 0.0f, 1.0f, 0.5f, percent()));
    l.add (makeFloat  (ids::envAmount,  "Env Amount", -1.0f, 1.0f, 0.35f, bipolarPercent()));

    l.add (makeFloat  (ids::fAttack,  "Flt Attack",  0.001f, 10.0f, 0.005f, seconds(), 0.5f));
    l.add (makeFloat  (ids::fDecay,   "Flt Decay",   0.001f, 10.0f, 0.6f, seconds(), 0.8f));
    l.add (makeFloat  (ids::fSustain, "Flt Sustain", 0.0f, 1.0f, 0.25f, percent()));
    l.add (makeFloat  (ids::fRelease, "Flt Release", 0.001f, 10.0f, 0.5f, seconds(), 0.8f));

    l.add (makeFloat  (ids::aAttack,  "Amp Attack",  0.001f, 10.0f, 0.004f, seconds(), 0.5f));
    l.add (makeFloat  (ids::aDecay,   "Amp Decay",   0.001f, 10.0f, 0.6f, seconds(), 0.8f));
    l.add (makeFloat  (ids::aSustain, "Amp Sustain", 0.0f, 1.0f, 0.8f, percent()));
    l.add (makeFloat  (ids::aRelease, "Amp Release", 0.001f, 10.0f, 0.35f, seconds(), 0.8f));

    l.add (makeChoice (ids::lfoShape,  "LFO Wave", { "Sine", "Triangle", "Saw", "Square", "S&H" }, 1));
    l.add (makeFloat  (ids::lfoRate,   "LFO Rate", 0.05f, 20.0f, 4.0f, hertz(), 2.0f));
    l.add (makeFloat  (ids::lfoPitch,  "Vibrato", 0.0f, 1.0f, 0.0f, percent()));
    l.add (makeFloat  (ids::lfoFilter, "To Filter", 0.0f, 1.0f, 0.0f, percent()));
    l.add (makeFloat  (ids::lfoPw,     "To PW", 0.0f, 1.0f, 0.0f, percent()));

    l.add (makeChoice (ids::voiceMode, "Voice Mode", { "Poly", "Mono", "Legato" }, 0));
    l.add (makeFloat  (ids::glide,     "Glide", 0.0f, 2.0f, 0.0f, seconds(), 0.25f));
    l.add (makeFloat  (ids::drift,     "Analog Drift", 0.0f, 1.0f, 0.35f, percent()));
    l.add (makeFloat  (ids::velSens,   "Velocity", 0.0f, 1.0f, 0.5f, percent()));
    l.add (makeFloat  (ids::spread,    "Stereo Spread", 0.0f, 1.0f, 0.5f, percent()));
    l.add (makeFloat  (ids::volume,    "Volume", -36.0f, 6.0f, -6.0f, decibels()));

    return l;
}

bool MiraAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void MiraAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = (float) sampleRate;
    for (int i = 0; i < numVoices; ++i)
        voices[(size_t) i].prepare (sampleRate, 0x9E3779B9u * (uint32_t) (i + 1));

    lfo.prepare (sampleRate);
    monoStack.clear();
    monoStack.reserve (128);
    sustainDown = false;
    lastPitch = -1.0f;

    cutoffSmooth.reset (sampleRate, 0.02);
    cutoffSmooth.setCurrentAndTargetValue (param (ids::cutoff));
    gainSmooth.reset (sampleRate, 0.02);
    gainSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param (ids::volume)));

    updateBlockParams();
}

void MiraAudioProcessor::updateBlockParams()
{
    auto& vp = voiceParams;
    vp.shape1     = (int) param (ids::osc1Shape);
    vp.shape2     = (int) param (ids::osc2Shape);
    vp.osc1Offset = 12.0f * param (ids::osc1Oct);
    vp.osc2Offset = 12.0f * param (ids::osc2Oct) + param (ids::osc2Semi) + 0.01f * param (ids::osc2Fine);
    vp.pulseWidth = param (ids::pulseWidth);
    vp.level1     = param (ids::osc1Level);
    vp.level2     = param (ids::osc2Level);
    vp.levelSub   = param (ids::subLevel);
    vp.levelNoise = param (ids::noiseLevel);
    vp.filterMode = (int) param (ids::filterMode);
    vp.resonance  = param (ids::resonance);

    const float drive = param (ids::drive);
    vp.driveGain  = 0.7f + drive * 6.0f;
    vp.driveComp  = 1.0f / (1.0f + drive * 1.5f);

    vp.keytrack   = param (ids::keytrack);
    vp.envAmount  = param (ids::envAmount);
    vp.velSens    = param (ids::velSens);
    vp.drift      = param (ids::drift);
    vp.lfoFilter  = param (ids::lfoFilter);
    vp.lfoPw      = param (ids::lfoPw);

    const float glide = param (ids::glide);
    glideOn = glide > 0.001f;
    vp.glideCoef = glideOn ? 1.0f - std::exp (-3.0f / (glide * sr)) : 1.0f;

    lfoShape = (int) param (ids::lfoShape);
    lfoRate = param (ids::lfoRate);
    vibratoDepth = 2.0f * param (ids::lfoPitch) + 0.5f * modWheel; // semitones

    for (auto& v : voices)
        v.setEnvelopes (param (ids::fAttack), param (ids::fDecay), param (ids::fSustain), param (ids::fRelease),
                        param (ids::aAttack), param (ids::aDecay), param (ids::aSustain), param (ids::aRelease));

    const int mode = (int) param (ids::voiceMode);
    if (mode != voiceMode)
    {
        releaseAll();
        monoStack.clear();
        voiceMode = mode;
    }

    cutoffSmooth.setTargetValue (param (ids::cutoff));
    gainSmooth.setTargetValue (juce::Decibels::decibelsToGain (param (ids::volume)));
}

void MiraAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    updateBlockParams();

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

void MiraAudioProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        pitchBend = 2.0f * (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
    else if (m.isSustainPedalOn())
        setSustain (true);
    else if (m.isSustainPedalOff())
        setSustain (false);
    else if (m.isControllerOfType (1))
    {
        modWheel = (float) m.getControllerValue() / 127.0f;
        vibratoDepth = 2.0f * param (ids::lfoPitch) + 0.5f * modWheel;
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        monoStack.clear();
        releaseAll();
    }
}

float MiraAudioProcessor::nextPan()
{
    // Alternate voices left/right with a little randomness, scaled by Stereo Spread.
    panToggle = -panToggle;
    return param (ids::spread) * (float) panToggle * (0.6f + 0.4f * panRng.next01());
}

void MiraAudioProcessor::noteOn (int note, float velocity)
{
    if (voiceMode == Poly)
    {
        MiraVoice* target = nullptr;

        for (auto& v : voices) // same note already ringing: reuse it
            if (v.isActive() && v.note == note) { target = &v; break; }

        if (target == nullptr)
            for (auto& v : voices)
                if (! v.isActive()) { target = &v; break; }

        auto oldest = [this] (bool releasedOnly) -> MiraVoice*
        {
            MiraVoice* best = nullptr;
            for (auto& v : voices)
                if ((! releasedOnly || (! v.keyDown && ! v.sustained)) && (best == nullptr || v.order < best->order))
                    best = &v;
            return best;
        };

        if (target == nullptr) target = oldest (true);
        if (target == nullptr) target = oldest (false);

        const float from = (glideOn && lastPitch >= 0.0f) ? lastPitch : -1.0f;
        target->start (note, velocity, from, true, ++noteCounter, nextPan());
        lastPitch = (float) note;
        return;
    }

    // Mono / Legato: one voice, last-note priority.
    auto& v = voices[0];
    const bool wasHeld = ! monoStack.empty();
    monoStack.erase (std::remove_if (monoStack.begin(), monoStack.end(), [note] (auto& e) { return e.first == note; }), monoStack.end());
    monoStack.emplace_back (note, velocity);

    const bool retrigger = voiceMode == Mono || ! wasHeld || ! v.isActive();
    const float from = (glideOn && v.isActive()) ? v.getPitch() : -1.0f;
    v.start (note, velocity, from, retrigger, ++noteCounter, 0.0f);
}

void MiraAudioProcessor::noteOff (int note)
{
    if (voiceMode == Poly)
    {
        for (auto& v : voices)
        {
            if (v.keyDown && v.note == note)
            {
                v.keyDown = false;
                if (sustainDown)
                    v.sustained = true;
                else
                    v.release();
            }
        }
        return;
    }

    monoStack.erase (std::remove_if (monoStack.begin(), monoStack.end(), [note] (auto& e) { return e.first == note; }), monoStack.end());
    auto& v = voices[0];

    if (monoStack.empty())
    {
        if (v.note == note)
        {
            v.keyDown = false;
            if (sustainDown)
                v.sustained = true;
            else
                v.release();
        }
    }
    else if (v.note == note)
    {
        // Fall back to the most recent note still held.
        const auto [prevNote, prevVel] = monoStack.back();
        const float from = glideOn ? v.getPitch() : -1.0f;
        v.start (prevNote, prevVel, from, voiceMode == Mono, ++noteCounter, 0.0f);
    }
}

void MiraAudioProcessor::setSustain (bool down)
{
    sustainDown = down;
    if (! down)
        for (auto& v : voices)
            if (v.sustained && ! v.keyDown)
            {
                v.sustained = false;
                v.release();
            }
}

void MiraAudioProcessor::releaseAll()
{
    for (auto& v : voices)
    {
        v.keyDown = v.sustained = false;
        v.release();
    }
}

void MiraAudioProcessor::render (juce::AudioBuffer<float>& buffer, int start, int numSamples)
{
    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    constexpr float voiceMix = 0.35f;

    for (int i = start; i < start + numSamples; ++i)
    {
        MiraSampleMods mods;
        mods.cutoffHz = cutoffSmooth.getNextValue();
        mods.lfo = lfo.process (lfoShape, lfoRate);
        mods.pitchOffset = pitchBend + mods.lfo * vibratoDepth;

        float l = 0.0f, r = 0.0f;
        for (auto& v : voices)
            v.render (l, r, voiceParams, mods);

        const float gain = gainSmooth.getNextValue() * voiceMix;
        if (right != nullptr)
        {
            left[i]  = l * gain;
            right[i] = r * gain;
        }
        else
        {
            left[i] = 0.5f * (l + r) * gain;
        }
    }
}

juce::AudioProcessorEditor* MiraAudioProcessor::createEditor()
{
    using namespace sui;
    std::vector<Row> rows {
        { { "Oscillator 1", { ids::osc1Shape, ids::osc1Oct, ids::pulseWidth } },
          { "Oscillator 2", { ids::osc2Shape, ids::osc2Oct, ids::osc2Semi, ids::osc2Fine } },
          { "Mixer",        { ids::osc1Level, ids::osc2Level, ids::subLevel, ids::noiseLevel } } },
        { { "Filter",       { ids::filterMode, ids::cutoff, ids::resonance, ids::drive, ids::keytrack, ids::envAmount } },
          { "Filter Envelope", { ids::fAttack, ids::fDecay, ids::fSustain, ids::fRelease } } },
        { { "Amp Envelope", { ids::aAttack, ids::aDecay, ids::aSustain, ids::aRelease } },
          { "LFO",          { ids::lfoShape, ids::lfoRate, ids::lfoPitch, ids::lfoFilter, ids::lfoPw } } },
        { { "Voice",        { ids::voiceMode, ids::glide, ids::drift, ids::velSens, ids::spread, ids::volume } } },
    };
    return new KnobPanelEditor (*this, apvts, "MIRA  /  analog poly", std::move (rows), juce::Colour (0xff4fd1c5));
}

void MiraAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MiraAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MiraAudioProcessor();
}
