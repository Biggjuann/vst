#include "PluginProcessor.h"

#include "FavoritesBar.h"
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
static const juce::String genKey    { "genkey" };
static const juce::String genScale  { "genscale" };
static const juce::String genSeed   { "genseed" };
static const juce::String genDensity { "gendensity" };
static const juce::String genAccent { "genaccent" };
static const juce::String genSlide  { "genslide" };
static const juce::String genRange  { "genrange" };
static const juce::String genEvolve { "genevolve" };
static const juce::String genVariation { "genvariation" };
static const juce::String midiOut   { "midiout" };
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
    p.genKey       = apvts.getRawParameterValue (ids::genKey);
    p.genScale     = apvts.getRawParameterValue (ids::genScale);
    p.genSeed      = apvts.getRawParameterValue (ids::genSeed);
    p.genDensity   = apvts.getRawParameterValue (ids::genDensity);
    p.genAccent    = apvts.getRawParameterValue (ids::genAccent);
    p.genSlide     = apvts.getRawParameterValue (ids::genSlide);
    p.genRange     = apvts.getRawParameterValue (ids::genRange);
    p.genEvolve    = apvts.getRawParameterValue (ids::genEvolve);
    p.genVariation = apvts.getRawParameterValue (ids::genVariation);
    p.midiOut      = apvts.getRawParameterValue (ids::midiOut);
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
    layout.add (makeChoice (ids::genKey,       "Key", gen::keyNames(), 0));
    layout.add (makeChoice (ids::genScale,     "Scale", gen::scaleNames(), 0));
    layout.add (makeInt    (ids::genSeed,      "Pattern #", 1, 9999, 1));
    layout.add (makeFloat  (ids::genDensity,   "Density", 0.0f, 1.0f, 0.75f, percent()));
    layout.add (makeFloat  (ids::genAccent,    "Accents", 0.0f, 1.0f, 0.4f, percent()));
    layout.add (makeFloat  (ids::genSlide,     "Slides", 0.0f, 1.0f, 0.35f, percent()));
    layout.add (makeFloat  (ids::genRange,     "Octaves", 0.0f, 1.0f, 0.4f, percent()));
    layout.add (makeChoice (ids::genEvolve,    "Evolve", { "Off", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" }, 0));
    layout.add (makeFloat  (ids::genVariation, "Variation", 0.0f, 1.0f, 0.25f, percent()));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::midiOut, 1 }, "MIDI Out", true));
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
    midiOutput.ensureSize (4096);
    numPendingOffs = 0;
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
    // MIDI out: note-offs scheduled in earlier blocks land first, then whatever the sequencer plays.
    midiOutput.clear();
    blockSize = buffer.getNumSamples();
    for (int i = 0; i < numPendingOffs;)
    {
        auto& off = pendingOffs[(size_t) i];
        if (off.samplesLeft < blockSize)
        {
            midiOutput.addEvent (juce::MidiMessage::noteOff (1, off.note), juce::jmax (0, off.samplesLeft));
            pendingOffs[(size_t) i] = pendingOffs[(size_t) --numPendingOffs];
        }
        else
        {
            off.samplesLeft -= blockSize;
            ++i;
        }
    }

    inBlock = true;
    tickSample = 0;
    processInternal (buffer, midi);
    inBlock = false;

    if (p.midiOut->load() > 0.5f)
        midi.swapWith (midiOutput);
    else
        midi.clear();
}

void SquelchAudioProcessor::emitNoteOn (int note, bool accent)
{
    if (inBlock)
        midiOutput.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) (accent ? accentVelocity : normalVelocity())), tickSample);
}

void SquelchAudioProcessor::emitNoteOff (int note, int delaySamples)
{
    const int at = (inBlock ? tickSample : 0) + delaySamples;
    if (inBlock && at < blockSize)
        midiOutput.addEvent (juce::MidiMessage::noteOff (1, note), at);
    else if (numPendingOffs < (int) pendingOffs.size())
        pendingOffs[(size_t) numPendingOffs++] = { note, inBlock ? at - blockSize : at };
}

