// Checks the Squelch pattern generator: notes stay in key, seeds are repeatable,
// and each control only changes its own aspect of the pattern.
#include <juce_core/juce_core.h>

#include "Generator.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::cout << "FAIL: " << msg << "\n"; } } while (0)

static int pitchOf (const seq::Step& s) { return s.key + 12 * s.octave; } // semitones above C2

int main()
{
    int patternsChecked = 0, notes = 0, accents = 0, slides = 0, rests = 0, ties = 0;
    for (int scale = 0; scale < gen::numScales; ++scale)
        for (int key = 0; key < 12; ++key)
            for (int seed = 1; seed <= 200; ++seed)
            {
                gen::Settings s;
                s.key = key; s.scale = scale; s.seed = seed;
                const auto steps = gen::generate (s);
                ++patternsChecked;
                CHECK (steps[0].time == seq::Note && steps[0].key % 12 == key % 12, "step 1 should be the root");
                for (auto& st : steps)
                {
                    CHECK (st.key >= 0 && st.key <= 12 && st.octave >= -1 && st.octave <= 1, "step out of range");
                    if (st.time == seq::Note)
                    {
                        ++notes; accents += st.accent; slides += st.slide;
                        const int degree = ((pitchOf (st) - key) % 12 + 12) % 12;
                        bool inScale = false;
                        for (int i = 0; i < gen::scales[scale].size; ++i)
                            inScale |= gen::scales[scale].notes[i] == degree;
                        CHECK (inScale, "note outside scale: scale " << scale << " key " << key << " seed " << seed);
                    }
                    else (st.time == seq::Rest ? rests : ties)++;
                }
            }

    // Repeatable: same settings -> same pattern.
    gen::Settings a; a.seed = 42;
    CHECK (gen::generate (a)[5].pack() == gen::generate (a)[5].pack(), "not deterministic");

    // Different seeds give different patterns (count unique among 1000).
    std::set<juce::String> unique;
    for (int seed = 1; seed <= 1000; ++seed)
    {
        gen::Settings s; s.seed = seed;
        juce::String sig;
        for (auto& st : gen::generate (s)) sig << (int) st.pack() << ",";
        unique.insert (sig);
    }
    CHECK (unique.size() > 990, "too many duplicate patterns: " << unique.size());

    // Changing Accent only changes accents; changing Key only transposes.
    gen::Settings b; b.seed = 7; b.accent = 0.1f;
    gen::Settings c = b; c.accent = 0.9f;
    gen::Settings d = b; d.key = 5;
    const auto pb = gen::generate (b), pc = gen::generate (c), pd = gen::generate (d);
    for (int i = 0; i < seq::numSteps; ++i)
    {
        CHECK (pb[(size_t) i].time == pc[(size_t) i].time && pb[(size_t) i].key == pc[(size_t) i].key
               && pb[(size_t) i].slide == pc[(size_t) i].slide, "accent knob changed more than accents");
        CHECK (pb[(size_t) i].time == pd[(size_t) i].time, "key change altered rhythm");
    }

    // Density changes the number of notes.
    gen::Settings lo; lo.seed = 3; lo.density = 0.2f;
    gen::Settings hi = lo; hi.density = 1.0f;
    auto count = [] (const std::array<seq::Step, seq::numSteps>& st) { int n = 0; for (auto& x : st) n += x.time == seq::Note; return n; };
    CHECK (count (gen::generate (lo)) < count (gen::generate (hi)), "density has no effect");

    // Mutate with variation 0 keeps the pattern, 1 replaces it.
    const auto m0 = gen::mutate (pb, pd, 0.0f, 1), m1 = gen::mutate (pb, pd, 1.0f, 1);
    for (int i = 0; i < seq::numSteps; ++i)
    {
        CHECK (m0[(size_t) i].pack() == pb[(size_t) i].pack(), "variation 0 changed the pattern");
        CHECK (m1[(size_t) i].pack() == pd[(size_t) i].pack(), "variation 1 kept old steps");
    }

    std::cout << patternsChecked << " patterns checked (" << notes << " notes: " << (100 * accents / notes) << "% accented, "
              << (100 * slides / notes) << "% slides; " << rests << " rests, " << ties << " ties), "
              << unique.size() << "/1000 unique seeds\n";
    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: ") << (failures ? juce::String (failures) : juce::String()) << "\n";
    return failures == 0 ? 0 : 1;
}
