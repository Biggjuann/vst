#include "drums/DrumGrid.h"
#include "drums/DrumMachine.h"
#include "ui/KnobPanelEditor.h"
#include "ui/ParamHelpers.h"

#include "Kit808.h"

using namespace kit808;

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
        { "lc", "Low Conga",    "LC", { 64 } },
        { "mc", "Mid Conga",    "MC", { 63 } },
        { "hc", "Hi Conga",     "HC", { 62 } },
        { "rs", "Rim Shot",     "RS", { 37 } },
        { "cl", "Claves",       "CL", { 75 } },
        { "cp", "Hand Clap",    "CP", { 39 } },
        { "ma", "Maracas",      "MA", { 70 } },
        { "cb", "Cowbell",      "CB", { 56 } },
        { "cy", "Cymbal",       "CY", { 49, 57 } },
        { "oh", "Open Hihat",   "OH", { 46 } },
        { "ch", "Closed Hihat", "CH", { 42, 44 } },
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace sparams;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    drum::DrumMachineProcessor::addCommonParameters (layout);

    for (auto& v : voiceList())
        layout.add (makeFloat (v.id + "_level", v.shortName + " Level", 0.0f, 1.0f, 0.75f, percent()));

    layout.add (makeFloat ("bd_tone",   "BD Tone",   0.0f, 1.0f, 0.4f, percent()));
    layout.add (makeFloat ("bd_decay",  "BD Decay",  0.0f, 1.0f, 0.5f, percent()));
    layout.add (makeFloat ("sd_tone",   "SD Tone",   0.0f, 1.0f, 0.5f, percent()));
    layout.add (makeFloat ("sd_snappy", "SD Snappy", 0.0f, 1.0f, 0.5f, percent()));
    for (auto id : { "lt", "mt", "ht", "lc", "mc", "hc" })
        layout.add (makeFloat (juce::String (id) + "_tune", juce::String (id).toUpperCase() + " Tuning", 0.0f, 1.0f, 0.5f, percent()));
    layout.add (makeFloat ("cy_tone",   "CY Tone",   0.0f, 1.0f, 0.5f, percent()));
    layout.add (makeFloat ("cy_decay",  "CY Decay",  0.0f, 1.0f, 0.5f, percent()));
    layout.add (makeFloat ("oh_decay",  "OH Decay",  0.0f, 1.0f, 0.4f, percent()));
    return layout;
}

void loadDemoPatterns (drum::PatternBank& bank)
{
    struct Demo { const char* rows[NumVoices + 1]; };
    // Rows in voice order: BD SD LT MT HT LC MC HC RS CL CP MA CB CY OH CH, then ACCENT. nullptr = empty.
    auto set = [&bank] (int p, std::initializer_list<std::pair<int, const char*>> rows)
    {
        for (auto& [row, text] : rows)
            bank.setRowFromString (p, row, text);
    };
    const int A = drum::accentRow;

    // 1: Electro / old-school hip hop
    set (0, { { BD, "x.........x..x.." }, { SD, "....x.......x..." }, { CP, "....x.......x..." },
              { CH, "x.x.x.x.x.x.x.x." }, { OH, "..............x." }, { A, "x.......x......." } });
    // 2: Four-on-the-floor
    set (1, { { BD, "x...x...x...x..." }, { CP, "....x.......x..." }, { CH, "x...x...x...x..." },
              { OH, "..x...x...x...x." }, { A, "x...x...x...x..." } });
    // 3: Trap-style hats and a long kick
    set (2, { { BD, "x......x..x....." }, { SD, "........x......." }, { CP, "........x......." },
              { CH, "xxxxxxxxxxx.xxxx" }, { OH, "...........x...." }, { A, "x...x...x...x..." } });
    // 4: Latin percussion groove
    set (3, { { BD, "x..x..x...x..x.." }, { RS, "..x..x....x..x.." }, { LC, "x.....x.....x..." },
              { MC, "...x.....x...x.." }, { HC, ".x..x..x.x..x..x" }, { CB, "x.x..x.x.x.x..x." },
              { MA, "xxxxxxxxxxxxxxxx" }, { CL, "x..x..x...x.x..." }, { A, "..x........x...." } });
    // 5: Slow jam / ballad
    set (4, { { BD, "x......xx......." }, { SD, "....x.......x..." }, { RS, "..........x....." },
              { CH, "x.x.x.x.x.x.x.x." }, { CY, "x..............." }, { A, "....x.......x..." } });
    // 6: Breakbeat with toms
    set (5, { { BD, "x.....x...x....." }, { SD, "....x..x.x..x..." }, { CH, "x.x.x.x.x.x.x.x." },
              { LT, "..............x." }, { MT, ".............x.." }, { HT, "............x..." } });
}
} // namespace

