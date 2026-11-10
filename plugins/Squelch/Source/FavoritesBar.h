#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

/**
    Squelch's favorites, stored as small XML files:
      Sounds   (.squelch)         knobs + generator settings + all 8 patterns
      Patterns (.squelchpattern)  one 16-step pattern and its length
    in %APPDATA%\Homebrew\Squelch\Favorites (Windows) / ~/Library/Application Support/... (Mac).
*/
namespace favorites
{
inline juce::File root()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Homebrew").getChildFile ("Squelch").getChildFile ("Favorites");
}
inline juce::File soundsFolder()   { return root().getChildFile ("Sounds"); }
inline juce::File patternsFolder() { return root().getChildFile ("Patterns"); }

inline juce::Array<juce::File> list (const juce::File& folder, const juce::String& pattern)
{
    auto files = folder.findChildFiles (juce::File::findFiles, false, pattern);
    files.sort();
    return files;
}

inline juce::File uniqueFile (const juce::File& folder, const juce::String& name, const juce::String& ext)
{
    folder.createDirectory();
    const auto clean = juce::File::createLegalFileName (name.trim().isEmpty() ? juce::String ("Untitled") : name.trim());
    return folder.getChildFile (clean + ext); // same name = overwrite, like saving a preset
}

inline bool saveSound (SquelchAudioProcessor& proc, const juce::String& name)
{
    auto xml = proc.createSoundFavorite (name);
    return xml != nullptr && xml->writeTo (uniqueFile (soundsFolder(), name, ".squelch"));
}

inline bool loadSound (SquelchAudioProcessor& proc, const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    return xml != nullptr && proc.loadSoundFavorite (*xml);
}

inline bool savePattern (SquelchAudioProcessor& proc, const juce::String& name)
{
    juce::XmlElement xml ("SquelchPattern");
    xml.setAttribute ("name", name);
    xml.setAttribute ("length", proc.getLength());
    xml.setAttribute ("steps", proc.patterns.serialise (proc.getSelectedPattern()));
    return xml.writeTo (uniqueFile (patternsFolder(), name, ".squelchpattern"));
}

inline bool loadPattern (SquelchAudioProcessor& proc, const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("SquelchPattern"))
        return false;
    auto parts = juce::StringArray::fromTokens (xml->getStringAttribute ("steps"), ",", "");
    if (parts.size() != seq::numSteps)
        return false;
    std::array<seq::Step, seq::numSteps> steps {};
    for (int i = 0; i < seq::numSteps; ++i)
        steps[(size_t) i] = seq::Step::unpack ((uint32_t) parts[i].getLargeIntValue());
    proc.loadIntoCurrentPattern (steps, xml->getIntAttribute ("length", seq::numSteps));
    return true;
}
} // namespace favorites

/** Drag this onto an Ableton track or clip slot to drop in the pattern as a MIDI clip. */
class MidiDragTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    MidiDragTile (SquelchAudioProcessor& p, juce::Colour c) : proc (p), colour (c)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip ("Drag onto an Ableton track to drop in the pattern as a MIDI clip.\nClick for more options.");
    }

    int bars = 4;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (hover ? colour.withAlpha (0.3f) : juce::Colour (0xff23272e));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (colour.withAlpha (0.8f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (juce::String::fromUTF8 ("\xe2\x87\xa9 Drag MIDI \xc2\xb7 ") + juce::String (bars) + (bars == 1 ? " bar" : " bars"),
                    getLocalBounds(), juce::Justification::centred);
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6)
            return;
        const auto file = writeTempFile();
        if (file == juce::File())
            return;
        dragging = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                    [safe = juce::Component::SafePointer<MidiDragTile> (this)]
                                                                    { if (safe != nullptr) safe->dragging = false; });
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() >= 6)
            return;

        juce::PopupMenu lengthMenu;
        for (int b : { 1, 2, 4, 8, 16 })
            lengthMenu.addItem (100 + b, juce::String (b) + (b == 1 ? " bar" : " bars"), true, b == bars);

        juce::PopupMenu menu;
        menu.addSectionHeader ("Clip length (includes Evolve changes)");
        menu.addSubMenu ("Length: " + juce::String (bars) + (bars == 1 ? " bar" : " bars"), lengthMenu);
        menu.addSeparator();
        menu.addItem (1, "Save as MIDI file...");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
        {
            if (r > 100)
            {
                bars = r - 100;
                repaint();
            }
            else if (r == 1)
                saveAs();
        });
    }

private:
    juce::String clipName() const
    {
        return "Squelch Pattern " + juce::String (proc.getSelectedPattern() + 1) + " (" + juce::String (bars) + " bars)";
    }

    juce::File writeTempFile()
    {
        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Squelch MIDI");
        folder.createDirectory();
        const auto file = folder.getChildFile (juce::File::createLegalFileName (clipName()) + ".mid");
        return write (file) ? file : juce::File();
    }

    bool write (const juce::File& file)
    {
        file.deleteFile();
        juce::FileOutputStream out (file);
        return out.openedOk() && proc.createMidiClip (bars).writeTo (out);
    }

    void saveAs()
    {
        chooser = std::make_unique<juce::FileChooser> ("Save pattern as MIDI",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (juce::File::createLegalFileName (clipName()) + ".mid"),
            "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File())
                                      write (f.withFileExtension ("mid"));
                              });
    }

    SquelchAudioProcessor& proc;
    juce::Colour colour;
    bool hover = false, dragging = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

