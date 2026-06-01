/*
  Brainwave Meter — GUI Editor
  Matrix-dark theme with clickable band panels, quantum/aura info,
  spectrum analyzer, particle rain, and full paranormal readout.
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
    void resized() override;
    
private:
    void timerCallback() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    
    void drawMatrixRain (juce::Graphics&);
    void drawSpectrum  (juce::Graphics&);
    void drawMeterBar  (juce::Graphics&, int bandIndex, juce::Rectangle<int>, float level, bool dominant);
    void drawInfoPanel (juce::Graphics&, juce::Rectangle<int>);
    
    BrainwaveMeterProcessor& processor;
    
    // Which band is expanded (-1 = none)
    int expandedBand = -1;
    int hoveredBand  = -1;
    
    // Matrix rain particles
    struct RainDrop { float x, y, speed, brightness; char glyph; };
    std::vector<RainDrop> rain;
    juce::Random rng;
    
    // Spectrum history for waterfall
    static constexpr int specHistLen = 64;
    std::array<std::array<float, 5>, specHistLen> specHistory = {};
    int specWritePos = 0;
    
    // Cached analysis (updated on timer, read on paint)
    BrainwaveMeterProcessor::Analysis currentAnalysis;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrainwaveMeterEditor)
};
