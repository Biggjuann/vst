#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "DivaImporter.h"

/** A factory sound: only parameters that differ from the defaults are listed. */
struct MiraFactoryPreset
{
    juce::String name, category;
    diva::Values values;
};

const std::vector<MiraFactoryPreset>& getMiraFactoryPresets();

/**
    Loads/saves Mira presets and imports Diva libraries.

    - Factory presets live in code.
    - User presets are XML files (.mirapreset) under
      %APPDATA%\Homebrew\Mira\Presets (Windows) or ~/Library/Application Support/... (Mac).
    - Imported Diva presets are converted once and saved under Presets\Diva Import\<library>,
      so they show up in the browser like any other user preset.
*/
class MiraPresetManager
{
public:
    static constexpr const char* fileExtension = ".mirapreset";

    struct Entry
    {
        juce::String name;
        juce::StringArray path; // menu path, e.g. { "Factory", "Bass" } or { "Diva Import", "Factory" }
        int factoryIndex = -1;
        juce::File file;
    };

    struct ImportResult
    {
        int imported = 0, failed = 0;
        juce::File first;
        juce::StringArray errors;
    };

    explicit MiraPresetManager (juce::AudioProcessorValueTreeState& state);

    static juce::File getUserPresetFolder();
    static juce::File getDivaImportFolder() { return getUserPresetFolder().getChildFile ("Diva Import"); }
    /** Where Diva usually keeps its presets on this machine (may not exist). */
    static juce::File getDefaultDivaFolder();

    /** Factory presets first, then every user/imported preset on disk. */
    std::vector<Entry> scanEntries() const;

    void loadFactory (int index);
    bool loadFile (const juce::File& file);
    bool loadEntry (const Entry& entry);
    /** Step through the given list relative to the current preset. */
    void step (const std::vector<Entry>& entries, int delta);

    juce::File saveUserPreset (const juce::String& name);

    /** Import .h2p files and/or folders containing them (recursively). */
    ImportResult importDiva (const juce::Array<juce::File>& filesOrFolders);
    /** Load a Diva preset straight away (also saves the converted copy). */
    bool importAndLoadDivaFile (const juce::File& h2p);

    juce::String getCurrentName() const;
    juce::String getCurrentId() const;

    void applyValues (const diva::Values& values, const juce::String& name, const juce::String& id);

private:
    static bool writePreset (const juce::File& file, const juce::String& name, const juce::String& category,
                             const juce::String& source, const diva::Values& values);
    diva::Values captureValues() const;

    juce::AudioProcessorValueTreeState& apvts;
};
