/*
  Brainwave Meter — GUI Editor
  Dark theme with animated band meters, BPM display, binaural detection
*/

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class BrainwaveMeterEditor  : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    BrainwaveMeterEditor (BrainwaveMeterProcessor&);
    ~BrainwaveMeterEditor() override = default;
    
    void paint (juce::Graphics&) override;
    void resized() override;
    
private:
    void timerCallback() override { repaint(); }
    
    BrainwaveMeterProcessor& processor;
    
    juce::Slider sensitivitySlider, smoothingSlider;
    juce::Label  sensitivityLabel,  smoothingLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment sensitivityAttach;
    juce::AudioProcessorValueTreeState::SliderAttachment smoothingAttach;
    
    // Cached analysis for smooth animations
    BrainwaveMeterProcessor::Analysis analysis;
    std::array<float, 5> displayBands = {};
    float displayBPM = 120.0f;
    float displayBinaural = 0.0f;
    int   displayDominant = 3;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrainwaveMeterEditor)
};
