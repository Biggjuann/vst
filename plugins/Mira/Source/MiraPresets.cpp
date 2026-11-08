#include "MiraPresets.h"

// Choice indices, for readability below.
namespace
{
constexpr float SAW = 0, PULSE = 1, TRI = 2, SINE = 3;                 // osc shapes
constexpr float LP24 = 0, LP12 = 1, BP12 = 2, HP24 = 3;                 // filter modes
constexpr float L_SINE = 0, L_TRI = 1, L_SAW = 2, L_SQR = 3, L_SH = 4;  // LFO shapes
constexpr float POLY = 0, MONO = 1, LEGATO = 2;                         // voice modes
constexpr float CH_OFF = 0, CH_I = 1, CH_II = 2, CH_I_II = 3;           // chorus modes
} // namespace

const std::vector<MiraFactoryPreset>& getMiraFactoryPresets()
{
    static const std::vector<MiraFactoryPreset> presets {
        { "Init", "Init", {} },

        // ---------------------------------------------------------------- Bass
        { "Fat Ladder Bass", "Bass", {
            { "osc1shape", SAW }, { "osc1oct", -1 }, { "osc2shape", SAW }, { "osc2oct", -1 }, { "osc2fine", 6 },
            { "osc1lvl", 0.8f }, { "osc2lvl", 0.7f }, { "sublvl", 0.5f },
            { "cutoff", 180 }, { "res", 0.25f }, { "drive", 0.45f }, { "keytrack", 0.4f }, { "envamt", 0.45f },
            { "fatt", 0.001f }, { "fdec", 0.35f }, { "fsus", 0.15f }, { "frel", 0.2f },
            { "aatt", 0.001f }, { "adec", 0.5f }, { "asus", 0.9f }, { "arel", 0.08f },
            { "voicemode", MONO }, { "glide", 0.03f }, { "drift", 0.25f }, { "velsens", 0.4f }, { "spread", 0 } } },

        { "Sub Pulse Bass", "Bass", {
            { "osc1shape", PULSE }, { "osc1oct", -1 }, { "pw", 0.3f }, { "osc2lvl", 0 }, { "sublvl", 0.8f },
            { "cutoff", 400 }, { "res", 0.1f }, { "envamt", 0.3f }, { "keytrack", 0.3f },
            { "fdec", 0.25f }, { "fsus", 0.2f },
            { "adec", 0.4f }, { "asus", 1 }, { "arel", 0.05f },
            { "voicemode", LEGATO }, { "spread", 0 }, { "drift", 0.15f } } },

        { "Acid Res Bass", "Bass", {
            { "osc1shape", SAW }, { "osc1oct", -1 }, { "osc2shape", PULSE }, { "osc2oct", -1 }, { "osc2fine", -8 },
            { "osc2lvl", 0.5f },
            { "cutoff", 120 }, { "res", 0.72f }, { "drive", 0.3f }, { "envamt", 0.55f }, { "keytrack", 0.3f },
            { "fdec", 0.22f }, { "fsus", 0 }, { "frel", 0.15f },
            { "asus", 1 }, { "arel", 0.06f },
            { "voicemode", MONO }, { "glide", 0.05f }, { "spread", 0 } } },

        { "Rubber Bass", "Bass", {
            { "osc1shape", TRI }, { "osc1oct", -1 }, { "osc2shape", SAW }, { "osc2oct", -2 }, { "osc2lvl", 0.4f },
            { "fmode", LP12 }, { "cutoff", 250 }, { "res", 0.45f }, { "envamt", 0.5f },
            { "fdec", 0.18f }, { "fsus", 0.1f },
            { "adec", 0.6f }, { "asus", 0.8f }, { "arel", 0.1f },
            { "voicemode", LEGATO }, { "glide", 0.06f }, { "spread", 0 } } },

        { "Dark Reese", "Bass", {
            { "osc1shape", SAW }, { "osc1oct", -1 }, { "osc2shape", SAW }, { "osc2oct", -1 }, { "osc2fine", 18 },
            { "osc2lvl", 0.9f },
            { "cutoff", 600 }, { "res", 0.15f }, { "drive", 0.5f }, { "envamt", 0.1f }, { "keytrack", 0.2f },
            { "aatt", 0.005f }, { "asus", 1 }, { "arel", 0.15f },
            { "voicemode", MONO }, { "drift", 0.6f }, { "spread", 0.3f } } },

        // ---------------------------------------------------------------- Lead
        { "Classic Mono Lead", "Lead", {
            { "osc2fine", 5 }, { "osc1lvl", 0.8f }, { "osc2lvl", 0.75f },
            { "cutoff", 1800 }, { "res", 0.35f }, { "drive", 0.3f }, { "envamt", 0.3f },
            { "fdec", 0.4f }, { "fsus", 0.5f },
            { "asus", 1 }, { "arel", 0.2f },
            { "lfoshape", L_SINE }, { "lforate", 5.5f }, { "lfopitch", 0.1f },
            { "voicemode", LEGATO }, { "glide", 0.08f }, { "spread", 0 } } },

        { "Square Lead", "Lead", {
            { "osc1shape", PULSE }, { "osc2shape", PULSE }, { "osc2oct", 1 }, { "osc2lvl", 0.3f },
            { "fmode", LP12 }, { "cutoff", 3000 }, { "res", 0.2f }, { "envamt", 0.2f },
            { "asus", 1 }, { "arel", 0.15f },
            { "lfoshape", L_SINE }, { "lforate", 5 }, { "lfopitch", 0.08f },
            { "voicemode", LEGATO }, { "glide", 0.05f }, { "spread", 0 } } },

        { "Screamer", "Lead", {
            { "osc2semi", 7 }, { "osc2lvl", 0.6f },
            { "cutoff", 2500 }, { "res", 0.6f }, { "drive", 0.75f }, { "envamt", 0.4f },
            { "fdec", 0.6f }, { "fsus", 0.4f },
            { "asus", 1 }, { "arel", 0.25f },
            { "lfoshape", L_SINE }, { "lforate", 6 }, { "lfopitch", 0.06f },
            { "voicemode", MONO }, { "glide", 0.04f }, { "spread", 0 } } },

        { "Soft Sine Lead", "Lead", {
            { "osc1shape", SINE }, { "osc2shape", TRI }, { "osc2oct", 1 }, { "osc2lvl", 0.3f }, { "osc2fine", 0 },
            { "cutoff", 4000 }, { "res", 0.1f }, { "envamt", 0.1f },
            { "aatt", 0.03f }, { "asus", 1 }, { "arel", 0.4f },
            { "lfoshape", L_SINE }, { "lforate", 5 }, { "lfopitch", 0.12f },
            { "voicemode", LEGATO }, { "glide", 0.12f }, { "chorus", CH_I }, { "spread", 0 } } },

        { "PWM Lead", "Lead", {
            { "osc1shape", PULSE }, { "pw", 0.4f }, { "osc2fine", -7 }, { "osc2lvl", 0.5f },
            { "cutoff", 2200 }, { "res", 0.25f }, { "envamt", 0.25f }, { "fdec", 0.5f }, { "fsus", 0.4f },
            { "asus", 1 }, { "arel", 0.2f },
            { "lfoshape", L_TRI }, { "lforate", 0.9f }, { "lfopw", 0.5f },
            { "voicemode", MONO }, { "glide", 0.06f }, { "spread", 0 } } },

        // ---------------------------------------------------------------- Pad
        { "Warm Analog Pad", "Pad", {
            { "osc2fine", 9 }, { "osc1lvl", 0.7f }, { "osc2lvl", 0.7f },
            { "cutoff", 900 }, { "res", 0.15f }, { "envamt", 0.25f },
            { "fatt", 1.2f }, { "fdec", 2 }, { "fsus", 0.6f }, { "frel", 2 },
            { "aatt", 0.9f }, { "adec", 1 }, { "asus", 0.9f }, { "arel", 1.8f },
            { "drift", 0.5f }, { "spread", 0.8f }, { "chorus", CH_II } } },

        { "Poly Strings", "Pad", {
            { "osc1shape", PULSE }, { "pw", 0.5f }, { "osc2shape", SAW }, { "osc2lvl", 0.5f }, { "osc2fine", 0 },
            { "cutoff", 2500 }, { "res", 0.05f }, { "envamt", 0.05f },
            { "aatt", 0.5f }, { "asus", 1 }, { "arel", 1.2f },
            { "lfoshape", L_TRI }, { "lforate", 0.6f }, { "lfopw", 0.4f },
            { "spread", 0.6f }, { "chorus", CH_II } } },

        { "Dark Sweep Pad", "Pad", {
            { "osc2shape", PULSE }, { "osc2fine", -12 },
            { "cutoff", 300 }, { "res", 0.45f }, { "envamt", 0.6f },
            { "fatt", 3 }, { "fdec", 4 }, { "fsus", 0.3f }, { "frel", 3 },
            { "aatt", 1.5f }, { "asus", 1 }, { "arel", 3 },
            { "lfoshape", L_TRI }, { "lforate", 0.15f }, { "lfofilter", 0.15f },
            { "spread", 0.7f }, { "chorus", CH_I } } },

        { "Glass Pad", "Pad", {
            { "osc1shape", TRI }, { "osc2shape", SINE }, { "osc2oct", 1 }, { "osc2lvl", 0.5f }, { "osc2fine", 4 },
            { "cutoff", 6000 }, { "res", 0.3f }, { "envamt", 0 },
            { "aatt", 1 }, { "asus", 1 }, { "arel", 2.5f },
            { "lfoshape", L_SINE }, { "lforate", 0.3f }, { "lfofilter", 0.1f },
            { "spread", 1 }, { "chorus", CH_II } } },

        { "Breathing Pad", "Pad", {
            { "osc2semi", 7 }, { "osc2lvl", 0.4f }, { "noiselvl", 0.1f },
            { "fmode", BP12 }, { "cutoff", 1200 }, { "res", 0.4f }, { "envamt", 0 },
            { "aatt", 2 }, { "asus", 1 }, { "arel", 3 },
            { "lfoshape", L_SINE }, { "lforate", 0.12f }, { "lfofilter", 0.4f },
            { "spread", 0.8f }, { "chorus", CH_I } } },

        // ---------------------------------------------------------------- Synth (keys, plucks, stabs)
        { "Analog Pluck", "Synth", {
            { "osc2shape", PULSE }, { "osc2oct", 1 }, { "osc2lvl", 0.3f },
            { "cutoff", 300 }, { "res", 0.3f }, { "envamt", 0.55f },
            { "fdec", 0.25f }, { "fsus", 0 }, { "frel", 0.25f },
            { "aatt", 0.001f }, { "adec", 0.6f }, { "asus", 0 }, { "arel", 0.4f },
            { "spread", 0.6f }, { "chorus", CH_I } } },

        { "Poly Brass", "Synth", {
            { "osc2fine", 6 }, { "osc2lvl", 0.8f },
            { "cutoff", 700 }, { "res", 0.1f }, { "envamt", 0.45f },
            { "fatt", 0.06f }, { "fdec", 0.5f }, { "fsus", 0.5f }, { "frel", 0.3f },
            { "aatt", 0.03f }, { "asus", 0.9f }, { "arel", 0.3f },
            { "velsens", 0.7f }, { "spread", 0.5f } } },

        { "Bell Keys", "Synth", {
            { "osc1shape", TRI }, { "osc2shape", SINE }, { "osc2oct", 2 }, { "osc2semi", 7 }, { "osc2lvl", 0.4f }, { "osc2fine", 0 },
            { "cutoff", 5000 }, { "res", 0 }, { "envamt", 0.2f },
            { "adec", 1.5f }, { "asus", 0 }, { "arel", 1 },
            { "spread", 0.5f }, { "chorus", CH_I } } },

        { "House Stab", "Synth", {
            { "osc2semi", 7 }, { "osc2lvl", 0.8f },
            { "cutoff", 700 }, { "res", 0.35f }, { "envamt", 0.5f },
            { "fdec", 0.3f }, { "fsus", 0 },
            { "adec", 0.35f }, { "asus", 0 }, { "arel", 0.2f },
            { "spread", 0.5f }, { "chorus", CH_II } } },

        { "Electric Keys", "Synth", {
            { "osc1shape", PULSE }, { "pw", 0.25f }, { "osc2shape", TRI }, { "osc2oct", 1 }, { "osc2lvl", 0.4f }, { "osc2fine", 0 },
            { "fmode", LP12 }, { "cutoff", 1500 }, { "res", 0.1f }, { "keytrack", 1 }, { "envamt", 0.2f },
            { "fdec", 0.8f }, { "fsus", 0.2f },
            { "aatt", 0.002f }, { "adec", 1.2f }, { "asus", 0.3f }, { "arel", 0.4f },
            { "velsens", 0.8f }, { "chorus", CH_I } } },

        // ---------------------------------------------------------------- FX
        { "Laser Zap", "FX", {
            { "osc2lvl", 0 },
            { "cutoff", 150 }, { "res", 1 }, { "drive", 0.4f }, { "keytrack", 1 }, { "envamt", 1 },
            { "fatt", 0.001f }, { "fdec", 0.15f }, { "fsus", 0 }, { "frel", 0.15f },
            { "adec", 0.3f }, { "asus", 0 }, { "arel", 0.2f },
            { "spread", 0.4f }, { "volume", 2 } } },

        { "Noise Wind", "FX", {
            { "osc1lvl", 0 }, { "osc2lvl", 0 }, { "noiselvl", 1 },
            { "fmode", BP12 }, { "cutoff", 800 }, { "res", 0.7f }, { "keytrack", 0.8f }, { "envamt", 0 },
            { "aatt", 1.5f }, { "asus", 1 }, { "arel", 2.5f },
            { "lfoshape", L_SINE }, { "lforate", 0.1f }, { "lfofilter", 0.8f },
            { "spread", 1 }, { "chorus", CH_II } } },

        { "Sci-Fi Wobble", "FX", {
            { "osc2shape", PULSE }, { "osc2oct", -1 },
            { "cutoff", 500 }, { "res", 0.6f }, { "envamt", 0 },
            { "asus", 1 }, { "arel", 0.3f },
            { "lfoshape", L_SQR }, { "lforate", 6 }, { "lfofilter", 0.5f }, { "lfopitch", 0.3f } } },

        { "Random Computer", "FX", {
            { "osc1shape", PULSE }, { "osc1oct", 1 }, { "osc2lvl", 0 },
            { "cutoff", 1200 }, { "res", 0.75f }, { "envamt", 0 },
            { "asus", 1 }, { "arel", 0.2f },
            { "lfoshape", L_SH }, { "lforate", 8 }, { "lfofilter", 0.9f },
            { "spread", 0.5f } } },

        { "Riser Sweep", "FX", {
            { "osc2fine", 25 }, { "osc2lvl", 0.8f }, { "noiselvl", 0.3f },
            { "cutoff", 150 }, { "res", 0.5f }, { "envamt", 1 },
            { "fatt", 6 }, { "fdec", 4 }, { "fsus", 1 }, { "frel", 2 },
            { "aatt", 3 }, { "asus", 1 }, { "arel", 2 },
            { "drift", 0.8f }, { "spread", 1 }, { "chorus", CH_II } } },
    };
    return presets;
}

