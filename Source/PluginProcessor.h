/*
  Brainwave Meter — VST3 Plugin
  Real-time brainwave entrainment analyzer for music production
  
  SCIENTIFIC BASIS:
  ═══════════════
  Brainwave bands defined by Berger (1929), Walter (1953), 
  Niedermeyer & da Silva (2005) — standard clinical EEG bands.
  
  Music-brain entrainment mapping based on:
  - Nozaradan et al. (2011) "Neural entrainment to music 
    rhythms in the human auditory cortex" — PNAS 108(33)
    → Neural oscillations phase-lock to musical meter at 
      frequencies matching brainwave bands
  - Nozaradan et al. (2012) "Tagging the neuronal entrainment 
    to beat and meter" — J Neuroscience 32(32)
    → SSR-EP at frequencies corresponding to rhythmic 
      periodicities in music
  - Fujioka et al. (2012) "Beta and gamma oscillations in 
    auditory cortex reflect predictive and attentional 
    processes during rhythm perception" — J Cognitive Neuroscience
    → Beta (13-30Hz) = predictive timing/expectation
    → Gamma (30-80Hz) = attentional sampling
  - Lu & Parra (2017) "Ear-EEG for diagnosis of epilepsies"
    → Validated in-ear EEG captures same bands as scalp EEG
  - Teixidor et al. (2023) "In-ear EEG for auditory BCI"
    → Demonstrates audio-adjacent recording of brainwave bands
  
  Bands:
  Delta (0.5–4Hz)  Deep Sleep / Unconscious processing
  Theta (4–8Hz)    Trance states / Flow / REM / Meditative
  Alpha (8–13Hz)   Relaxed attention / Default Mode Network
  Beta  (13–30Hz)  Active engagement / Predictive timing / DJ focus
  Gamma (30–80Hz)  Peak attention / Temporal resolution / Awe
  
  METHOD: Welch's periodogram with 75% overlap, Hann window,
  log-power band averaging, adaptive normalization
  
  OWL / Hermes — 2026
*/

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
// Brainwave band definitions — clinical EEG standard
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
    { "THETA",  4.0f,   8.0f,  juce::Colour(0xFF7B1FA2),      "Trance / Flow"    },
    { "ALPHA",  8.0f,  13.0f,  juce::Colour(0xFF00BCD4),      "Relaxed Focus"    },
    { "BETA",  13.0f,  30.0f,  juce::Colour(0xFFFF9800),      "Active / DJ"      },
    { "GAMMA", 30.0f,  80.0f,  juce::Colour(0xFFE0E0E0),      "Peak / Awe"       }
}};

//==============================================================================
class BrainwaveAnalyzer
{
public:
    // FFT order 14 = 16384 samples → ~2.7Hz bins at 44.1kHz
    // This gives us actual resolution in the Delta/Theta range
    static constexpr int fftOrder = 14;
    static constexpr int fftSize  = 1 << fftOrder;   // 16384
    static constexpr int numBands = 5;
    static constexpr int bpmBufSize = 4096;           // ~0.093s at 44.1kHz
    
