#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// JUCE Standalone compatibility
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BrainwaveMeterProcessor();
}

//==============================================================================
BrainwaveMeterProcessor::BrainwaveMeterProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameters())
{
    sensitivity = apvts.getRawParameterValue ("SENSITIVITY");
    smoothing   = apvts.getRawParameterValue ("SMOOTHING");
}

void BrainwaveMeterProcessor::prepareToPlay (double sampleRate, int)
{
    analyzer.prepare (sampleRate);
}

void BrainwaveMeterProcessor::releaseResources() {}

void BrainwaveMeterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    if (numChannels == 0 || numSamples == 0)
        return;
    
    // Sensitivity gain
    const float gain = sensitivity ? std::pow (10.0f, *sensitivity / 20.0f) : 1.0f;
    
    // Push samples to analyzer (mono sum for analysis)
    for (int i = 0; i < numSamples; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            mono += buffer.getSample (ch, i);
        mono /= numChannels;
        analyzer.pushSample (mono * gain);
    }
    
    // Update cached analysis for GUI thread
    {
        juce::ScopedLock sl (lock);
        cached.bands       = analyzer.getBandLevels();
        cached.bpm         = analyzer.getBPM();
        cached.binauralHz  = analyzer.getBinauralHz();
        cached.dominant    = analyzer.getDominantBand();
        cached.totalEnergy = analyzer.getTotalEnergy();
    }
}

void BrainwaveMeterProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    // Convert to float and delegate
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();
    juce::AudioBuffer<float> floatBuffer (numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const double* src = buffer.getReadPointer (ch);
        float* dst = floatBuffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            dst[i] = static_cast<float> (src[i]);
    }
    processBlock (floatBuffer, midi);
}

BrainwaveMeterProcessor::Analysis BrainwaveMeterProcessor::getAnalysis() const
{
    juce::ScopedLock sl (lock);
    return cached;
}

juce::AudioProcessorEditor* BrainwaveMeterProcessor::createEditor()
{
    return new BrainwaveMeterEditor (*this);
}

juce::AudioProcessorValueTreeState::ParameterLayout BrainwaveMeterProcessor::createParameters()
{
    return
    {
        std::make_unique<juce::AudioParameterFloat> (
            "SENSITIVITY", "Sensitivity",
            juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f,
            "dB", juce::AudioProcessorParameter::genericParameter,
            [](float v, int) { return juce::String (v, 1) + " dB"; },
            [](const juce::String& s) { return s.getFloatValue(); }
        ),
        std::make_unique<juce::AudioParameterFloat> (
            "SMOOTHING", "Smoothing",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f,
            "%", juce::AudioProcessorParameter::genericParameter,
            [](float v, int) { return juce::String (static_cast<int> (v * 100)) + "%"; },
            [](const juce::String& s) { return s.getFloatValue() / 100.0f; }
        )
    };
}
