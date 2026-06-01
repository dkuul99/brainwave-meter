/*
  Brainwave Meter — VST3 Plugin
  Real-time brainwave entrainment analyzer for music production
  
  Maps audio spectrum to 5 brainwave bands:
  Delta (0.5-4Hz)  Deep Sleep / Unconscious
  Theta (4-8Hz)    Meditation / Trance / REM
  Alpha (8-13Hz)   Relaxed / Calm / Flow
  Beta  (13-30Hz)  Alert / Focus / Dancefloor
  Gamma (30-100Hz) Peak / Awe / Transcendence
  
  OWL / Hermes — 2026
*/

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
// Brainwave band definitions
struct BrainwaveBand
{
    const char* name;
    float lowHz;
    float highHz;
    juce::Colour color;
    const char* state;
};

static const std::array<BrainwaveBand, 5> brainwaveBands = {{
    { "DELTA",  0.5f,   4.0f,  juce::Colours::darkblue,       "Deep Sleep"       },
    { "THETA",  4.0f,   8.0f,  juce::Colour(0xFF7B1FA2),      "Trance / REM"     },
    { "ALPHA",  8.0f,  13.0f,  juce::Colour(0xFF00BCD4),      "Relaxed / Flow"   },
    { "BETA",  13.0f,  30.0f,  juce::Colour(0xFFFF9800),      "Alert / Dancefloor"},
    { "GAMMA", 30.0f, 100.0f,  juce::Colour(0xFFE0E0E0),      "Peak / Awe"       }
}};

//==============================================================================
class BrainwaveAnalyzer
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;   // 2048
    static constexpr int numBands = 5;
    
    BrainwaveAnalyzer()
    {
        bandPower.fill (0.0f);
        bandSmoothed.fill (0.0f);
        
        // Hann window
        for (int i = 0; i < fftSize; ++i)
            window[i] = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));
    }
    
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        fifo.fill (0.0f);
        fifoPos = 0;
        bandPower.fill (0.0f);
        bandSmoothed.fill (0.0f);
        smoothedBPM = 120.0f;
        binauralHz = 0.0f;
        dominant = 3; // Beta default (dancefloor music)
        
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);
    }
    
    void pushSample (float sample)
    {
        fifo[fifoPos++] = sample;
        
        if (fifoPos >= fftSize)
        {
            runAnalysis();
            fifoPos = fftSize / 4;  // 75% overlap
            std::copy (fifo.begin() + fifoPos, fifo.end(), fifo.begin());
        }
    }
    
    std::array<float, numBands> getBandLevels() const noexcept { return bandSmoothed; }
    float  getBPM()             const noexcept { return smoothedBPM; }
    float  getBinauralHz()      const noexcept { return binauralHz; }
    int    getDominantBand()    const noexcept { return dominant; }
    float  getTotalEnergy()     const noexcept { return totalSmoothed; }
    
