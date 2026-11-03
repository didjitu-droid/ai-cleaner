#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>
#include "Biquad.h"
#include "Presets.h"

namespace aic
{
    struct EngineParams
    {
        std::array<float, numModules> amounts {}; // effective amounts 0..1 (module setting x heart)
        int   stem = vocals;
        bool  autoCuts = true;
        float lowCut = 20.0f;      // Hz (manual mode)
        float highCut = 20000.0f;  // Hz (manual mode)
        float outputGainDb = 0.0f;
        float mix = 1.0f;          // 0..1
        bool  autoGain = true;
        bool  delta = false;
        bool  bypass = false;
    };

    // The whole cleaning chain: spectral repair (STFT) -> frequency cuts -> stereo fix -> punch -> gain.
    class CleanerEngine
    {
    public:
        static constexpr int displayPoints = 128;
        static constexpr float displayMinHz = 20.0f;
        static constexpr float displayMaxHz = 20000.0f;

        void prepare (double sampleRate, int numChannels);
        void reset();
        // fftSize for the STFT + a few frames of look-ahead for the Bleed separation.
        int  getLatencySamples() const noexcept { return fftSize + lookFrames * hopSize; }
        void process (juce::AudioBuffer<float>& buffer, const EngineParams& p);

        // Thread-safe reads for the user interface.
        void  copyDisplaySpectrum (std::array<float, displayPoints>& dest);
        float getCurrentLowCut()  const noexcept { return uiLowCut.load(); }
        float getCurrentHighCut() const noexcept { return uiHighCut.load(); }

    private:
        void processFrame (const EngineParams& p);
        void updateAutoCuts (const EngineParams& p);
        void updateFilters (const EngineParams& p, int numSamples);
        float binToHz (int k) const noexcept { return (float) k * (float) sampleRate / (float) fftSize; }

        double sampleRate = 44100.0;
        int numChannels = 2;

        // STFT
        int fftOrder = 11, fftSize = 2048, hopSize = 512, numBins = 1025;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> window;
        std::vector<std::vector<float>> inRing, outAccum, fftBuf;
        int writePos = 0, hopCounter = 0;

        // Look-ahead: spectra are processed lookFrames hops late so the Bleed module can see
        // a little of the future (needed to tell a held note from a short drum hit).
        static constexpr int lookFrames = 4, pastFrames = 16, ringFrames = pastFrames + 1 + lookFrames;
        std::vector<std::vector<std::vector<float>>> specRing;  // [slot][channel][2 * fftSize]
        std::vector<std::vector<float>> powRing;                 // [slot][bin]
        long long frameCount = 0;
        std::vector<std::vector<float>> dryDelay;                // keeps dry aligned with the look-ahead
        int dryPos = 0;
        std::vector<float> hMed, pMed;                           // harmonic / percussive estimates

        // Spectral state (per bin)
        std::vector<float> power, slowP, floorP /* bleed peak reference */, longP, dispP, prefix, gainDbSmooth, targetDb, tmpDb, prevPower, localMax, medScratch;
        std::vector<std::vector<float>> powerHistory; // for deverb
        int historyFrames = 6, historyPos = 0;
        float sibRatioSmooth = -30.0f, mudReductionSmooth = 0.0f;
        bool spectralPrimed = false;
        int framesSinceAuto = 0;

        // Frequency cuts
        std::vector<Butterworth4> highPass, lowPass;
        float autoLow = 30.0f, autoHigh = 18000.0f;
        float curLow = 30.0f, curHigh = 18000.0f;
        bool filtersInitialised = false;
        bool highPassActive = true, lowPassActive = true;
        std::atomic<float> uiLowCut { 30.0f }, uiHighCut { 18000.0f };

        // Stereo fix
        Biquad sideLowPass;
        float corrLR = 0.0f, corrLL = 0.0f, corrRR = 0.0f, sideGainSmooth = 1.0f;

        // Punch
        float envFast = 0.0f, envSlow = 0.0f, punchGainSmooth = 1.0f;

        // Auto gain
        float msDry = 0.0f, msWet = 0.0f, autoGainSmooth = 1.0f;

        // Bypass / output smoothing
        float bypassMix = 0.0f, outGainSmooth = 1.0f, mixSmooth = 1.0f;

        // Display
        juce::SpinLock displayLock;
        std::array<float, displayPoints> displaySpectrum {};
    };
}
