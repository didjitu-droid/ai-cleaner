#include "UIComponents.h"
#include "FontData.h"

namespace ui
{
namespace
{
    struct Typefaces
    {
        juce::Typeface::Ptr anton   = juce::Typeface::createSystemTypefaceFor (FontData::AntonRegular_ttf, (size_t) FontData::AntonRegular_ttfSize);
        juce::Typeface::Ptr semi    = juce::Typeface::createSystemTypefaceFor (FontData::OutfitSemiBold_ttf, (size_t) FontData::OutfitSemiBold_ttfSize);
        juce::Typeface::Ptr bold    = juce::Typeface::createSystemTypefaceFor (FontData::OutfitBold_ttf, (size_t) FontData::OutfitBold_ttfSize);
    };

    juce::Font makeFont (const juce::Typeface::Ptr& tf, float height, float kerning)
    {
        juce::Font f (juce::FontOptions{}.withTypeface (tf).withHeight (height));
        if (kerning != 0.0f)
            f.setExtraKerningFactor (kerning);
        return f;
    }

    constexpr float knobStart = juce::MathConstants<float>::pi * 1.25f;
    constexpr float knobSweep = juce::MathConstants<float>::pi * 1.5f;

    juce::Point<float> polar (float cx, float cy, float r, float angle)
    {
        return { cx + r * std::sin (angle), cy - r * std::cos (angle) };
    }
}

juce::Font anton (float height, float kerning)
{
    juce::SharedResourcePointer<Typefaces> tf;
    return makeFont (tf->anton, height, kerning);
}

juce::Font outfit (float height, bool bold, float kerning)
{
    juce::SharedResourcePointer<Typefaces> tf;
    return makeFont (bold ? tf->bold : tf->semi, height, kerning);
}

float textWidth (const juce::Font& font, const juce::String& text)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (font, text, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

void drawSpacedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                     const juce::Font& font, juce::Justification just)
{
    g.setFont (font);
    g.drawText (text, area, just, false);
}

//==============================================================================
PillButton::PillButton (const juce::String& offText, const juce::String& onText)
    : juce::Button (offText), textOff (offText), textOn (onText)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PillButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    auto fill = on ? onFill : offFill;
    if (highlighted && ! on) fill = fill.brighter (0.25f);
    if (down) fill = fill.darker (0.15f);

    const float radius = r.getHeight() * 0.5f;
    g.setColour (fill);
    g.fillRoundedRectangle (r, radius);
    g.setColour (on ? onBorder : border);
    g.drawRoundedRectangle (r, radius, 2.0f);

    g.setColour (on ? onTextColour : offTextColour);
    g.setFont (outfit (fontSize, true));
    g.drawText (on && textOn.isNotEmpty() ? textOn : textOff, r, juce::Justification::centred, false);
}