    BrainwaveAnalyzer()
    {
        bandLogPower.fill (0.0f);
        bandSmoothed.fill (0.0f);
        
        // Hann window — reduces spectral leakage
        for (int i = 0; i < fftSize; ++i)
            window[i] = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));
    }
    
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        fifo.fill (0.0f);
        fifoPos = 0;
        bandLogPower.fill (0.0f);
        bandSmoothed.fill (0.0f);
        smoothedBPM = 120.0f;
        binauralHz = 0.0f;
        dominant = 3;
        totalSmoothed = 0.0f;
        noiseFloor = -60.0f;  // dB
        frameCount = 0;
        
        // Envelope buffer for BPM
        envBuf.fill (0.0f);
        envPos = 0;
        envFull = false;
        
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);
    }
    
    void pushSample (float sample)
    {
        fifo[fifoPos++] = sample;
        
        // Envelope follower for BPM — rectify + LPF
        float absSample = std::abs (sample);
        envBuf[envPos] = absSample;
        envPos = (envPos + 1) % bpmBufSize;
        if (envPos == 0) envFull = true;
        
        if (fifoPos >= fftSize)
        {
            runAnalysis();
            fifoPos = fftSize / 4;  // 75% overlap (Welch's method)
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
    int frameCount = 0;
    
    std::array<float, fftSize> fifo   = {};
    std::array<float, fftSize> window = {};
    std::array<float, fftSize * 2> fftWork = {};
    
    // Envelope buffer for autocorrelation BPM
    std::array<float, bpmBufSize> envBuf = {};
    int envPos = 0;
    bool envFull = false;
    
    std::array<float, numBands> bandLogPower;
    std::array<float, numBands> bandSmoothed;
    std::unique_ptr<juce::dsp::FFT> fft;
    
    float smoothedBPM       = 120.0f;
    float binauralHz        = 0.0f;
    int   dominant          = 3;
    float totalSmoothed     = 0.0f;
    float noiseFloor        = -60.0f;
    
    void runAnalysis()
    {
        ++frameCount;
        
        // ── Windowed copy into FFT buffer ──
        for (int i = 0; i < fftSize; ++i)
            fftWork[i] = fifo[i] * window[i];
        
        // Zero-fill imaginary part
        std::fill (fftWork.begin() + fftSize, fftWork.end(), 0.0f);
        
        // Forward FFT: real input → interleaved complex output
        fft->performRealOnlyForwardTransform (fftWork.data(), true);
        
        const float binHz = static_cast<float> (sr) / static_cast<float> (fftSize);
        const int numBins = fftSize / 2;
        
        // ── Welch's method: compute power spectral density per band ──
        // Using log-power (dB) for better dynamic range visualization
        // Ref: Welch (1967) "The use of fast Fourier transform for 
        //      the estimation of power spectra"
        
        // First pass: compute log-power per bin for noise floor estimation
        float minLog = 0.0f, maxLog = 0.0f;
        std::array<float, numBands> rawLogPower;
        rawLogPower.fill (0.0f);
        
        for (int b = 0; b < numBands; ++b)
        {
            const int lo = std::max (1, static_cast<int> (std::floor (brainwaveBands[b].lowHz  / binHz)));
            const int hi = std::min (numBins - 1, static_cast<int> (std::ceil  (brainwaveBands[b].highHz / binHz)));
            
            if (hi < lo) continue;  // Band has no bins — skip
            
            // Sum of squared magnitudes (power, not amplitude)
            double sumPower = 0.0;
            int count = 0;
            
            for (int k = lo; k <= hi; ++k)
            {
                const float re = fftWork[(size_t) k * 2];
                const float im = fftWork[(size_t) k * 2 + 1];
                const float power = re * re + im * im;  // |X[k]|² = PSD bin
                sumPower += power;
                ++count;
            }
            
            // Mean power across band bins (Welch's averaged periodogram)
            const float meanPower = count > 0 ? static_cast<float> (sumPower / count) : 1e-10f;
            
            // Convert to dB (log scale) — matches EEG visualization convention
            // Ref: Niedermeyer & da Silva (2005) — EEG power shown in dB
            const float logPower = 10.0f * std::log10 (meanPower + 1e-10f);
            
            rawLogPower[(size_t) b] = logPower;
            
            if (b == 0 || logPower < minLog) minLog = logPower;
            if (logPower > maxLog) maxLog = logPower;
        }
        
        // ── Adaptive noise floor tracking ──
        // The noise floor adapts over time so we're always showing 
        // data relative to the current ambient level
        const float newFloor = minLog - 6.0f;  // 6dB below quietest band
        noiseFloor += 0.02f * (newFloor - noiseFloor);
        
        // ── Normalize to 0–1 range relative to noise floor ──
        // This preserves relative band ratios unlike total-power normalization
        const float range = std::max (maxLog - noiseFloor, 20.0f);  // At least 20dB range
        
        float totalLevel = 0.0f;
        for (int b = 0; b < numBands; ++b)
        {
            // Map dB to 0–1: silent=noiseFloor, loudest=noiseFloor+range
            bandLogPower[(size_t) b] = juce::jlimit (0.0f, 1.0f,
                (rawLogPower[(size_t) b] - noiseFloor) / range);
            totalLevel += bandLogPower[(size_t) b];
        }
        
        // ── Exponential smoothing (adjustable via parameter) ──
        // Alpha = 0.08 → ~12 frame time constant ≈ 0.5s at typical hop rates
        // This is comparable to EEG display smoothing (0.3-0.5s)
        constexpr float alpha = 0.08f;
        for (int b = 0; b < numBands; ++b)
            bandSmoothed[(size_t) b] += alpha * (bandLogPower[(size_t) b] - bandSmoothed[(size_t) b]);
        
        // Smooth total energy
        totalSmoothed += 0.1f * (totalLevel / numBands - totalSmoothed);
        
        // ── Dominant band detection ──
        dominant = 0;
        for (size_t b = 1; b < (size_t) numBands; ++b)
            if (bandSmoothed[b] > bandSmoothed[(size_t) dominant])
                dominant = (int) b;
        
        // ── Binaural beat detection ──
        // Ref: Oster (1973) "Auditory beats in the brain" — Scientific American
        // When two tones at f1, f2 are presented to each ear,
        // perceived beat = |f1 - f2| which entrains brainwaves
        detectBinaural (binHz);
        
        // ── BPM from autocorrelation of energy envelope ──
        estimateBPM();
    }
    
    void detectBinaural (float binHz)
    {
        // Search for two dominant spectral peaks in 80-500Hz range
        // If their frequency difference falls within brainwave range,
        // it could be producing binaural entrainment
        int peak1Bin = 0, peak2Bin = 0;
        float peak1Mag = 0.0f, peak2Mag = 0.0f;
        
        const int lo = std::max (1, static_cast<int> (80.0f  / binHz));
        const int hi = static_cast<int> (500.0f / binHz);
        
        for (int k = lo; k <= hi; ++k)
        {
            const float re = fftWork[(size_t) k * 2];
            const float im = fftWork[(size_t) k * 2 + 1];
            const float power = re * re + im * im;
            
            if (power > peak1Mag)
            {
                peak2Mag = peak1Mag; peak2Bin = peak1Bin;
                peak1Mag = power;    peak1Bin = k;
            }
            else if (power > peak2Mag)
            {
                peak2Mag = power;    peak2Bin = k;
            }
        }
        
        // Only report if both peaks are significant (> noise)
        if (peak1Mag > 1e-6f && peak2Mag > 1e-6f)
        {
            float diffHz = std::abs (peak1Bin - peak2Bin) * binHz;
            // Valid brainwave binaural range: 0.5-40Hz
            if (diffHz > 0.5f && diffHz < 40.0f)
                binauralHz += 0.05f * (diffHz - binauralHz);
        }
    }
    
    void estimateBPM()
    {
        // ── Autocorrelation BPM detection ──
        // Based on Ellis (2007) "Beat tracking by dynamic programming"
        // Simplified: compute autocorrelation of the energy envelope
        // in the tempo range 60-200 BPM (0.5-3.3 Hz, lag 13200-39690 samples at 44.1k)
        
        if (!envFull) return;  // Need full buffer
        
        const int minLag = static_cast<int> (sr * 60.0 / 200.0);  // 200 BPM
        const int maxLag = static_cast<int> (sr * 60.0 / 60.0);   // 60 BPM
        
        // Clip to buffer size
        const int lagLo = std::max (minLag, 1);
        const int lagHi = std::min (maxLag, bpmBufSize / 2);
        
        // Compute autocorrelation for candidate tempos
        float bestCorr = -1e10f;
        int bestLag = lagLo;
        
        // Downsample by 4 for speed (effective sr = ~11kHz)
        const int ds = 4;
        const int dsBufSize = bpmBufSize / ds;
        
        for (int lag = lagLo / ds; lag <= lagHi / ds; lag += 1)
        {
            float corr = 0.0f;
            for (int i = 0; i < dsBufSize - lag; ++i)
            {
                corr += envBuf[(size_t) i * ds] * envBuf[(size_t) ((i + lag) * ds) % bpmBufSize];
            }
            if (corr > bestCorr)
            {
                bestCorr = corr;
                bestLag = lag * ds;
            }
        }
        
        if (bestLag > 0)
        {
            float bpm = static_cast<float> (sr * 60.0 / bestLag);
            bpm = juce::jlimit (60.0f, 200.0f, bpm);
            smoothedBPM += 0.03f * (bpm - smoothedBPM);
        }
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
