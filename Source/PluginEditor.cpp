#include "PluginEditor.h"

namespace BwmColours
{
    static const juce::Colour bg           { 0xFF0A0A14 };
    static const juce::Colour panelBg      { 0xFF12121E };
    static const juce::Colour gridLine     { 0xFF1A1A2E };
    static const juce::Colour text         { 0xFFB8B8D0 };
    static const juce::Colour textDim      { 0xFF606080 };
    static const juce::Colour accent       { 0xFF4FC3F7 };
}

//==============================================================================
BrainwaveMeterEditor::BrainwaveMeterEditor (BrainwaveMeterProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setOpaque (true);
    startTimerHz (30);
    setSize (460, 340);
}

BrainwaveMeterEditor::~BrainwaveMeterEditor()
{
    stopTimer();
}

//==============================================================================
void BrainwaveMeterEditor::paint (juce::Graphics& g)
{
    using namespace BwmColours;
    
    g.fillAll (bg);
    
    const auto a = processor.getAnalysis();
    
    // ── Title ──
    g.setColour (accent);
    g.setFont (juce::Font (16.0f).withStyle (juce::Font::bold));
    g.drawText ("BRAINWAVE METER", getLocalBounds().removeFromTop (24),
                juce::Justification::centred, false);
    
    // ── Band meters ──
    auto meterArea = getLocalBounds().reduced (20, 36);
    const int bandW = meterArea.getWidth() / 5;
    
    for (int b = 0; b < 5; ++b)
    {
        const auto& band = brainwaveBands[b];
        const float level = a.bands[b];
        const int x = meterArea.getX() + b * bandW;
        const int y = meterArea.getY();
        const int w = bandW - 6;
        const int h = meterArea.getHeight() - 40;
        
        // Background
        g.setColour (panelBg);
        g.fillRoundedRectangle (x, y, w, h, 4.0f);
        
        // Grid lines
        g.setColour (gridLine);
        for (int i = 1; i < 10; ++i)
        {
            float gy = y + h * i / 10.0f;
            g.drawHorizontalLine (static_cast<int> (gy), x + 2.0f, x + w - 2.0f);
        }
        
        // Filled meter (bottom-up, with gradient)
        const int fillH = static_cast<int> (h * juce::jlimit (0.0f, 1.0f, level));
        if (fillH > 0)
        {
            juce::ColourGradient grad (band.color.withAlpha (0.9f), 
                                       static_cast<float> (x), static_cast<float> (y + h - fillH),
                                       band.color.withAlpha (0.3f), 
                                       static_cast<float> (x), static_cast<float> (y + h),
                                       false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (x + 2, y + h - fillH, w - 4, fillH, 3.0f);
        }
        
        // Dominant band highlight
        if (b == a.dominant)
        {
            g.setColour (juce::Colours::white.withAlpha (0.15f));
            g.fillRoundedRectangle (x, y, w, h, 4.0f);
        }
        
        // Level text
        g.setColour (text);
        g.setFont (9.0f);
        g.drawText (juce::String (level * 100.0f, 0) + "%", 
                    x, y + 2, w, 14, juce::Justification::centredRight, false);
        
        // Band name
        g.setColour (band.color);
        g.setFont (juce::Font (11.0f).withStyle (juce::Font::bold));
        g.drawText (band.name, x, y + h + 3, w, 16, juce::Justification::centred, false);
        
        // Hz range
        g.setColour (textDim);
        g.setFont (8.0f);
        g.drawText (juce::String (band.lowHz, 1) + "-" + juce::String (band.highHz, 0) + "Hz",
                    x, y + h + 17, w, 12, juce::Justification::centred, false);
        
        // State label
        g.setColour (text);
        g.setFont (8.0f);
        g.drawText (band.state, x, y + h + 28, w, 12, juce::Justification::centred, false);
    }
    
    // ── Bottom readout ──
    auto bottom = getLocalBounds().removeFromBottom (32).reduced (20, 0);
    
    // BPM
    g.setColour (accent);
    g.setFont (juce::Font (13.0f).withStyle (juce::Font::bold));
    g.drawText ("BPM: " + juce::String (a.bpm, 1), 
                bottom.removeFromLeft (120), juce::Justification::left, false);
    
    // Binaural
    if (a.binauralHz > 0.5f)
    {
        g.setColour (juce::Colours::yellowgreen);
        g.setFont (11.0f);
        g.drawText ("Binaural: " + juce::String (a.binauralHz, 1) + "Hz",
                    bottom.removeFromLeft (140), juce::Justification::left, false);
    }
    
    // Dominant band state
    g.setColour (brainwaveBands[a.dominant].color);
    g.setFont (11.0f);
    g.drawText (brainwaveBands[a.dominant].state,
                bottom, juce::Justification::centredRight, false);
}

void BrainwaveMeterEditor::timerCallback()
{
    repaint();
}
