#include "PluginEditor.h"

using namespace juce;

namespace
{
const Colour kBody    { 0xff4f6248 };   // swampy olive hammertone
const Colour kInk     { 0xfff3ecd8 };   // cream silkscreen
const Colour kInkDim  { 0xffc9c0a6 };
const Colour kKnob    { 0xff17181a };

Point<float> polar (Point<float> c, float r, float a) { return { c.x + r * std::sin (a), c.y - r * std::cos (a) }; }
} // namespace

//==============================================================================
PedalLookAndFeel::PedalLookAndFeel()
{
    setColour (BubbleComponent::backgroundColourId, Colour (0xff20261d));
    setColour (BubbleComponent::outlineColourId, kInk.withAlpha (0.6f));
    setColour (TooltipWindow::textColourId, kInk);
}

void PedalLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, Slider& s)
{
    const bool small = s.getProperties()["kind"].toString() == "small";
    const bool isDelay = s.getProperties()["kind"].toString() == "delay";
    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = area.getCentre();
    const float rk = jmin (area.getWidth(), area.getHeight()) * 0.5f * (small ? 0.56f : 0.58f);
    const float angle = startAngle + pos * (endAngle - startAngle);

    // ---- printed scale
    g.setColour (kInk);
    if (isDelay)
    {
        g.setFont (FontOptions (10.0f, Font::bold));
        for (float ms : { 20.0f, 50.0f, 80.0f, 110.0f, 150.0f, 200.0f, 300.0f })
        {
            const float a = startAngle + (float) s.valueToProportionOfLength (ms) * (endAngle - startAngle);
            g.drawLine (Line<float> (polar (c, rk + 3, a), polar (c, rk + 8, a)), 1.4f);
            g.drawText (String ((int) ms), Rectangle<float> (30, 12).withCentre (polar (c, rk + 17, a)), Justification::centred);
        }
    }
    else
    {
        for (int v = 0; v <= 10; ++v)
        {
            const float a = startAngle + (float) v / 10.0f * (endAngle - startAngle);
            g.fillEllipse (Rectangle<float> (v % 5 == 0 ? 4.0f : 2.6f, v % 5 == 0 ? 4.0f : 2.6f).withCentre (polar (c, rk + 7, a)));
        }
        if (s.getProperties()["ends"].toString().isNotEmpty())
        {
            const auto ends = StringArray::fromTokens (s.getProperties()["ends"].toString(), "|", "");
            g.setFont (FontOptions (9.5f, Font::bold));
            g.drawText (ends[0], Rectangle<float> (34, 12).withCentre (polar (c, rk + 17, startAngle)), Justification::centred);
            g.drawText (ends[1], Rectangle<float> (34, 12).withCentre (polar (c, rk + 17, endAngle)), Justification::centred);
        }
    }

    // ---- knob: black knurled body, satin top, white indicator
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (Rectangle<float> (rk * 2.0f, rk * 2.0f).withCentre (c.translated (2.5f, 4.5f)));

    const auto body = Rectangle<float> (rk * 2.0f, rk * 2.0f).withCentre (c);
    g.setGradientFill (ColourGradient (Colour (0xff3a3c40), body.getTopLeft(), kKnob, body.getBottomRight(), false));
    g.fillEllipse (body);
    g.setColour (Colours::white.withAlpha (0.07f));
    for (int i = 0; i < 36; ++i)
    {
        const float a = angle + MathConstants<float>::twoPi * (float) i / 36.0f;
        g.drawLine (Line<float> (polar (c, rk * 0.78f, a), polar (c, rk - 0.8f, a)), 1.0f);
    }
    const auto top = body.reduced (rk * 0.2f);
    g.setGradientFill (ColourGradient (Colour (0xff4a4d52), top.getCentre().translated (-rk * 0.3f, -rk * 0.45f),
                                       Colour (0xff141517), top.getBottomRight(), true));
    g.fillEllipse (top);

    g.setColour (Colours::white);
    g.drawLine (Line<float> (polar (c, rk * 0.25f, angle), polar (c, rk - 2.5f, angle)), small ? 2.2f : 3.0f);
}

void PedalLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool, bool down)
{
    const auto bounds = b.getLocalBounds().toFloat();
    const auto c = bounds.getCentre();

    if (b.getProperties()["kind"].toString() == "footswitch")
    {
        const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        // hex nut + domed chrome plunger
        Path hex;
        for (int i = 0; i < 6; ++i)
        {
            const auto p = polar (c, r * 0.96f, (float) i * MathConstants<float>::pi / 3.0f);
            i == 0 ? hex.startNewSubPath (p) : hex.lineTo (p);
        }
        hex.closeSubPath();
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillPath (hex, AffineTransform::translation (2.5f, 4.0f));
        g.setGradientFill (ColourGradient (Colour (0xffd8d8d8), c.translated (-r, -r), Colour (0xff4f4f4f), c.translated (r, r), false));
        g.fillPath (hex);
        const float pr = r * (down ? 0.52f : 0.58f);
        auto cap = Rectangle<float> (pr * 2, pr * 2).withCentre (c.translated (0, down ? 1.5f : 0.0f));
        g.setGradientFill (ColourGradient (Colours::white, cap.getTopLeft(), Colour (0xff6d6d6d), cap.getBottomRight(), false));
        g.fillEllipse (cap);
        g.setColour (Colours::white.withAlpha (0.6f));
        g.drawEllipse (cap.reduced (pr * 0.3f), 1.0f);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawEllipse (cap, 1.0f);
        return;
    }

    // mini toggle
    g.setGradientFill (ColourGradient (Colour (0xffd0d0d0), c.translated (-7, -7), Colour (0xff666666), c.translated (7, 7), false));
    g.fillEllipse (Rectangle<float> (16, 16).withCentre (c));
    const auto tip = c.translated (0, b.getToggleState() ? -16.0f : 16.0f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawLine (Line<float> (c.translated (2, 2), tip.translated (2, 2)), 5.0f);
    g.setGradientFill (ColourGradient (Colours::white, tip, Colour (0xff8a8a8a), c, false));
    g.drawLine (Line<float> (c, tip), 5.0f);
    g.fillEllipse (Rectangle<float> (8, 8).withCentre (tip));
}

//==============================================================================
PedalPanel::PedalPanel (MudpuppyProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);

    auto setupKnob = [this] (Slider& s, const String& kind, double def, const String& ends = {})
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (MathConstants<float>::pi * -0.8f, MathConstants<float>::pi * 0.8f, true);
        s.setPopupDisplayEnabled (true, true, this);
        s.setDoubleClickReturnValue (true, def);
        s.setMouseDragSensitivity (220);
        s.getProperties().set ("kind", kind);
        if (ends.isNotEmpty()) s.getProperties().set ("ends", ends);
        s.setPaintingIsUnclipped (true);
        addAndMakeVisible (s);
    };

    setupKnob (delay, "delay", 110.0);
    setupKnob (feedback, "big", 2.5, "MIN|MAX");
    setupKnob (blend, "big", 5.0, "DRY|WET");
    setupKnob (wobble, "small", 2.0, "0|10");
    setupKnob (output, "small", 0.0, "-24|+12");

    auto& s = proc.apvts;
    aDelay    = std::make_unique<SA> (s, "delay", delay);
    aFeedback = std::make_unique<SA> (s, "feedback", feedback);
    aBlend    = std::make_unique<SA> (s, "blend", blend);
    aWobble   = std::make_unique<SA> (s, "wobble", wobble);
    aOutput   = std::make_unique<SA> (s, "output", output);

    on.getProperties().set ("kind", "footswitch");
    addAndMakeVisible (on);
    addAndMakeVisible (trails);
    aOn     = std::make_unique<BA> (s, "on", on);
    aTrails = std::make_unique<BA> (s, "trails", trails);

    delay.setTooltip ("Delay time, 20-300 ms. Slapback lives around 80-140 ms");
    feedback.setTooltip ("Number of repeats. Past about 8.5 it runs away into oscillation");
    blend.setTooltip ("How loud the echoes are against your dry guitar");
    wobble.setTooltip ("Clock wobble: a little shimmer in the repeats");
    output.setTooltip ("Output level");
    trails.setTooltip ("Up = repeats ring out after you switch the pedal off");
    on.setTooltip ("Footswitch");

    finish = Image (Image::RGB, W, H, false);
    {
        Random r (71);
        Image::BitmapData bd (finish, Image::BitmapData::writeOnly);
        for (int yy = 0; yy < H; ++yy)
            for (int xx = 0; xx < W; ++xx)
            {
                // hammertone: soft dimples + fine speckle
                const float dimple = std::sin ((float) xx * 0.23f + std::sin ((float) yy * 0.19f) * 2.6f)
                                     * std::sin ((float) yy * 0.21f + std::sin ((float) xx * 0.17f) * 2.6f);
                const float speck = (r.nextFloat() - 0.5f) * 0.07f;
                bd.setPixelColour (xx, yy, kBody.withMultipliedBrightness (1.0f + dimple * 0.09f + speck));
            }
    }

    setSize (W, H);
    startTimerHz (20);
}

PedalPanel::~PedalPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PedalPanel::resized()
{
    delay.setBounds (28, 70, 124, 124);
    feedback.setBounds (158, 70, 124, 124);
    blend.setBounds (288, 70, 124, 124);

    wobble.setBounds (58, 372, 84, 84);
    output.setBounds (298, 372, 84, 84);
    trails.setBounds (202, 384, 36, 48);

    on.setBounds (W / 2 - 42, 518, 84, 84);
}

void PedalPanel::timerCallback()
{
    if (on.getToggleState() != ledOn)
    {
        ledOn = on.getToggleState();
        repaint (W / 2 - 30, 470, 60, 40);
    }
}

