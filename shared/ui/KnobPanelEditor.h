#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

/**
    A data-driven plugin editor: describe rows of labelled sections, each holding parameter
    IDs, and it builds knobs (float/int params) or drop-downs (choice params) bound to the
    AudioProcessorValueTreeState. Lets each synth get a tidy UI with a few lines of code.
*/
namespace sui
{
struct Section
{
    juce::String title;
    juce::StringArray paramIDs;
};

using Row = std::vector<Section>;

/** Optional extra components: one on the right of the title bar, one under the knob rows. */
struct EditorExtras
{
    std::unique_ptr<juce::Component> titleBar;
    int titleBarWidth = 0;
    std::unique_ptr<juce::Component> footer;
    int footerHeight = 0;
    int footerMinWidth = 0;
    /** Optional file drag-and-drop onto the whole window. */
    std::function<bool (const juce::StringArray&)> canDropFiles;
    std::function<void (const juce::StringArray&)> filesDropped;
};

class SynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit SynthLookAndFeel (juce::Colour accentColour) : accent (accentColour)
    {
        setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.8f));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff15171b));
        setColour (juce::ComboBox::outlineColourId, accent.withAlpha (0.45f));
        setColour (juce::ComboBox::textColourId, juce::Colours::white);
        setColour (juce::ComboBox::arrowColourId, accent);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff15171b));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.35f));
        setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.65f));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff23272e));
        setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.85f));
        setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.85f));
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::ComboBox::buttonColourId, juce::Colour (0xff23272e));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        const float radius = (float) juce::jmin (w, h) * 0.5f - 5.0f;
        const float cx = (float) x + (float) w * 0.5f;
        const float cy = (float) y + (float) h * 0.5f;
        const float angle = startAngle + pos * (endAngle - startAngle);
        const juce::PathStrokeType stroke (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track;
        track.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff2b2f37));
        g.strokePath (track, stroke);

        juce::Path value;
        value.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour (accent);
        g.strokePath (value, stroke);

        const float body = radius * 0.72f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a505a), cx, cy - body,
                                                 juce::Colour (0xff2a2e35), cx, cy + body, false));
        g.fillEllipse (cx - body, cy - body, body * 2.0f, body * 2.0f);

        juce::Path pointer;
        pointer.addRoundedRectangle (-1.5f, -body, 3.0f, body * 0.5f, 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (cx, cy));
    }

    juce::Colour accent;
};

class KnobPanelEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget
{
public:
    KnobPanelEditor (juce::AudioProcessor& owner, juce::AudioProcessorValueTreeState& stateToUse,
                     juce::String titleToShow, std::vector<Row> rowsToShow, juce::Colour accentColour,
                     EditorExtras extrasToUse = {})
        : AudioProcessorEditor (owner),
          state (stateToUse),
          title (std::move (titleToShow)),
          rows (std::move (rowsToShow)),
          lnf (accentColour),
          extras (std::move (extrasToUse))
    {
        setLookAndFeel (&lnf);
        if (extras.titleBar != nullptr)
            addAndMakeVisible (*extras.titleBar);
        if (extras.footer != nullptr)
            addAndMakeVisible (*extras.footer);

        int widest = 0;
        for (auto& row : rows)
        {
            int w = 0;
            for (auto& section : row)
            {
                for (auto& id : section.paramIDs)
                    addControl (id);
                w += sectionNaturalWidth (section) + gap;
            }
            widest = juce::jmax (widest, w - gap);
        }

        widest = juce::jmax (widest, extras.footerMinWidth);
        const int footerSpace = extras.footer != nullptr ? extras.footerHeight + gap : 0;
        const int height = headerHeight + (int) rows.size() * (rowHeight + gap) - gap + footerSpace + margin;
        setSize (widest + 2 * margin, height);
    }

