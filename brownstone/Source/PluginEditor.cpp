#include "PluginEditor.h"

using namespace juce;

namespace
{
const Colour kPlate     { 0xff4a2c1d };   // chocolate faceplate
const Colour kPlateDark { 0xff331d12 };
const Colour kText      { 0xfff1e6cc };   // cream lettering
const Colour kTolex     { 0xff5a3826 };
const Colour kIvory     { 0xfff4ead2 };
const Colour kIvoryDark { 0xffbfae8a };
const Colour kPointer   { 0xff3a2416 };

Point<float> polar (Point<float> c, float r, float a) { return { c.x + r * std::sin (a), c.y - r * std::cos (a) }; }
} // namespace

//==============================================================================
AmpLookAndFeel::AmpLookAndFeel()
{
    setColour (BubbleComponent::backgroundColourId, kPlateDark);
    setColour (BubbleComponent::outlineColourId, kText.withAlpha (0.6f));
    setColour (TooltipWindow::textColourId, kText);
}

void AmpLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, Slider& s)
{
    const auto kind = s.getProperties()["kind"].toString();
    const bool selector = kind == "selector";
    const bool small = kind == "small";
    const auto labels = StringArray::fromTokens (s.getProperties()["labels"].toString(), "|", "");

    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = area.getCentre();
    const float r = jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const float rk = r * (selector ? 0.52f : (small ? 0.60f : 0.58f));
    const float angle = startAngle + pos * (endAngle - startAngle);

    // ---- scale printed on the faceplate
    g.setColour (kText);
    if (! labels.isEmpty())
    {
        g.setFont (FontOptions (selector ? 10.5f : 9.5f, Font::bold));
        for (int i = 0; i < labels.size(); ++i)
        {
            const float a = startAngle + (float) i / (float) jmax (1, labels.size() - 1) * (endAngle - startAngle);
            g.drawLine (Line<float> (polar (c, rk + 3, a), polar (c, rk + 8, a)), 1.4f);
            g.drawText (labels[i], Rectangle<float> (selector ? 70.0f : 40.0f, 14).withCentre (polar (c, rk + (selector ? 21.0f : 19.0f), a)),
                        Justification::centred);
        }
    }
    else
    {
        for (int v = 0; v <= 10; ++v)
        {
            const float a = startAngle + (float) v / 10.0f * (endAngle - startAngle);
            g.drawLine (Line<float> (polar (c, rk + 3, a), polar (c, rk + (v % 5 == 0 ? 9.0f : 6.5f), a)),
                        v % 5 == 0 ? 1.6f : 1.0f);
            if (! small && v > 0)
            {
                g.setFont (FontOptions (11.0f, Font::bold));
                g.drawText (String (v), Rectangle<float> (20, 14).withCentre (polar (c, rk + 17, a)), Justification::centred);
            }
        }
        if (small)
        {
            g.setFont (FontOptions (9.5f, Font::bold));
            const auto& range = s.getRange();
            auto label = [&] (double v, const String& t)
            {
                const float a = startAngle + (float) s.valueToProportionOfLength (v) * (endAngle - startAngle);
                g.drawText (t, Rectangle<float> (30, 12).withCentre (polar (c, rk + 17, a)), Justification::centred);
            };
            label (range.getStart(), String ((int) range.getStart()));
            label (0.0, "0");
            label (range.getEnd(), "+" + String ((int) range.getEnd()));
        }
    }

    if (selector)
    {
        // pointer knob: round body with a tapered nose
        Path p;
        const float body = rk * 0.80f, tip = rk * 1.18f, half = rk * 0.34f;
        p.addEllipse (-body, -body, body * 2.0f, body * 2.0f);
        p.startNewSubPath (-half, -body * 0.55f);
        p.lineTo (0, -tip);
        p.lineTo (half, -body * 0.55f);
        p.closeSubPath();
        p.applyTransform (AffineTransform::rotation (angle).translated (c));
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillPath (p, AffineTransform::translation (2.5f, 4.0f));
        g.setGradientFill (ColourGradient (kIvory, c.translated (-rk * 0.5f, -rk * 0.6f), kIvoryDark, c.translated (rk, rk), true));
        g.fillPath (p);
        g.setColour (kPointer);
        g.drawLine (Line<float> (polar (c, rk * 0.2f, angle), polar (c, tip - 4.0f, angle)), 2.4f);
        return;
    }

    // ---- shadow, skirt, cap
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (Rectangle<float> (rk * 2.0f, rk * 2.0f).withCentre (c.translated (2.5f, 4.0f)));

    const auto skirt = Rectangle<float> (rk * 2.0f, rk * 2.0f).withCentre (c);
    g.setGradientFill (ColourGradient (kIvory, skirt.getTopLeft(), kIvoryDark.darker (0.25f), skirt.getBottomRight(), false));
    g.fillEllipse (skirt);
    g.setColour (Colours::black.withAlpha (0.22f));
    for (int i = 0; i < 28; ++i)
    {
        const float a = angle + MathConstants<float>::twoPi * (float) i / 28.0f;
        g.drawLine (Line<float> (polar (c, rk * 0.80f, a), polar (c, rk - 0.5f, a)), 1.0f);
    }

    const auto cap = skirt.reduced (rk * 0.22f);
    g.setGradientFill (ColourGradient (Colours::white, cap.getCentre().translated (-rk * 0.3f, -rk * 0.4f),
                                       kIvoryDark, cap.getBottomRight(), true));
    g.fillEllipse (cap);
    g.setColour (Colours::black.withAlpha (0.12f));
    g.drawEllipse (cap.reduced (0.5f), 1.0f);

    g.setColour (kPointer);
    g.drawLine (Line<float> (polar (c, rk * 0.18f, angle), polar (c, rk - 2.0f, angle)), small ? 2.0f : 2.6f);
}

void AmpLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool, bool down)
{
    const auto bounds = b.getLocalBounds().toFloat();
    const auto c = bounds.getCentre();

    if (b.getProperties()["kind"].toString() == "footswitch")
    {
        const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        Path hex;
        for (int i = 0; i < 6; ++i)
        {
            const auto p = polar (c, r * 0.95f, MathConstants<float>::pi / 6.0f + (float) i * MathConstants<float>::pi / 3.0f);
            i == 0 ? hex.startNewSubPath (p) : hex.lineTo (p);
        }
        hex.closeSubPath();
        g.setGradientFill (ColourGradient (Colour (0xffbfbfbf), c.translated (-r, -r), Colour (0xff5c5c5c), c.translated (r, r), false));
        g.fillPath (hex);
        const float pr = r * (down ? 0.50f : 0.56f);
        auto cap = Rectangle<float> (pr * 2, pr * 2).withCentre (c.translated (0, down ? 1.5f : 0.0f));
        g.setGradientFill (ColourGradient (Colours::white, cap.getTopLeft(), Colour (0xff7d7d7d), cap.getBottomRight(), false));
        g.fillEllipse (cap);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawEllipse (cap, 1.0f);
        return;
    }

    // bat-handle toggle
    g.setGradientFill (ColourGradient (Colour (0xffd0d0d0), c.translated (-8, -8), Colour (0xff6a6a6a), c.translated (8, 8), false));
    g.fillEllipse (Rectangle<float> (18, 18).withCentre (c));
    const auto tip = c.translated (0, b.getToggleState() ? -20.0f : 20.0f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawLine (Line<float> (c.translated (2, 2), tip.translated (2, 2)), 6.0f);
    g.setGradientFill (ColourGradient (Colours::white, tip, Colour (0xff8a8a8a), c, false));
    g.drawLine (Line<float> (c, tip), 6.0f);
    g.fillEllipse (Rectangle<float> (9, 9).withCentre (tip));
}

//==============================================================================
FrontPanel::FrontPanel (BrownstoneProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);

    auto setupKnob = [this] (Slider& s, const String& kind, double def)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (MathConstants<float>::pi * -0.82f, MathConstants<float>::pi * 0.82f, true);
        s.setPopupDisplayEnabled (true, true, this);
        s.setDoubleClickReturnValue (true, def);
        s.setMouseDragSensitivity (220);
        s.getProperties().set ("kind", kind);
        addAndMakeVisible (s);
    };

    setupKnob (speed, "big", 4.5);
    setupKnob (division, "big", 3.0);
    setupKnob (intensity, "big", 6.0);
    setupKnob (crossover, "big", 5.0);
    setupKnob (output, "small", 0.0);
    setupKnob (mode, "selector", 0.0);

    division.getProperties().set ("labels", "1/1|1/2|1/4.|1/4|1/4T|1/8.|1/8|1/8T|1/16");
    division.setMouseDragSensitivity (160);
    mode.setRotaryParameters (MathConstants<float>::pi * -0.33f, MathConstants<float>::pi * 0.33f, true);
    mode.getProperties().set ("labels", "HARMONIC|BIAS|OPTO");
    mode.setPopupDisplayEnabled (false, false, nullptr);
    mode.setMouseDragSensitivity (90);
    mode.setPaintingIsUnclipped (true); // lets "HARMONIC" print past the knob's edge

    auto& s = proc.apvts;
    aSpeed     = std::make_unique<SA> (s, "speed", speed);
    aDivision  = std::make_unique<SA> (s, "division", division);
    aIntensity = std::make_unique<SA> (s, "intensity", intensity);
    aCrossover = std::make_unique<SA> (s, "crossover", crossover);
    aOutput    = std::make_unique<SA> (s, "output", output);
    aMode      = std::make_unique<SA> (s, "mode", mode);

    on.getProperties().set ("kind", "footswitch");
    for (auto* b : { &on, &sync, &stereo })
        addAndMakeVisible (*b);
    aOn     = std::make_unique<BA> (s, "on", on);
    aSync   = std::make_unique<BA> (s, "sync", sync);
    aStereo = std::make_unique<BA> (s, "stereo", stereo);

    speed.setTooltip ("Speed of the pulse");
    division.setTooltip ("Pulse rate locked to your song's tempo");
    intensity.setTooltip ("How deep the pulse goes");
    crossover.setTooltip ("Harmonic mode: where the guitar is split into lows and highs");
    mode.setTooltip ("HARMONIC = brown-era swirl, BIAS = tweed-style swell, OPTO = blackface-style throb");
    output.setTooltip ("Output level");
    sync.setTooltip ("Lock the speed to the song tempo");
    stereo.setTooltip ("Up = the pulse moves across left and right. Down = mono, like the amp");
    on.setTooltip ("Tremolo footswitch");

    lastSync = sync.getToggleState();
    speed.setVisible (! lastSync);
    division.setVisible (lastSync);

    tolex = Image (Image::RGB, W, H, false);
    {
        Random r (61);
        Image::BitmapData bd (tolex, Image::BitmapData::writeOnly);
        for (int yy = 0; yy < H; ++yy)
            for (int xx = 0; xx < W; ++xx)
            {
                const float grain = (r.nextFloat() - 0.5f) * 0.10f;
                const float pebble = std::sin ((float) xx * 0.9f + std::sin ((float) yy * 0.7f) * 2.0f)
                                     * std::sin ((float) yy * 0.85f + std::sin ((float) xx * 0.6f) * 2.0f) * 0.05f;
                bd.setPixelColour (xx, yy, kTolex.withMultipliedBrightness (1.0f + grain + pebble));
            }
    }

    setSize (W, H);
    startTimerHz (30);
}

