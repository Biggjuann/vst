#include "PluginProcessor.h"

#include "SequencerPanel.h"
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
static const juce::String seqOn     { "seqon" };
static const juce::String pattern   { "pattern" };
static const juce::String length    { "length" };
static const juce::String tempo     { "tempo" };
static const juce::String clock     { "clock" };
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
    p.seqOn     = apvts.getRawParameterValue (ids::seqOn);
    p.pattern   = apvts.getRawParameterValue (ids::pattern);
    p.length    = apvts.getRawParameterValue (ids::length);
    p.tempo     = apvts.getRawParameterValue (ids::tempo);
    p.clock     = apvts.getRawParameterValue (ids::clock);
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

    juce::StringArray patternNames, lengthNames;
    for (int i = 1; i <= seq::numPatterns; ++i) patternNames.add (juce::String (i));
    for (int i = 1; i <= seq::numSteps; ++i)    lengthNames.add (juce::String (i));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::seqOn, 1 }, "Sequencer", false));
    layout.add (makeChoice (ids::pattern, "Pattern", patternNames, 0));
    layout.add (makeChoice (ids::length,  "Length", lengthNames, seq::numSteps - 1));
    layout.add (makeChoice (ids::clock,   "Clock", { "Host", "Internal" }, 0));
    layout.add (makeFloat  (ids::tempo,   "Tempo", 60.0f, 200.0f, 125.0f,
                            Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " BPM"; })));

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
    internalPpq = 0.0;
    seqRunning = false;
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

    const bool seqOn = p.seqOn->load() > 0.5f;
    if (seqOn != seqWasOn)
    {
        allNotesOff();
        sequencerStop();
        internalPpq = 0.0; // RUN always starts the internal clock from step 1
        seqWasOn = seqOn;
    }
    if (seqOn)
    {
        processSequencer (buffer, midi);
        return;
    }

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
        allNotesOff();
}

void SquelchAudioProcessor::allNotesOff()
{
    heldNotes.clear();
    ampEnv.noteOff();
}

void SquelchAudioProcessor::noteOn (int note, int velocity)
{
    startNote (note, velocity >= (int) p.accentVel->load());
}

void SquelchAudioProcessor::startNote (int note, bool accent)
{
    const bool legato = ! heldNotes.empty();
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
    heldNotes.push_back (note);

    accented = accent;

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

//==============================================================================
// Step sequencer
//
// Runs on 32nd-note "ticks": even ticks start a step, odd ticks are the half-step point
// where the gate closes (unless the step slides or the next step is a tie). That matches
// the original's behaviour: notes are half a 16th long, slides and ties hold the gate,
// and a slid-to note glides without retriggering the envelopes.

void SquelchAudioProcessor::processSequencer (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const int numSamples = buffer.getNumSamples();

    // While the sequencer runs, MIDI keys transpose the pattern (C3 = untransposed).
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
            transpose.store (juce::jlimit (-24, 24, m.getNoteNumber() - 60));
    }

    // Clock: follow the host transport (Host), or run free at "Tempo" (Internal, or when
    // the host provides no clock at all, e.g. the standalone app).
    double ppq = internalPpq, bpm = p.tempo->load();
    bool running = true, hostClock = false;
    const bool useHost = p.clock->load() < 0.5f;
    if (auto* host = useHost ? getPlayHead() : nullptr)
    {
        if (const auto pos = host->getPosition())
        {
            if (pos->getPpqPosition().hasValue() && pos->getBpm().hasValue())
            {
                hostClock = true;
                ppq = *pos->getPpqPosition();
                bpm = *pos->getBpm();
                running = pos->getIsPlaying();
            }
        }
    }

    if (! running || bpm <= 0.0)
    {
        if (seqRunning)
            sequencerStop();
        render (buffer, 0, numSamples);
        return;
    }

    const double samplesPerQuarter = sr * 60.0 / bpm;
    const double blockQuarters = numSamples / samplesPerQuarter;

    if (! seqRunning || std::abs (ppq - expectedPpq) > 1.0e-3)
    {
        // Just started, or the playhead jumped (loop, locate): resync to the next tick.
        if (! seqRunning)
            activePattern = (int) p.pattern->load();
        seqRunning = true;
        lastTick = (int64_t) std::ceil (ppq * 8.0 - 1.0e-6) - 1;
    }

    int position = 0;
    for (int64_t tick = lastTick + 1;; ++tick)
    {
        const double offset = ((double) tick / 8.0 - ppq) * samplesPerQuarter;
        if (offset >= numSamples)
            break;
        const int at = juce::jlimit (0, numSamples - 1, (int) std::floor (offset));
        if (at > position)
        {
            render (buffer, position, at - position);
            position = at;
        }
        sequencerTick (tick);
        lastTick = tick;
    }
    if (position < numSamples)
        render (buffer, position, numSamples - position);

    expectedPpq = ppq + blockQuarters;
    if (! hostClock)
        internalPpq += blockQuarters;
}

