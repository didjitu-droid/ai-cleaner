#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "CleanerEngine.h"

namespace ui
{
    // Colours from the design (sunset / Vice City palette).
    namespace col
    {
        const juce::Colour text      { 0xffFFF2F7 };
        const juce::Colour muted     { 0xffD9C6EF };
        const juce::Colour label     { 0xffB9A4D6 };
        const juce::Colour hint      { 0xffEBDDF8 };
        const juce::Colour panel     { 0xff160A28 };
        const juce::Colour panelLine { 0xff3B1E62 };
        const juce::Colour track     { 0xff2E1A4D };
        const juce::Colour face      { 0xff24123F };
        const juce::Colour pillLine  { 0xff5E3A8C };
        const juce::Colour pink      { 0xffFF2E88 };
        const juce::Colour orange    { 0xffFF8A3D };
        const juce::Colour yellow    { 0xffFFC857 };
        const juce::Colour teal      { 0xff2CE0D0 };
        const juce::Colour violet    { 0xffA78BFA };
        const juce::Colour magenta   { 0xffC026D3 };
    }

    juce::Font anton (float height, float kerning = 0.0f);
    juce::Font outfit (float height, bool bold, float kerning = 0.0f);
    float textWidth (const juce::Font& font, const juce::String& text);
    void drawSpacedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                         const juce::Font& font, juce::Justification just);

    //==========================================================================
    // Rounded "pill" toggle used for Auto Gain, Listen, Bypass, stems and AUTO.
    class PillButton : public juce::Button
    {
    public:
        PillButton (const juce::String& offText, const juce::String& onText = {});

        juce::Colour border = col::pillLine, onBorder = col::pink;
        juce::Colour offFill = col::panel, offTextColour = col::text;
        juce::Colour onFill = col::pink, onTextColour = col::panel;
        float fontSize = 13.0f;

        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    private:
        juce::String textOff, textOn;
    };

    //==========================================================================
    // Rotary knob. For cleaning modules the bright arc shows the effective amount
    // (module maximum x heart) and a small dot shows the module maximum.
    class Knob : public juce::Slider
    {
    public:
        enum class Kind { module, plain };

        Knob (const juce::String& name, juce::Colour colour, Kind kind);

        std::function<float()> heart;                 // 0..1, modules only
        std::function<juce::String (double)> format;  // value text for plain knobs

        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override;

    private:
        juce::String title;
        juce::Colour colour;
        Kind kind;
    };

    //==========================================================================
    // The beating heart: one control for the whole plugin.
    class HeartControl : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        HeartControl (juce::RangedAudioParameter& cleanParam, std::function<bool()> isListeningToRemoved);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        bool keyPressed (const juce::KeyPress&) override;

    private:
        void timerCallback() override;
        static juce::Path heartPath();

        juce::RangedAudioParameter& param;
        juce::ParameterAttachment attachment;
        std::function<bool()> isDelta;
        float value = 65.0f;          // 0..100
        float dragStartValue = 0.0f, dragStartY = 0.0f;
        bool dragging = false;
        double beatPhase = 0.0, wavePhase = 0.0;
        juce::Path heart = heartPath();
    };

    //==========================================================================
    // Spectrum + low/high cut curve with draggable handles.
    class FreqDisplay : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        FreqDisplay (juce::AudioProcessorValueTreeState&, aic::CleanerEngine&);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;

        // Area (in this component) where the AUTO button sits, so the header leaves room for it.
        static constexpr float autoButtonX = 140.0f;

    private:
        void timerCallback() override;
        juce::Rectangle<float> plotArea() const;
        float freqToX (float f) const;
        float xToFreq (float x) const;
        float shownLow() const;
        float shownHigh() const;
        static juce::String formatHz (float f);

        juce::AudioProcessorValueTreeState& apvts;
        aic::CleanerEngine& engine;
        juce::RangedAudioParameter& lowParam;
        juce::RangedAudioParameter& highParam;
        juce::RangedAudioParameter& autoParam;
        juce::ParameterAttachment lowAtt, highAtt, autoAtt;
        float lowValue = 90.0f, highValue = 16000.0f;
        bool autoOn = true;

        enum class Drag { none, low, high } dragging = Drag::none;
        std::array<float, aic::CleanerEngine::displayPoints> spectrum {};
    };

    //==========================================================================
    // "MAAURA" / "DANIDANE" with the Instagram glyph; opens the profile.
    class InstaLink : public juce::Button
    {
    public:
        InstaLink (const juce::String& name, const juce::String& url);
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        void clicked() override;
        float preferredWidth() const;

    private:
        juce::URL url;
    };
}
