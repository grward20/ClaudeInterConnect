#include "PluginProcessor.h"

class ConnectionServer : public juce::InterprocessConnectionServer
{
public:
    ConnectionServer(ClaudeInterConnectAudioProcessor& owner) : owner_(owner) {}

    juce::InterprocessConnection* createConnectionObject() override
    {
        return &owner_;
    }

private:
    ClaudeInterConnectAudioProcessor& owner_;
};

ClaudeInterConnectAudioProcessor::ClaudeInterConnectAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      InterprocessConnection(true)
{
    // Add a small delay to ensure APVTS is fully initialized
    juce::Timer::callAfterDelay(100, [this]()
    {
        auto* inOutParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("InOut"));
        jassert(inOutParam != nullptr);
        
        // Debug the initial parameter value
        DBG("Initial InOut parameter value: " + String(inOutParam->get() ? "true" : "false"));
        
        // Add parameter listener
        apvts.addParameterListener("InOut", this);
        
        if (inOutParam->get()) // Sender mode
        {
            DBG("=== INITIALIZING AS SENDER ===");
            isServer = true;
            connectionServer.reset(new ConnectionServer(*this));
            
            if (!connectionServer->beginWaitingForSocket(PORT_NUMBER))
            {
                DBG("Failed to start server on port " + juce::String(PORT_NUMBER));
            }
            else
            {
                DBG("Server successfully started and waiting on port " + juce::String(PORT_NUMBER));
            }
        }
        else // Receiver mode
        {
            DBG("=== INITIALIZING AS RECEIVER ===");
            isServer = false;
            
            DBG("Attempting to connect to server on port " + juce::String(PORT_NUMBER));
            if (!connectToSocket("localhost", PORT_NUMBER, 2000))
            {
                DBG("Failed to connect to server. Make sure sender instance is started first!");
            }
            else
            {
                DBG("Successfully connected to server as client");
            }
        }
    });
}

ClaudeInterConnectAudioProcessor::~ClaudeInterConnectAudioProcessor()
{
    apvts.removeParameterListener("InOut", this);
    if (isServer)
        connectionServer->stop();
    disconnect();
}

void ClaudeInterConnectAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sharedAudioBuffer.setSize(2, samplesPerBlock);
    sharedAudioBuffer.clear();
}

void ClaudeInterConnectAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto* inOutParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("InOut"));
        bool isSender = inOutParam->get();

    if (isSender && isConnected)
    {
        // Add debug for audio sending
        DBG("Sending audio block: " + juce::String(buffer.getNumSamples()) + " samples");
        // Server/Sender: send audio data
        juce::MemoryBlock dataToSend;
        const size_t bufferSize = sizeof(float) * buffer.getNumSamples() * buffer.getNumChannels();
        dataToSend.setSize(bufferSize);
        
        float* rawData = static_cast<float*>(dataToSend.getData());
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const float* channelData = buffer.getReadPointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                rawData[sample * buffer.getNumChannels() + channel] = channelData[sample];
            }
        }
        
        sendMessage(dataToSend);
    }
    else if (!isSender && isConnected)
    {
        // Add debug for audio receiving
        DBG("Receiving audio block");
        // Client/Receiver: process received audio data
        buffer.clear();
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            buffer.addFrom(channel, 0, sharedAudioBuffer.getReadPointer(channel),
                         buffer.getNumSamples());
        }
    }
    else
    {
        DBG("Not processing audio - isSender: " + (isSender ? String("true") : String("false")) +
            ", isConnected: " + (isConnected.load() ? String("true") : String("false")));
    }
}

void ClaudeInterConnectAudioProcessor::connectionMade()
{
    isConnected = true;
    DBG("Connection established");
}

void ClaudeInterConnectAudioProcessor::connectionLost()
{
    isConnected = false;
    DBG("Connection lost");
}

void ClaudeInterConnectAudioProcessor::messageReceived(const juce::MemoryBlock& message)
{
    if (!isServer)
    {
        
        DBG("Received message of size: " + juce::String(message.getSize()));
        const float* rawData = static_cast<const float*>(message.getData());
        const size_t numSamples = message.getSize() / (sizeof(float) * 2); // Changed to size_t

        for (int channel = 0; channel < 2; ++channel)
        {
            float* writePtr = sharedAudioBuffer.getWritePointer(channel);
            for (size_t sample = 0; sample < numSamples; ++sample) // Changed to size_t
            {
                writePtr[sample] = rawData[sample * 2 + channel];
            }
        }
    }
}

juce::AudioProcessorEditor* ClaudeInterConnectAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void ClaudeInterConnectAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // saves the state - gw written
    MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void ClaudeInterConnectAudioProcessor::releaseResources()
{
}

void ClaudeInterConnectAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto tree = ValueTree::readFromData(data, sizeInBytes);
    if ( tree.isValid())
    {
        apvts.replaceState(tree);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
    ClaudeInterConnectAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    
    layout.add(std::make_unique<juce::AudioParameterBool>(ParameterID {"InOut", 1}, // parameterID
                                                          "Send/Receive",  // parameter name
                                                          false           // default value
                                                          ));
    
    return layout;
}

void ClaudeInterConnectAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "InOut")
    {
        DBG("InOut parameter changed to: " + String(newValue > 0.5f ? "Sender" : "Receiver"));
    }
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ClaudeInterConnectAudioProcessor();
}
