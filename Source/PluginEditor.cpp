#include "PluginEditor.h"

namespace BwmColours
{
    static const juce::Colour bg           { 0xFF0A0A14 };
    static const juce::Colour panelBg      { 0xFF12121E };
    static const juce::Colour gridLine     { 0xFF1E1E30 };
    static const juce::Colour textDim      { 0xFF6B6B80 };
    static const juce::Colour textBright   { 0xFFE0E0F0 };
}

static juce::Font boldFont (float size)   { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }
static juce::Font normalFont (float size) { return juce::Font (juce::FontOptions (size)); }

//==============================================================================
BrainwaveMeterEditor::BrainwaveMeterEditor (BrainwaveMeterProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      sensitivityAttach (p.getAPVTS(), "SENSITIVITY", sensitivitySlider),
      smoothingAttach   (p.getAPVTS(), "SMOOTHING",   smoothingSlider)
{
    setOpaque (true);
    
    for (auto* slider : { &sensitivitySlider, &smoothingSlider })
    {
        slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
        slider->setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (0xFF4040FF));
        slider->setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xFF202030));
        slider->setColour (juce::Slider::textBoxTextColourId,         BwmColours::textDim);
        slider->setColour (juce::Slider::textBoxBackgroundColourId,   BwmColours::panelBg);
        slider->setColour (juce::Slider::textBoxOutlineColourId,      juce::Colour (0xFF1E1E30));
        addAndMakeVisible (slider);
    }
    
    sensitivityLabel.setText ("SENSITIVITY", juce::dontSendNotification);
    smoothingLabel.setText   ("SMOOTHING",   juce::dontSendNotification);
    for (auto* label : { &sensitivityLabel, &smoothingLabel })
    {
        label->setColour (juce::Label::textColourId, BwmColours::textDim);
        label->setFont (boldFont (10.0f));
        label->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);
    }
    
    setSize (380, 480);
    startTimerHz (30);
}

