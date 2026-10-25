#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TremoloEngine.h"

class BrownstoneProcessor : public juce::AudioProcessor
{
public:
    BrownstoneProcessor();
    ~BrownstoneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brownstone 61"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    float getLampLevel() const { return engine.lampLevel.load(); }

private:
    brownstone::TremoloEngine engine;

    std::atomic<float>* pSpeed = nullptr;
    std::atomic<float>* pDivision = nullptr;
    std::atomic<float>* pIntensity = nullptr;
    std::atomic<float>* pCrossover = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pSync = nullptr;
    std::atomic<float>* pOn = nullptr;
    std::atomic<float>* pStereo = nullptr;
    std::atomic<float>* pOutput = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrownstoneProcessor)
};
