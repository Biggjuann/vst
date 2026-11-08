#include "drums/DrumGrid.h"
#include "drums/DrumMachine.h"
#include "ui/KnobPanelEditor.h"
#include "ui/ParamHelpers.h"

#include "Kit909.h"

using namespace kit909;

namespace
{
// id, name, short, MIDI notes (Ableton Drum Rack / GM layout)
std::vector<drum::VoiceInfo> voiceList()
{
    return {
        { "bd", "Bass Drum",    "BD", { 36, 35 } },
        { "sd", "Snare Drum",   "SD", { 38, 40 } },
        { "lt", "Low Tom",      "LT", { 41, 43 } },
        { "mt", "Mid Tom",      "MT", { 45, 47 } },
        { "ht", "Hi Tom",       "HT", { 48, 50 } },
        { "rs", "Rim Shot",     "RS", { 37 } },
        { "cp", "Hand Clap",    "CP", { 39 } },
        { "ch", "Closed Hihat", "CH", { 42, 44 } },
        { "oh", "Open Hihat",   "OH", { 46 } },
        { "cr", "Crash",        "CR", { 49, 57 } },
        { "rd", "Ride",         "RD", { 51, 59 } },
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace sparams;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    drum::DrumMachineProcessor::addCommonParameters (layout);

    for (auto& v : voiceList())
        layout.add (makeFloat (v.id + "_level", v.shortName + " Level", 0.0f, 1.0f, 0.75f, percent()));

    layout.add (makeFloat ("bd_tune",   "BD Tune",   0.0f, 1.0f, 0.5f, percent()));
    layout.add (makeFloat ("bd_attack", "BD Attack", 0.0f, 1.0f, 0.5f, percent()));
    layout.add (makeFloat ("bd_decay",  "BD Decay",  0.0f, 1.0f, 0.45f, percent()));
    layout.add (makeFloat ("sd_tune",   "SD Tune",   0.0f, 1.0f, 0.5f, percent()));
    layout.add (makeFloat ("sd_tone",   "SD Tone",   0.0f, 1.0f, 0.5f, percent()));
    layout.add (makeFloat ("sd_snappy", "SD Snappy", 0.0f, 1.0f, 0.55f, percent()));
    for (auto id : { "lt", "mt", "ht" })
    {
        layout.add (makeFloat (juce::String (id) + "_tune",  juce::String (id).toUpperCase() + " Tune",  0.0f, 1.0f, 0.5f, percent()));
        layout.add (makeFloat (juce::String (id) + "_decay", juce::String (id).toUpperCase() + " Decay", 0.0f, 1.0f, 0.5f, percent()));
    }
    layout.add (makeFloat ("ch_decay", "CH Decay", 0.0f, 1.0f, 0.3f, percent()));
    layout.add (makeFloat ("oh_decay", "OH Decay", 0.0f, 1.0f, 0.4f, percent()));
    layout.add (makeFloat ("cr_tune",  "Crash Tune", 0.0f, 1.0f, 0.5f, percent()));
    layout.add (makeFloat ("rd_tune",  "Ride Tune",  0.0f, 1.0f, 0.5f, percent()));
    return layout;
}

void loadDemoPatterns (drum::PatternBank& bank)
{
    auto set = [&bank] (int p, std::initializer_list<std::pair<int, const char*>> rows)
    {
        for (auto& [row, text] : rows)
            bank.setRowFromString (p, row, text);
    };
    const int A = drum::accentRow;

    // 1: House
    set (0, { { BD, "x...x...x...x..." }, { CP, "....x.......x..." }, { CH, "x...x...x...x..." },
              { OH, "..x...x...x...x." }, { A, "x...x...x...x..." } });
    // 2: Techno with ride
    set (1, { { BD, "x...x...x...x..." }, { RD, "..x...x...x...x." }, { CH, "xx.xxx.xxx.xxx.x" },
              { RS, "...x.....x...x.." }, { CP, "....x.......x..." }, { A, "x...x...x...x..x" } });
    // 3: Acid / Chicago
    set (2, { { BD, "x...x...x...x..." }, { SD, "....x.......x..x" }, { CH, "xxxxxxxxxxxxxxxx" },
              { OH, "..x...x...x...x." }, { CP, "....x.......x..." }, { A, "..x...x...x...x." } });
    // 4: Breakbeat
    set (3, { { BD, "x.....x...x....." }, { SD, "....x..x.x..x..." }, { CH, "x.x.x.x.x.x.x.x." },
              { OH, "..............x." }, { A, "x...x.......x..." } });
    // 5: Tom fill
    set (4, { { BD, "x...x...x...x..." }, { HT, "x.x............." }, { MT, "....x.x........." },
              { LT, "........x.x.x..." }, { SD, "..............xx" }, { CR, "x..............." },
              { A, "x...x...x...x.xx" } });
    // 6: Garage swing (try Shuffle ~50%)
    set (5, { { BD, "x.....x...x....." }, { SD, "....x.......x..." }, { RS, "..x....x..x...x." },
              { CH, "x.xxx.xxx.xxx.xx" }, { OH, "......x.......x." }, { A, "x.......x......." } });
}
} // namespace

class NonaProcessor final : public drum::DrumMachineProcessor
{
public:
    NonaProcessor() : DrumMachineProcessor (voiceList(), createLayout()) { loadDemoPatterns (patterns); }

