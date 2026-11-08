#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

/**
    Squelch's 16-step grid.

    Rows (one column per step):
      NOTE   click for the 13-key menu, or drag / mouse-wheel up and down
      DOWN   octave down          UP     octave up
      ACCENT                      SLIDE  glide into the next step
      TIME   click to cycle  Note -> Tie -> Rest
*/
class StepGrid : public juce::Component, private juce::Timer
{
public:
    StepGrid (SquelchAudioProcessor& proc, juce::Colour accentColour) : processor (proc), accent (accentColour)
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const int pattern = currentPattern();
        const int length = currentLength();
        const int playing = processor.currentStep.load();
        const bool showPlayhead = processor.playingPattern.load() == pattern;

        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        for (int r = 0; r < numRows; ++r)
        {
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.drawText (rowNames[r], rowBounds (r).withWidth (labelWidth - 6), juce::Justification::centredRight);
        }

        for (int i = 0; i < seq::numSteps; ++i)
        {
            const auto s = processor.patterns.get (pattern, i);
            const bool inRange = i < length;
            const float alpha = inRange ? 1.0f : 0.3f;

            // Step number + playhead
            auto header = cell (-1, i);
            const bool isPlaying = showPlayhead && playing == i;
            g.setColour (isPlaying ? accent : juce::Colours::white.withAlpha (0.35f * alpha));
            g.fillRoundedRectangle (header.toFloat().reduced (3.0f, 6.0f), 2.0f);
            g.setColour (isPlaying ? juce::Colours::black : juce::Colours::white.withAlpha (0.6f * alpha));
            g.drawText (juce::String (i + 1), header, juce::Justification::centred);

            // Note
            auto noteCell = cell (0, i).reduced (2);
            g.setColour (juce::Colour (0xff23272e).withAlpha (alpha));
            g.fillRoundedRectangle (noteCell.toFloat(), 4.0f);
            if (s.time == seq::Note)
            {
                g.setColour (juce::Colours::white.withAlpha (alpha));
                g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
                g.drawText (seq::keyName (s.key), noteCell, juce::Justification::centred);
                g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            }
            else
            {
                g.setColour (juce::Colours::white.withAlpha (0.35f * alpha));
                g.drawText (s.time == seq::Tie ? "tie" : "rest", noteCell, juce::Justification::centred);
            }

            led (g, cell (1, i), s.octave < 0, alpha);
            led (g, cell (2, i), s.octave > 0, alpha);
            led (g, cell (3, i), s.accent, alpha);
            led (g, cell (4, i), s.slide, alpha);

            // Time
            auto timeCell = cell (5, i).reduced (3, 2);
            const juce::Colour timeColour = s.time == seq::Note ? accent : s.time == seq::Tie ? juce::Colour (0xff6fa8ff) : juce::Colour (0xff3a3f47);
            g.setColour (timeColour.withAlpha (alpha));
            g.fillRoundedRectangle (timeCell.toFloat(), 3.0f);
            g.setColour ((s.time == seq::Rest ? juce::Colours::white : juce::Colours::black).withAlpha (0.8f * alpha));
            g.drawText (s.time == seq::Note ? "N" : s.time == seq::Tie ? "T" : "R", timeCell, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto [row, step] = cellAt (e.getPosition());
        if (step < 0)
            return;
        dragStartY = e.y;
        dragStartKey = processor.patterns.get (currentPattern(), step).key;
        dragStep = (row == 0) ? step : -1;
        dragged = false;

        auto s = processor.patterns.get (currentPattern(), step);
        switch (row)
        {
            case 1: s.octave = s.octave < 0 ? 0 : -1; break;
            case 2: s.octave = s.octave > 0 ? 0 : 1; break;
            case 3: s.accent = ! s.accent; break;
            case 4: s.slide = ! s.slide; break;
            case 5: s.time = (seq::Time) (((int) s.time + 1) % 3); break;
            default: return;
        }
        write (step, s);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragStep < 0)
            return;
        const int delta = (dragStartY - e.y) / 8;
        if (delta != 0)
            dragged = true;
        auto s = processor.patterns.get (currentPattern(), dragStep);
        const int key = juce::jlimit (0, 12, dragStartKey + delta);
        if (key != s.key || s.time != seq::Note)
        {
            s.key = key;
            s.time = seq::Note;
            write (dragStep, s);
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragStep >= 0 && ! dragged)
            showNoteMenu (dragStep);
        dragStep = -1;
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        const auto [row, step] = cellAt (e.getPosition());
        if (row != 0 || step < 0 || std::abs (wheel.deltaY) < 1.0e-6f)
            return;
        auto s = processor.patterns.get (currentPattern(), step);
        s.key = juce::jlimit (0, 12, s.key + (wheel.deltaY > 0 ? 1 : -1));
        s.time = seq::Note;
        write (step, s);
    }

