#include "PluginEditor.h"
#include <cmath>

//==============================================================================
// Band knowledge base: quantum/aura use cases per brainwave frequency
static const char* bandFreqs[5]     = { "DELTA", "THETA", "ALPHA", "BETA", "GAMMA" };
static const char* bandRanges[5]    = { "0.5-4 Hz", "4-8 Hz", "8-13 Hz", "13-30 Hz", "30-80 Hz" };
static const char* bandStates[5]    = { "Deep Sleep / Unconscious", "Trance / Flow / REM",
                                        "Relaxed Focus / Gateway", "Active Engagement / DJ State",
                                        "Peak Awareness / Awe" };

// Quantum realignment knowledge per band (newline-separated)
static const char* quantumText[5] = {
    "Access to collective unconscious (Jung)\n"
    "Quantum field coherence\n"
    "Timeline / past-life healing\n"
    "Dissolution of ego boundaries",

    "Hypnagogic gateway\n"
    "Default Mode Network quieting\n"
    "Psi phenomena window (telepathy)\n"
    "Manifestation & intention amp",

    "Conscious/subconscious bridge\n"
    "Superlearning & memory consolidation\n"
    "8Hz Schumann Earth resonance match\n"
    "Binaural entrainment sweet spot",

    "Active cognition & sensory processing\n"
    "Predictive timing (rhythm anticipation)\n"
    "Focus & analytical precision\n"
    "Crowd energy synchronization",

    "Highest conscious processing speed\n"
    "40Hz binding problem resolution\n"
    "Satori / insight (Eureka) moments\n"
    "Non-local awareness expansion"
};

// Aura frequency knowledge per band
static const char* auraText[5] = {
    "Deep indigo to violet aura\n"
    "Crown & root chakra resonance\n"
    "Psychic protection during projection\n"
    "Karmic pattern dissolution",

    "Purple to gold aura expansion\n"
    "Third eye chakra activation\n"
    "Intuitive channel opening\n"
    "Emotional body healing",

    "Blue to turquoise brightening\n"
    "Heart coherence synchronization\n"
    "Anxiety dissolution frequency\n"
    "Creative flow state access",

    "Orange to yellow aura energizing\n"
    "Solar plexus chakra activation\n"
    "Confidence & willpower frequency\n"
    "Social coherence & empathy",

    "White to golden light aura\n"
    "Full chakra column illumination\n"
    "Witness consciousness (Sakshi)\n"
    "Quantum observation peak"
};

//==============================================================================
BrainwaveMeterEditor::BrainwaveMeterEditor (BrainwaveMeterProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setOpaque (true);
    startTimerHz (30);
    setSize (780, 480);
    
    rain.clear();
    for (int i = 0; i < 40; ++i)
    {
        RainDrop d;
        d.x = (float) (rng.nextDouble() * 780.0);
        d.y = (float) (rng.nextDouble() * 480.0);
        d.speed = 0.5f + (float) rng.nextDouble() * 2.5f;
        d.brightness = 0.1f + (float) rng.nextDouble() * 0.4f;
        const char glyphs[] = "0123456789ABCDEF";
        d.glyph = glyphs[rng.nextInt(16)];
        rain.push_back (d);
    }
    
    specHistory.fill ({});
}

BrainwaveMeterEditor::~BrainwaveMeterEditor()
{
    stopTimer();
}

//==============================================================================
void BrainwaveMeterEditor::timerCallback()
{
    currentAnalysis = processor.getAnalysis();
    
    specWritePos = (specWritePos + 1) % specHistLen;
    for (int b = 0; b < 5; ++b)
        specHistory[(size_t) specWritePos][(size_t) b]
            = currentAnalysis.bands[(size_t) b];
    
    for (auto& d : rain)
    {
        d.y += d.speed;
        if (d.y > 490.0f)
        {
            d.y = -10.0f;
            d.x = (float) (rng.nextDouble() * 780.0);
        }
        d.brightness = std::max (0.05f, d.brightness
            + (float) (rng.nextDouble() - 0.5) * 0.05f);
    }
    
    repaint();
}

//==============================================================================
void BrainwaveMeterEditor::mouseDown (const juce::MouseEvent& e)
{
    auto bounds = getLocalBounds();
    auto meterArea = bounds.reduced (12, 36 + 50 + 8); // skip title + spectrum
    int bottomH = 80;
    meterArea.removeFromBottom (bottomH);
    
    const int bandW = meterArea.getWidth() / 5;
    
    for (int b = 0; b < 5; ++b)
    {
        auto r = meterArea;
        r.setX (meterArea.getX() + b * bandW);
        r.setWidth (bandW - 4);
        
        if (r.contains (e.getPosition()))
        {
            expandedBand = (expandedBand == b) ? -1 : b;
            repaint();
            return;
        }
    }
    
    if (expandedBand >= 0)
    {
        expandedBand = -1;
        repaint();
    }
}

