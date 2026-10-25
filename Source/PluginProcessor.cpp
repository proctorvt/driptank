#include "PluginProcessor.h"
#include "PluginEditor.h"

DriptankProcessor::DriptankProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DRIPTANK", createLayout())
{
    pDwell  = apvts.getRawParameterValue ("dwell");
    pMix    = apvts.getRawParameterValue ("mix");
    pTone   = apvts.getRawParameterValue ("tone");
    pTank   = apvts.getRawParameterValue ("tank");
    pOutput = apvts.getRawParameterValue ("output");
    pOn     = apvts.getRawParameterValue ("reverb");
    pStereo = apvts.getRawParameterValue ("stereo");
    pKick   = apvts.getRawParameterValue ("kick");
}

juce::AudioProcessorValueTreeState::ParameterLayout DriptankProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto knob = [] (float v, int) { return String (v, 1); };
    auto knobAttr = AudioParameterFloatAttributes().withStringFromValueFunction (knob);

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dwell", 1 }, "Dwell",
                                                        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 6.0f, knobAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mixer",
                                                        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 4.0f, knobAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tone", 1 }, "Tone",
                                                        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 6.0f, knobAttr));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "tank", 1 }, "Tank",
                                                         StringArray { "Short", "Medium", "Long" }, 2));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
            [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; })));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "reverb", 1 }, "Reverb On", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "stereo", 1 }, "Stereo Springs", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "kick", 1 }, "Kick Tank", false));

    return { p.begin(), p.end() };
}

bool DriptankProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;
    return in.size() <= out.size(); // mono->mono, mono->stereo, stereo->stereo
}

void DriptankProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

void DriptankProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    driptank::EngineParams p;
    p.dwell = pDwell->load();
    p.mix = pMix->load();
    p.tone = pTone->load();
    p.tank = (int) std::lround (pTank->load());
    p.outputDb = pOutput->load();
    p.reverbOn = pOn->load() > 0.5f;
    p.stereo = pStereo->load() > 0.5f;

    // kick on the rising edge, so it can be automated or MIDI-learned in the DAW
    const bool k = pKick->load() > 0.5f;
    if (k && ! lastKick)
        engine.triggerKick();
    lastKick = k;

    if (numOut == 0 || numIn == 0)
        return;

    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : nullptr;
    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    // engine reads each input sample before writing that output sample, so in-place is safe
    engine.process (inL, inR, outL, outR, n, p);
}

void DriptankProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DriptankProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* DriptankProcessor::createEditor()
{
    return new DriptankEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DriptankProcessor();
}