FrontPanel::~FrontPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FrontPanel::resized()
{
    speed.setBounds (40, 190, 130, 130);
    division.setBounds (40, 190, 130, 130);
    intensity.setBounds (170, 190, 130, 130);
    crossover.setBounds (300, 190, 130, 130);
    mode.setBounds (430, 184, 124, 116);
    output.setBounds (552, 206, 92, 92);

    sync.setBounds (432, 344, 36, 48);
    stereo.setBounds (584, 344, 36, 48);
    on.setBounds (668, 252, 64, 64);
}

juce::Rectangle<int> FrontPanel::lampArea() const
{
    return { on.getBounds().getCentreX() - 40, 166, 80, 80 };
}

void FrontPanel::timerCallback()
{
    const bool s = sync.getToggleState();
    if (s != lastSync)
    {
        lastSync = s;
        speed.setVisible (! s);
        division.setVisible (s);
        repaint();
    }

    const float target = on.getToggleState() ? proc.getLampLevel() : 0.0f;
    if (std::abs (target - lamp) > 0.01f)
    {
        lamp = target;
        repaint (lampArea());
    }
}

void FrontPanel::paintGrille (Graphics& g, Rectangle<float> r)
{
    g.saveState();
    g.reduceClipRegion (r.toNearestInt());
    g.setColour (Colour (0xffc8b183));
    g.fillRect (r);
    for (float xx = r.getX(); xx < r.getRight(); xx += 3.0f)
    {
        g.setColour (Colour (0xff8f7449).withAlpha (((int) xx / 3) % 2 ? 0.45f : 0.25f));
        g.drawVerticalLine ((int) xx, r.getY(), r.getBottom());
    }
    for (float yy = r.getY(); yy < r.getBottom(); yy += 3.0f)
    {
        g.setColour (Colour (0xff5c4528).withAlpha (((int) yy / 3) % 2 ? 0.30f : 0.15f));
        g.drawHorizontalLine ((int) yy, r.getX(), r.getRight());
    }
    g.setGradientFill (ColourGradient (Colours::transparentBlack, r.getCentre(), Colours::black.withAlpha (0.38f), r.getTopLeft(), true));
    g.fillRect (r);
    g.restoreState();
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawRect (r, 2.0f);
}