class OctoProcessor final : public drum::DrumMachineProcessor
{
public:
    OctoProcessor() : DrumMachineProcessor (voiceList(), createLayout()) { loadDemoPatterns (patterns); }

    void prepareVoices (double sampleRate) override { kit.prepare (sampleRate); updateVoices(); }

    void updateVoices() override
    {
        Params p;
        for (int v = 0; v < NumVoices; ++v)
            p.level[v] = param (voices[(size_t) v].id + "_level");
        p.bdTone = param ("bd_tone");
        p.bdDecay = param ("bd_decay");
        p.sdTone = param ("sd_tone");
        p.sdSnappy = param ("sd_snappy");
        for (auto v : { LT, MT, HT, LC, MC, HC })
            p.tune[v] = param (voices[(size_t) v].id + "_tune");
        p.cyTone = param ("cy_tone");
        p.cyDecay = param ("cy_decay");
        p.ohDecay = param ("oh_decay");
        kit.setParams (p);
    }

    void triggerVoice (int voice, float level, float accentAmount) override { kit.trigger (voice, level, accentAmount); }

    void renderVoices (float* left, float* right, int numSamples) override
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = kit.render(); // the original is a mono machine
            left[i] += s;
            right[i] += s;
        }
    }

    juce::AudioProcessorEditor* createEditor() override
    {
        using namespace sui;
        std::vector<Row> rows {
            { { "Bass Drum",  { "bd_level", "bd_tone", "bd_decay" } },
              { "Snare Drum", { "sd_level", "sd_tone", "sd_snappy" } },
              { "Low Tom",    { "lt_level", "lt_tune" } },
              { "Mid Tom",    { "mt_level", "mt_tune" } },
              { "Hi Tom",     { "ht_level", "ht_tune" } } },
            { { "Low Conga",  { "lc_level", "lc_tune" } },
              { "Mid Conga",  { "mc_level", "mc_tune" } },
              { "Hi Conga",   { "hc_level", "hc_tune" } },
              { "Rim / Claves", { "rs_level", "cl_level" } },
              { "Clap / Maracas", { "cp_level", "ma_level" } },
              { "Cowbell",    { "cb_level" } } },
            { { "Cymbal",     { "cy_level", "cy_tone", "cy_decay" } },
              { "Open Hihat", { "oh_level", "oh_decay" } },
              { "Closed Hihat", { "ch_level" } },
              { "Master",     { "accent", "swing", "volume" } } },
        };

        const juce::Colour colour (0xffff5a36);
        auto sequencer = std::make_unique<drum::SequencerSection> (*this, colour);
        EditorExtras extras;
        extras.footerHeight = sequencer->preferredHeight();
        extras.footerMinWidth = drum::SequencerSection::minimumWidth;
        extras.footer = std::move (sequencer);
        return new KnobPanelEditor (*this, apvts, "OCTO  /  analog rhythm composer", std::move (rows), colour, std::move (extras));
    }

private:
    Kit kit;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OctoProcessor();
}
