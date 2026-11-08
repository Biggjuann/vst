#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>

#include "ui/ParamHelpers.h"

/**
    Shared engine for the drum machines: TR-style step sequencer (one row per voice plus an
    accent row), host sync / internal clock, swing, MIDI note triggering, pattern storage.
    A plugin derives from DrumMachineProcessor and supplies its voices.
*/
namespace drum
{
constexpr int numSteps = 16;
constexpr int numPatterns = 16;
constexpr int maxVoices = 16;
constexpr int accentRow = maxVoices; // the last row of every pattern

struct VoiceInfo
{
    juce::String id;        // parameter prefix, e.g. "bd"
    juce::String name;      // "Bass Drum"
    juce::String shortName; // "BD"
    std::vector<int> midiNotes;
};

/** 16 patterns x (16 voice rows + accent row) x 16 steps, stored as bitmasks. */
class PatternBank
{
public:
    bool get (int pattern, int row, int step) const
    {
        return (rows[(size_t) wrap (pattern)][(size_t) row].load (std::memory_order_relaxed) >> step) & 1u;
    }

    void set (int pattern, int row, int step, bool on)
    {
        auto& r = rows[(size_t) wrap (pattern)][(size_t) row];
        auto v = r.load();
        v = on ? (v | (1u << step)) : (v & ~(1u << step));
        r.store (v);
        ++version;
    }

    uint32_t getRow (int pattern, int row) const { return rows[(size_t) wrap (pattern)][(size_t) row].load(); }
    void setRow (int pattern, int row, uint32_t mask) { rows[(size_t) wrap (pattern)][(size_t) row].store (mask & 0xffffu); ++version; }

    /** "x...x...x...x..." per row; '.' off, 'x' on. Used for demo patterns. */
    void setRowFromString (int pattern, int row, const char* text)
    {
        uint32_t mask = 0;
        for (int i = 0; i < numSteps && text[i] != 0; ++i)
            if (text[i] != '.' && text[i] != ' ')
                mask |= 1u << i;
        setRow (pattern, row, mask);
    }

    juce::String serialise (int pattern) const
    {
        juce::StringArray parts;
        for (int r = 0; r <= maxVoices; ++r)
            parts.add (juce::String::toHexString ((int) getRow (pattern, r)));
        return parts.joinIntoString (",");
    }

    void deserialise (int pattern, const juce::String& text)
    {
        auto parts = juce::StringArray::fromTokens (text, ",", "");
        for (int r = 0; r <= maxVoices && r < parts.size(); ++r)
            setRow (pattern, r, (uint32_t) parts[r].getHexValue32());
    }

    std::atomic<uint32_t> version { 0 };

private:
    static int wrap (int p) { return ((p % numPatterns) + numPatterns) % numPatterns; }
    std::array<std::array<std::atomic<uint32_t>, maxVoices + 1>, numPatterns> rows {};
};

class DrumMachineProcessor : public juce::AudioProcessor
{
public:
    DrumMachineProcessor (std::vector<VoiceInfo> voiceList, juce::AudioProcessorValueTreeState::ParameterLayout layout)
        : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "PARAMS", std::move (layout)),
          voices (std::move (voiceList))
    {
        jassert ((int) voices.size() <= maxVoices);
        noteToVoice.fill (-1);
        for (int v = 0; v < (int) voices.size(); ++v)
            for (int n : voices[(size_t) v].midiNotes)
                noteToVoice[(size_t) juce::jlimit (0, 127, n)] = v;

        run = apvts.getRawParameterValue ("run");
        pattern = apvts.getRawParameterValue ("pattern");
        length = apvts.getRawParameterValue ("length");
        swing = apvts.getRawParameterValue ("swing");
        clock = apvts.getRawParameterValue ("clock");
        tempo = apvts.getRawParameterValue ("tempo");
        accent = apvts.getRawParameterValue ("accent");
        volume = apvts.getRawParameterValue ("volume");
    }