//==============================================================================
Knob::Knob (const juce::String& name, juce::Colour c, Kind k)
    : title (name), colour (c), kind (k)
{
    setName (name);
    setTitle (name);
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setMouseDragSensitivity (180);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void Knob::mouseEnter (const juce::MouseEvent& e)
{
    if (kind == Kind::module)
    {
        const float v = (float) valueToProportionOfLength (getValue());
        const float h = heart ? heart() : 1.0f;
        setTooltip (title + "  -  max " + juce::String (juce::roundToInt (v * 100.0f)) + "%, applied now "
                    + juce::String (juce::roundToInt (v * h * 100.0f)) + "% (follows the heart)");
    }
    juce::Slider::mouseEnter (e);
}

void Knob::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float size = 54.0f, s = size / 78.0f;
    const float cx = b.getCentreX(), cy = b.getY() + 2.0f + size * 0.5f;

    const float v = (float) valueToProportionOfLength (getValue());
    const float eff = kind == Kind::module ? v * (heart ? heart() : 1.0f) : v;

    auto arc = [&] (float frac, float radius)
    {
        juce::Path p;
        if (frac > 0.005f)
            p.addCentredArc (cx, cy, radius, radius, 0.0f, knobStart, knobStart + knobSweep * frac, true);
        return p;
    };
    const juce::PathStrokeType stroke (6.0f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    g.setColour (col::track);
    g.strokePath (arc (1.0f, 33.0f * s), stroke);

    const juce::Colour main = kind == Kind::module ? colour : col::text;
    if (kind == Kind::module)
    {
        g.setColour (colour.withAlpha (0.3f));
        g.strokePath (arc (v, 33.0f * s), stroke);
    }
    g.setColour (main);
    g.strokePath (arc (eff, 33.0f * s), stroke);

    g.setColour (col::face);
    g.fillEllipse (cx - 23.0f * s, cy - 23.0f * s, 46.0f * s, 46.0f * s);
    g.setColour (col::panelLine);
    g.drawEllipse (cx - 23.0f * s, cy - 23.0f * s, 46.0f * s, 46.0f * s, 2.0f * s);

    const auto tip = polar (cx, cy, 17.0f * s, knobStart + knobSweep * eff);
    g.setColour (col::text);
    g.drawLine ({ { cx, cy }, tip }, 3.5f * s);

    if (kind == Kind::module)
    {
        const auto dot = polar (cx, cy, 33.0f * s, knobStart + knobSweep * v);
        g.setColour (colour);
        g.fillEllipse (dot.x - 3.5f * s * 1.4f, dot.y - 3.5f * s * 1.4f, 7.0f * s * 1.4f, 7.0f * s * 1.4f);
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (col::yellow);
        g.drawEllipse (cx - 36.0f * s, cy - 36.0f * s, 72.0f * s, 72.0f * s, 1.5f);
    }

    const float textTop = b.getY() + size + 2.0f;
    g.setColour (col::text);
    g.setFont (outfit (13.0f, true));
    g.drawText (title, juce::Rectangle<float> (b.getX(), textTop, b.getWidth(), 16.0f), juce::Justification::centred, false);

    juce::String valueText;
    if (kind == Kind::module)
        valueText = juce::String (juce::roundToInt (eff * 100.0f)) + "%";
    else
        valueText = format ? format (getValue()) : juce::String (getValue(), 1);

    g.setColour (kind == Kind::module ? colour : col::muted);
    g.setFont (outfit (12.0f, false));
    g.drawText (valueText, juce::Rectangle<float> (b.getX(), textTop + 16.0f, b.getWidth(), 15.0f), juce::Justification::centred, false);
}

//==============================================================================
juce::Path HeartControl::heartPath()
{
    juce::Path p;
    p.startNewSubPath (160.0f, 236.0f);
    p.cubicTo (98.0f, 190.0f, 52.0f, 158.0f, 52.0f, 112.0f);
    p.cubicTo (52.0f, 80.0f, 76.0f, 58.0f, 106.0f, 58.0f);
    p.cubicTo (130.0f, 58.0f, 148.0f, 72.0f, 160.0f, 92.0f);
    p.cubicTo (172.0f, 72.0f, 190.0f, 58.0f, 214.0f, 58.0f);
    p.cubicTo (244.0f, 58.0f, 268.0f, 80.0f, 268.0f, 112.0f);
    p.cubicTo (268.0f, 158.0f, 222.0f, 190.0f, 160.0f, 236.0f);
    p.closeSubPath();
    return p;
}

HeartControl::HeartControl (juce::RangedAudioParameter& p, std::function<bool()> delta)
    : param (p),
      attachment (p, [this] (float v) { value = v; repaint(); }, nullptr),
      isDelta (std::move (delta))
{
    attachment.sendInitialUpdate();
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTitle ("Master Clean");
    setDescription ("One control for all cleaning modules. Drag up or down.");
    setTooltip ("Drag up/down: more or less cleaning. All 8 modules follow. Double-click: back to 65%.");
    startTimerHz (30);
}

void HeartControl::timerCallback()
{
    const double dt = 1.0 / 30.0;
    const double period = 1.6 - 0.9 * (value / 100.0);
    beatPhase = std::fmod (beatPhase + dt / period, 1.0);
    wavePhase += 3.0 * dt;
    if (isShowing())
        repaint();
}

void HeartControl::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float scale = juce::jmin (b.getWidth() / 320.0f, b.getHeight() / 280.0f);
    const float ox = b.getCentreX() - 160.0f * scale, oy = b.getCentreY() - 147.0f * scale;
    const auto toScreen = juce::AffineTransform::scale (scale).translated (ox, oy);

    // Heartbeat: same keyframes as the design.
    static const float keyT[] = { 0.0f, 0.12f, 0.24f, 0.36f, 0.60f, 1.0f };
    static const float keyS[] = { 1.0f, 1.06f, 0.98f, 1.04f, 1.0f, 1.0f };
    float beat = 1.0f;
    const float ph = (float) beatPhase;
    for (int i = 0; i < 5; ++i)
        if (ph >= keyT[i] && ph <= keyT[i + 1])
        {
            const float t = (ph - keyT[i]) / (keyT[i + 1] - keyT[i]);
            const float e = 0.5f - 0.5f * std::cos (t * juce::MathConstants<float>::pi);
            beat = keyS[i] + (keyS[i + 1] - keyS[i]) * e;
        }

    const auto t = juce::AffineTransform::scale (beat, beat, 160.0f, 140.0f).followedBy (toScreen);
    auto shape = heart;
    shape.applyTransform (t);

    g.setColour (col::face);
    g.fillPath (shape);

    // Liquid level = amount of cleaning.
    const float v = juce::jlimit (0.0f, 1.0f, value / 100.0f);
    const float level = 236.0f - 178.0f * v;
    const float amp = (v > 0.02f && v < 0.98f) ? 5.0f : 0.0f;
    auto wave = [&] (float lvl, float phase, float a)
    {
        juce::Path p;
        p.startNewSubPath (40.0f, 260.0f);
        p.lineTo (40.0f, lvl);
        for (int x = 40; x <= 280; x += 8)
            p.lineTo ((float) x, lvl + std::sin ((float) x / 34.0f + phase) * a);
        p.lineTo (280.0f, 260.0f);
        p.closeSubPath();
        p.applyTransform (t);
        return p;
    };

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (shape);
        juce::ColourGradient grad (col::orange, juce::Point<float> (160.0f, 236.0f).transformedBy (t),
                                   col::magenta, juce::Point<float> (160.0f, 58.0f).transformedBy (t), false);
        grad.addColour (0.55, col::pink);
        g.setGradientFill (grad);
        g.fillPath (wave (level, (float) wavePhase, amp));
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.fillPath (wave (level + 4.0f, (float) wavePhase + 1.7f, amp * 0.8f));
    }

    juce::Path shine;
    shine.startNewSubPath (92.0f, 100.0f);
    shine.cubicTo (96.0f, 84.0f, 108.0f, 76.0f, 122.0f, 76.0f);
    shine.applyTransform (t);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.strokePath (shine, juce::PathStrokeType (8.0f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (col::text);
    g.strokePath (shape, juce::PathStrokeType (6.0f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (isDelta && isDelta())
    {
        juce::Path crack;
        crack.startNewSubPath (160.0f, 92.0f);
        crack.lineTo (148.0f, 126.0f);
        crack.lineTo (170.0f, 150.0f);
        crack.lineTo (150.0f, 182.0f);
        crack.lineTo (166.0f, 208.0f);
        crack.lineTo (160.0f, 236.0f);
        crack.applyTransform (t);
        g.strokePath (crack, juce::PathStrokeType (6.0f * scale, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (col::yellow);
        g.drawRoundedRectangle (b.reduced (2.0f), 16.0f, 2.0f);
    }
}

void HeartControl::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    dragStartValue = value;
    dragStartY = e.position.y;
    attachment.beginGesture();
}

void HeartControl::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const float speed = e.mods.isShiftDown() ? 0.15f : 1.0f;   // shift = fine adjust
    const float nv = juce::jlimit (0.0f, 100.0f, dragStartValue + (dragStartY - e.position.y) / 180.0f * 100.0f * speed);
    attachment.setValueAsPartOfGesture (nv);
}

void HeartControl::mouseUp (const juce::MouseEvent&)
{
    if (dragging) attachment.endGesture();
    dragging = false;
}

void HeartControl::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (65.0f);
}

void HeartControl::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    attachment.setValueAsCompleteGesture (juce::jlimit (0.0f, 100.0f, value + w.deltaY * 20.0f));
}

bool HeartControl::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::upKey || k == juce::KeyPress::rightKey)
    { attachment.setValueAsCompleteGesture (juce::jmin (100.0f, value + 5.0f)); return true; }
    if (k == juce::KeyPress::downKey || k == juce::KeyPress::leftKey)
    { attachment.setValueAsCompleteGesture (juce::jmax (0.0f, value - 5.0f)); return true; }
    return false;
}

