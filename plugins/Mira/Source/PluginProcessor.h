#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "MiraVoice.h"

/**
    Mira: an 8-voice virtual-analog polysynth aiming for that thick, warm,
    "expensive analog" sound: band-limited oscillators, a zero-delay-feedback
    ladder filter with saturation, RC-style envelopes, and per-voice analog drift.
*/
class MiraAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int numVoices = 8;

    MiraAudioProcessor();

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
    enum VoiceMode { Poly = 0, Mono, Legato };

    void handleMidi (const juce::MidiMessage& m);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void setSustain (bool down);
    void releaseAll();
    void updateBlockParams();
    void render (juce::AudioBuffer<float>& buffer, int start, int numSamples);
    float param (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    float nextPan();

    std::array<MiraVoice, numVoices> voices;
    MiraVoiceParams voiceParams;
    sdsp::Lfo lfo;
    sdsp::Noise panRng { 0xBADA55u };

    std::vector<std::pair<int, float>> monoStack; // (note, velocity), most recent last
    int voiceMode = Poly;
    bool glideOn = false, sustainDown = false;
    float lastPitch = -1.0f;
    float pitchBend = 0.0f, modWheel = 0.0f;
    float vibratoDepth = 0.0f, lfoRate = 4.0f;
    int lfoShape = 0;
    uint64_t noteCounter = 0;
    int panToggle = 1;
    float sr = 44100.0f;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSmooth;
    juce::SmoothedValue<float> gainSmooth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiraAudioProcessor)
};
