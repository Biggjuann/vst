#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "DrumMachine.h"

namespace drum
{
/**
    The TR-style programming grid: one row per instrument plus an ACCENT row, 16 steps.
    Click a cell to toggle it (drag to paint), click an instrument name to hear it.
*/
class StepMatrix : public juce::Component, private juce::Timer
{
public:
    static constexpr int labelWidth = 120, headerHeight = 18, rowHeight = 19;

    StepMatrix (DrumMachineProcessor& p, juce::Colour accentColour) : proc (p), accent (accentColour) { startTimerHz (30); }

    int numRows() const { return (int) proc.voices.size() + 1; }
    int preferredHeight() const { return headerHeight + numRows() * rowHeight + 4; }

    void paint (juce::Graphics& g) override
    {
        const int pat = currentPattern();
        const int len = currentLength();
        const bool showPlayhead = proc.playingPattern.load() == pat;
        const int playing = proc.currentStep.load();

        for (int s = 0; s < numSteps; ++s)
        {
            auto h = cell (-1, s);
            const bool isPlaying = showPlayhead && playing == s;
            g.setColour (isPlaying ? accent : juce::Colours::white.withAlpha (s < len ? 0.3f : 0.1f));
            g.fillRoundedRectangle (h.toFloat().reduced (2.0f, 5.0f), 2.0f);
            g.setColour (isPlaying ? juce::Colours::black : juce::Colours::white.withAlpha (0.6f));
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawText (juce::String (s + 1), h, juce::Justification::centred);
        }

        for (int r = 0; r < numRows(); ++r)
        {
            const bool isAccentRow = r == numRows() - 1;
            const int bankRow = isAccentRow ? accentRow : r;
            auto label = juce::Rectangle<int> (0, rowTop (r), labelWidth - 8, rowHeight);
            g.setColour (isAccentRow ? accent : juce::Colours::white.withAlpha (hoverLabel == r ? 1.0f : 0.7f));
            g.setFont (juce::FontOptions (11.0f, isAccentRow ? juce::Font::bold : juce::Font::plain));
            g.drawText (isAccentRow ? juce::String ("ACCENT") : proc.voices[(size_t) r].name, label, juce::Justification::centredRight);

            for (int s = 0; s < numSteps; ++s)
            {
                const bool on = proc.patterns.get (pat, bankRow, s);
                const float alpha = s < len ? 1.0f : 0.3f;
                // Groups of four, like the colour-coded buttons on the hardware.
                const bool altGroup = (s / 4) % 2 == 1;
                auto c = cell (r, s).toFloat().reduced (2.0f, 2.0f);
                g.setColour ((on ? (isAccentRow ? accent.brighter (0.3f) : accent)
                                 : juce::Colour (altGroup ? 0xff2f343c : 0xff262a31)).withAlpha (alpha));
                g.fillRoundedRectangle (c, 3.0f);
                if (showPlayhead && playing == s)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.18f));
                    g.fillRoundedRectangle (c, 3.0f);
                }
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int r = e.x < labelWidth ? rowAt (e.y) : -1;
        if (r != hoverLabel) { hoverLabel = r; repaint(); }
        setMouseCursor (r >= 0 && r < (int) proc.voices.size() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override { hoverLabel = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int r = rowAt (e.y);
        if (r < 0)
            return;
        if (e.x < labelWidth)
        {
            if (r < (int) proc.voices.size())
                proc.audition (r);
            return;
        }
        const int s = stepAt (e.x);
        const int row = bankRow (r);
        paintValue = ! proc.patterns.get (currentPattern(), row, s);
        paintRow = r;
        proc.patterns.set (currentPattern(), row, s, paintValue);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (paintRow < 0 || e.x < labelWidth)
            return;
        const int r = rowAt (e.y);
        if (r < 0)
            return;
        const int s = stepAt (e.x);
        if (proc.patterns.get (currentPattern(), bankRow (r), s) != paintValue)
        {
            proc.patterns.set (currentPattern(), bankRow (r), s, paintValue);
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent&) override { paintRow = -1; }

private:
    int bankRow (int r) const { return r == numRows() - 1 ? accentRow : r; }
    int currentPattern() const { return (int) proc.param ("pattern"); }
    int currentLength() const { return (int) proc.param ("length") + 1; }
    int rowTop (int r) const { return r < 0 ? 0 : headerHeight + r * rowHeight; }

    int rowAt (int y) const
    {
        if (y < headerHeight) return -1;
        const int r = (y - headerHeight) / rowHeight;
        return r < numRows() ? r : -1;
    }

    int stepAt (int x) const
    {
        const float w = (float) (getWidth() - labelWidth) / (float) numSteps;
        return juce::jlimit (0, numSteps - 1, (int) ((float) (x - labelWidth) / w));
    }

    juce::Rectangle<int> cell (int r, int s) const
    {
        const float w = (float) (getWidth() - labelWidth) / (float) numSteps;
        const int x0 = labelWidth + juce::roundToInt (w * (float) s);
        const int x1 = labelWidth + juce::roundToInt (w * (float) (s + 1));
        return { x0, rowTop (r), x1 - x0, r < 0 ? headerHeight : rowHeight };
    }

    void timerCallback() override
    {
        const auto v = proc.patterns.version.load();
        const int step = proc.currentStep.load(), pat = currentPattern(), len = currentLength();
        if (v != lastVersion || step != lastStep || pat != lastPattern || len != lastLength)
        {
            lastVersion = v; lastStep = step; lastPattern = pat; lastLength = len;
            repaint();
        }
    }

    DrumMachineProcessor& proc;
    juce::Colour accent;
    int paintRow = -1, hoverLabel = -1;
    bool paintValue = true;
    uint32_t lastVersion = 0;
    int lastStep = -2, lastPattern = -1, lastLength = -1;
};

/** Transport/pattern bar + the step matrix. */
class SequencerSection : public juce::Component
{
public:
    static constexpr int barHeight = 44;
    static constexpr int minimumWidth = 960;

    SequencerSection (DrumMachineProcessor& p, juce::Colour accentColour)
        : proc (p), matrix (p, accentColour), colour (accentColour)
    {
        auto& state = proc.apvts;
        runButton.setClickingTogglesState (true);
        runButton.setTooltip ("Start the pattern sequencer. With Clock = Host it plays while Ableton's transport runs.");
        runAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "run", runButton);

        for (int i = 1; i <= numPatterns; ++i) patternBox.addItem ("Pattern " + juce::String (i), i);
        for (int i = 1; i <= numSteps; ++i)    lengthBox.addItem (juce::String (i) + " steps", i);
        clockBox.addItem ("Host sync", 1);
        clockBox.addItem ("Internal", 2);
        patternAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "pattern", patternBox);
        lengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "length", lengthBox);
        clockAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "clock", clockBox);

