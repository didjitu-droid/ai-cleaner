#include "PluginEditor.h"

using namespace ui;

namespace
{
    const juce::Colour moduleColours[aic::numModules] = {
        col::pink, col::orange, juce::Colour (0xffFF6FB5), col::yellow,
        juce::Colour (0xff5EEAD4), col::violet, col::teal, juce::Colour (0xffFFE08A)
    };

    // Design layout (900 x 560)
    const juce::Rectangle<int> heartCard   { 20, 106, 250, 302 };
    const juce::Rectangle<int> modulePanel { 282, 106, 598, 302 };
    const juce::Rectangle<int> freqPanel   { 20, 416, 860, 96 };
    constexpr int footerTop = 516, footerHeight = 40;
}

AICleanerEditor::AICleanerEditor (AICleanerProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    auto& apvts = processor.apvts;
    addAndMakeVisible (content);
    content.setBounds (0, 0, baseWidth, baseHeight);

    // Header toggles
    autoGainButton.border = col::text;  autoGainButton.onBorder = col::text;
    autoGainButton.onFill = col::text;  autoGainButton.onTextColour = col::panel;
    autoGainButton.setTooltip ("Keeps the cleaned sound as loud as the original, so you judge quality, not volume.");

    deltaButton.border = col::orange;   deltaButton.onBorder = col::orange;
    deltaButton.onFill = col::orange;   deltaButton.onTextColour = col::panel;
    deltaButton.setTooltip ("Hear only what AI Cleaner removes. If you hear music here, clean less.");

    bypassButton.onFill = col::pink;    bypassButton.onBorder = col::pink;
    bypassButton.setTooltip ("Compare with the original sound.");

    for (auto* b : { &autoGainButton, &deltaButton, &bypassButton })
    {
        b->setClickingTogglesState (true);
        content.addAndMakeVisible (*b);
    }
    autoGainAtt = std::make_unique<ButtonAttachment> (apvts, "autoGain", autoGainButton);
    deltaAtt    = std::make_unique<ButtonAttachment> (apvts, "delta", deltaButton);
    bypassAtt   = std::make_unique<ButtonAttachment> (apvts, "bypass", bypassButton);

    // Stem selector
    for (int s = 0; s < aic::numStems; ++s)
    {
        auto* b = stemButtons.add (new PillButton (aic::stemName (s)));
        b->fontSize = 14.0f;
        b->setTooltip (juce::String ("Load the starting settings for ") + aic::stemName (s) + " stems.");
        b->onClick = [this, s] { processor.applyStemPreset (s); };
        content.addAndMakeVisible (b);
    }

    // Heart
    heart = std::make_unique<HeartControl> (*apvts.getParameter ("clean"),
                                            [this] { return deltaButton.getToggleState(); });
    content.addAndMakeVisible (*heart);

    // Module knobs
    auto heartValue = [this] { return processor.apvts.getRawParameterValue ("clean")->load() / 100.0f; };
    for (int m = 0; m < aic::numModules; ++m)
    {
        auto* k = moduleKnobs.add (new Knob (aic::moduleName (m), moduleColours[m], Knob::Kind::module));
        k->heart = heartValue;
        content.addAndMakeVisible (k);
        moduleAtts.add (new SliderAttachment (apvts, aic::moduleParamID (m), *k));
        k->setDoubleClickReturnValue (true, (double) aic::stemPreset (aic::vocals).amounts[(size_t) m]);
    }

    outputKnob.format = [] (double v) { return (v > 0.0 ? "+" : "") + juce::String (v, 1) + " dB"; };
    mixKnob.format = [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; };
    content.addAndMakeVisible (outputKnob);
    content.addAndMakeVisible (mixKnob);
    outputAtt = std::make_unique<SliderAttachment> (apvts, "output", outputKnob);
    mixAtt    = std::make_unique<SliderAttachment> (apvts, "mix", mixKnob);
    outputKnob.setDoubleClickReturnValue (true, 0.0);
    mixKnob.setDoubleClickReturnValue (true, 100.0);

    // Frequency cuts
    freq = std::make_unique<FreqDisplay> (apvts, processor.engine);
    content.addAndMakeVisible (*freq);
    autoCutsButton.border = col::teal;   autoCutsButton.onBorder = col::teal;
    autoCutsButton.onFill = col::teal;   autoCutsButton.onTextColour = col::panel;
    autoCutsButton.fontSize = 12.0f;
    autoCutsButton.setClickingTogglesState (true);
    autoCutsButton.setTooltip ("AUTO listens to the stem and places the Low Cut and High Cut for you.");
    content.addAndMakeVisible (autoCutsButton);
    autoCutsAtt = std::make_unique<ButtonAttachment> (apvts, "autoCuts", autoCutsButton);

    // Footer links
    content.addAndMakeVisible (maauraLink);
    content.addAndMakeVisible (danidaneLink);

    // Fixed layout at design size
    autoGainButton.setBounds (546, 10, 92, 36);
    deltaButton.setBounds (646, 10, 148, 36);
    bypassButton.setBounds (802, 10, 78, 36);

    const int stemWidths[aic::numStems] = { 84, 76, 66, 112, 92 };
    int x = 248;
    for (int s = 0; s < aic::numStems; ++s)
    {
        stemButtons[s]->setBounds (x, 64, stemWidths[s], 34);
        x += stemWidths[s] + 6;
    }

    heart->setBounds (60, 152, 180, 156);

    for (int m = 0; m < aic::numModules; ++m)
        moduleKnobs[m]->setBounds (290 + (m % 4) * 120, m < 4 ? 146 : 289, 120, 92);
    outputKnob.setBounds (788, 146, 84, 92);
    mixKnob.setBounds (788, 289, 84, 92);

    freq->setBounds (freqPanel);
    autoCutsButton.setBounds (freqPanel.getX() + (int) FreqDisplay::autoButtonX, freqPanel.getY() + 6, 134, 26);

    layoutFooter();
    timerCallback();   // set the initial stem highlight

    // Window size: resizable, keeps the design's proportions, starts small enough for laptop screens.
    setResizable (true, true);
    setResizeLimits (585, 364, 1440, 896);
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);

    float scale = processor.getEditorScale();
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        scale = juce::jmin (scale, (float) (display->userArea.getHeight() - 140) / (float) baseHeight);
    scale = juce::jlimit (0.65f, 1.6f, scale);
    setSize (juce::roundToInt (baseWidth * scale), juce::roundToInt (baseHeight * scale));

    startTimerHz (30);
}

