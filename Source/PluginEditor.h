#pragma once

#include "PluginProcessor.h"
#include "UIComponents.h"

class AICleanerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AICleanerEditor (AICleanerProcessor&);
    ~AICleanerEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    static constexpr int baseWidth = 900, baseHeight = 560;

private:
    // Everything is laid out at the design size (900 x 560) inside this component,
    // which is then scaled to the window size.
    class Content : public juce::Component
    {
    public:
        explicit Content (AICleanerEditor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
    private:
        AICleanerEditor& owner;
    };

    void timerCallback() override;
    void layoutFooter();

    AICleanerProcessor& processor;
    Content content { *this };
    juce::TooltipWindow tooltips { this, 600 };

    ui::PillButton autoGainButton { "Auto Gain" };
    ui::PillButton deltaButton { "Listen: removed", "Listening: removed" };
    ui::PillButton bypassButton { "Bypass", "Bypassed" };
    juce::OwnedArray<ui::PillButton> stemButtons;
    ui::PillButton autoCutsButton { juce::String (juce::CharPointer_UTF8 ("AUTO off \xc2\xb7 manual")),
                                   juce::String (juce::CharPointer_UTF8 ("AUTO \xc2\xb7 detected")) };

    std::unique_ptr<ui::HeartControl> heart;
    juce::OwnedArray<ui::Knob> moduleKnobs;
    ui::Knob outputKnob { "Output", ui::col::text, ui::Knob::Kind::plain };
    ui::Knob mixKnob { "Mix", ui::col::text, ui::Knob::Kind::plain };
    std::unique_ptr<ui::FreqDisplay> freq;

    ui::InstaLink maauraLink { "MAAURA", "https://www.instagram.com/maaura.music/" };
    ui::InstaLink danidaneLink { "DANIDANE", "https://www.instagram.com/danidane.music/" };
    float footerCreatedX = 0.0f, footerAndX = 0.0f;

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<ButtonAttachment> autoGainAtt, deltaAtt, bypassAtt, autoCutsAtt;
    juce::OwnedArray<SliderAttachment> moduleAtts;
    std::unique_ptr<SliderAttachment> outputAtt, mixAtt;

    float lastClean = -1.0f;
    int lastStem = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AICleanerEditor)
};