/** Title-bar strip: Favorites menu, Save, MIDI Out toggle, Drag MIDI. */
class FavoritesBar : public juce::Component
{
public:
    explicit FavoritesBar (SquelchAudioProcessor& p) : proc (p), dragTile (p, juce::Colour (0xffffa62b))
    {
        favButton.onClick = [this] { showFavoritesMenu(); };
        saveButton.onClick = [this] { showSaveMenu(); };
        midiOutButton.setClickingTogglesState (true);
        midiOutButton.setTooltip ("Send the sequencer's notes out as MIDI, so another MIDI track can record them "
                                  "(in Ableton: set that track's MIDI From to this track).");
        midiOutAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "midiout", midiOutButton);

        for (auto* c : std::initializer_list<juce::Component*> { &favButton, &saveButton, &midiOutButton, &dragTile })
            addAndMakeVisible (*c);
    }

    static constexpr int preferredWidth = 600;

    void resized() override
    {
        auto r = getLocalBounds();
        dragTile.setBounds (r.removeFromRight (180));
        r.removeFromRight (8);
        midiOutButton.setBounds (r.removeFromRight (80));
        r.removeFromRight (14);
        saveButton.setBounds (r.removeFromRight (60));
        r.removeFromRight (6);
        favButton.setBounds (r);
    }

private:
    void showFavoritesMenu()
    {
        sounds = favorites::list (favorites::soundsFolder(), "*.squelch");
        patternFiles = favorites::list (favorites::patternsFolder(), "*.squelchpattern");

        juce::PopupMenu soundMenu, patternMenu, deleteMenu;
        for (int i = 0; i < sounds.size(); ++i)
        {
            soundMenu.addItem (1000 + i, sounds[i].getFileNameWithoutExtension());
            deleteMenu.addItem (3000 + i, "Sound: " + sounds[i].getFileNameWithoutExtension());
        }
        for (int i = 0; i < patternFiles.size(); ++i)
        {
            patternMenu.addItem (2000 + i, patternFiles[i].getFileNameWithoutExtension());
            deleteMenu.addItem (4000 + i, "Pattern: " + patternFiles[i].getFileNameWithoutExtension());
        }
        if (sounds.isEmpty())       soundMenu.addItem (-1, "(none saved yet)", false);
        if (patternFiles.isEmpty()) patternMenu.addItem (-1, "(none saved yet)", false);

        juce::PopupMenu menu;
        menu.addSectionHeader ("Load");
        menu.addSubMenu ("Sounds (knobs + all 8 patterns)", soundMenu);
        menu.addSubMenu ("Patterns (into Pattern " + juce::String (proc.getSelectedPattern() + 1) + ")", patternMenu);
        menu.addSeparator();
        menu.addSubMenu ("Delete", deleteMenu, deleteMenu.getNumItems() > 0);
        menu.addItem (5, "Open favorites folder");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&favButton), [this] (int r)
        {
            if (r >= 1000 && r < 2000 && favorites::loadSound (proc, sounds[r - 1000]))
                setCurrent (sounds[r - 1000].getFileNameWithoutExtension());
            else if (r >= 2000 && r < 3000 && favorites::loadPattern (proc, patternFiles[r - 2000]))
                setCurrent (patternFiles[r - 2000].getFileNameWithoutExtension() + " (pattern)");
            else if (r >= 3000 && r < 4000)
                confirmDelete (sounds[r - 3000]);
            else if (r >= 4000 && r < 5000)
                confirmDelete (patternFiles[r - 4000]);
            else if (r == 5)
            {
                favorites::root().createDirectory();
                favorites::root().startAsProcess();
            }
        });
    }

    void showSaveMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Save sound (knobs + all 8 patterns)...");
        menu.addItem (2, "Save Pattern " + juce::String (proc.getSelectedPattern() + 1) + " only...");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&saveButton), [this] (int r)
        {
            if (r == 1 || r == 2)
                askName (r == 1);
        });
    }

    void askName (bool sound)
    {
        auto* window = new juce::AlertWindow (sound ? "Save Favorite Sound" : "Save Favorite Pattern", "Name:",
                                              juce::MessageBoxIconType::NoIcon, this);
        window->addTextEditor ("name", currentName.upToFirstOccurrenceOf (" (pattern)", false, false));
        window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        window->enterModalState (true, juce::ModalCallbackFunction::create ([this, window, sound] (int result)
        {
            std::unique_ptr<juce::AlertWindow> owner (window);
            if (result != 1)
                return;
            const auto name = window->getTextEditorContents ("name");
            const bool ok = sound ? favorites::saveSound (proc, name) : favorites::savePattern (proc, name);
            if (ok)
                setCurrent (sound ? name : name + " (pattern)");
            else
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Favorites", "Couldn't save the favorite.");
        }), false);
    }

    void confirmDelete (const juce::File& file)
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Delete favorite",
                                            "Delete \"" + file.getFileNameWithoutExtension() + "\"?", "Delete", "Cancel", this,
                                            juce::ModalCallbackFunction::create ([file] (int r) { if (r == 1) file.deleteFile(); }));
    }

    void setCurrent (const juce::String& name)
    {
        currentName = name;
        favButton.setButtonText (juce::String::fromUTF8 ("\xe2\x99\xa5 ") + name);
    }

    SquelchAudioProcessor& proc;
    juce::TextButton favButton { juce::String::fromUTF8 ("\xe2\x99\xa5 Favorites") }, saveButton { "Save" }, midiOutButton { "MIDI Out" };
    MidiDragTile dragTile;
    juce::Array<juce::File> sounds, patternFiles;
    juce::String currentName;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiOutAttachment;
};