AICleanerEditor::~AICleanerEditor()
{
    stopTimer();
}

void AICleanerEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    processor.setEditorScale (scale);
}

void AICleanerEditor::layoutFooter()
{
    const auto f = outfit (12.0f, false, 0.22f);
    const float wCreated = textWidth (f, "CREATED BY"), wAnd = textWidth (f, "AND");
    const float wM = maauraLink.preferredWidth(), wD = danidaneLink.preferredWidth();
    const float gap = 8.0f;
    const float total = wCreated + gap + wM + gap + wAnd + gap + wD;
    float x = (baseWidth - total) * 0.5f;

    footerCreatedX = x;               x += wCreated + gap;
    maauraLink.setBounds (juce::roundToInt (x), footerTop + 2, juce::roundToInt (wM), footerHeight - 4);
    x += wM + gap;
    footerAndX = x;                   x += wAnd + gap;
    danidaneLink.setBounds (juce::roundToInt (x), footerTop + 2, juce::roundToInt (wD), footerHeight - 4);
}

void AICleanerEditor::timerCallback()
{
    const float clean = processor.apvts.getRawParameterValue ("clean")->load();
    if (std::abs (clean - lastClean) > 0.01f)
    {
        lastClean = clean;
        for (auto* k : moduleKnobs) k->repaint();
        content.repaint (heartCard);
    }

    const int stem = (int) processor.apvts.getRawParameterValue ("stem")->load();
    if (stem != lastStem)
    {
        lastStem = stem;
        for (int s = 0; s < stemButtons.size(); ++s)
            stemButtons[s]->setToggleState (s == stem, juce::dontSendNotification);
    }
}