void SquelchAudioProcessor::sequencerTick (int64_t tick)
{
    const int length = (int) p.length->load() + 1;
    const int64_t absoluteStep = tick >= 0 ? tick / 2 : (tick - 1) / 2;
    const int step = (int) (((absoluteStep % length) + length) % length);
    const int nextStep = (step + 1) % length;
    const bool stepStart = (tick & 1) == 0;

    // Pattern changes wait for the downbeat, like the original.
    if (stepStart && step == 0)
        activePattern = (int) p.pattern->load();
    playingPattern.store (activePattern);

    const auto s = patterns.get (activePattern, step);

    if (stepStart)
    {
        currentStep.store (step);

        if (s.time == seq::Rest)
        {
            if (seqNote >= 0)
                noteOff (seqNote);
            seqNote = -1;
            prevSlide = false;
            return;
        }

        if (s.time == seq::Tie)
        {
            // Keep the previous note sounding; its slide flag carries into the next step.
            prevSlide = seqNote >= 0 && s.slide;
            return;
        }

        const int note = s.midiNote (transpose.load());
        if (prevSlide && seqNote >= 0)
        {
            startNote (note, s.accent); // legato: glides, no envelope retrigger
            if (note != seqNote)
                noteOff (seqNote);
        }
        else
        {
            if (seqNote >= 0)
                noteOff (seqNote);
            startNote (note, s.accent);
        }
        seqNote = note;
        prevSlide = s.slide;
    }
    else if (seqNote >= 0 && s.time != seq::Rest)
    {
        const bool holdGate = s.slide || patterns.get (activePattern, nextStep).time == seq::Tie;
        if (! holdGate)
        {
            noteOff (seqNote);
            seqNote = -1;
        }
    }
}

void SquelchAudioProcessor::reset()
{
    sequencerStop();
    internalPpq = 0.0;
}

void SquelchAudioProcessor::sequencerStop()
{
    if (seqNote >= 0)
        noteOff (seqNote);
    seqNote = -1;
    prevSlide = false;
    seqRunning = false;
    currentStep.store (-1);
    allNotesOff();
}

//==============================================================================
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
    EditorExtras extras;
    extras.footer = std::make_unique<SequencerPanel> (*this);
    extras.footerHeight = SequencerPanel::preferredHeight;
    extras.footerMinWidth = SequencerPanel::minimumWidth;
    return new KnobPanelEditor (*this, apvts, "SQUELCH  /  acid bass", std::move (rows), juce::Colour (0xffffa62b), std::move (extras));
}

void SquelchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("PATTERNS"), nullptr);
    juce::ValueTree bank ("PATTERNS");
    for (int i = 0; i < seq::numPatterns; ++i)
        bank.setProperty ("p" + juce::String (i), patterns.serialise (i), nullptr);
    state.appendChild (bank, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SquelchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            const auto bank = state.getChildWithName ("PATTERNS");
            for (int i = 0; i < seq::numPatterns; ++i)
                if (bank.hasProperty ("p" + juce::String (i)))
                    patterns.deserialise (i, bank["p" + juce::String (i)].toString());
            apvts.replaceState (state);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SquelchAudioProcessor();
}