//==============================================================================
FreqDisplay::FreqDisplay (juce::AudioProcessorValueTreeState& state, aic::CleanerEngine& eng)
    : apvts (state), engine (eng),
      lowParam (*state.getParameter ("lowCut")),
      highParam (*state.getParameter ("highCut")),
      autoParam (*state.getParameter ("autoCuts")),
      lowAtt  (lowParam,  [this] (float v) { lowValue = v; repaint(); }, nullptr),
      highAtt (highParam, [this] (float v) { highValue = v; repaint(); }, nullptr),
      autoAtt (autoParam, [this] (float v) { autoOn = v > 0.5f; repaint(); }, nullptr)
{
    lowAtt.sendInitialUpdate();
    highAtt.sendInitialUpdate();
    autoAtt.sendInitialUpdate();
    spectrum.fill (-120.0f);
    setTitle ("Frequency cuts");
    setTooltip ("Drag the turquoise dot (Low Cut) or the orange dot (High Cut). Moving one switches AUTO off.");
    startTimerHz (30);
}

juce::Rectangle<float> FreqDisplay::plotArea() const
{
    return { 12.0f, 36.0f, (float) getWidth() - 26.0f, (float) getHeight() - 50.0f };
}

float FreqDisplay::freqToX (float f) const
{
    const auto pa = plotArea();
    return pa.getX() + (std::log10 (juce::jmax (1.0f, f)) - std::log10 (20.0f)) / 3.0f * pa.getWidth();
}