//==============================================================================
void AICleanerEditor::Content::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    juce::ColourGradient bg (juce::Colour (0xff1E0838), 0.0f, 0.0f, juce::Colour (0xffB3205F), 0.0f, b.getHeight(), false);
    bg.addColour (0.55, juce::Colour (0xff4E1268));
    g.setGradientFill (bg);
    g.fillRect (b);

    // Title
    {
        const auto f = anton (28.0f, 0.06f);
        const float wAi = textWidth (f, "AI ");
        g.setFont (f);
        g.setColour (col::text);
        g.drawText ("AI ", juce::Rectangle<float> (20.0f, 0.0f, wAi + 4.0f, 56.0f), juce::Justification::centredLeft, false);
        g.setColour (col::pink);
        g.drawText ("CLEANER", juce::Rectangle<float> (20.0f + wAi, 0.0f, 200.0f, 56.0f), juce::Justification::centredLeft, false);
    }

    g.setColour (col::muted);
    g.setFont (outfit (11.0f, true, 0.2f));
    g.drawText ("STEM", juce::Rectangle<float> (198.0f, 64.0f, 46.0f, 34.0f), juce::Justification::centredLeft, false);

    // Panels
    for (auto r : { heartCard, modulePanel })
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (r.toFloat(), 18.0f);
        g.setColour (col::panelLine);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 18.0f, 1.0f);
    }

    // Heart card texts
    const auto card = heartCard.toFloat();
    g.setColour (col::yellow);
    g.setFont (outfit (11.0f, true, 0.18f));
    g.drawText (juce::CharPointer_UTF8 ("ONE HEART \xc2\xb7 CLEANS ALL"), juce::Rectangle<float> (card.getX(), 130.0f, card.getWidth(), 16.0f),
                juce::Justification::centred, false);

    // Up/down arrows hint
    {
        juce::Path a;
        const float ax = 42.0f, ay = 204.0f;
        a.addTriangle (ax, ay + 3.0f, ax + 5.0f, ay + 10.0f, ax - 5.0f, ay + 10.0f);
        a.addTriangle (ax, ay + 49.0f, ax + 5.0f, ay + 42.0f, ax - 5.0f, ay + 42.0f);
        g.setColour (col::muted);
        g.fillPath (a);
        juce::Path line;
        line.startNewSubPath (ax, ay + 13.0f);
        line.lineTo (ax, ay + 39.0f);
        juce::Path dashed;
        const float dashes[] = { 2.0f, 3.5f };
        juce::PathStrokeType (1.6f).createDashedStroke (dashed, line, dashes, 2);
        g.fillPath (dashed);
    }

    {
        const float clean = owner.processor.apvts.getRawParameterValue ("clean")->load();
        const auto f1 = anton (16.0f, 0.1f), f2 = anton (24.0f);
        const juce::String l1 = "MASTER CLEAN", l2 = juce::String (juce::roundToInt (clean)) + "%";
        const float w1 = textWidth (f1, l1), w2 = textWidth (f2, l2), gap = 8.0f;
        float x = card.getCentreX() - (w1 + gap + w2) * 0.5f;
        g.setColour (col::text);
        g.setFont (f1);
        g.drawText (l1, juce::Rectangle<float> (x, 316.0f, w1 + 4.0f, 30.0f), juce::Justification::bottomLeft, false);
        g.setColour (col::yellow);
        g.setFont (f2);
        g.drawText (l2, juce::Rectangle<float> (x + w1 + gap, 312.0f, w2 + 6.0f, 34.0f), juce::Justification::bottomLeft, false);
    }

    g.setColour (col::hint);
    g.setFont (outfit (12.0f, false));
    g.drawText ("Drag it: all 8 modules follow", juce::Rectangle<float> (card.getX(), 350.0f, card.getWidth(), 18.0f),
                juce::Justification::centred, false);

    // Module panel labels
    g.setColour (col::label);
    g.setFont (outfit (10.0f, true, 0.2f));
    g.drawText ("CLEAN UP", juce::Rectangle<float> (296.0f, 128.0f, 200.0f, 14.0f), juce::Justification::centredLeft, false);
    g.drawText ("STEM REPAIR", juce::Rectangle<float> (296.0f, 271.0f, 200.0f, 14.0f), juce::Justification::centredLeft, false);
    g.setColour (col::panelLine);
    g.fillRect (juce::Rectangle<float> (779.0f, 118.0f, 1.0f, 278.0f));

    // Footer
    g.setColour (col::text);
    g.setFont (outfit (12.0f, false, 0.22f));
    g.drawText ("CREATED BY", juce::Rectangle<float> (owner.footerCreatedX, (float) footerTop, 140.0f, (float) footerHeight),
                juce::Justification::centredLeft, false);
    g.drawText ("AND", juce::Rectangle<float> (owner.footerAndX, (float) footerTop, 60.0f, (float) footerHeight),
                juce::Justification::centredLeft, false);
}
