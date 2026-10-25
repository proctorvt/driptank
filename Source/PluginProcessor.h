#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SpringTankEngine.h"

class DriptankProcessor : public juce::AudioProcessor
{
public:
    DriptankProcessor();
    ~DriptankProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Driptank 63"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    /** Kicking the tank from the GUI button. */
    void kick() { engine.triggerKick(); }

private:
    driptank::SpringTankEngine engine;

    std::atomic<float>* pDwell = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pTone = nullptr;
    std::atomic<float>* pTank = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pOn = nullptr;
    std::atomic<float>* pStereo = nullptr;
    std::atomic<float>* pKick = nullptr;
    bool lastKick = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriptankProcessor)
};