    void prepareVoices (double sampleRate) override { kit.prepare (sampleRate); updateVoices(); }

    void updateVoices() override
    {
        Params p;
        for (int v = 0; v < NumVoices; ++v)
            p.level[v] = param (voices[(size_t) v].id + "_level");
        p.bdTune = param ("bd_tune");
        p.bdAttack = param ("bd_attack");
        p.bdDecay = param ("bd_decay");
        p.sdTune = param ("sd_tune");
        p.sdTone = param ("sd_tone");
        p.sdSnappy = param ("sd_snappy");
        const char* toms[] = { "lt", "mt", "ht" };
        for (int i = 0; i < 3; ++i)
        {
            p.tomTune[i] = param (juce::String (toms[i]) + "_tune");
            p.tomDecay[i] = param (juce::String (toms[i]) + "_decay");
        }
        p.chDecay = param ("ch_decay");
        p.ohDecay = param ("oh_decay");
        p.crTune = param ("cr_tune");
        p.rdTune = param ("rd_tune");
        kit.setParams (p);
    }

    void triggerVoice (int voice, float level, float accentAmount) override { kit.trigger (voice, level, accentAmount); }

    void renderVoices (float* left, float* right, int numSamples) override
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = kit.render();
            left[i] += s;
            right[i] += s;
        }
    }

    juce::AudioProcessorEditor* createEditor() override
    {
        using namespace sui;
        std::vector<Row> rows {
            { { "Bass Drum",  { "bd_level", "bd_tune", "bd_attack", "bd_decay" } },
              { "Snare Drum", { "sd_level", "sd_tune", "sd_tone", "sd_snappy" } },
              { "Rim / Clap", { "rs_level", "cp_level" } } },
            { { "Low Tom",    { "lt_level", "lt_tune", "lt_decay" } },
              { "Mid Tom",    { "mt_level", "mt_tune", "mt_decay" } },
              { "Hi Tom",     { "ht_level", "ht_tune", "ht_decay" } } },
            { { "Hi-Hat",     { "ch_level", "ch_decay", "oh_level", "oh_decay" } },
              { "Cymbal",     { "cr_level", "cr_tune", "rd_level", "rd_tune" } },
              { "Master",     { "accent", "swing", "volume" } } },
        };

        const juce::Colour colour (0xffe8e2d0);
        auto sequencer = std::make_unique<drum::SequencerSection> (*this, colour);
        EditorExtras extras;
        extras.footerHeight = sequencer->preferredHeight();
        extras.footerMinWidth = drum::SequencerSection::minimumWidth;
        extras.footer = std::move (sequencer);
        return new KnobPanelEditor (*this, apvts, "NONA  /  hybrid rhythm composer", std::move (rows), colour, std::move (extras));
    }

private:
    Kit kit;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NonaProcessor();
}