void SquelchAudioProcessor::processInternal (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    {
        const juce::SpinLock::ScopedTryLockType lock (generatorLock);
        if (lock.isLocked())
            updateGenerator();
    }

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
        tickSample = at;
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
    {
        activePattern = (int) p.pattern->load();
        evolve();
    }
    playingPattern.store (activePattern);

    const auto s = patterns.get (activePattern, step);

    if (stepStart)
    {
        currentStep.store (step);

        if (s.time == seq::Rest)
        {
            if (seqNote >= 0)
            {
                noteOff (seqNote);
                emitNoteOff (seqNote);
            }
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
            {
                noteOff (seqNote);
                // MIDI out: overlap the notes slightly so the recorded clip replays as a slide.
                emitNoteOn (note, s.accent);
                emitNoteOff (seqNote, (int) (0.006 * sr));
            }
        }
        else
        {
            if (seqNote >= 0)
            {
                noteOff (seqNote);
                emitNoteOff (seqNote);
            }
            startNote (note, s.accent);
            emitNoteOn (note, s.accent);
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
            emitNoteOff (seqNote);
            seqNote = -1;
        }
    }
}

//==============================================================================
// Pattern generator

gen::Settings SquelchAudioProcessor::readGeneratorSettings() const
{
    gen::Settings g;
    g.key = (int) p.genKey->load();
    g.scale = (int) p.genScale->load();
    g.seed = (int) p.genSeed->load();
    g.density = p.genDensity->load();
    g.accent = p.genAccent->load();
    g.slide = p.genSlide->load();
    g.range = p.genRange->load();
    return g;
}

void SquelchAudioProcessor::updateGenerator()
{
    const auto settings = readGeneratorSettings();
    if (adoptGeneratorSettings.exchange (false))
    {
        // Plugin just loaded / a set was restored: keep the saved patterns as they are.
        lastGenSettings = settings;
        return;
    }
    if (settings != lastGenSettings)
    {
        lastGenSettings = settings;
        evolveCount = 0;
        patterns.setPattern ((int) p.pattern->load(), gen::generate (settings));
    }
}

void SquelchAudioProcessor::evolve()
{
    static constexpr int barsPerEvolve[] = { 0, 1, 2, 4, 8, 16 };
    const int bars = barsPerEvolve[juce::jlimit (0, 5, (int) p.genEvolve->load())];

    if (firstDownbeat || bars == 0)
    {
        firstDownbeat = false;
        loopsSinceEvolve = 0;
        return;
    }
    if (++loopsSinceEvolve < bars)
        return;
    loopsSinceEvolve = 0;

    // Blend a new pattern (from a seed derived from Pattern #) into the playing one.
    const int index = ++evolveCount;
    patterns.setPattern (activePattern, gen::evolve (getPatternSteps (activePattern), readGeneratorSettings(), index, p.genVariation->load()));
}

void SquelchAudioProcessor::reset()
{
    sequencerStop();
    internalPpq = 0.0;
}

void SquelchAudioProcessor::sequencerStop()
{
    if (seqNote >= 0)
    {
        noteOff (seqNote);
        emitNoteOff (seqNote);
    }
    seqNote = -1;
    prevSlide = false;
    seqRunning = false;
    firstDownbeat = true;
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
        { { "Pattern Generator", { ids::genKey, ids::genScale, ids::genSeed, ids::genDensity, ids::genAccent,
                                   ids::genSlide, ids::genRange, ids::genEvolve, ids::genVariation } } },
    };
    EditorExtras extras;
    extras.footer = std::make_unique<SequencerPanel> (*this);
    extras.footerHeight = SequencerPanel::preferredHeight;
    extras.footerMinWidth = SequencerPanel::minimumWidth;
    extras.titleBar = std::make_unique<FavoritesBar> (*this);
    extras.titleBarWidth = FavoritesBar::preferredWidth;
    return new KnobPanelEditor (*this, apvts, "SQUELCH  /  acid bass", std::move (rows), juce::Colour (0xffffa62b), std::move (extras));
}

juce::ValueTree SquelchAudioProcessor::createFullState()
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("PATTERNS"), nullptr);
    juce::ValueTree bank ("PATTERNS");
    for (int i = 0; i < seq::numPatterns; ++i)
        bank.setProperty ("p" + juce::String (i), patterns.serialise (i), nullptr);
    state.appendChild (bank, nullptr);
    return state;
}

void SquelchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createFullState().createXml())
        copyXmlToBinary (*xml, destData);
}

//==============================================================================
// Favorites

std::array<seq::Step, seq::numSteps> SquelchAudioProcessor::getPatternSteps (int pattern) const
{
    std::array<seq::Step, seq::numSteps> steps {};
    for (int i = 0; i < seq::numSteps; ++i)
        steps[(size_t) i] = patterns.get (pattern, i);
    return steps;
}

std::unique_ptr<juce::XmlElement> SquelchAudioProcessor::createSoundFavorite (const juce::String& name)
{
    auto xml = createFullState().createXml();
    if (xml != nullptr)
        xml->setAttribute ("favoriteName", name);
    return xml;
}

