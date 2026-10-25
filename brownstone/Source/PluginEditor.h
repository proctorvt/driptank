#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class AmpLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AmpLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getSliderPopupFont (juce::Slider&) override { return juce::FontOptions (13.0f, juce::Font::bold); }
    int getSliderPopupPlacement (juce::Slider&) override { return juce::BubbleComponent::above; }
};

class FrontPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 780, H = 420;

    explicit FrontPanel (BrownstoneProcessor&);
    ~FrontPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintGrille (juce::Graphics&, juce::Rectangle<float>);
    juce::Rectangle<int> lampArea() const;

    BrownstoneProcessor& proc;
    AmpLookAndFeel lnf;

    juce::Slider speed, division, intensity, crossover, mode, output;
    juce::ToggleButton on, sync, stereo;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> aSpeed, aDivision, aIntensity, aCrossover, aMode, aOutput;
    std::unique_ptr<BA> aOn, aSync, aStereo;

    juce::Image tolex;
    float lamp = 0.0f;
    bool lastSync = false;
    juce::TooltipWindow tips { this, 700 };
};

class BrownstoneEditor : public juce::AudioProcessorEditor
{
public:
    explicit BrownstoneEditor (BrownstoneProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff24150d)); }

private:
    FrontPanel panel;
};