    static constexpr int labelWidth = 62;
    static constexpr int preferredHeight = 20 + 40 + 5 * 24;

private:
    static constexpr int numRows = 6;
    static constexpr const char* rowNames[numRows] = { "NOTE", "DOWN", "UP", "ACCENT", "SLIDE", "TIME" };

    int currentPattern() const { return (int) processor.apvts.getRawParameterValue ("pattern")->load(); }
    int currentLength() const { return (int) processor.apvts.getRawParameterValue ("length")->load() + 1; }

    int rowTop (int row) const { return row < 0 ? 0 : row == 0 ? 20 : 60 + (row - 1) * 24; }
    int rowHeight (int row) const { return row < 0 ? 20 : row == 0 ? 40 : 24; }
    juce::Rectangle<int> rowBounds (int row) const { return { 0, rowTop (row), getWidth(), rowHeight (row) }; }

    juce::Rectangle<int> cell (int row, int step) const
    {
        const float w = (float) (getWidth() - labelWidth) / (float) seq::numSteps;
        const int x0 = labelWidth + juce::roundToInt (w * (float) step);
        const int x1 = labelWidth + juce::roundToInt (w * (float) (step + 1));
        return { x0, rowTop (row), x1 - x0, rowHeight (row) };
    }

    std::pair<int, int> cellAt (juce::Point<int> p) const
    {
        if (p.x < labelWidth)
            return { -1, -1 };
        const int step = juce::jlimit (0, seq::numSteps - 1,
                                       (int) ((float) (p.x - labelWidth) / ((float) (getWidth() - labelWidth) / seq::numSteps)));
        for (int r = 0; r < numRows; ++r)
            if (rowBounds (r).contains (p))
                return { r, step };
        return { -1, -1 };
    }

    void led (juce::Graphics& g, juce::Rectangle<int> area, bool on, float alpha) const
    {
        auto r = area.toFloat().withSizeKeepingCentre (juce::jmin (26.0f, area.getWidth() - 6.0f), 12.0f);
        g.setColour ((on ? accent : juce::Colour (0xff2b2f37)).withAlpha (alpha));
        g.fillRoundedRectangle (r, 3.0f);
        if (on)
        {
            g.setColour (accent.withAlpha (0.25f * alpha));
            g.drawRoundedRectangle (r.expanded (2.0f), 4.0f, 2.0f);
        }
    }

    void showNoteMenu (int step)
    {
        juce::PopupMenu menu;
        const auto s = processor.patterns.get (currentPattern(), step);
        for (int k = 12; k >= 0; --k)
            menu.addItem (k + 1, seq::keyName (k), true, s.time == seq::Note && s.key == k);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (cell (0, step))),
                            [this, step] (int result)
                            {
                                if (result <= 0)
                                    return;
                                auto st = processor.patterns.get (currentPattern(), step);
                                st.key = result - 1;
                                st.time = seq::Note;
                                write (step, st);
                            });
    }

    void write (int step, const seq::Step& s)
    {
        processor.patterns.set (currentPattern(), step, s);
        repaint();
    }

    void timerCallback() override
    {
        const auto v = processor.patterns.version.load();
        const int step = processor.currentStep.load();
        const int pat = currentPattern();
        const int len = currentLength();
        if (v != lastVersion || step != lastStep || pat != lastPattern || len != lastLength)
        {
            lastVersion = v; lastStep = step; lastPattern = pat; lastLength = len;
            repaint();
        }
    }

    SquelchAudioProcessor& processor;
    juce::Colour accent;
    int dragStep = -1, dragStartY = 0, dragStartKey = 0;
    bool dragged = false;
    uint32_t lastVersion = 0;
    int lastStep = -2, lastPattern = -1, lastLength = -1;
};