//==============================================================================
MiraPresetManager::MiraPresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state) {}

juce::File MiraPresetManager::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Homebrew").getChildFile ("Mira").getChildFile ("Presets");
}

juce::File MiraPresetManager::getDefaultDivaFolder()
{
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    const juce::File candidates[] = {
        docs.getChildFile ("u-he/Diva.data/Presets/Diva"),
       #if JUCE_WINDOWS
        juce::File ("C:\\Program Files\\Common Files\\VST3\\u-he\\Diva.data\\Presets\\Diva"),
        juce::File ("C:\\Program Files\\VSTPlugins\\u-he\\Diva.data\\Presets\\Diva"),
       #else
        juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Audio/Presets/u-he/Diva"),
        juce::File ("/Library/Audio/Presets/u-he/Diva"),
       #endif
    };
    for (auto& f : candidates)
        if (f.isDirectory())
            return f;
    return docs;
}

std::vector<MiraPresetManager::Entry> MiraPresetManager::scanEntries() const
{
    std::vector<Entry> entries;
    const auto& factory = getMiraFactoryPresets();
    for (int i = 0; i < (int) factory.size(); ++i)
    {
        Entry e;
        e.name = factory[(size_t) i].name;
        e.path = { "Factory", factory[(size_t) i].category };
        e.factoryIndex = i;
        entries.push_back (e);
    }

    const auto root = getUserPresetFolder();
    if (root.isDirectory())
    {
        auto files = root.findChildFiles (juce::File::findFiles, true, juce::String ("*") + fileExtension);
        // Sort by folder then name, case-insensitively, for a tidy menu.
        std::sort (files.begin(), files.end(), [&root] (const juce::File& a, const juce::File& b)
        {
            return a.getRelativePathFrom (root).compareIgnoreCase (b.getRelativePathFrom (root)) < 0;
        });

        for (auto& f : files)
        {
            Entry e;
            e.name = f.getFileNameWithoutExtension();
            e.file = f;
            e.path.add ("User");
            for (auto p = f.getParentDirectory(); p != root && p.isAChildOf (root); p = p.getParentDirectory())
                e.path.insert (1, p.getFileName());
            entries.push_back (e);
        }
    }
    return entries;
}