float FreqDisplay::xToFreq (float x) const
{
    const auto pa = plotArea();
    return std::pow (10.0f, std::log10 (20.0f) + (x - pa.getX()) / pa.getWidth() * 3.0f);
}

float FreqDisplay::shownLow() const  { return autoOn ? engine.getCurrentLowCut()  : lowValue; }
float FreqDisplay::shownHigh() const { return autoOn ? engine.getCurrentHighCut() : highValue; }

juce::String FreqDisplay::formatHz (float f)
{
    if (f >= 1000.0f) return juce::String (f / 1000.0f, 1) + " kHz";
    return juce::String (juce::roundToInt (f)) + " Hz";
}

void FreqDisplay::timerCallback()
{
    engine.copyDisplaySpectrum (spectrum);
    if (isShowing())
        repaint();
}

void FreqDisplay::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (b.reduced (0.5f), 18.0f);
    g.setColour (col::panelLine);
    g.drawRoundedRectangle (b.reduced (0.5f), 18.0f, 1.0f);

    // Header
    g.setColour (col::muted);
    g.setFont (outfit (11.0f, true, 0.16f));
    g.drawText ("FREQUENCY CUTS", juce::Rectangle<float> (12.0f, 6.0f, autoButtonX - 16.0f, 26.0f), juce::Justification::centredLeft, false);

    const float lo = shownLow(), hi = shownHigh();
    {
        const auto f = outfit (13.0f, true), fv = outfit (13.0f, true);
        const juce::String hiV = formatHz (hi), loV = formatHz (lo);
        float x = b.getRight() - 14.0f;
        auto drawPair = [&] (const juce::String& lbl, const juce::String& val, juce::Colour c)
        {
            const float wv = textWidth (fv, val), wl = textWidth (f, lbl);
            x -= wv;
            g.setColour (c);  g.setFont (fv); g.drawText (val, juce::Rectangle<float> (x, 6.0f, wv + 2.0f, 26.0f), juce::Justification::centredLeft, false);
            x -= wl;
            g.setColour (col::text); g.setFont (f); g.drawText (lbl, juce::Rectangle<float> (x, 6.0f, wl + 2.0f, 26.0f), juce::Justification::centredLeft, false);
            x -= 18.0f;
        };
        drawPair ("High Cut ", hiV, col::orange);
        drawPair ("Low Cut ", loV, col::teal);
    }

    const auto pa = plotArea();
    const float xl = freqToX (lo), xh = freqToX (hi);

    g.setColour (col::teal.withAlpha (0.10f));
    g.fillRect (juce::Rectangle<float> (pa.getX(), pa.getY(), juce::jmax (0.0f, xl - pa.getX()), pa.getHeight()));
    g.setColour (col::orange.withAlpha (0.12f));
    g.fillRect (juce::Rectangle<float> (xh, pa.getY(), juce::jmax (0.0f, pa.getRight() - xh), pa.getHeight()));

    g.setFont (outfit (10.0f, false));
    for (auto [f, txt] : { std::pair<float, const char*> { 100.0f, "100 Hz" }, { 1000.0f, "1 kHz" }, { 10000.0f, "10 kHz" } })
    {
        const float x = freqToX (f);
        g.setColour (col::panelLine);
        g.drawVerticalLine (juce::roundToInt (x), pa.getY(), pa.getBottom());
        g.setColour (col::label);
        g.drawText (txt, juce::Rectangle<float> (x + 4.0f, pa.getBottom() + 1.0f, 60.0f, 12.0f), juce::Justification::centredLeft, false);
    }

    // Live spectrum
    {
        juce::Path sp;
        const float logMin = std::log (aic::CleanerEngine::displayMinHz), logMax = std::log (aic::CleanerEngine::displayMaxHz);
        sp.startNewSubPath (pa.getX(), pa.getBottom());
        for (int i = 0; i < aic::CleanerEngine::displayPoints; ++i)
        {
            const float f = std::exp (logMin + (logMax - logMin) * ((float) i + 0.5f) / aic::CleanerEngine::displayPoints);
            const float lvl = juce::jlimit (0.0f, 1.0f, (spectrum[(size_t) i] + 90.0f) / 84.0f);
            sp.lineTo (freqToX (f), pa.getBottom() - lvl * pa.getHeight() * 0.9f);
        }
        sp.lineTo (pa.getRight(), pa.getBottom());
        sp.closeSubPath();
        g.setColour (col::violet.withAlpha (0.22f));
        g.fillPath (sp);
    }

    // Filter curve (24 dB/oct on both sides)
    const float base = pa.getY() + 12.0f;
    {
        juce::Path cv;
        for (int i = 0; i <= 160; ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 160.0f);
            float db = 0.0f;
            if (f < lo) db -= 24.0f * std::log2 (lo / f);
            if (f > hi) db -= 24.0f * std::log2 (f / hi);
            db = juce::jmax (-30.0f, db);
            const float y = juce::jmin (pa.getBottom(), base - db * (pa.getBottom() - base) / 30.0f);
            if (i == 0) cv.startNewSubPath (freqToX (f), y); else cv.lineTo (freqToX (f), y);
        }
        g.setColour (col::pink);
        g.strokePath (cv, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    for (auto [x, c, d] : { std::tuple<float, juce::Colour, Drag> { xl, col::teal, Drag::low }, { xh, col::orange, Drag::high } })
    {
        const float r = dragging == d ? 9.5f : 8.0f;
        g.setColour (c);
        g.fillEllipse (x - r, base - r, r * 2.0f, r * 2.0f);
        g.setColour (col::panel);
        g.drawEllipse (x - r, base - r, r * 2.0f, r * 2.0f, 2.0f);
    }
}

