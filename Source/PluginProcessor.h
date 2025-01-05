#pragma once

#include <JuceHeader.h>

class ClaudeInterConnectAudioProcessor : public juce::AudioProcessor,
                                       public juce::InterprocessConnection,
                                       public juce::AudioProcessorValueTreeState::Listener
{
public:
    ClaudeInterConnectAudioProcessor();
    ~ClaudeInterConnectAudioProcessor() override;

    // Standard JUCE plugin methods
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    
    // InterprocessConnection override methods
    void connectionMade() override;
    void connectionLost() override;
    void messageReceived(const juce::MemoryBlock& message) override;

    // Editor methods
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // Plugin properties
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Program handling
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override {}
    const juce::String getProgramName(int index) override { return {}; }
    void changeProgramName(int index, const juce::String& newName) override {}

    // State handling
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    // Parameter listener method
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    
    // APVTS methods and instance
    using APVTS = juce::AudioProcessorValueTreeState;
    static APVTS::ParameterLayout createParameterLayout();
    APVTS apvts{*this, nullptr, "Parameters", createParameterLayout()};

private:
    static const int PORT_NUMBER = 52364;
    bool isServer = false;
    std::unique_ptr<juce::InterprocessConnectionServer> connectionServer;
    juce::AudioBuffer<float> sharedAudioBuffer;
    std::atomic<bool> isConnected{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClaudeInterConnectAudioProcessor)
};
