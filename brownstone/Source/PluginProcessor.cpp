#include "PluginProcessor.h"
#include "PluginEditor.h"

BrownstoneProcessor::BrownstoneProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BROWNSTONE", createLayout())
{
    pSpeed     = apvts.getRawParameterValue ("speed");
    pDivision  = apvts.getRawParameterValue ("division");
    pIntensity = apvts.getRawParameterValue ("intensity");
    pCrossover = apvts.getRawParameterValue ("crossover");
    pMode      = apvts.getRawParameterValue ("mode");
    pSync      = apvts.getRawParameterValue ("sync");
    pOn        = apvts.getRawParameterValue ("on");
    pStereo    = apvts.getRawParameterValue ("stereo");
    pOutput    = apvts.getRawParameterValue ("output");
}

juce::AudioProcessorValueTreeState::ParameterLayout BrownstoneProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto knobAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); });

    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "speed", 1 }, "Speed", NormalisableRange<float> (0.0f, 10.0f, 0.01f), 4.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return String (v, 1) + "  (" + String (brownstone::speedToHz (v), 1) + " Hz)"; })));

    StringArray divs;
    for (int i = 0; i < brownstone::kNumDivisions; ++i)
        divs.add (brownstone::divisionName (i));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Sync Rate", divs, 3));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "intensity", 1 }, "Intensity",
                                                        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 6.0f, knobAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "crossover", 1 }, "Crossover", NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return String (v, 1) + "  (" + String ((int) brownstone::crossoverToHz (v)) + " Hz)"; })));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                                                         StringArray { "Harmonic", "Bias", "Opto" }, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Tempo Sync", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "on", 1 }, "Tremolo On", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "stereo", 1 }, "Stereo", false));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
            [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; })));

    return { p.begin(), p.end() };
}

bool BrownstoneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    return in.size() <= out.size();
}

void BrownstoneProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

void BrownstoneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);
    if (numIn == 0 || numOut == 0)
        return;

    brownstone::EngineParams p;
    p.speed = pSpeed->load();
    p.division = (int) std::lround (pDivision->load());
    p.intensity = pIntensity->load();
    p.crossover = pCrossover->load();
    p.mode = (brownstone::Mode) juce::jlimit (0, 2, (int) std::lround (pMode->load()));
    p.sync = pSync->load() > 0.5f;
    p.on = pOn->load() > 0.5f;
    p.stereo = pStereo->load() > 0.5f;
    p.outputDb = pOutput->load();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) p.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) p.ppqAtBlockStart = *ppq;
            p.playing = pos->getIsPlaying() && pos->getPpqPosition().hasValue();
        }

    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : nullptr;
    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    engine.process (inL, inR, outL, outR, n, p); // reads each input sample before writing it
}

void BrownstoneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BrownstoneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* BrownstoneProcessor::createEditor() { return new BrownstoneEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BrownstoneProcessor(); }
