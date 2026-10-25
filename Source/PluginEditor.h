#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
class TankLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TankLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;

    juce::Font getPopupFont() { return juce::FontOptions (13.0f, juce::Font::bold); }
    juce::Font getSliderPopupFont (juce::Slider&) override { return getPopupFont(); }
    int getSliderPopupPlacement (juce::Slider&) override { return juce::BubbleComponent::above; }

private:
    void drawFootswitch (juce::Graphics&, juce::Rectangle<float>, bool on, bool down);
    void drawBatToggle (juce::Graphics&, juce::Rectangle<float>, bool on);
};

//==============================================================================
/** Everything is laid out at 780 x 420 and scaled as one piece when the window is resized. */
class FrontPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 780, H = 420;

    explicit FrontPanel (DriptankProcessor&);
    ~FrontPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void startShake();
    void paintGrille (juce::Graphics&, juce::Rectangle<float>);

    DriptankProcessor& proc;
    TankLookAndFeel lnf;

    juce::Slider dwell, mix, tone, tank, output;
    juce::ToggleButton reverbOn, stereo;
    juce::TextButton kickButton { "KICK" };

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> aDwell, aMix, aTone, aTank, aOutput;
    std::unique_ptr<BA> aOn, aStereo;

    juce::Image tolex;
    float shake = 0.0f;
    int shakeTicks = 0;
    juce::Random rng;
    juce::TooltipWindow tips { this, 700 };
};

//==============================================================================
class DriptankEditor : public juce::AudioProcessorEditor
{
public:
    explicit DriptankEditor (DriptankProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff24150d)); }

private:
    FrontPanel panel;
};