void MiraPresetManager::applyValues (const diva::Values& values, const juce::String& name, const juce::String& id)
{
    // Reset everything first so presets are complete snapshots.
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& [paramId, value] : values)
        if (auto* rp = apvts.getParameter (paramId))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));

    apvts.state.setProperty ("presetName", name, nullptr);
    apvts.state.setProperty ("presetId", id, nullptr);
}

void MiraPresetManager::loadFactory (int index)
{
    const auto& factory = getMiraFactoryPresets();
    if (index < 0 || index >= (int) factory.size())
        return;
    const auto& p = factory[(size_t) index];
    applyValues (p.values, p.name, "factory:" + juce::String (index));
}

bool MiraPresetManager::loadFile (const juce::File& file)
{
    if (file.hasFileExtension ("h2p"))
        return importAndLoadDivaFile (file);

    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("MiraPreset"))
        return false;

    diva::Values values;
    for (auto* param : xml->getChildWithTagNameIterator ("PARAM"))
        values.emplace_back (param->getStringAttribute ("id"), (float) param->getDoubleAttribute ("value"));

    applyValues (values, xml->getStringAttribute ("name", file.getFileNameWithoutExtension()), file.getFullPathName());
    return true;
}

bool MiraPresetManager::loadEntry (const Entry& entry)
{
    if (entry.factoryIndex >= 0)
    {
        loadFactory (entry.factoryIndex);
        return true;
    }
    return loadFile (entry.file);
}

