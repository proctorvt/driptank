#include "PluginEditor.h"

using namespace juce;

namespace
{
const Colour kCream     { 0xffeee3c8 };
const Colour kCreamDark { 0xffd6c8a6 };
const Colour kInk       { 0xff3a2416 };
const Colour kTolex     { 0xff4a2c1c };
const Colour kKnobLight { 0xff6e4630 };
const Colour kKnobDark  { 0xff24140c };
const Colour kChrome    { 0xffdedede };

Point<float> polar (Point<float> c, float r, float angle)
{
    return { c.x + r * std::sin (angle), c.y - r * std::cos (angle) };
}
} // namespace

//==============================================================================
TankLookAndFeel::TankLookAndFeel()
{
    setColour (BubbleComponent::backgroundColourId, kInk);
    setColour (BubbleComponent::outlineColourId, kCream.withAlpha (0.6f));
    setColour (TooltipWindow::textColourId, kCream);
}

void TankLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, Slider& s)
{
    const auto kind = s.getProperties()["kind"].toString();
    const bool selector = kind == "selector";
    const bool small = kind == "small";

    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = area.getCentre();
    const float r = jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const float rk = r * (selector ? 0.52f : (small ? 0.60f : 0.58f));
    const float angle = startAngle + pos * (endAngle - startAngle);

    // ---- scale printed on the faceplate
    g.setColour (kInk);
    if (selector)
    {
        const auto labels = StringArray::fromTokens (s.getProperties()["labels"].toString(), "|", "");
        g.setFont (FontOptions (10.5f, Font::bold));
        for (int i = 0; i < labels.size(); ++i)
        {
            const float a = startAngle + (float) i / (float) jmax (1, labels.size() - 1) * (endAngle - startAngle);
            g.drawLine (Line<float> (polar (c, rk + 3, a), polar (c, rk + 9, a)), 1.6f);
            const auto p = polar (c, rk + 21, a);
            g.drawText (labels[i], Rectangle<float> (40, 14).withCentre (p), Justification::centred);
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
                g.drawText (String (v), Rectangle<float> (20, 14).withCentre (polar (c, rk + 17, a)),
                            Justification::centred);
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
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillPath (p, AffineTransform::translation (2.5f, 4.0f));
        g.setGradientFill (ColourGradient (Colour (0xff6e4630), c.translated (-rk * 0.5f, -rk * 0.6f), Colour (0xff1c100a),
                                           c.translated (rk, rk), true));
        g.fillPath (p);
        g.setColour (kCream);
        g.drawLine (Line<float> (polar (c, rk * 0.2f, angle), polar (c, tip - 4.0f, angle)), 2.4f);
        return;
    }

    // ---- drop shadow
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (Rectangle<float> (rk * 2.0f, rk * 2.0f).withCentre (c.translated (2.5f, 4.0f)));

    // ---- knurled skirt
    const auto skirt = Rectangle<float> (rk * 2.0f, rk * 2.0f).withCentre (c);
    g.setGradientFill (ColourGradient (kKnobLight, skirt.getTopLeft(), kKnobDark, skirt.getBottomRight(), false));
    g.fillEllipse (skirt);

    g.setColour (Colours::black.withAlpha (0.45f));
    for (int i = 0; i < 28; ++i)
    {
        const float a = angle + MathConstants<float>::twoPi * (float) i / 28.0f;
        g.drawLine (Line<float> (polar (c, rk * 0.80f, a), polar (c, rk - 0.5f, a)), 1.0f);
    }

    // ---- cap + pointer line
    const auto cap = skirt.reduced (rk * 0.22f);
    g.setGradientFill (ColourGradient (Colour (0xff7a5038), cap.getCentre().translated (-rk * 0.3f, -rk * 0.4f),
                                       Colour (0xff2c1a10), cap.getBottomRight(), true));
    g.fillEllipse (cap);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawEllipse (cap.reduced (1.0f), 1.0f);
    g.setColour (kCream);
    g.drawLine (Line<float> (polar (c, rk * 0.18f, angle), polar (c, rk - 2.0f, angle)), small ? 2.0f : 2.6f);
}

void TankLookAndFeel::drawFootswitch (Graphics& g, Rectangle<float> b, bool on, bool down)
{
    const auto c = b.getCentre();
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;

    // hex nut
    Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const auto p = polar (c, r * 0.95f, MathConstants<float>::pi / 6.0f + (float) i * MathConstants<float>::pi / 3.0f);
        i == 0 ? hex.startNewSubPath (p) : hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setGradientFill (ColourGradient (Colour (0xffbfbfbf), c.translated (-r, -r), Colour (0xff5c5c5c), c.translated (r, r), false));
    g.fillPath (hex);

    // plunger
    const float pr = r * (down ? 0.50f : 0.56f);
    auto cap = Rectangle<float> (pr * 2, pr * 2).withCentre (c.translated (0, down ? 1.5f : 0.0f));
    g.setGradientFill (ColourGradient (Colours::white, cap.getTopLeft(), Colour (0xff7d7d7d), cap.getBottomRight(), false));
    g.fillEllipse (cap);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (cap, 1.0f);
    ignoreUnused (on);
}

void TankLookAndFeel::drawBatToggle (Graphics& g, Rectangle<float> b, bool on)
{
    const auto c = b.getCentre();
    // mounting nut
    g.setGradientFill (ColourGradient (Colour (0xffd0d0d0), c.translated (-8, -8), Colour (0xff6a6a6a), c.translated (8, 8), false));
    g.fillEllipse (Rectangle<float> (18, 18).withCentre (c));
    // bat handle
    const float len = 20.0f;
    const auto tip = c.translated (0, on ? -len : len);
    g.setColour (Colours::black.withAlpha (0.3f));
    g.drawLine (Line<float> (c.translated (2, 2), tip.translated (2, 2)), 6.0f);
    g.setGradientFill (ColourGradient (Colours::white, tip, Colour (0xff8a8a8a), c, false));
    g.drawLine (Line<float> (c, tip), 6.0f);
    g.fillEllipse (Rectangle<float> (9, 9).withCentre (tip));
}

void TankLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool, bool down)
{
    const auto kind = b.getProperties()["kind"].toString();
    if (kind == "footswitch")
        drawFootswitch (g, b.getLocalBounds().toFloat(), b.getToggleState(), down);
    else
        drawBatToggle (g, b.getLocalBounds().toFloat(), b.getToggleState());
}

void TankLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    // rubber "kick" pad
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    if (down) r = r.translated (0, 1.5f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (1.5f, 2.5f), 7.0f);
    g.setGradientFill (ColourGradient (highlighted ? Colour (0xff4a4038) : Colour (0xff3a322c), r.getTopLeft(),
                                       Colour (0xff141010), r.getBottomLeft(), false));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (Colours::white.withAlpha (0.08f));
    for (float yy = r.getY() + 5; yy < r.getBottom() - 3; yy += 4)
        g.drawHorizontalLine ((int) yy, r.getX() + 6, r.getRight() - 6);
}

void TankLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool down)
{
    g.setColour (kCream);
    g.setFont (FontOptions (14.0f, Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds().translated (0, down ? 1 : 0), Justification::centred);
}

//==============================================================================
FrontPanel::FrontPanel (DriptankProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);

    auto setupKnob = [this] (Slider& s, const String& kind, double defaultValue)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (MathConstants<float>::pi * -0.82f, MathConstants<float>::pi * 0.82f, true);
        s.setPopupDisplayEnabled (true, true, this);
        s.setDoubleClickReturnValue (true, defaultValue);
        s.setMouseDragSensitivity (220);
        s.getProperties().set ("kind", kind);
        addAndMakeVisible (s);
    };

    setupKnob (dwell, "big", 6.0);
    setupKnob (mix, "big", 4.0);
    setupKnob (tone, "big", 6.0);
    setupKnob (output, "small", 0.0);
    setupKnob (tank, "selector", 2.0);
    tank.setRotaryParameters (MathConstants<float>::pi * -0.33f, MathConstants<float>::pi * 0.33f, true);
    tank.getProperties().set ("labels", "SHORT|MED|LONG");
    tank.setPopupDisplayEnabled (false, false, nullptr);
    tank.setMouseDragSensitivity (90);

    auto& s = proc.apvts;
    aDwell  = std::make_unique<SA> (s, "dwell", dwell);
    aMix    = std::make_unique<SA> (s, "mix", mix);
    aTone   = std::make_unique<SA> (s, "tone", tone);
    aOutput = std::make_unique<SA> (s, "output", output);
    aTank   = std::make_unique<SA> (s, "tank", tank);

    reverbOn.getProperties().set ("kind", "footswitch");
    stereo.getProperties().set ("kind", "bat");
    addAndMakeVisible (reverbOn);
    addAndMakeVisible (stereo);
    aOn     = std::make_unique<BA> (s, "reverb", reverbOn);
    aStereo = std::make_unique<BA> (s, "stereo", stereo);
    reverbOn.onStateChange = [this] { repaint(); };

    dwell.setTooltip ("Dwell: how hard the driver tube hits the springs");
    mix.setTooltip ("Mixer: dry guitar vs. reverb");
    tone.setTooltip ("Tone: darkens or brightens the reverb");
    tank.setTooltip ("Tank: short, medium or long-decay springs");
    output.setTooltip ("Output level");
    stereo.setTooltip ("Up = the two springs spread left/right. Down = mono, like the original");
    reverbOn.setTooltip ("Reverb footswitch");
    kickButton.setTooltip ("Give the tank a kick");

    kickButton.onClick = [this]
    {
        proc.kick();
        startShake();
    };
    addAndMakeVisible (kickButton);

    // procedural tolex texture
    tolex = Image (Image::RGB, W, H, false);
    {
        Random r (63);
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
}