void BrainwaveMeterEditor::mouseMove (const juce::MouseEvent& e)
{
    auto bounds = getLocalBounds();
    auto meterArea = bounds.reduced (12, 36 + 50 + 8);
    meterArea.removeFromBottom (80);
    
    const int bandW = meterArea.getWidth() / 5;
    int newHover = -1;
    
    for (int b = 0; b < 5; ++b)
    {
        auto r = meterArea;
        r.setX (meterArea.getX() + b * bandW);
        r.setWidth (bandW - 4);
        if (r.contains (e.getPosition()))
        {
            newHover = b;
            break;
        }
    }
    
    if (newHover != hoveredBand)
    {
        hoveredBand = newHover;
        repaint();
    }
}

void BrainwaveMeterEditor::resized()
{
    repaint();
}

//==============================================================================
static void drawMultiline (juce::Graphics& g, const char* text, int x, int y,
                            int w, int lineH, int& textY, int maxY)
{
    juce::String txt (text);
    juce::StringArray lines;
    lines.addLines (txt);
    for (int i = 0; i < lines.size(); ++i)
    {
        if (textY > maxY) break;
        g.drawText (lines[i].trim(), x, textY, w, lineH,
                    juce::Justification::topLeft, false);
        textY += lineH;
    }
}

//==============================================================================
void BrainwaveMeterEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Background
    g.fillAll (juce::Colour (0xFF020204));
    
    // Matrix rain
    g.setFont (juce::Font (juce::FontOptions ({ 10.0f })));
    for (const auto& d : rain)
    {
        float a = d.brightness;
        g.setColour (juce::Colour (0xFF00FF41).withAlpha (a));
        g.drawSingleLineText (juce::String::charToString ((juce::juce_wchar) d.glyph),
                              (int) d.x, (int) d.y);
        for (int t = 1; t < 4; ++t)
        {
            float ta = a * (1.0f - t * 0.3f);
            g.setColour (juce::Colour (0xFF003300).withAlpha (ta));
            g.drawSingleLineText (juce::String::charToString ((juce::juce_wchar) d.glyph),
                                  (int) d.x, (int) (d.y - t * 12));
        }
    }
    
    // Title bar
    auto titleBar = bounds.removeFromTop (32);
    g.setColour (juce::Colour (0xFF080C0A));
    g.fillRect (titleBar);
    g.setColour (juce::Colour (0xFF005522));
    g.drawHorizontalLine (titleBar.getBottom(), 0.0f, (float) titleBar.getWidth());
    
    g.setColour (juce::Colour (0xFF00FF41));
    g.setFont (juce::Font (juce::FontOptions ({ 16.0f }).withStyle ("Bold")));
    g.drawText ("> BRAINWAVE_METER v1.0", titleBar.reduced (12, 0),
                juce::Justification::centredLeft, false);
    g.setColour (juce::Colour (0xFF306040));
    g.setFont (juce::Font (juce::FontOptions ({ 10.0f })));
    g.drawText ("OWL // HERMES 2026", titleBar.reduced (12, 0),
                juce::Justification::centredRight, false);
    
    // Spectrum strip
    auto specArea = bounds.removeFromTop (50).reduced (12, 4);
    g.setColour (juce::Colour (0xFF040605));
    g.fillRect (specArea);
    g.setColour (juce::Colour (0xFF005522));
    g.drawRect (specArea, 1);
    
    if (specArea.getWidth() > 0)
    {
        const int colW = std::max (1, specArea.getWidth() / specHistLen);
        const juce::Colour specCols[5] = {
            juce::Colour (0xFF2244AA), juce::Colour (0xFF8833CC),
            juce::Colour (0xFF00CCAA), juce::Colour (0xFFDD8800),
            juce::Colour (0xFF44FF88)
        };
        for (int h = 0; h < specHistLen; ++h)
        {
            int hi = (specWritePos + 1 + h) % specHistLen;
            int x = specArea.getX() + h * colW;
            for (int b = 0; b < 5; ++b)
            {
                float lvl = specHistory[(size_t) hi][(size_t) b];
                int bh = (int) (lvl * specArea.getHeight());
                if (bh <= 0) continue;
                g.setColour (specCols[b].withAlpha (0.25f + lvl * 0.5f));
                g.fillRect (x, specArea.getBottom() - bh, colW, bh);
            }
        }
    }
    
    g.setColour (juce::Colour (0xFF306040));
    g.setFont (juce::Font (juce::FontOptions ({ 9.0f })));
    g.drawText ("SPECTRUM", specArea.getX() + 2, specArea.getY() + 1,
                60, 10, juce::Justification::topLeft, false);
    
    // Separator
    g.setColour (juce::Colour (0xFF005522));
    g.drawHorizontalLine (specArea.getBottom() + 4, 12.0f,
                          (float) (bounds.getWidth() - 12));
    
    auto workArea = bounds.reduced (12, 6);
    
    // Layout
    int infoH = (expandedBand >= 0) ? 180 : 0;
    auto meterArea = workArea.removeFromTop (workArea.getHeight() - infoH - 6);
    auto infoArea  = workArea;
    
    // Dominant band tint
    const juce::Colour cols[5] = {
        juce::Colour (0xFF2244AA), juce::Colour (0xFF8833CC),
        juce::Colour (0xFF00CCAA), juce::Colour (0xFFDD8800),
        juce::Colour (0xFF44FF88)
    };
    int dom = currentAnalysis.dominant;
    g.setColour (cols[dom].withAlpha (0.08f));
    g.fillRect (meterArea);
    g.setColour (juce::Colour (0xFF005522));
    g.drawRect (meterArea, 1);
    
    const int bandW = meterArea.getWidth() / 5;
    bool anyExp = (expandedBand >= 0);
    
    // Draw meter bars
    for (int b = 0; b < 5; ++b)
    {
        auto r = meterArea;
        r.setX (meterArea.getX() + b * bandW);
        
        if (anyExp && expandedBand != b)
            continue;
        if (anyExp)
            r.setWidth (meterArea.getWidth() - 4);
        else
            r.setWidth (bandW - 4);
        r.reduce (2, 2);
        
        float lvl = currentAnalysis.bands[(size_t) b];
        bool isDom = (b == dom);
        bool isHover = (hoveredBand == b) && !anyExp;
        
        // Panel bg
        g.setColour (juce::Colour (0xFF040605));
        g.fillRect (r);
        
        // Border
        g.setColour (isDom ? cols[b].withAlpha (0.6f)
                     : isHover ? juce::Colour (0xFF005522).withAlpha (0.8f)
                     : juce::Colour (0xFF005522));
        g.drawRect (r, isDom ? 2 : 1);
        
        // Grid
        g.setColour (juce::Colour (0xFF0A1210));
        for (int i = 1; i < 8; ++i)
        {
            int gy = r.getY() + r.getHeight() * i / 8;
            g.drawHorizontalLine (gy, (float) (r.getX() + 2), (float) (r.getRight() - 2));
        }
        
        // Left color bar
        g.setColour (cols[b].withAlpha (0.3f));
        g.fillRect (r.getX(), r.getY(), 3, r.getHeight());
        
        // Fill
        float clamped = juce::jlimit (0.0f, 1.0f, lvl);
        int fillH = (int) (r.getHeight() * clamped);
        if (fillH > 0)
        {
            juce::ColourGradient grad (
                cols[b].withAlpha (0.95f), (float) r.getX(),
                (float) (r.getBottom() - fillH),
                cols[b].withAlpha (0.15f), (float) r.getX(),
                (float) r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRect (r.getX() + 2, r.getBottom() - fillH,
                        r.getWidth() - 4, fillH);
            
            // Peak line
            g.setColour (cols[b].withAlpha (0.8f));
            g.fillRect (r.getX() + 1, r.getBottom() - fillH - 1,
                        r.getWidth() - 2, 2);
        }
        
        // Expanded: show knowledge base
        if (expandedBand == b)
        {
            g.setColour (juce::Colour (0xFF080C0A).withAlpha (0.92f));
            g.fillRect (r);
            g.setColour (cols[b].withAlpha (0.5f));
            g.drawRect (r, 2);
            
            int tx = r.getX() + 6, ty = r.getY() + 4, tw = r.getWidth() - 12;
            
            // Title
            g.setColour (cols[b]);
            g.setFont (juce::Font (juce::FontOptions ({ 14.0f }).withStyle ("Bold")));
            g.drawText (juce::String (bandFreqs[b]) + "  " + bandRanges[b],
                        tx, ty, tw, 16, juce::Justification::centred, false);
            ty += 18;
            
            // State
            g.setColour (juce::Colour (0xFF00FFFF));
            g.setFont (juce::Font (juce::FontOptions ({ 9.0f })));
            g.drawText (juce::String ("[ ") + bandStates[b] + " ]",
                        tx, ty, tw, 12, juce::Justification::centred, false);
            ty += 16;
            
            g.setColour (juce::Colour (0xFF005522));
            g.drawHorizontalLine (ty, (float) tx, (float) (tx + tw));
            ty += 6;
            
            // Quantum
            g.setColour (juce::Colour (0xFF00FF41));
            g.setFont (juce::Font (juce::FontOptions ({ 9.0f }).withStyle ("Bold")));
            g.drawText ("> QUANTUM REALIGNMENT:", tx, ty, tw, 12,
                        juce::Justification::topLeft, false);
            ty += 13;
            g.setColour (juce::Colour (0xFFE0FFE0));
            g.setFont (juce::Font (juce::FontOptions ({ 9.0f })));
            drawMultiline (g, quantumText[b], tx + 4, ty, tw - 4, 12, ty, r.getBottom() - 30);
            
            ty += 3;
            
            // Aura
            g.setColour (juce::Colour (0xFF00FF41));
            g.setFont (juce::Font (juce::FontOptions ({ 9.0f }).withStyle ("Bold")));
            g.drawText ("> AURA FREQUENCY:", tx, ty, tw, 12,
                        juce::Justification::topLeft, false);
            ty += 13;
            g.setColour (juce::Colour (0xFFE0FFE0));
            g.setFont (juce::Font (juce::FontOptions ({ 9.0f })));
            drawMultiline (g, auraText[b], tx + 4, ty, tw - 4, 12, ty, r.getBottom() - 16);
            
            // Collapse hint
            g.setColour (juce::Colour (0xFF306040));
            g.setFont (juce::Font (juce::FontOptions ({ 8.0f })));
            g.drawText ("[ click to collapse ]",
                        tx, r.getBottom() - 13, tw, 10,
                        juce::Justification::centred, false);
            continue;
        }
        
        // Compact labels
        // Name
        g.setColour (cols[b]);
        g.setFont (juce::Font ("Monospace",
                               bandW > 130 ? 12.0f : 9.0f, juce::Font::bold));
        g.drawText (bandFreqs[b], r.getX() + 5, r.getY() + 2,
                    r.getWidth() - 6, 14, juce::Justification::topLeft, false);
        
        // Percentage inside fill
        if (fillH > 14)
        {
            g.setColour (juce::Colour (0xFFE0FFE0));
            g.setFont (juce::Font ("Monospace", 8.0f, juce::Font::bold));
            g.drawText (juce::String ((int) (lvl * 100)) + "%",
                        r.getX(), r.getBottom() - fillH,
                        r.getWidth(), 12, juce::Justification::centred, false);
        }
    }
    
    // Info panel (bottom) — only when nothing expanded
    if (!anyExp && infoArea.getHeight() > 20)
    {
        g.setColour (juce::Colour (0xFF080C0A));
        g.fillRect (infoArea);
        g.setColour (juce::Colour (0xFF005522));
        g.drawRect (infoArea, 1);
        
        auto left = infoArea.reduced (8, 4);
        
        // BPM
        g.setColour (juce::Colour (0xFF00FF41));
        g.setFont (juce::Font (juce::FontOptions ({ 12.0f }).withStyle ("Bold")));
        g.drawText ("> BPM: " + juce::String (currentAnalysis.bpm, 1),
                    left.getX(), left.getY(), left.getWidth(), 16,
                    juce::Justification::left, false);
        
        int ly = left.getY() + 18;
        
        // Binaural
        if (currentAnalysis.binauralHz > 0.5f)
        {
            g.setColour (juce::Colour (0xFF00FFFF));
            g.setFont (juce::Font (juce::FontOptions ({ 10.0f })));
            g.drawText ("> BINAURAL: "
                        + juce::String (currentAnalysis.binauralHz, 1) + " Hz",
                        left.getX(), ly, left.getWidth(), 14,
                        juce::Justification::left, false);
            ly += 14;
        }
        
        // Dominant
        g.setColour (cols[dom]);
        g.setFont (juce::Font (juce::FontOptions ({ 10.0f }).withStyle ("Bold")));
        g.drawText ("> DOMINANT: " + juce::String (bandFreqs[dom])
                    + "  [" + bandStates[dom] + "]",
                    left.getX(), ly, left.getWidth(), 14,
                    juce::Justification::left, false);
        ly += 16;
        
        // Click hint
        g.setColour (juce::Colour (0xFF306040));
        g.setFont (juce::Font (juce::FontOptions ({ 9.0f })));
        g.drawText ("[ click any band for quantum realignment & aura data ]",
                    left.getX(), ly, left.getWidth(), 12,
                    juce::Justification::left, false);
    }
}