void FreqDisplay::mouseMove (const juce::MouseEvent& e)
{
    const auto pa = plotArea();
    setMouseCursor (e.position.y >= pa.getY() - 6.0f ? juce::MouseCursor::LeftRightResizeCursor
                                                      : juce::MouseCursor::NormalCursor);
}

void FreqDisplay::mouseDown (const juce::MouseEvent& e)
{
    const auto pa = plotArea();
    if (e.position.y < pa.getY() - 6.0f)
        return;

    const float xl = freqToX (shownLow()), xh = freqToX (shownHigh());
    dragging = std::abs (e.position.x - xl) <= std::abs (e.position.x - xh) ? Drag::low : Drag::high;

    if (autoOn)
    {
        // Take over from AUTO without a jump: start from where AUTO had placed the cuts.
        lowAtt.setValueAsCompleteGesture (juce::jlimit (20.0f, 500.0f, engine.getCurrentLowCut()));
        highAtt.setValueAsCompleteGesture (juce::jlimit (2000.0f, 20000.0f, engine.getCurrentHighCut()));
        autoAtt.setValueAsCompleteGesture (0.0f);
    }

    (dragging == Drag::low ? lowAtt : highAtt).beginGesture();
    mouseDrag (e);
}

void FreqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == Drag::none) return;
    const float f = xToFreq (e.position.x);
    if (dragging == Drag::low)
        lowAtt.setValueAsPartOfGesture (juce::jlimit (20.0f, 500.0f, f));
    else
        highAtt.setValueAsPartOfGesture (juce::jlimit (2000.0f, 20000.0f, f));
}

void FreqDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging == Drag::low)  lowAtt.endGesture();
    if (dragging == Drag::high) highAtt.endGesture();
    dragging = Drag::none;
    repaint();
}

//==============================================================================
InstaLink::InstaLink (const juce::String& name, const juce::String& link)
    : juce::Button (name), url (link)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Open " + name + " on Instagram");
}

float InstaLink::preferredWidth() const
{
    return 16.0f + 5.0f + textWidth (outfit (12.0f, true, 0.22f), getName()) + 2.0f;
}

void InstaLink::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto b = getLocalBounds().toFloat();
    const juce::Colour c = highlighted ? col::yellow : col::text;
    const float s = 16.0f / 24.0f, ix = b.getX(), iy = b.getCentreY() - 8.0f;

    g.setColour (c);
    g.drawRoundedRectangle (ix + 3.0f * s, iy + 3.0f * s, 18.0f * s, 18.0f * s, 5.0f * s, 2.0f * s);
    g.drawEllipse (ix + 8.0f * s, iy + 8.0f * s, 8.0f * s, 8.0f * s, 2.0f * s);
    g.fillEllipse (ix + 16.1f * s, iy + 5.5f * s, 2.4f * s, 2.4f * s);

    g.setFont (outfit (12.0f, true, 0.22f));
    g.drawText (getName(), juce::Rectangle<float> (ix + 21.0f, b.getY(), b.getWidth() - 21.0f, b.getHeight()),
                juce::Justification::centredLeft, false);
}

void InstaLink::clicked()
{
    url.launchInDefaultBrowser();
}
}
