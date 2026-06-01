/*
  Brainwave Meter — GUI Editor
  Dark theme with animated band meters, BPM, binaural detection
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
    ~BrainwaveMeterEditor() override;
    
    void paint (juce::Graphics&) override;
    void resized() override {}
    
private:
    void timerCallback() override;
    
    BrainwaveMeterProcessor& processor;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrainwaveMeterEditor)
};