    /** Parameters every drum machine shares. */
    static void addCommonParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
    {
        using namespace sparams;
        juce::StringArray patternNames, lengthNames;
        for (int i = 1; i <= numPatterns; ++i) patternNames.add (juce::String (i));
        for (int i = 1; i <= numSteps; ++i)    lengthNames.add (juce::String (i));

        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "run", 1 }, "Run", false));
        layout.add (makeChoice ("pattern", "Pattern", patternNames, 0));
        layout.add (makeChoice ("length", "Length", lengthNames, numSteps - 1));
        layout.add (makeFloat ("swing", "Shuffle", 0.0f, 1.0f, 0.0f, percent()));
        layout.add (makeChoice ("clock", "Clock", { "Host", "Internal" }, 0));
        layout.add (makeFloat ("tempo", "Tempo", 60.0f, 200.0f, 120.0f,
                               Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " BPM"; })));
        layout.add (makeFloat ("accent", "Accent", 0.0f, 1.0f, 0.6f, percent()));
        layout.add (makeFloat ("volume", "Volume", -36.0f, 6.0f, -6.0f, decibels()));
    }

    // ---- What a machine provides ----
    virtual void prepareVoices (double sampleRate) = 0;
    /** Read the voice parameters (called once per block on the audio thread). */
    virtual void updateVoices() = 0;
    /** level: 0..1 (MIDI velocity / sequencer normal = 1), accent: accent amount 0..1 to apply. */
    virtual void triggerVoice (int voice, float level, float accentAmount) = 0;
    /** Add the voices' output into the (cleared) buffers. */
    virtual void renderVoices (float* left, float* right, int numSamples) = 0;

    // ---- juce::AudioProcessor ----
    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        sr = sampleRate;
        scratchRight.assign ((size_t) juce::jmax (samplesPerBlock, 512) * 2, 0.0f);
        events.ensureStorageAllocated (1024);
        prepareVoices (sampleRate);
        gainSmooth.reset (sampleRate, 0.02);
        gainSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volume->load()));
        stopSequencer();
        internalPpq = 0.0;
    }

    void releaseResources() override {}
    void reset() override { stopSequencer(); internalPpq = 0.0; }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        const auto out = layouts.getMainOutputChannelSet();
        return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals noDenormals;
        buffer.clear();
        updateVoices();

        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0)
            return;
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
        if ((int) scratchRight.size() < numSamples)
            scratchRight.resize ((size_t) numSamples); // only if the host exceeds its announced block size
        float* r = right != nullptr ? right : scratchRight.data();

        // Gather events: MIDI notes, sequencer steps and UI auditions, in time order.
        events.clearQuick();
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn())
            {
                const int v = noteToVoice[(size_t) m.getNoteNumber()];
                if (v >= 0)
                {
                    // Velocity 100 = normal hit; 115 and above also gets the machine's accent.
                    const float level = juce::jlimit (0.05f, 1.0f, (float) m.getVelocity() / 100.0f);
                    events.add ({ juce::jlimit (0, numSamples - 1, meta.samplePosition), v, level, m.getVelocity() >= 115 });
                }
            }
        }

        if (const auto audition = auditionRequests.exchange (0))
            for (int v = 0; v < (int) voices.size(); ++v)
                if ((audition >> v) & 1u)
                    events.add ({ 0, v, 1.0f, false });

        scheduleSequencer (numSamples);

        std::sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.sample < b.sample; });

        int position = 0;
        const float accentAmount = accent->load();
        for (auto& e : events)
        {
            if (e.sample > position)
            {
                renderVoices (left + position, r + position, e.sample - position);
                position = e.sample;
            }
            triggerVoice (e.voice, e.level, e.accent ? accentAmount : 0.0f);
        }
        if (position < numSamples)
            renderVoices (left + position, r + position, numSamples - position);

        gainSmooth.setTargetValue (juce::Decibels::decibelsToGain (volume->load()));
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = gainSmooth.getNextValue();
            left[i] *= g;
            r[i] *= g;
            if (right == nullptr)
                left[i] = 0.5f * (left[i] + r[i]);
        }
    }
    using AudioProcessor::processBlock;

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

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        auto state = apvts.copyState();
        state.removeChild (state.getChildWithName ("DRUMPATTERNS"), nullptr);
        juce::ValueTree bank ("DRUMPATTERNS");
        for (int i = 0; i < numPatterns; ++i)
            bank.setProperty ("p" + juce::String (i), patterns.serialise (i), nullptr);
        state.appendChild (bank, nullptr);
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes))
            if (xml->hasTagName (apvts.state.getType()))
            {
                auto state = juce::ValueTree::fromXml (*xml);
                const auto bank = state.getChildWithName ("DRUMPATTERNS");
                for (int i = 0; i < numPatterns; ++i)
                    if (bank.hasProperty ("p" + juce::String (i)))
                        patterns.deserialise (i, bank["p" + juce::String (i)].toString());
                apvts.replaceState (state);
            }
    }

    /** Ask the audio thread to play a voice (from the UI). */
    void audition (int voice) { auditionRequests.fetch_or (1u << voice); }

    float param (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }

    juce::AudioProcessorValueTreeState apvts;
    const std::vector<VoiceInfo> voices;
    PatternBank patterns;
    std::atomic<int> currentStep { -1 }, playingPattern { 0 };