bool SquelchAudioProcessor::loadSoundFavorite (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (apvts.state.getType()))
        return false;

    const auto state = juce::ValueTree::fromXml (xml);
    // Hold the generator lock so it doesn't "regenerate" over the patterns we're loading.
    const juce::SpinLock::ScopedLockType lock (generatorLock);

    for (const auto& child : state)
    {
        const auto id = child["id"].toString();
        // Leave the transport alone: loading a favorite shouldn't start/stop or retime playback.
        if (id.isEmpty() || id == ids::seqOn || id == ids::clock || id == ids::tempo || id == ids::midiOut)
            continue;
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) child["value"]));
    }

    const auto bank = state.getChildWithName ("PATTERNS");
    for (int i = 0; i < seq::numPatterns; ++i)
        if (bank.hasProperty ("p" + juce::String (i)))
            patterns.deserialise (i, bank["p" + juce::String (i)].toString());

    adoptGeneratorSettings.store (true);
    return true;
}

void SquelchAudioProcessor::loadIntoCurrentPattern (const std::array<seq::Step, seq::numSteps>& steps, int length)
{
    patterns.setPattern (getSelectedPattern(), steps);
    if (auto* param = apvts.getParameter (ids::length))
        param->setValueNotifyingHost (param->convertTo0to1 ((float) (juce::jlimit (1, seq::numSteps, length) - 1)));
}

//==============================================================================
// MIDI export: the same rules as the live sequencer, written out as a clip.

juce::MidiFile SquelchAudioProcessor::createMidiClip (int bars) const
{
    constexpr int ticksPerQuarter = 960, stepTicks = ticksPerQuarter / 4, halfStep = stepTicks / 2;
    constexpr int slideOverlap = 12; // a few ms, so the clip replays as a slide

    static constexpr int barsPerEvolve[] = { 0, 1, 2, 4, 8, 16 };
    const int evolveEvery = barsPerEvolve[juce::jlimit (0, 5, (int) p.genEvolve->load())];
    const auto settings = readGeneratorSettings();
    const float variation = p.genVariation->load();
    const int length = getLength();
    const int shift = transpose.load();
    const int normalVel = normalVelocity();

    auto steps = getPatternSteps (getSelectedPattern());
    int evolveIndex = evolveCount.load(), loops = 0;

    juce::MidiMessageSequence sequence;
    auto on = [&] (int note, int tick, bool accent)
    {
        sequence.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) (accent ? accentVelocity : normalVel)), tick);
    };
    auto off = [&] (int note, int tick) { sequence.addEvent (juce::MidiMessage::noteOff (1, note), tick); };

    int current = -1;
    bool prevSlideOut = false;
    const int totalSteps = bars * seq::numSteps;
    for (int s = 0; s < totalSteps; ++s)
    {
        const int step = s % length;
        if (step == 0 && s > 0 && evolveEvery > 0 && ++loops % evolveEvery == 0)
            steps = gen::evolve (steps, settings, ++evolveIndex, variation);

        const auto& st = steps[(size_t) step];
        const auto& next = steps[(size_t) ((step + 1) % length)];
        const int t = s * stepTicks;

        if (st.time == seq::Rest)
        {
            if (current >= 0) off (current, t);
            current = -1;
            prevSlideOut = false;
        }
        else if (st.time == seq::Tie)
        {
            prevSlideOut = current >= 0 && st.slide;
        }
        else
        {
            const int note = st.midiNote (shift);
            if (prevSlideOut && current >= 0)
            {
                if (note != current)
                {
                    on (note, t, st.accent);
                    off (current, t + slideOverlap);
                }
            }
            else
            {
                if (current >= 0) off (current, t);
                on (note, t, st.accent);
            }
            current = note;
            prevSlideOut = st.slide;
        }

        // Half-step: the gate closes unless this step slides or the next one is a tie.
        if (current >= 0 && st.time != seq::Rest && ! (st.slide || next.time == seq::Tie))
        {
            off (current, t + halfStep);
            current = -1;
        }
    }
    if (current >= 0)
        off (current, totalSteps * stepTicks);

    sequence.sort();
    sequence.updateMatchedPairs();

    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::textMetaEvent (3, "Squelch Pattern " + juce::String (getSelectedPattern() + 1)), 0);
    track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    track.addSequence (sequence, 0);
    track.addEvent (juce::MidiMessage::endOfTrack(), totalSteps * stepTicks);
    track.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ticksPerQuarter);
    file.addTrack (track);
    return file;
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
            adoptGeneratorSettings.store (true);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SquelchAudioProcessor();
}
