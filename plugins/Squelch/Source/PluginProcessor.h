#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "dsp/Envelope.h"
#include "dsp/LadderFilter.h"
#include "dsp/Oscillator.h"

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
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
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

private:
    void handleMidi (const juce::MidiMessage& m);
    void noteOn (int note, int velocity);
    void noteOff (int note);
    void render (juce::AudioBuffer<float>& buffer, int start, int numSamples);

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
    } p;

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
