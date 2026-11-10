// End-to-end checks for Squelch's MIDI out, MIDI clip export and favorites.
#include <juce_audio_utils/juce_audio_utils.h>

#include "FavoritesBar.h"
#include "PluginProcessor.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::cout << "FAIL: " << msg << "\n"; } else std::cout << "ok:   " << msg << "\n"; } while (0)

constexpr double sr = 48000.0;
constexpr int block = 512;

static void setParam (SquelchAudioProcessor& p, const juce::String& id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

struct NoteEvent { int note; double start, end; int velocity; };

/** Pair note-ons/offs (times in 16th-note steps). */
static std::vector<NoteEvent> pairNotes (const std::vector<std::tuple<double, int, int>>& events) // (time, note, vel; vel 0 = off)
{
    std::vector<NoteEvent> notes;
    std::map<int, size_t> open;
    for (auto& [t, note, vel] : events)
    {
        if (vel > 0) { open[note] = notes.size(); notes.push_back ({ note, t, -1.0, vel }); }
        else if (open.count (note)) { notes[open[note]].end = t; open.erase (note); }
    }
    return notes;
}

static std::vector<NoteEvent> runLive (SquelchAudioProcessor& p, int bars, juce::AudioBuffer<float>* audioOut)
{
    const double samplesPerStep = sr * 60.0 / 120.0 / 4.0; // 120 BPM
    const int total = (int) (bars * 16 * samplesPerStep);
    std::vector<std::tuple<double, int, int>> events;
    juce::AudioBuffer<float> buffer (2, block);
    if (audioOut) audioOut->setSize (2, total + block);
    for (int pos = 0; pos < total + block; pos += block)
    {
        juce::MidiBuffer midi;
        buffer.clear();
        p.processBlock (buffer, midi);
        if (audioOut) audioOut->copyFrom (0, pos, buffer, 0, 0, block);
        for (const auto m : midi)
        {
            const auto msg = m.getMessage();
            const double step = (pos + m.samplePosition) / samplesPerStep;
            if ((msg.isNoteOn() && step < bars * 16 - 0.01) || (msg.isNoteOff() && step <= bars * 16 + 1e-6))
                events.emplace_back (step, msg.getNoteNumber(), msg.isNoteOn() ? msg.getVelocity() : 0);
        }
    }
    return pairNotes (events);
}

static std::vector<NoteEvent> clipNotes (const juce::MidiFile& file)
{
    std::vector<std::tuple<double, int, int>> events;
    const auto* track = file.getTrack (0);
    for (const auto* e : *track)
        if (e->message.isNoteOn() || e->message.isNoteOff())
            events.emplace_back (e->message.getTimeStamp() / 240.0, e->message.getNoteNumber(), e->message.isNoteOn() ? e->message.getVelocity() : 0);
    return pairNotes (events);
}

static std::unique_ptr<SquelchAudioProcessor> makeProcessor()
{
    auto p = std::make_unique<SquelchAudioProcessor>();
    p->setPlayConfigDetails (0, 2, sr, block);
    p->prepareToPlay (sr, block);
    setParam (*p, "clock", 1.0f);  // Internal
    setParam (*p, "tempo", 120.0f);
    return p;
}

static bool compare (const std::vector<NoteEvent>& live, const std::vector<NoteEvent>& clip, const char* what)
{
    bool same = live.size() == clip.size();
    for (size_t i = 0; same && i < live.size(); ++i)
        same = live[i].note == clip[i].note && std::abs (live[i].start - clip[i].start) < 0.01
            && std::abs (live[i].end - clip[i].end) < 0.06 && live[i].velocity == clip[i].velocity;
    if (! same)
    {
        std::cout << "  " << what << " live/clip differ:\n";
        for (size_t i = 0; i < std::max (live.size(), clip.size()); ++i)
        {
            auto show = [] (const std::vector<NoteEvent>& v, size_t k) { return k < v.size() ? juce::String (v[k].note) + "@" + juce::String (v[k].start, 2) + "-" + juce::String (v[k].end, 2) + " v" + juce::String (v[k].velocity) : juce::String ("-"); };
            std::cout << "    " << show (live, i) << "   |   " << show (clip, i) << "\n";
        }
    }
    return same;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    // 1. Live MIDI out == exported clip (demo pattern 1: slides, ties, rests, accents, octaves)
    {
        auto p = makeProcessor();
        const auto clip = clipNotes (p->createMidiClip (4));
        setParam (*p, "seqon", 1.0f);
        juce::AudioBuffer<float> liveAudio;
        const auto live = runLive (*p, 4, &liveAudio);
        CHECK (! live.empty(), "sequencer sends MIDI out (" + juce::String ((int) live.size()) + " notes in 4 bars)");
        CHECK (compare (live, clip, "pattern 1"), "Drag MIDI clip matches live MIDI out note-for-note");

        int slides = 0, accents = 0;
        for (size_t i = 0; i + 1 < live.size(); ++i) slides += live[i].end > live[i + 1].start; // overlapping = slide
        for (auto& n : live) accents += n.velocity == 127;
        CHECK (slides > 0 && accents > 0, "slides recorded as overlapping notes (" + juce::String (slides) + "), accents as velocity 127 (" + juce::String (accents) + ")");

        // 2. Replaying the clip through a fresh Squelch (sequencer off) sounds like the sequencer did.
        auto replay = makeProcessor();
        juce::MidiFile file = p->createMidiClip (4); // pattern unchanged (no Evolve)
        juce::MidiMessageSequence seq (*file.getTrack (0));
        const double samplesPerTick = sr * 60.0 / 120.0 / 960.0;
        juce::AudioBuffer<float> replayAudio (2, liveAudio.getNumSamples()), buf (2, block);
        int next = 0;
        for (int pos = 0; pos + block <= replayAudio.getNumSamples(); pos += block)
        {
            juce::MidiBuffer midi;
            while (next < seq.getNumEvents() && seq.getEventPointer (next)->message.getTimeStamp() * samplesPerTick < pos + block)
            {
                const auto& m = seq.getEventPointer (next)->message;
                if (m.isNoteOnOrOff())
                    midi.addEvent (m, juce::jlimit (0, block - 1, (int) (m.getTimeStamp() * samplesPerTick) - pos));
                ++next;
            }
            buf.clear();
            replay->processBlock (buf, midi);
            replayAudio.copyFrom (0, pos, buf, 0, 0, block);
        }
        double dot = 0, a2 = 0, b2 = 0;
        for (int i = 0; i < replayAudio.getNumSamples(); ++i)
        {
            const double a = liveAudio.getSample (0, i), b = replayAudio.getSample (0, i);
            dot += a * b; a2 += a * a; b2 += b * b;
        }
        const double corr = dot / std::sqrt (a2 * b2 + 1e-12);
        CHECK (corr > 0.97, "clip replayed through Squelch matches the sequencer's audio (correlation " + juce::String (corr, 4) + ")");
    }

    // 3. Export predicts Evolve: 8 bars, evolve every bar
    {
        auto p = makeProcessor();
        setParam (*p, "genevolve", 1.0f);   // "1 bar"
        setParam (*p, "genvariation", 0.5f);
        p->pollGenerator();
        const auto clip = clipNotes (p->createMidiClip (8));
        setParam (*p, "seqon", 1.0f);
        const auto live = runLive (*p, 8, nullptr);
        CHECK (compare (live, clip, "evolve"), "clip export includes the same Evolve changes playback makes (8 bars)");
    }

    // 4. Favorites round-trip
    {
        auto p = makeProcessor();
        p->pollGenerator(); // like the editor's timer: the generator is live from here on
        setParam (*p, "cutoff", 1234.0f);
        setParam (*p, "seqon", 1.0f);
        auto original = p->getPatternSteps (0);
        CHECK (favorites::saveSound (*p, "Test Sound"), "save favorite sound");
        CHECK (favorites::savePattern (*p, "Test Pattern"), "save favorite pattern");

        setParam (*p, "cutoff", 300.0f);
        setParam (*p, "genseed", 777.0f);   // regenerates pattern 1
        p->pollGenerator();
        setParam (*p, "seqon", 0.0f);
        CHECK (p->getPatternSteps (0)[1].pack() != original[1].pack() || p->getPatternSteps (0)[2].pack() != original[2].pack(), "pattern changed before reload");

        CHECK (favorites::loadSound (*p, favorites::soundsFolder().getChildFile ("Test Sound.squelch")), "load favorite sound");
        p->pollGenerator(); // the generator must not overwrite the loaded patterns
        bool same = true;
        for (int i = 0; i < 16; ++i) same &= p->getPatternSteps (0)[(size_t) i].pack() == original[(size_t) i].pack();
        CHECK (same, "patterns restored exactly (and not regenerated)");
        CHECK (std::abs (p->apvts.getRawParameterValue ("cutoff")->load() - 1234.0f) < 1.0f, "knobs restored");
        CHECK (p->apvts.getRawParameterValue ("seqon")->load() < 0.5f, "loading a favorite leaves RUN alone");

        p->patterns.setPattern (0, seq::randomPattern (5));
        CHECK (favorites::loadPattern (*p, favorites::patternsFolder().getChildFile ("Test Pattern.squelchpattern")), "load favorite pattern");
        same = true;
        for (int i = 0; i < 16; ++i) same &= p->getPatternSteps (0)[(size_t) i].pack() == original[(size_t) i].pack();
        CHECK (same, "pattern favorite restored into the selected pattern");
    }

    std::cout << (failures == 0 ? "\nALL PASSED\n" : "\nFAILURES: ") << (failures ? juce::String (failures) : juce::String()) << "\n";
    return failures == 0 ? 0 : 1;
}
