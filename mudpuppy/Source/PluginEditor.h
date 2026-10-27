#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PedalLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getSliderPopupFont (juce::Slider&) override { return juce::FontOptions (13.0f, juce::Font::bold); }
    int getSliderPopupPlacement (juce::Slider&) override { return juce::BubbleComponent::above; }
};

class PedalPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 440, H = 640;

    explicit PedalPanel (MudpuppyProcessor&);
    ~PedalPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MudpuppyProcessor& proc;
    PedalLookAndFeel lnf;
    juce::Slider delay, feedback, blend, wobble, output;
    juce::ToggleButton on, trails;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> aDelay, aFeedback, aBlend, aWobble, aOutput;
    std::unique_ptr<BA> aOn, aTrails;

    juce::Image finish;
    bool ledOn = true;
    juce::TooltipWindow tips { this, 700 };
};

class MudpuppyEditor : public juce::AudioProcessorEditor
{
public:
    explicit MudpuppyEditor (MudpuppyProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff1c1f1a)); }

private:
    PedalPanel panel;
};