void MiraPresetManager::step (const std::vector<Entry>& entries, int delta)
{
    if (entries.empty())
        return;

    const auto id = getCurrentId();
    int current = -1;
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        const auto& e = entries[(size_t) i];
        const auto entryId = e.factoryIndex >= 0 ? "factory:" + juce::String (e.factoryIndex) : e.file.getFullPathName();
        if (entryId == id)
            current = i;
    }

    const int n = (int) entries.size();
    const int next = current < 0 ? 0 : ((current + delta) % n + n) % n;
    loadEntry (entries[(size_t) next]);
}

diva::Values MiraPresetManager::captureValues() const
{
    diva::Values values;
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            values.emplace_back (rp->paramID, rp->convertFrom0to1 (rp->getValue()));
    return values;
}

bool MiraPresetManager::writePreset (const juce::File& file, const juce::String& name, const juce::String& category,
                                     const juce::String& source, const diva::Values& values)
{
    juce::XmlElement xml ("MiraPreset");
    xml.setAttribute ("name", name);
    xml.setAttribute ("category", category);
    if (source.isNotEmpty())
        xml.setAttribute ("source", source);
    for (auto& [id, value] : values)
    {
        auto* p = xml.createNewChildElement ("PARAM");
        p->setAttribute ("id", id);
        p->setAttribute ("value", value);
    }
    file.getParentDirectory().createDirectory();
    return xml.writeTo (file);
}