        tempoSlider.setSliderStyle (juce::Slider::LinearBar);
        tempoSlider.setTooltip ("Tempo for the Internal clock");
        tempoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, "tempo", tempoSlider);

        clearButton.onClick = [this] { for (int r = 0; r <= maxVoices; ++r) proc.patterns.setRow (pattern(), r, 0); };
        copyButton.onClick = [this] { for (int r = 0; r <= maxVoices; ++r) clipboard[(size_t) r] = proc.patterns.getRow (pattern(), r); hasClip = true; };
        pasteButton.onClick = [this] { if (hasClip) for (int r = 0; r <= maxVoices; ++r) proc.patterns.setRow (pattern(), r, clipboard[(size_t) r]); };

        for (auto* c : std::initializer_list<juce::Component*> { &runButton, &patternBox, &lengthBox, &clockBox, &tempoSlider,
                                                                 &clearButton, &copyButton, &pasteButton, &matrix })
            addAndMakeVisible (*c);
    }

    int preferredHeight() const { return barHeight + matrix.preferredHeight() + 10; }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff181b20));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
        g.setColour (colour.withAlpha (0.22f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (colour.withAlpha (0.9f));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("RHYTHM PATTERN", 12, 0, 140, barHeight - 4, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("click a name to hear it", getWidth() - 170, 0, 158, barHeight - 4, juce::Justification::centredRight);
    }

    void resized() override
    {
        auto top = getLocalBounds().reduced (10, 8).removeFromTop (26);
        top.removeFromLeft (140);
        auto place = [&top] (juce::Component& c, int w) { c.setBounds (top.removeFromLeft (w)); top.removeFromLeft (6); };
        place (runButton, 54);
        place (patternBox, 110);
        place (lengthBox, 92);
        place (clockBox, 96);
        place (tempoSlider, 96);
        place (clearButton, 54);
        place (copyButton, 50);
        place (pasteButton, 50);
        matrix.setBounds (getLocalBounds().reduced (10, 0).withTrimmedTop (barHeight).withTrimmedBottom (6));
    }

private:
    int pattern() const { return (int) proc.param ("pattern"); }

    DrumMachineProcessor& proc;
    StepMatrix matrix;
    juce::Colour colour;
    juce::TextButton runButton { "RUN" }, clearButton { "Clear" }, copyButton { "Copy" }, pasteButton { "Paste" };
    juce::ComboBox patternBox, lengthBox, clockBox;
    juce::Slider tempoSlider;
    std::array<uint32_t, maxVoices + 1> clipboard {};
    bool hasClip = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> runAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> patternAttachment, lengthAttachment, clockAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tempoAttachment;
};
} // namespace drum