void FrontPanel::paint (Graphics& g)
{
    g.drawImageAt (tolex, 0, 0);
    g.setColour (Colour (0xffe7dcc0).withAlpha (0.55f));
    g.drawRoundedRectangle (Rectangle<float> (8, 8, W - 16, H - 16), 10.0f, 2.0f);

    const auto grille = Rectangle<float> (26, 22, W - 52, 122);
    paintGrille (g, grille);

    // logo
    {
        const auto logo = Rectangle<float> (60, 46, 330, 74);
        g.setFont (FontOptions ("Georgia", 52.0f, Font::bold | Font::italic));
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawText ("Brownstone", logo.translated (2, 3), Justification::centredLeft);
        g.setGradientFill (ColourGradient (Colours::white, logo.getTopLeft(), Colour (0xff9a8f80), logo.getBottomLeft(), false));
        g.drawText ("Brownstone", logo, Justification::centredLeft);
        g.setFont (FontOptions (13.0f, Font::bold));
        g.setColour (Colour (0xff3a2416).withAlpha (0.85f));
        g.drawText ("'61  HARMONIC TREMOLO", Rectangle<float> (400, 72, 260, 30), Justification::centredLeft);
    }

    // chocolate faceplate
    const auto plate = Rectangle<float> (26, 160, W - 52, 236);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (plate.translated (2, 3), 6.0f);
    g.setGradientFill (ColourGradient (kPlate.brighter (0.15f), plate.getTopLeft(), kPlateDark, plate.getBottomLeft(), false));
    g.fillRoundedRectangle (plate, 6.0f);
    g.setColour (kText.withAlpha (0.45f));
    g.drawRoundedRectangle (plate.reduced (6.0f), 4.0f, 1.2f);

    for (auto pt : { Point<float> (40, 174), Point<float> (W - 40.0f, 174), Point<float> (40, 382), Point<float> (W - 40.0f, 382) })
    {
        g.setGradientFill (ColourGradient (Colours::white, pt.translated (-3, -3), Colour (0xff707070), pt.translated (3, 3), false));
        g.fillEllipse (Rectangle<float> (9, 9).withCentre (pt));
        g.setColour (Colour (0xff505050));
        g.drawLine (Line<float> (pt.translated (-3, 2), pt.translated (3, -2)), 1.2f);
    }

    g.setColour (kText);
    g.setFont (FontOptions (15.0f, Font::bold));
    auto label = [&] (const String& t, Rectangle<int> under, int dy = 4)
    {
        g.drawText (t, Rectangle<int> (under.getX() - 20, under.getBottom() + dy, under.getWidth() + 40, 20), Justification::centred);
    };
    label (lastSync ? "RATE" : "SPEED", speed.getBounds());
    label ("INTENSITY", intensity.getBounds());
    label ("CROSSOVER", crossover.getBounds());
    label ("MODE", mode.getBounds(), 2);
    g.setFont (FontOptions (12.5f, Font::bold));
    label ("OUTPUT", output.getBounds(), 6);

    // toggles
    g.setFont (FontOptions (12.5f, Font::bold));
    g.drawText ("TEMPO SYNC", Rectangle<int> (sync.getX() - 104, sync.getBounds().getCentreY() - 10, 100, 20), Justification::centredRight);
    g.drawText ("STEREO", Rectangle<int> (stereo.getX() - 74, stereo.getBounds().getCentreY() - 10, 70, 20), Justification::centredRight);
    g.setFont (FontOptions (10.0f, Font::bold));
    g.drawText ("ON", Rectangle<int> (sync.getRight() + 2, sync.getY(), 40, 14), Justification::centredLeft);
    g.drawText ("OFF", Rectangle<int> (sync.getRight() + 2, sync.getBottom() - 14, 40, 14), Justification::centredLeft);
    g.drawText ("WIDE", Rectangle<int> (stereo.getRight() + 2, stereo.getY(), 40, 14), Justification::centredLeft);
    g.drawText ("MONO", Rectangle<int> (stereo.getRight() + 2, stereo.getBottom() - 14, 40, 14), Justification::centredLeft);

    g.setFont (FontOptions (12.5f, Font::bold));
    g.drawText ("TREMOLO", Rectangle<int> (on.getX() - 14, on.getBottom() + 4, on.getWidth() + 28, 20), Justification::centred);

    // amber jewel that pulses with the tremolo
    {
        const auto c = Point<float> ((float) on.getBounds().getCentreX(), 206.0f);
        const float lv = jlimit (0.0f, 1.0f, lamp);
        if (lv > 0.02f)
        {
            g.setGradientFill (ColourGradient (Colour (0xffffa020).withAlpha (0.6f * lv), c, Colours::transparentBlack, c.translated (36, 0), true));
            g.fillEllipse (Rectangle<float> (72, 72).withCentre (c));
        }
        g.setGradientFill (ColourGradient (Colour (0xffe8e8e8), c.translated (-14, -14), Colour (0xff5a5a5a), c.translated (14, 14), false));
        g.fillEllipse (Rectangle<float> (34, 34).withCentre (c));
        const auto lens = Rectangle<float> (24, 24).withCentre (c);
        const auto hot = Colour (0xffffd08a).interpolatedWith (Colour (0xff6a3a0a), 1.0f - lv);
        const auto deep = Colour (0xffe07a00).interpolatedWith (Colour (0xff2a1404), 1.0f - lv);
        g.setGradientFill (ColourGradient (hot, lens.getCentre().translated (-4, -5), deep, lens.getBottomRight(), true));
        g.fillEllipse (lens);
        g.setColour (Colours::white.withAlpha (0.12f + 0.25f * lv));
        for (int i = 0; i < 6; ++i)
            g.drawLine (Line<float> (c, polar (c, 11.5f, (float) i * MathConstants<float>::pi / 3.0f)), 0.8f);
    }
}

//==============================================================================
BrownstoneEditor::BrownstoneEditor (BrownstoneProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (FrontPanel::W / 2, FrontPanel::H / 2, FrontPanel::W * 2, FrontPanel::H * 2);
    getConstrainer()->setFixedAspectRatio ((double) FrontPanel::W / (double) FrontPanel::H);
    setSize (FrontPanel::W, FrontPanel::H);
}

void BrownstoneEditor::resized()
{
    panel.setTransform (AffineTransform::scale ((float) getWidth() / (float) FrontPanel::W));
}