juce::File MiraPresetManager::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim().isEmpty() ? juce::String ("Untitled") : name.trim());
    const auto file = getUserPresetFolder().getChildFile (clean + fileExtension);
    if (! writePreset (file, clean, "User", {}, captureValues()))
        return {};
    apvts.state.setProperty ("presetName", clean, nullptr);
    apvts.state.setProperty ("presetId", file.getFullPathName(), nullptr);
    return file;
}

MiraPresetManager::ImportResult MiraPresetManager::importDiva (const juce::Array<juce::File>& filesOrFolders)
{
    ImportResult result;
    const auto importRoot = getDivaImportFolder();

    auto importOne = [&] (const juce::File& h2p, const juce::File& destFolder)
    {
        auto converted = diva::convertFile (h2p);
        if (! converted.ok)
        {
            ++result.failed;
            result.errors.add (h2p.getFileName() + ": " + converted.error);
            return;
        }
        const auto dest = destFolder.getChildFile (juce::File::createLegalFileName (converted.name) + fileExtension);
        if (writePreset (dest, converted.name, converted.category, "Diva: " + h2p.getFileName(), converted.values))
        {
            ++result.imported;
            if (result.first == juce::File())
                result.first = dest;
        }
        else
        {
            ++result.failed;
            result.errors.add (h2p.getFileName() + ": could not write " + dest.getFullPathName());
        }
    };

    for (auto& item : filesOrFolders)
    {
        if (item.isDirectory())
        {
            const auto libraryFolder = importRoot.getChildFile (juce::File::createLegalFileName (item.getFileName()));
            for (auto& f : item.findChildFiles (juce::File::findFiles, true, "*.h2p"))
            {
                auto dest = libraryFolder;
                const auto rel = f.getParentDirectory().getRelativePathFrom (item);
                if (rel.isNotEmpty() && rel != ".")
                    dest = libraryFolder.getChildFile (rel);
                importOne (f, dest);
            }
        }
        else if (item.hasFileExtension ("h2p"))
        {
            importOne (item, importRoot.getChildFile ("Imported"));
        }
    }
    return result;
}

bool MiraPresetManager::importAndLoadDivaFile (const juce::File& h2p)
{
    auto result = importDiva ({ h2p });
    return result.imported > 0 && loadFile (result.first);
}

juce::String MiraPresetManager::getCurrentName() const
{
    return apvts.state.getProperty ("presetName", "Init").toString();
}

juce::String MiraPresetManager::getCurrentId() const
{
    return apvts.state.getProperty ("presetId", "factory:0").toString();
}
