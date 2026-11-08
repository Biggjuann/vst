// Converts Diva .h2p presets with Mira's importer and prints one JSON object per line:
//   DivaConvert <file-or-folder>...
//   DivaConvert --factory        (prints Mira's factory presets instead)
// Used to check the importer against large preset collections.
#include <juce_core/juce_core.h>

#include "DivaImporter.h"
#include "MiraPresets.h"

namespace
{
/** Minimal processor so MiraPresetManager can be exercised outside a plugin. */
struct DummyProcessor : juce::AudioProcessor
{
    DummyProcessor() : state (*this, nullptr, "PARAMS", { std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "cutoff", 1 }, "Cutoff", 20.0f, 20000.0f, 1000.0f) }) {}
    const juce::String getName() const override { return "Dummy"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    using juce::AudioProcessor::processBlock;
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
    juce::AudioProcessorValueTreeState state;
};
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    int ok = 0, failed = 0;

    if (argc > 2 && juce::String (argv[1]) == "--import")
    {
        // Runs the same import the plugin's "Import Diva" button does.
        DummyProcessor proc;
        MiraPresetManager manager (proc.state);
        juce::Array<juce::File> items;
        for (int i = 2; i < argc; ++i)
            items.add (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i]));
        const auto result = manager.importDiva (items);
        std::cout << "imported " << result.imported << ", failed " << result.failed << "\n"
                  << "folder " << MiraPresetManager::getDivaImportFolder().getFullPathName() << "\n"
                  << "browser entries " << manager.scanEntries().size() << "\n";
        const bool loaded = manager.loadFile (result.first);
        std::cout << "load first: " << (loaded ? "ok" : "FAILED") << " -> " << manager.getCurrentName()
                  << " cutoff=" << proc.state.getRawParameterValue ("cutoff")->load() << "\n";
        return result.imported > 0 && loaded ? 0 : 1;
    }
    if (argc > 1 && juce::String (argv[1]) == "--factory")
    {
        for (auto& p : getMiraFactoryPresets())
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("name", p.name);
            obj->setProperty ("category", p.category);
            auto* values = new juce::DynamicObject();
            for (auto& [id, v] : p.values)
                values->setProperty (id, v);
            obj->setProperty ("values", juce::var (values));
            std::cout << juce::JSON::toString (juce::var (obj), true) << "\n";
        }
        return 0;
    }

    for (int i = 1; i < argc; ++i)
    {
        const juce::File item (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i]));
        auto files = item.isDirectory() ? item.findChildFiles (juce::File::findFiles, true, "*.h2p")
                                        : juce::Array<juce::File> { item };
        for (auto& f : files)
        {
            const auto c = diva::convertFile (f);
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("file", f.getFullPathName());
            obj->setProperty ("ok", c.ok);
            obj->setProperty ("name", c.name);
            obj->setProperty ("category", c.category);
            obj->setProperty ("error", c.error);
            auto* values = new juce::DynamicObject();
            for (auto& [id, v] : c.values)
                values->setProperty (id, v);
            obj->setProperty ("values", juce::var (values));
            std::cout << juce::JSON::toString (juce::var (obj), true) << "\n";
            (c.ok ? ok : failed)++;
        }
    }
    std::cerr << "converted " << ok << ", failed " << failed << "\n";
    return failed > 0 ? 1 : 0;
}