void PedalPanel::paint (Graphics& g)
{
    const auto box = getLocalBounds().toFloat().reduced (10.0f);

    // enclosure
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (box.translated (2, 4), 22.0f);
    {
        Graphics::ScopedSaveState ss (g);
        Path clip;
        clip.addRoundedRectangle (box, 22.0f);
        g.reduceClipRegion (clip);
        g.drawImageAt (finish, 0, 0);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.10f), box.getTopLeft(),
                                           Colours::black.withAlpha (0.25f), box.getBottomRight(), false));
        g.fillRect (box);
    }
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (box.reduced (1.5f), 21.0f, 1.5f);

    for (auto pt : { Point<float> (34, 34), Point<float> (W - 34.0f, 34), Point<float> (34, H - 34.0f), Point<float> (W - 34.0f, H - 34.0f) })
    {
        g.setGradientFill (ColourGradient (Colours::white, pt.translated (-3, -3), Colour (0xff6a6a6a), pt.translated (3, 3), false));
        g.fillEllipse (Rectangle<float> (10, 10).withCentre (pt));
        g.setColour (Colour (0xff404040));
        g.drawLine (Line<float> (pt.translated (-3, 0), pt.translated (3, 0)), 1.2f);
        g.drawLine (Line<float> (pt.translated (0, -3), pt.translated (0, 3)), 1.2f);
    }

    // knob labels
    g.setColour (kInk);
    g.setFont (FontOptions (14.0f, Font::bold));
    auto under = [&] (const String& t, Rectangle<int> r, int dy)
    {
        g.drawText (t, Rectangle<int> (r.getX() - 20, r.getBottom() + dy, r.getWidth() + 40, 18), Justification::centred);
    };
    under ("DELAY", delay.getBounds(), 6);
    under ("FEEDBACK", feedback.getBounds(), 6);
    under ("BLEND", blend.getBounds(), 6);
    g.setFont (FontOptions (12.0f, Font::bold));
    under ("WOBBLE", wobble.getBounds(), 4);
    under ("OUTPUT", output.getBounds(), 4);
    g.setFont (FontOptions (10.0f, Font::bold));
    g.drawText ("TRAILS", Rectangle<int> (trails.getX() - 20, trails.getY() - 18, trails.getWidth() + 40, 14), Justification::centred);
    g.setColour (kInkDim);
    g.drawText ("ON", Rectangle<int> (trails.getRight() + 2, trails.getY(), 30, 12), Justification::centredLeft);
    g.drawText ("OFF", Rectangle<int> (trails.getRight() + 2, trails.getBottom() - 12, 30, 12), Justification::centredLeft);

    // logo with ripples
    {
        const float cy = 282.0f;
        g.setColour (kInk.withAlpha (0.55f));
        for (int i = 0; i < 3; ++i)
        {
            Path wave;
            const float yy = cy + 34.0f + (float) i * 7.0f;
            wave.startNewSubPath (70.0f, yy + 2.6f * std::sin ((70.0f + (float) i * 9.0f) * 0.09f));
            for (float xx = 74.0f; xx <= W - 70.0f; xx += 4.0f)
                wave.lineTo (xx, yy + 2.6f * std::sin ((xx + (float) i * 9.0f) * 0.09f));
            g.strokePath (wave, PathStrokeType (1.4f));
        }
        g.setFont (FontOptions ("Georgia", 58.0f, Font::bold | Font::italic));
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawText ("Mudpuppy", Rectangle<float> (0, cy - 40, (float) W, 64).translated (2, 3), Justification::centred);
        g.setColour (kInk);
        g.drawText ("Mudpuppy", Rectangle<float> (0, cy - 40, (float) W, 64), Justification::centred);
        g.setFont (FontOptions (12.0f, Font::bold));
        g.setColour (kInkDim);
        g.drawText ("A N A L O G   B B D   D E L A Y", Rectangle<float> (0, cy + 52, (float) W, 16), Justification::centred);
    }

    // status LED
    {
        const auto c = Point<float> ((float) W / 2.0f, 490.0f);
        if (ledOn)
        {
            g.setGradientFill (ColourGradient (Colour (0xffff2a10).withAlpha (0.55f), c, Colours::transparentBlack, c.translated (20, 0), true));
            g.fillEllipse (Rectangle<float> (40, 40).withCentre (c));
        }
        g.setColour (Colour (0xff2b2b2b));
        g.fillEllipse (Rectangle<float> (16, 16).withCentre (c));
        g.setGradientFill (ColourGradient (ledOn ? Colour (0xffffb0a0) : Colour (0xff5a1a12), c.translated (-2, -2),
                                           ledOn ? Colour (0xffd81808) : Colour (0xff2a0805), c.translated (4, 4), true));
        g.fillEllipse (Rectangle<float> (11, 11).withCentre (c));
    }
}

//==============================================================================
MudpuppyEditor::MudpuppyEditor (MudpuppyProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (PedalPanel::W / 2, PedalPanel::H / 2, PedalPanel::W * 2, PedalPanel::H * 2);
    getConstrainer()->setFixedAspectRatio ((double) PedalPanel::W / (double) PedalPanel::H);
    setSize (PedalPanel::W, PedalPanel::H);
}

void MudpuppyEditor::resized()
{
    panel.setTransform (AffineTransform::scale ((float) getWidth() / (float) PedalPanel::W));
}