FrontPanel::~FrontPanel()
{
    setLookAndFeel (nullptr);
}

void FrontPanel::resized()
{
    // faceplate row (y = 168 .. 400)
    dwell.setBounds (40, 190, 130, 130);
    mix.setBounds (170, 190, 130, 130);
    tone.setBounds (300, 190, 130, 130);
    tank.setBounds (430, 184, 124, 116);
    output.setBounds (552, 206, 92, 92);

    stereo.setBounds (470, 334, 36, 48);
    reverbOn.setBounds (668, 252, 64, 64);
    kickButton.setBounds (556, 338, 84, 38);
}

void FrontPanel::paintGrille (Graphics& g, Rectangle<float> r)
{
    g.saveState();
    g.reduceClipRegion (r.toNearestInt());
    g.setColour (Colour (0xffcdb98c));
    g.fillRect (r);

    // woven wheat cloth
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
    // a faint oxblood stripe pattern, like old grille cloth
    g.setColour (Colour (0xff7a2a1e).withAlpha (0.10f));
    for (float xx = r.getX() - r.getHeight(); xx < r.getRight(); xx += 14.0f)
        g.drawLine (xx, r.getBottom(), xx + r.getHeight(), r.getY(), 2.0f);

    // vignette
    g.setGradientFill (ColourGradient (Colours::transparentBlack, r.getCentre(), Colours::black.withAlpha (0.35f),
                                       r.getTopLeft(), true));
    g.fillRect (r);
    g.restoreState();

    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawRect (r, 2.0f);
}