/** Transport/pattern controls + the step grid. */
class SequencerPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int preferredHeight = 44 + StepGrid::preferredHeight + 14;
    static constexpr int minimumWidth = 1040;

    explicit SequencerPanel (SquelchAudioProcessor& proc)
        : processor (proc), grid (proc, juce::Colour (0xffffa62b))
    {
        auto& state = processor.apvts;

        runButton.setClickingTogglesState (true);
        runButton.setTooltip ("Turn the step sequencer on. It plays when Ableton's transport is running "
                              "(or at the Tempo setting when there's no host clock). MIDI keys transpose the pattern; C3 = no transpose.");
        runAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "seqon", runButton);

        for (int i = 1; i <= seq::numPatterns; ++i) patternBox.addItem ("Pattern " + juce::String (i), i);
        for (int i = 1; i <= seq::numSteps; ++i)    lengthBox.addItem (juce::String (i) + " steps", i);
        patternAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "pattern", patternBox);
        lengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "length", lengthBox);

        clockBox.addItem ("Host sync", 1);
        clockBox.addItem ("Internal", 2);
        clockBox.setTooltip ("Host sync: plays in time with Ableton while its transport runs.\n"
                             "Internal: RUN starts/stops the sequencer at its own Tempo, even with Ableton stopped.");
        clockAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "clock", clockBox);

        tempoSlider.setSliderStyle (juce::Slider::LinearBar);
        tempoSlider.setTooltip ("Tempo for the Internal clock (also used when the host has no clock)");
        tempoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, "tempo", tempoSlider);

        randomButton.onClick = [this] { processor.patterns.setPattern (pattern(), seq::randomPattern ((uint32_t) juce::Random::getSystemRandom().nextInt())); };
        clearButton.onClick = [this]
        {
            std::array<seq::Step, seq::numSteps> blank {};
            processor.patterns.setPattern (pattern(), blank);
        };
        shiftLeft.onClick = [this] { processor.patterns.shift (pattern(), -1); };
        shiftRight.onClick = [this] { processor.patterns.shift (pattern(), +1); };
        copyButton.onClick = [this]
        {
            for (int i = 0; i < seq::numSteps; ++i)
                clipboard[(size_t) i] = processor.patterns.get (pattern(), i);
            hasClipboard = true;
        };
        pasteButton.onClick = [this] { if (hasClipboard) processor.patterns.setPattern (pattern(), clipboard); };

        for (auto* c : std::initializer_list<juce::Component*> { &runButton, &patternBox, &lengthBox, &clockBox, &tempoSlider, &randomButton, &clearButton,
                                                                 &shiftLeft, &shiftRight, &copyButton, &pasteButton, &grid })
            addAndMakeVisible (*c);

        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff181b20));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
        g.setColour (juce::Colour (0xffffa62b).withAlpha (0.22f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);

        g.setColour (juce::Colour (0xffffa62b).withAlpha (0.9f));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("SEQUENCER", 12, 0, 100, 40, juce::Justification::centredLeft);

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (juce::FontOptions (12.0f));
        const int t = processor.transpose.load();
        g.drawText ("Transpose " + juce::String (t > 0 ? "+" : "") + juce::String (t),
                    getWidth() - 120, 0, 108, 40, juce::Justification::centredRight);
    }

    void resized() override
    {
        auto top = getLocalBounds().reduced (10, 8).removeFromTop (26);
        top.removeFromLeft (92);
        auto place = [&top] (juce::Component& c, int w) { c.setBounds (top.removeFromLeft (w)); top.removeFromLeft (6); };
        place (runButton, 54);
        place (patternBox, 104);
        place (lengthBox, 92);
        place (clockBox, 96);
        place (tempoSlider, 96);
        place (randomButton, 66);
        place (clearButton, 54);
        place (shiftLeft, 28);
        place (shiftRight, 28);
        place (copyButton, 50);
        place (pasteButton, 50);

        grid.setBounds (getLocalBounds().reduced (10, 0).withTrimmedTop (44).withTrimmedBottom (14));
    }

private:
    int pattern() const { return (int) processor.apvts.getRawParameterValue ("pattern")->load(); }

    void timerCallback() override
    {
        const int t = processor.transpose.load();
        if (t != lastTranspose)
        {
            lastTranspose = t;
            repaint();
        }
    }

    SquelchAudioProcessor& processor;
    StepGrid grid;

    juce::TextButton runButton { "RUN" }, randomButton { "Random" }, clearButton { "Clear" };
    juce::TextButton shiftLeft { "<" }, shiftRight { ">" }, copyButton { "Copy" }, pasteButton { "Paste" };
    juce::ComboBox patternBox, lengthBox, clockBox;
    juce::Slider tempoSlider;

    std::array<seq::Step, seq::numSteps> clipboard {};
    bool hasClipboard = false;
    int lastTranspose = 0;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> runAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> patternAttachment, lengthAttachment, clockAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tempoAttachment;
};
