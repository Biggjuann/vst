#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "MiraPresets.h"

/** Preset browser strip shown in Mira's title bar: < [name] > Save Import. */
class PresetBar : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (MiraPresetManager& managerToUse) : manager (managerToUse)
    {
        for (auto* b : { &prevButton, &nextButton, &saveButton, &importButton, &nameButton })
            addAndMakeVisible (*b);

        prevButton.onClick = [this] { refreshEntries(); manager.step (entries, -1); };
        nextButton.onClick = [this] { refreshEntries(); manager.step (entries, +1); };
        nameButton.onClick = [this] { showBrowser(); };
        saveButton.onClick = [this] { showSaveDialog(); };
        importButton.onClick = [this] { showImportMenu(); };

        nameButton.setTooltip ("Browse presets");
        importButton.setTooltip ("Import Diva presets (.h2p) - or drag them onto the window");

        refreshEntries();
        startTimerHz (5);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        importButton.setBounds (r.removeFromRight (96));
        r.removeFromRight (6);
        saveButton.setBounds (r.removeFromRight (56));
        r.removeFromRight (6);
        nextButton.setBounds (r.removeFromRight (28));
        prevButton.setBounds (r.removeFromLeft (28));
        nameButton.setBounds (r.reduced (4, 0));
    }

    /** Handles .h2p / .mirapreset files or folders dropped on the editor. */
    void handleDroppedFiles (const juce::StringArray& paths)
    {
        juce::Array<juce::File> divaItems;
        for (auto& path : paths)
        {
            const juce::File f (path);
            if (f.hasFileExtension (MiraPresetManager::fileExtension))
                manager.loadFile (f);
            else
                divaItems.add (f);
        }
        if (! divaItems.isEmpty())
            runImport (divaItems);
    }

    static bool canHandle (const juce::StringArray& paths)
    {
        for (auto& path : paths)
        {
            const juce::File f (path);
            if (f.isDirectory() || f.hasFileExtension ("h2p") || f.hasFileExtension (MiraPresetManager::fileExtension))
                return true;
        }
        return false;
    }

private:
    void timerCallback() override
    {
        const auto name = manager.getCurrentName();
        if (nameButton.getButtonText() != name)
            nameButton.setButtonText (name);
    }

    void refreshEntries() { entries = manager.scanEntries(); }

    void showBrowser()
    {
        refreshEntries();
        const auto currentId = manager.getCurrentId();

        // Build nested menus from each entry's path.
        struct Node
        {
            std::map<juce::String, std::unique_ptr<Node>> children;
            std::vector<std::pair<int, juce::String>> items; // (entry index, name)
            juce::StringArray order;
        };
        Node root;
        for (int i = 0; i < (int) entries.size(); ++i)
        {
            Node* node = &root;
            for (auto& part : entries[(size_t) i].path)
            {
                auto& child = node->children[part];
                if (child == nullptr)
                {
                    child = std::make_unique<Node>();
                    node->order.add (part);
                }
                node = child.get();
            }
            node->items.emplace_back (i, entries[(size_t) i].name);
        }

        std::function<juce::PopupMenu (const Node&)> build = [&] (const Node& node)
        {
            juce::PopupMenu menu;
            for (auto& name : node.order)
                menu.addSubMenu (name, build (*node.children.at (name)));
            for (auto& [index, name] : node.items)
            {
                const auto& e = entries[(size_t) index];
                const auto id = e.factoryIndex >= 0 ? "factory:" + juce::String (e.factoryIndex) : e.file.getFullPathName();
                menu.addItem (index + 1, name, true, id == currentId);
            }
            return menu;
        };

        auto menu = build (root);
        if (entries.size() == getMiraFactoryPresets().size())
        {
            menu.addSeparator();
            menu.addItem (-1, "(Saved and imported presets will appear under \"User\")", false);
        }

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton),
                            [this] (int result)
                            {
                                if (result > 0 && result <= (int) entries.size())
                                    manager.loadEntry (entries[(size_t) result - 1]);
                            });
    }

    void showSaveDialog()
    {
        auto* window = new juce::AlertWindow ("Save Preset", "Name:", juce::MessageBoxIconType::NoIcon, this);
        window->addTextEditor ("name", manager.getCurrentName());
        window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        window->enterModalState (true, juce::ModalCallbackFunction::create ([this, window] (int result)
        {
            std::unique_ptr<juce::AlertWindow> owner (window);
            if (result == 1)
                manager.saveUserPreset (window->getTextEditorContents ("name"));
        }), false);
    }

    void showImportMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Import Diva preset files (.h2p)...");
        menu.addItem (2, "Import a whole Diva library folder...");
        menu.addSeparator();
        menu.addItem (3, "Open user preset folder");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&importButton), [this] (int r)
        {
            if (r == 1)
                choose (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                        | juce::FileBrowserComponent::canSelectMultipleItems, "*.h2p");
            else if (r == 2)
                choose (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, "*");
            else if (r == 3)
            {
                MiraPresetManager::getUserPresetFolder().createDirectory();
                MiraPresetManager::getUserPresetFolder().startAsProcess();
            }
        });
    }

    void choose (int browserFlags, const juce::String& pattern)
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose Diva presets", MiraPresetManager::getDefaultDivaFolder(), pattern);
        chooser->launchAsync (browserFlags, [this] (const juce::FileChooser& fc)
        {
            const auto results = fc.getResults();
            if (! results.isEmpty())
                runImport (results);
        });
    }

    void runImport (const juce::Array<juce::File>& items)
    {
        juce::MouseCursor::showWaitCursor();
        const auto result = manager.importDiva (items);
        juce::MouseCursor::hideWaitCursor();

        if (result.imported > 0)
            manager.loadFile (result.first);
        refreshEntries();

        juce::String message;
        message << "Imported " << result.imported << " Diva preset" << (result.imported == 1 ? "" : "s");
        if (result.failed > 0)
            message << "\n" << result.failed << " could not be converted.";
        message << "\n\nFind them in the preset menu under User > Diva Import.\n"
                << "Diva conversions are close approximations: Mira has fewer oscillator and filter models.";
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Diva Import", message);
    }

    MiraPresetManager& manager;
    std::vector<MiraPresetManager::Entry> entries;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" }, importButton { "Import Diva" };
    juce::TextButton nameButton { "Init" };
};