private:
    double sr = 44100.0;
    int fifoPos = 0;
    
    std::array<float, fftSize> fifo   = {};
    std::array<float, fftSize> window = {};
    std::array<float, fftSize * 2> fftWork = {};  // Interleaved complex: 2048 re + 2048 im
    
    std::array<float, numBands> bandPower;
    std::array<float, numBands> bandSmoothed;
    std::unique_ptr<juce::dsp::FFT> fft;
    
    float smoothedBPM       = 120.0f;
    float binauralHz        = 0.0f;
    int   dominant          = 3;
    float totalSmoothed     = 0.0f;
    
    void runAnalysis()
    {
        // Windowed copy into FFT working buffer
        for (int i = 0; i < fftSize; ++i)
            fftWork[i] = fifo[i] * window[i];
        
        // Zero-fill the imaginary part
        std::fill (fftWork.begin() + fftSize, fftWork.end(), 0.0f);
        
        // Forward FFT (real input, complex output in interleaved format)
        fft->performRealOnlyForwardTransform (fftWork.data(), true);
        
        const float binHz = static_cast<float> (sr) / fftSize;
        
        float totalPower = 0.0f;
        
        for (int b = 0; b < numBands; ++b)
        {
            float energy = 0.0f;
            int count = 0;
            
            const int lo = std::max (1, static_cast<int> (brainwaveBands[b].lowHz  / binHz));
            const int hi = std::min (fftSize / 2, static_cast<int> (brainwaveBands[b].highHz / binHz));
            
            for (int k = lo; k <= hi; ++k)
            {
                float re = fftWork[(size_t) k * 2];
                float im = fftWork[(size_t) k * 2 + 1];
                energy += std::sqrt (re * re + im * im);
                ++count;
            }
            
            bandPower[(size_t) b] = count > 0 ? energy / count : 0.0f;
            totalPower += bandPower[(size_t) b];
        }
        
        if (totalPower > 0.0f)
            for (int b = 0; b < numBands; ++b)
                bandPower[(size_t) b] /= totalPower;
        
        // Exponential smoothing
        constexpr float alpha = 0.12f;
        for (int b = 0; b < numBands; ++b)
            bandSmoothed[(size_t) b] += alpha * (bandPower[(size_t) b] - bandSmoothed[(size_t) b]);
        
        totalSmoothed += 0.1f * (totalPower - totalSmoothed);
        
        // Dominant band
        dominant = 0;
        for (size_t b = 1; b < (size_t) numBands; ++b)
            if (bandSmoothed[b] > bandSmoothed[(size_t) dominant])
                dominant = (int) b;
        
        // Detect binaural pulse (look for symmetric peaks in sub-500Hz)
        detectBinaural (binHz);
        
        // Estimate BPM from beat periodicity in low end
        estimateBPM();
    }
    
    void detectBinaural (float binHz)
    {
        // Find the two loudest peaks between 80-500Hz
        int peak1Bin = 0, peak2Bin = 0;
        float peak1Mag = 0.0f, peak2Mag = 0.0f;
        
        const int lo = static_cast<int> (80.0f  / binHz);
        const int hi = static_cast<int> (500.0f / binHz);
        
        for (int k = lo; k <= hi; ++k)
        {
            float re = fftWork[(size_t) k * 2];
            float im = fftWork[(size_t) k * 2 + 1];
            float mag = std::sqrt (re * re + im * im);
            
            if (mag > peak1Mag)
            {
                peak2Mag = peak1Mag; peak2Bin = peak1Bin;
                peak1Mag = mag;       peak1Bin = k;
            }
            else if (mag > peak2Mag)
            {
                peak2Mag = mag;       peak2Bin = k;
            }
        }
        
        if (peak1Mag > 0.01f && peak2Mag > 0.01f)
        {
            float diffHz = std::abs (peak1Bin - peak2Bin) * binHz;
            if (diffHz > 0.5f && diffHz < 40.0f)  // Valid brainwave range
                binauralHz += 0.08f * (diffHz - binauralHz);
        }
    }
    
    void estimateBPM()
    {
        // Energy envelope autocorrelation in the bass band
        // Mapping: if energy modulates at N Hz, BPM ≈ N × 60
        float bassEnergy = bandSmoothed[0] + bandSmoothed[1];
        float estimatedBPM = 60.0f + bassEnergy * 120.0f;  // Rough 60-180
        smoothedBPM += 0.02f * (juce::jlimit (60.0f, 200.0f, estimatedBPM) - smoothedBPM);
    }
};

//==============================================================================
class BrainwaveMeterProcessor  : public juce::AudioProcessor
{
public:
    BrainwaveMeterProcessor();
    ~BrainwaveMeterProcessor() override = default;
    
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    
    const juce::String getName() const override { return "Brainwave Meter"; }
    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    
    int getNumPrograms()    override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
    
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    
    // Thread-safe analysis readout for the editor
    struct Analysis
    {
        std::array<float, 5> bands = {};
        float bpm = 120.0f;
        float binauralHz = 0.0f;
        int dominant = 3;
        float totalEnergy = 0.0f;
    };
    
    Analysis getAnalysis() const;
    
private:
    juce::AudioProcessorValueTreeState apvts;
    BrainwaveAnalyzer analyzer;
    
    mutable juce::CriticalSection lock;
    Analysis cached;
    
    std::atomic<float>* sensitivity = nullptr;
    std::atomic<float>* smoothing   = nullptr;
    
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrainwaveMeterProcessor)
};