//==============================================================================
void BrainwaveMeterEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    
    analysis = processor.getAnalysis();
    
    for (int b = 0; b < 5; ++b)
        displayBands[b] += 0.18f * (analysis.bands[b] - displayBands[b]);
    displayBPM      += 0.06f * (analysis.bpm          - displayBPM);
    displayBinaural += 0.08f * (analysis.binauralHz   - displayBinaural);
    displayDominant  = analysis.dominant;
    
    g.fillAll (BwmColours::bg);
    
    auto bounds = getLocalBounds().reduced (16, 12);
    
    // Title
    g.setColour (BwmColours::textBright);
    g.setFont (boldFont (22.0f));
    g.drawText ("BRAINWAVE METER", bounds.removeFromTop (30), Justification::centred);
    
    // Dominant state
    g.setColour (brainwaveBands[(size_t) displayDominant].color.withAlpha (0.9f));
    g.setFont (normalFont (13.0f));
    g.drawText (brainwaveBands[(size_t) displayDominant].state,
               bounds.removeFromTop (18), Justification::centred);
    
    bounds.removeFromTop (8);
    
    // ═══ METER BARS ═══
    auto meterArea = bounds.removeFromTop (220);
    
    constexpr int barWidth   = 48;
    constexpr int barGap     = 12;
    constexpr int totalWidth = 5 * barWidth + 4 * barGap;
    const int startX   = meterArea.getX() + (meterArea.getWidth() - totalWidth) / 2;
    const int barBottom = meterArea.getBottom();
    const int barTop    = meterArea.getY() + 20;
    const int maxBarH   = barBottom - barTop;
    
    for (int b = 0; b < 5; ++b)
    {
        const auto& band = brainwaveBands[(size_t) b];
        const int x = startX + b * (barWidth + barGap);
        
        const float level = jlimit (0.0f, 1.0f, displayBands[(size_t) b]);
        const int barH = static_cast<int> (level * maxBarH);
        
        // Background track
        g.setColour (BwmColours::panelBg);
        g.fillRoundedRectangle (Rectangle<int> (x, barTop, barWidth, maxBarH).toFloat(), 4.0f);
        
        // Filled bar
        if (barH > 2)
        {
            auto fillRect = Rectangle<int> (x, barBottom - barH, barWidth, barH).toFloat();
            
            if (b == displayDominant)
            {
                g.setColour (band.color.withAlpha (0.15f));
                g.fillRoundedRectangle (fillRect.expanded (6, 0), 6.0f);
            }
            
            ColourGradient grad (band.color.withAlpha (0.5f),
                                 fillRect.getX(), fillRect.getBottom(),
                                 band.color,
                                 fillRect.getX(), fillRect.getY(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fillRect, 4.0f);
        }
        
        // Grid lines
        g.setColour (BwmColours::gridLine.withAlpha (0.4f));
        for (int tick = 1; tick < 5; ++tick)
        {
            float y = barTop + (maxBarH * tick / 5.0f);
            g.drawHorizontalLine (static_cast<int> (y), x + 2, x + barWidth - 2);
        }
        
        // Percentage
        g.setColour (BwmColours::textBright);
        g.setFont (boldFont (12.0f));
        g.drawText (String (static_cast<int> (level * 100)) + "%",
                    Rectangle<int> (x, barBottom - barH - 18, barWidth, 16),
                    Justification::centred);
        
        // Band name
        g.setColour (band.color.withAlpha (b == displayDominant ? 1.0f : 0.6f));
        g.setFont (boldFont (10.0f));
        g.drawText (band.name,
                    Rectangle<int> (x - 4, barBottom + 4, barWidth + 8, 14),
                    Justification::centred);
        
        // Frequency range
        g.setColour (BwmColours::textDim);
        g.setFont (normalFont (8.0f));
        g.drawText (String (band.lowHz, 1) + "-" + String (band.highHz, 0) + "Hz",
                    Rectangle<int> (x - 4, barBottom + 17, barWidth + 8, 12),
                    Justification::centred);
    }
    
    bounds.removeFromTop (38);
    
    // ═══ BOTTOM INFO ═══
    g.setColour (BwmColours::gridLine);
    g.drawHorizontalLine (bounds.getY(), bounds.getX(), bounds.getRight());
    bounds.removeFromTop (10);
    
    // BPM
    auto bpmArea = bounds.removeFromTop (40);
    g.setColour (BwmColours::textDim);
    g.setFont (normalFont (10.0f));
    g.drawText ("BPM", bpmArea.removeFromLeft (50), Justification::centredLeft);
    
    g.setColour (BwmColours::textBright);
    g.setFont (boldFont (26.0f));
    g.drawText (String (static_cast<int> (displayBPM + 0.5f)),
                bpmArea, Justification::centredLeft);
    
    // Binaural
    auto binArea = bounds.removeFromTop (30);
    g.setColour (BwmColours::textDim);
    g.setFont (normalFont (10.0f));
    g.drawText ("BINAURAL", binArea.removeFromLeft (70), Justification::centredLeft);
    
    if (displayBinaural > 0.5f)
    {
        const char* binauralState = "-";
        for (int b = 0; b < 5; ++b)
        {
            if (displayBinaural >= brainwaveBands[(size_t) b].lowHz
             && displayBinaural <  brainwaveBands[(size_t) b].highHz)
            {
                binauralState = brainwaveBands[(size_t) b].name;
                g.setColour (brainwaveBands[(size_t) b].color);
                break;
            }
        }
        g.setFont (boldFont (16.0f));
        g.drawText (String (displayBinaural, 1) + " Hz  " + String (binauralState),
                    binArea, Justification::centredLeft);
    }
    else
    {
        g.setColour (BwmColours::textDim);
        g.setFont (normalFont (13.0f));
        g.drawText ("No binaural detected", binArea, Justification::centredLeft);
    }
    
    // Sliders
    bounds.removeFromTop (10);
    auto sliderRow = bounds.removeFromTop (80);
    auto sliderHalf = sliderRow.getWidth() / 2;
    
    sensitivityLabel.setBounds (sliderRow.getX(), sliderRow.getY(), sliderHalf, 16);
    smoothingLabel.setBounds   (sliderRow.getX() + sliderHalf, sliderRow.getY(), sliderHalf, 16);
    sensitivitySlider.setBounds (sliderRow.getX() + 10, sliderRow.getY() + 18, sliderHalf - 20, 56);
    smoothingSlider.setBounds   (sliderRow.getX() + sliderHalf + 10, sliderRow.getY() + 18, sliderHalf - 20, 56);
}

void BrainwaveMeterEditor::resized() {}
