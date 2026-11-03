#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CleanerEngine.h"

class AICleanerProcessor : public juce::AudioProcessor
{
public:
    AICleanerProcessor();
    ~AICleanerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AI Cleaner"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Loads the per-stem starting point: stem type, all module maximums and the manual cuts.
    void applyStemPreset (int stem);

    juce::AudioProcessorValueTreeState apvts;
    aic::CleanerEngine engine;

    float getEditorScale() const noexcept { return editorScale; }
    void  setEditorScale (float s) noexcept { editorScale = s; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    std::atomic<float>* cleanParam = nullptr;
    std::atomic<float>* stemParam = nullptr;
    std::array<std::atomic<float>*, aic::numModules> moduleParams {};
    std::atomic<float>* autoCutsParam = nullptr;
    std::atomic<float>* lowCutParam = nullptr;
    std::atomic<float>* highCutParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* autoGainParam = nullptr;
    std::atomic<float>* deltaParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    float editorScale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AICleanerProcessor)
};
