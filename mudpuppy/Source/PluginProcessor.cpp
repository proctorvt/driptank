#include "PluginProcessor.h"
#include "PluginEditor.h"

MudpuppyProcessor::MudpuppyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MUDPUPPY", createLayout())
{
    pDelay    = apvts.getRawParameterValue ("delay");
    pFeedback = apvts.getRawParameterValue ("feedback");
    pBlend    = apvts.getRawParameterValue ("blend");
    pWobble   = apvts.getRawParameterValue ("wobble");
    pOutput   = apvts.getRawParameterValue ("output");
    pOn       = apvts.getRawParameterValue ("on");
    pTrails   = apvts.getRawParameterValue ("trails");
}

juce::AudioProcessorValueTreeState::ParameterLayout MudpuppyProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto knob = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); });

    NormalisableRange<float> delayRange (mudpuppy::kMinDelayMs, mudpuppy::kMaxDelayMs, 0.1f);
    delayRange.setSkewForCentre (110.0f); // the slapback zone gets the middle of the knob
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "delay", 1 }, "Delay", delayRange, 110.0f,
        AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return String ((int) std::lround (v)) + " ms"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "feedback", 1 }, "Feedback",
                                                        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 2.5f, knob));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "blend", 1 }, "Blend",
                                                        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f, knob));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "wobble", 1 }, "Wobble",
                                                        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 2.0f, knob));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
            [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; })));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "on", 1 }, "Delay On", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "trails", 1 }, "Trails", false));
    return { p.begin(), p.end() };
}

bool MudpuppyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    return in.size() <= out.size();
}

void MudpuppyProcessor::prepareToPlay (double sampleRate, int) { engine.prepare (sampleRate); }

void MudpuppyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);
    if (numIn == 0 || numOut == 0)
        return;

    mudpuppy::EngineParams p;
    p.delayMs = pDelay->load();
    p.feedback = pFeedback->load();
    p.blend = pBlend->load();
    p.wobble = pWobble->load();
    p.outputDb = pOutput->load();
    p.on = pOn->load() > 0.5f;
    p.trails = pTrails->load() > 0.5f;

    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : nullptr;
    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    engine.process (inL, inR, outL, outR, n, p); // reads each input sample before writing it
}

void MudpuppyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MudpuppyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MudpuppyProcessor::createEditor() { return new MudpuppyEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MudpuppyProcessor(); }