protected:
    double sr = 44100.0;

private:
    struct Event { int sample; int voice; float level; bool accent; };

    void stopSequencer()
    {
        running = false;
        currentStep.store (-1);
    }

    /** Adds this block's step triggers to `events`. Steps are 16ths; shuffle delays the off-beat 16ths. */
    void scheduleSequencer (int numSamples)
    {
        const bool on = run->load() > 0.5f;
        if (on != wasOn)
        {
            wasOn = on;
            stopSequencer();
            internalPpq = 0.0;
        }
        if (! on)
            return;

        double ppq = internalPpq, bpm = tempo->load();
        bool playing = true, hostClock = false;
        if (auto* host = clock->load() < 0.5f ? getPlayHead() : nullptr)
            if (const auto pos = host->getPosition())
                if (pos->getPpqPosition().hasValue() && pos->getBpm().hasValue())
                {
                    hostClock = true;
                    ppq = *pos->getPpqPosition();
                    bpm = *pos->getBpm();
                    playing = pos->getIsPlaying();
                }

        if (! playing || bpm <= 0.0)
        {
            stopSequencer();
            return;
        }

        const double samplesPerQuarter = sr * 60.0 / bpm;
        const double blockQuarters = numSamples / samplesPerQuarter;
        const double shuffle = swing->load() * (1.0 / 3.0) * 0.25; // up to a triplet feel, in quarters

        auto stepTime = [shuffle] (int64_t s) { return (double) s * 0.25 + ((s & 1) != 0 ? shuffle : 0.0); };

        if (! running || std::abs (ppq - expectedPpq) > 1.0e-3)
        {
            if (! running)
                activePattern = (int) pattern->load();
            running = true;
            // First step whose time is at or after the current position.
            int64_t s = (int64_t) std::floor (ppq * 4.0) - 1;
            while (stepTime (s) < ppq - 1.0e-6)
                ++s;
            lastStep = s - 1;
        }

        for (int64_t s = lastStep + 1;; ++s)
        {
            const double offset = (stepTime (s) - ppq) * samplesPerQuarter;
            if (offset >= numSamples)
                break;
            const int at = juce::jlimit (0, numSamples - 1, (int) std::floor (offset));
            lastStep = s;

            const int len = (int) length->load() + 1;
            const int step = (int) (((s % len) + len) % len);
            if (step == 0)
                activePattern = (int) pattern->load(); // pattern changes land on the downbeat
            playingPattern.store (activePattern);
            currentStep.store (step);

            const bool accented = patterns.get (activePattern, accentRow, step);
            for (int v = 0; v < (int) voices.size(); ++v)
                if (patterns.get (activePattern, v, step))
                    events.add ({ at, v, 1.0f, accented });
        }

        expectedPpq = ppq + blockQuarters;
        if (! hostClock)
            internalPpq += blockQuarters;
    }

    std::atomic<float>* run = nullptr;
    std::atomic<float>* pattern = nullptr;
    std::atomic<float>* length = nullptr;
    std::atomic<float>* swing = nullptr;
    std::atomic<float>* clock = nullptr;
    std::atomic<float>* tempo = nullptr;
    std::atomic<float>* accent = nullptr;
    std::atomic<float>* volume = nullptr;

    std::array<int, 128> noteToVoice {};
    std::atomic<uint32_t> auditionRequests { 0 };
    juce::Array<Event> events;
    std::vector<float> scratchRight;
    juce::SmoothedValue<float> gainSmooth;

    bool wasOn = false, running = false;
    int activePattern = 0;
    int64_t lastStep = 0;
    double expectedPpq = 0.0, internalPpq = 0.0;
};
} // namespace drum
