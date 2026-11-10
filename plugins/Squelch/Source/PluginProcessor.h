#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "dsp/Envelope.h"
#include "dsp/LadderFilter.h"
#include "dsp/Oscillator.h"
#include "Generator.h"
#include "Sequencer.h"

/**
    Squelch: a monophonic acid bass synth in the spirit of the classic silver bass box.

    How to play it from an Ableton MIDI clip:
      - Slide: overlap a note with the next one (legato) and the pitch glides,
               without retriggering the envelopes.
      - Accent: notes with velocity >= "Accent Vel" are accented: louder, shorter and
               brighter filter envelope, and successive accents build up the "wow" sweep.
*/
class SquelchAudioProcessor final : public juce::AudioProcessor
{
public:
    SquelchAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; } // the sequencer's notes, for recording
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // ---- Step sequencer (shared with the editor) ----
    seq::PatternBank patterns;
    std::atomic<int> currentStep { -1 };     // step being played, -1 when stopped
    std::atomic<int> playingPattern { 0 };   // pattern actually playing (changes on the downbeat)
    std::atomic<int> transpose { 0 };        // set by MIDI keys while the sequencer runs

    /** Called from the editor's timer so generator edits apply even when no audio is running. */
    void pollGenerator()
    {
        const juce::SpinLock::ScopedLockType lock (generatorLock);
        updateGenerator();
    }

    // ---- Favorites ----
    /** Everything: knobs, generator settings and all 8 patterns. */
    std::unique_ptr<juce::XmlElement> createSoundFavorite (const juce::String& name);
    bool loadSoundFavorite (const juce::XmlElement& xml);
    /** Replace the selected pattern (and set its length). */
    void loadIntoCurrentPattern (const std::array<seq::Step, seq::numSteps>& steps, int length);
    std::array<seq::Step, seq::numSteps> getPatternSteps (int pattern) const;
    int getSelectedPattern() const { return (int) p.pattern->load(); }
    int getLength() const { return (int) p.length->load() + 1; }

    // ---- MIDI export ----
    /** The selected pattern as a MIDI clip, `bars` long, including upcoming Evolve changes. */
    juce::MidiFile createMidiClip (int bars) const;
    /** Velocities used for MIDI out: accented notes 127, others just below the Accent Vel threshold. */
    int normalVelocity() const { return juce::jlimit (1, 126, juce::jmin (90, (int) p.accentVel->load() - 1)); }
    static constexpr int accentVelocity = 127;

private:
    void processInternal (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);
    void emitNoteOn (int note, bool accent);
    void emitNoteOff (int note, int delaySamples = 0);
    juce::ValueTree createFullState();
    void handleMidi (const juce::MidiMessage& m);
    void noteOn (int note, int velocity);
    void startNote (int note, bool accent);
    void noteOff (int note);
    void allNotesOff();
    void render (juce::AudioBuffer<float>& buffer, int start, int numSamples);

    void processSequencer (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);
    void sequencerTick (int64_t tick);
    void sequencerStop();
    gen::Settings readGeneratorSettings() const;
    void updateGenerator();
    void evolve();

    struct Params
    {
        std::atomic<float>* tune = nullptr;
        std::atomic<float>* wave = nullptr;
        std::atomic<float>* cutoff = nullptr;
        std::atomic<float>* resonance = nullptr;
        std::atomic<float>* envMod = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* accent = nullptr;
        std::atomic<float>* slide = nullptr;
        std::atomic<float>* accentVel = nullptr;
        std::atomic<float>* drive = nullptr;
        std::atomic<float>* volume = nullptr;
        std::atomic<float>* seqOn = nullptr;
        std::atomic<float>* pattern = nullptr;
        std::atomic<float>* length = nullptr;
        std::atomic<float>* tempo = nullptr;
        std::atomic<float>* clock = nullptr;
        std::atomic<float>* genKey = nullptr;
        std::atomic<float>* genScale = nullptr;
        std::atomic<float>* genSeed = nullptr;
        std::atomic<float>* genDensity = nullptr;
        std::atomic<float>* genAccent = nullptr;
        std::atomic<float>* genSlide = nullptr;
        std::atomic<float>* genRange = nullptr;
        std::atomic<float>* genEvolve = nullptr;
        std::atomic<float>* genVariation = nullptr;
        std::atomic<float>* midiOut = nullptr;
    } p;

    // MIDI output (sequencer notes), with note-offs that may land in a later block.
    juce::MidiBuffer midiOutput;
    struct PendingOff { int note; int samplesLeft; };
    std::array<PendingOff, 16> pendingOffs {};
    int numPendingOffs = 0, tickSample = 0, blockSize = 0;
    bool inBlock = false;

    // Generator: regenerate when its controls change (but not when a saved set loads).
    gen::Settings lastGenSettings;
    juce::SpinLock generatorLock;
    std::atomic<bool> adoptGeneratorSettings { true };
    int loopsSinceEvolve = 0;
    std::atomic<int> evolveCount { 0 };
    bool firstDownbeat = true;

    // Sequencer runtime state (audio thread only)
    bool seqWasOn = false, seqRunning = false, prevSlide = false;
    int seqNote = -1, activePattern = 0;
    int64_t lastTick = 0;
    double expectedPpq = 0.0, internalPpq = 0.0;

    float sr = 44100.0f;

    sdsp::Oscillator osc;
    sdsp::LadderFilter filter;
    sdsp::Envelope ampEnv, filterEnv;

    std::vector<int> heldNotes;
    float pitch = 48.0f, targetPitch = 48.0f;
    bool sliding = false, accented = false;
    float accentSweep = 0.0f, accentGain = 1.0f;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSmooth;
    juce::SmoothedValue<float> resSmooth, envModSmooth, waveSmooth, driveSmooth, gainSmooth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SquelchAudioProcessor)
};