    ~KnobPanelEditor() override
    {
        extras.titleBar.reset();
        extras.footer.reset();
        controls.clear();
        setLookAndFeel (nullptr);
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        return extras.canDropFiles != nullptr && extras.canDropFiles (files);
    }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        if (extras.filesDropped != nullptr)
            extras.filesDropped (files);
    }

    void paint (juce::Graphics& g) override
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1e2127), 0.0f, 0.0f,
                                                 juce::Colour (0xff0f1013), 0.0f, (float) getHeight(), false));
        g.fillAll();

        g.setColour (lnf.accent);
        g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
        g.drawText (title, margin, 8, getWidth() - 2 * margin, headerHeight - 14, juce::Justification::centredLeft);

        for (auto& box : sectionBoxes)
        {
            g.setColour (juce::Colour (0xff181b20));
            g.fillRoundedRectangle (box.bounds.toFloat(), 8.0f);
            g.setColour (lnf.accent.withAlpha (0.22f));
            g.drawRoundedRectangle (box.bounds.toFloat().reduced (0.5f), 8.0f, 1.0f);
            g.setColour (lnf.accent.withAlpha (0.9f));
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (box.title.toUpperCase(), box.bounds.withHeight (sectionTitleHeight).reduced (10, 0),
                        juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        sectionBoxes.clear();
        const int available = getWidth() - 2 * margin;
        int y = headerHeight;

        for (auto& row : rows)
        {
            int natural = 0;
            for (auto& section : row)
                natural += sectionNaturalWidth (section);
            natural += gap * ((int) row.size() - 1);
            const int extraPerSection = row.empty() ? 0 : (available - natural) / (int) row.size();

            int x = margin;
            for (auto& section : row)
            {
                const int w = sectionNaturalWidth (section) + extraPerSection;
                const juce::Rectangle<int> box (x, y, w, rowHeight);
                sectionBoxes.push_back ({ section.title, box });

                const int n = section.paramIDs.size();
                const float step = (float) (w - 2 * sectionPad) / (float) juce::jmax (1, n);
                for (int i = 0; i < n; ++i)
                {
                    auto* c = find (section.paramIDs[i]);
                    if (c == nullptr)
                        continue;

                    const int cx = x + sectionPad + juce::roundToInt (step * (float) i + (step - (float) knobWidth) * 0.5f);
                    const int cy = y + sectionTitleHeight;
                    c->label->setBounds (cx, cy, knobWidth, 14);
                    if (c->combo != nullptr)
                        c->combo->setBounds (cx + 2, cy + 34, knobWidth - 4, 24);
                    else
                        c->slider->setBounds (cx, cy + 14, knobWidth, knobHeight - 14);
                }
                x += w + gap;
            }
            y += rowHeight + gap;
        }

        if (extras.titleBar != nullptr)
            extras.titleBar->setBounds (getWidth() - margin - extras.titleBarWidth, 10, extras.titleBarWidth, headerHeight - 20);
        if (extras.footer != nullptr)
            extras.footer->setBounds (margin, y, available, extras.footerHeight);
    }

private:
    struct Control
    {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        // Attachments are declared last so they are destroyed before the widgets.
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
    };

    struct SectionBox
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    static constexpr int knobWidth = 78, knobHeight = 96, sectionPad = 6, sectionTitleHeight = 24;
    static constexpr int rowHeight = sectionTitleHeight + knobHeight + 6;
    static constexpr int margin = 14, gap = 10, headerHeight = 48;

    static int sectionNaturalWidth (const Section& s) { return s.paramIDs.size() * knobWidth + 2 * sectionPad; }

    Control* find (const juce::String& id)
    {
        auto it = controls.find (id);
        return it == controls.end() ? nullptr : it->second.get();
    }

    void addControl (const juce::String& id)
    {
        auto* param = state.getParameter (id);
        jassert (param != nullptr); // typo in a param ID?
        if (param == nullptr || controls.count (id) > 0)
            return;

        auto c = std::make_unique<Control>();
        c->label = std::make_unique<juce::Label> (id, param->getName (24));
        c->label->setJustificationType (juce::Justification::centred);
        c->label->setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (*c->label);

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
        {
            c->combo = std::make_unique<juce::ComboBox>();
            c->combo->addItemList (choice->choices, 1);
            addAndMakeVisible (*c->combo);
            c->comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id, *c->combo);
        }
        else
        {
            c->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            c->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobWidth, 18);
            addAndMakeVisible (*c->slider);
            c->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, *c->slider);
            c->slider->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        }

        controls.emplace (id, std::move (c));
    }

    juce::AudioProcessorValueTreeState& state;
    juce::String title;
    std::vector<Row> rows;
    SynthLookAndFeel lnf;
    std::map<juce::String, std::unique_ptr<Control>> controls;
    std::vector<SectionBox> sectionBoxes;
    EditorExtras extras;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobPanelEditor)
};
} // namespace sui
