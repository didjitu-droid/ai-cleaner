#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace aic;

juce::AudioProcessorValueTreeState::ParameterLayout AICleanerProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = AudioParameterFloatAttributes().withLabel ("%")
                   .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "clean", 1 }, "Master Clean",
                                                        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 65.0f, pct));

    StringArray stems;
    for (int s = 0; s < numStems; ++s) stems.add (stemName (s));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "stem", 1 }, "Stem", stems, 0));

    const auto& vocal = stemPreset (vocals);
    for (int m = 0; m < numModules; ++m)
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { moduleParamID (m), 1 }, moduleName (m),
                                                            NormalisableRange<float> (0.0f, 100.0f, 0.1f),
                                                            vocal.amounts[(size_t) m], pct));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "autoCuts", 1 }, "Auto Cuts", true));

    auto hz = AudioParameterFloatAttributes().withLabel ("Hz")
                  .withStringFromValueFunction ([] (float v, int)
                  {
                      return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz";
                  });

    NormalisableRange<float> lowRange (20.0f, 500.0f, 0.1f);
    lowRange.setSkewForCentre (90.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lowCut", 1 }, "Low Cut", lowRange, vocal.lowCut, hz));

    NormalisableRange<float> highRange (2000.0f, 20000.0f, 1.0f);
    highRange.setSkewForCentre (8000.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "highCut", 1 }, "High Cut", highRange, vocal.highCut, hz));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                                                        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                        AudioParameterFloatAttributes().withLabel ("dB")
                                                            .withStringFromValueFunction ([] (float v, int)
                                                            { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; })));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                                                        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "autoGain", 1 }, "Auto Gain", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "delta", 1 }, "Listen: removed", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

AICleanerProcessor::AICleanerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AICleaner", createLayout())
{
    cleanParam    = apvts.getRawParameterValue ("clean");
    stemParam     = apvts.getRawParameterValue ("stem");
    for (int m = 0; m < numModules; ++m)
        moduleParams[(size_t) m] = apvts.getRawParameterValue (moduleParamID (m));
    autoCutsParam = apvts.getRawParameterValue ("autoCuts");
    lowCutParam   = apvts.getRawParameterValue ("lowCut");
    highCutParam  = apvts.getRawParameterValue ("highCut");
    outputParam   = apvts.getRawParameterValue ("output");
    mixParam      = apvts.getRawParameterValue ("mix");
    autoGainParam = apvts.getRawParameterValue ("autoGain");
    deltaParam    = apvts.getRawParameterValue ("delta");
    bypassParam   = apvts.getRawParameterValue ("bypass");
}

bool AICleanerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void AICleanerProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, juce::jmax (1, getTotalNumOutputChannels()));
    setLatencySamples (engine.getLatencySamples());
}

void AICleanerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    EngineParams p;
    const float heart = cleanParam->load() / 100.0f;
    for (int m = 0; m < numModules; ++m)
        p.amounts[(size_t) m] = juce::jlimit (0.0f, 1.0f, moduleParams[(size_t) m]->load() / 100.0f * heart);
    p.stem         = (int) stemParam->load();
    p.autoCuts     = autoCutsParam->load() > 0.5f;
    p.lowCut       = lowCutParam->load();
    p.highCut      = highCutParam->load();
    p.outputGainDb = outputParam->load();
    p.mix          = mixParam->load() / 100.0f;
    p.autoGain     = autoGainParam->load() > 0.5f;
    p.delta        = deltaParam->load() > 0.5f;
    p.bypass       = bypassParam->load() > 0.5f;

    engine.process (buffer, p);
}

void AICleanerProcessor::applyStemPreset (int stem)
{
    stem = juce::jlimit (0, numStems - 1, stem);
    const auto& preset = stemPreset (stem);

    auto set = [this] (const juce::String& id, float plainValue)
    {
        if (auto* param = apvts.getParameter (id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
            param->endChangeGesture();
        }
    };

    set ("stem", (float) stem);
    for (int m = 0; m < numModules; ++m)
        set (moduleParamID (m), preset.amounts[(size_t) m]);
    set ("lowCut", preset.lowCut);
    set ("highCut", preset.highCut);
}

juce::AudioProcessorEditor* AICleanerProcessor::createEditor()
{
    return new AICleanerEditor (*this);
}

void AICleanerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("editorScale", editorScale, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AICleanerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            editorScale = (float) (double) tree.getProperty ("editorScale", 1.0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AICleanerProcessor();
}