void FrontPanel::paint (Graphics& g)
{
    g.drawImageAt (tolex, 0, 0);

    // piping around the cabinet
    g.setColour (Colour (0xffe7dcc0).withAlpha (0.55f));
    g.drawRoundedRectangle (Rectangle<float> (8, 8, W - 16, H - 16), 10.0f, 2.0f);

    // grille, jiggles when you kick the tank
    auto grille = Rectangle<float> (26, 22, W - 52, 122).translated (shake, shake * 0.4f);
    paintGrille (g, grille);

    // logo plate on the grille
    {
        auto logo = Rectangle<float> (60, 46, 270, 74).translated (shake, shake * 0.4f);
        g.setFont (FontOptions ("Georgia", 54.0f, Font::bold | Font::italic));
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawText ("Driptank", logo.translated (2, 3), Justification::centredLeft);
        g.setGradientFill (ColourGradient (Colours::white, logo.getTopLeft(), Colour (0xff9a8f80), logo.getBottomLeft(), false));
        g.drawText ("Driptank", logo, Justification::centredLeft);

        g.setFont (FontOptions (13.0f, Font::bold));
        g.setColour (Colour (0xff3a2416).withAlpha (0.85f));
        g.drawText ("'63  TUBE SPRING REVERB", Rectangle<float> (330, 72, 260, 30).translated (shake, shake * 0.4f),
                    Justification::centredLeft);
    }

    // faceplate
    auto plate = Rectangle<float> (26, 160, W - 52, 236);
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (plate.translated (2, 3), 6.0f);
    g.setGradientFill (ColourGradient (kCream, plate.getTopLeft(), kCreamDark, plate.getBottomLeft(), false));
    g.fillRoundedRectangle (plate, 6.0f);
    g.setColour (kInk.withAlpha (0.6f));
    g.drawRoundedRectangle (plate.reduced (6.0f), 4.0f, 1.2f);

    // screws
    for (auto pt : { Point<float> (40, 174), Point<float> (W - 40.0f, 174), Point<float> (40, 382), Point<float> (W - 40.0f, 382) })
    {
        g.setGradientFill (ColourGradient (Colours::white, pt.translated (-3, -3), Colour (0xff707070), pt.translated (3, 3), false));
        g.fillEllipse (Rectangle<float> (9, 9).withCentre (pt));
        g.setColour (Colour (0xff505050));
        g.drawLine (Line<float> (pt.translated (-3, 2), pt.translated (3, -2)), 1.2f);
    }

    // labels
    g.setColour (kInk);
    g.setFont (FontOptions (15.0f, Font::bold));
    auto label = [&] (const String& t, Rectangle<int> under, int dy = 4)
    {
        g.drawText (t, Rectangle<int> (under.getX() - 20, under.getBottom() + dy, under.getWidth() + 40, 20),
                    Justification::centred);
    };
    label ("DWELL", dwell.getBounds());
    label ("MIXER", mix.getBounds());
    label ("TONE", tone.getBounds());
    label ("TANK", tank.getBounds(), 2);
    g.setFont (FontOptions (12.5f, Font::bold));
    label ("OUTPUT", output.getBounds(), 6);

    g.setFont (FontOptions (10.0f, Font::bold));
    g.drawText ("WIDE", Rectangle<int> (stereo.getRight() + 2, stereo.getY(), 40, 14), Justification::centredLeft);
    g.drawText ("MONO", Rectangle<int> (stereo.getRight() + 2, stereo.getBottom() - 14, 40, 14), Justification::centredLeft);
    g.setFont (FontOptions (12.5f, Font::bold));
    g.drawText ("SPRINGS", Rectangle<int> (stereo.getX() - 74, stereo.getBounds().getCentreY() - 10, 70, 20), Justification::centredRight);

    g.drawText ("REVERB", Rectangle<int> (reverbOn.getX() - 10, reverbOn.getBottom() + 4, reverbOn.getWidth() + 20, 20),
                Justification::centred);

    // jewel pilot lamp
    {
        const bool on = reverbOn.getToggleState();
        const auto c = Point<float> ((float) reverbOn.getBounds().getCentreX(), 206.0f);
        if (on)
        {
            g.setGradientFill (ColourGradient (Colour (0xffff3a1a).withAlpha (0.55f), c, Colours::transparentBlack,
                                               c.translated (34, 0), true));
            g.fillEllipse (Rectangle<float> (68, 68).withCentre (c));
        }
        g.setGradientFill (ColourGradient (Colour (0xffe8e8e8), c.translated (-14, -14), Colour (0xff5a5a5a), c.translated (14, 14), false));
        g.fillEllipse (Rectangle<float> (34, 34).withCentre (c));
        const auto lens = Rectangle<float> (24, 24).withCentre (c);
        g.setGradientFill (ColourGradient (on ? Colour (0xffffb08a) : Colour (0xff8a2a20), lens.getCentre().translated (-4, -5),
                                           on ? Colour (0xffd01808) : Colour (0xff3a0a06), lens.getBottomRight(), true));
        g.fillEllipse (lens);
        // facets
        g.setColour (Colours::white.withAlpha (on ? 0.35f : 0.15f));
        for (int i = 0; i < 6; ++i)
            g.drawLine (Line<float> (c, polar (c, 11.5f, (float) i * MathConstants<float>::pi / 3.0f)), 0.8f);
    }
}

void FrontPanel::startShake()
{
    shakeTicks = 18;
    startTimerHz (60);
}

void FrontPanel::timerCallback()
{
    if (--shakeTicks <= 0)
    {
        shake = 0.0f;
        stopTimer();
    }
    else
    {
        shake = (rng.nextFloat() * 2.0f - 1.0f) * (float) shakeTicks * 0.35f;
    }
    repaint (0, 0, W, 150);
}

//==============================================================================
DriptankEditor::DriptankEditor (DriptankProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (FrontPanel::W / 2, FrontPanel::H / 2, FrontPanel::W * 2, FrontPanel::H * 2);
    getConstrainer()->setFixedAspectRatio ((double) FrontPanel::W / (double) FrontPanel::H);
    setSize (FrontPanel::W, FrontPanel::H);
}

void DriptankEditor::resized()
{
    const float scale = (float) getWidth() / (float) FrontPanel::W;
    panel.setTransform (AffineTransform::scale (scale));
}
