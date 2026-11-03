#include "CleanerEngine.h"
#include <algorithm>
#include <cmath>

namespace aic
{
namespace
{
    constexpr float eps = 1.0e-12f;

    inline float toDb (float power) noexcept { return 10.0f * std::log10 (power + eps); }
    inline float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
    inline float clampf (float x, float lo, float hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }

    // Smooth 0..1 ramp between two frequencies.
    inline float ramp (float f, float f0, float f1) noexcept { return clamp01 ((f - f0) / (f1 - f0)); }

    // One-pole coefficient for a given time constant, evaluated every `period` seconds.
    inline float onePole (double seconds, double period) noexcept
    {
        return (float) (1.0 - std::exp (-period / std::max (1.0e-6, seconds)));
    }
}

void CleanerEngine::prepare (double newSampleRate, int newNumChannels)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    numChannels = juce::jlimit (1, 2, newNumChannels);

    fftOrder = sampleRate > 130000.0 ? 13 : (sampleRate > 65000.0 ? 12 : 11);
    fftSize = 1 << fftOrder;
    hopSize = fftSize / 4;
    numBins = fftSize / 2 + 1;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    // sqrt-Hann analysis and synthesis windows (periodic) -> perfect reconstruction at 75 % overlap.
    window.assign ((size_t) fftSize, 0.0f);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = (float) std::sqrt (0.5 - 0.5 * std::cos (2.0 * kPi * i / fftSize));

    inRing.assign ((size_t) numChannels, std::vector<float> ((size_t) fftSize, 0.0f));
    outAccum.assign ((size_t) numChannels, std::vector<float> ((size_t) fftSize, 0.0f));
    fftBuf.assign ((size_t) numChannels, std::vector<float> ((size_t) fftSize * 2, 0.0f));

    for (auto* v : { &power, &slowP, &floorP, &longP, &dispP, &gainDbSmooth, &targetDb, &tmpDb, &prevPower, &localMax, &hMed, &pMed })
        v->assign ((size_t) numBins, 0.0f);
    medScratch.assign (64, 0.0f);

    specRing.assign ((size_t) (lookFrames + 1),
                     std::vector<std::vector<float>> ((size_t) numChannels, std::vector<float> ((size_t) fftSize * 2, 0.0f)));
    powRing.assign ((size_t) ringFrames, std::vector<float> ((size_t) numBins, 0.0f));
    dryDelay.assign ((size_t) numChannels, std::vector<float> ((size_t) (lookFrames * hopSize), 0.0f));
    prefix.assign ((size_t) numBins + 1, 0.0f);

    // Deverb looks ~60 ms back in time.
    historyFrames = std::max (2, (int) std::round (0.06 * sampleRate / hopSize));
    powerHistory.assign ((size_t) historyFrames, std::vector<float> ((size_t) numBins, 0.0f));

    highPass.assign ((size_t) numChannels, {});
    lowPass.assign ((size_t) numChannels, {});

    reset();
}

void CleanerEngine::reset()
{
    for (auto& v : inRing)   std::fill (v.begin(), v.end(), 0.0f);
    for (auto& v : outAccum) std::fill (v.begin(), v.end(), 0.0f);
    for (auto& v : powerHistory) std::fill (v.begin(), v.end(), 0.0f);
    for (auto& slot : specRing) for (auto& v : slot) std::fill (v.begin(), v.end(), 0.0f);
    for (auto& v : powRing)  std::fill (v.begin(), v.end(), 0.0f);
    for (auto& v : dryDelay) std::fill (v.begin(), v.end(), 0.0f);
    frameCount = 0;
    dryPos = 0;
    for (auto* v : { &power, &slowP, &longP, &dispP, &gainDbSmooth, &targetDb, &tmpDb, &prevPower })
        std::fill (v->begin(), v->end(), 0.0f);
    std::fill (floorP.begin(), floorP.end(), 0.0f);

    writePos = hopCounter = historyPos = 0;
    framesSinceAuto = 0;
    spectralPrimed = false;
    sibRatioSmooth = -30.0f;
    mudReductionSmooth = 0.0f;

    for (auto& f : highPass) f.reset();
    for (auto& f : lowPass)  f.reset();
    filtersInitialised = false;

    sideLowPass.reset();
    sideLowPass.setLowPass (sampleRate, 150.0, 0.7071);
    corrLR = corrLL = corrRR = 0.0f;
    sideGainSmooth = 1.0f;
    envFast = envSlow = 0.0f;
    punchGainSmooth = 1.0f;
    msDry = msWet = 0.0f;
    autoGainSmooth = 1.0f;
    bypassMix = 0.0f;
    outGainSmooth = 1.0f;
    mixSmooth = 1.0f;

    const juce::SpinLock::ScopedLockType sl (displayLock);
    displaySpectrum.fill (-120.0f);
}

void CleanerEngine::copyDisplaySpectrum (std::array<float, displayPoints>& dest)
{
    const juce::SpinLock::ScopedLockType sl (displayLock);
    dest = displaySpectrum;
}

//==============================================================================
void CleanerEngine::process (juce::AudioBuffer<float>& buffer, const EngineParams& p)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int chans = std::min (numChannels, buffer.getNumChannels());
    if (numSamples <= 0 || chans <= 0 || fft == nullptr)
        return;

    updateFilters (p, numSamples);

    const double dt = 1.0 / sampleRate;
    const float corrCoef   = onePole (0.30, dt);
    const float sideCoef   = onePole (0.05, dt);
    const float fastAtt    = onePole (0.0005, dt), fastRel = onePole (0.030, dt);
    const float slowAtt    = onePole (0.025, dt),  slowRel = onePole (0.200, dt);
    const float punchCoef  = onePole (0.002, dt);
    const float msCoef     = onePole (1.5, dt);
    const float agCoef     = onePole (0.25, dt);
    const float paramCoef  = onePole (0.02, dt);

    const float stereoAmt = numChannels > 1 ? p.amounts[stereo] : 0.0f;
    const float punchAmt  = p.amounts[punch];
    const float targetOut = juce::Decibels::decibelsToGain (p.outputGainDb);
    const float targetMix = clamp01 (p.mix);
    const float targetBypass = p.bypass ? 1.0f : 0.0f;

    float* ch[2] = { buffer.getWritePointer (0), chans > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int n = 0; n < numSamples; ++n)
    {
        float dry[2] = { 0.0f, 0.0f }, wet[2] = { 0.0f, 0.0f };

        // --- STFT stage: push input, pull spectrally cleaned output (delayed by fftSize) ---
        for (int c = 0; c < chans; ++c)
        {
            auto& ring = inRing[(size_t) c];
            auto& acc  = outAccum[(size_t) c];
            const float delayedIn = ring[(size_t) writePos];   // input from exactly fftSize samples ago
            ring[(size_t) writePos] = ch[c][n];
            auto& dd = dryDelay[(size_t) c];                   // + the look-ahead delay
            dry[c] = dd[(size_t) dryPos];
            dd[(size_t) dryPos] = delayedIn;
            wet[c] = acc[(size_t) writePos];
            acc[(size_t) writePos] = 0.0f;
        }

        writePos = (writePos + 1) % fftSize;
        dryPos = (dryPos + 1) % (lookFrames * hopSize);
        if (++hopCounter >= hopSize)
        {
            hopCounter = 0;
            processFrame (p);
        }

        // --- Frequency cuts ---
        for (int c = 0; c < chans; ++c)
        {
            if (highPassActive) wet[c] = highPass[(size_t) c].process (wet[c]);
            if (lowPassActive)  wet[c] = lowPass[(size_t) c].process (wet[c]);
        }

        // --- Stereo fix ---
        if (chans > 1)
        {
            const float l = wet[0], r = wet[1];
            corrLR += corrCoef * (l * r - corrLR);
            corrLL += corrCoef * (l * l - corrLL);
            corrRR += corrCoef * (r * r - corrRR);
            const float corr = corrLR / std::sqrt (corrLL * corrRR + 1.0e-18f);

            const float phasey = clamp01 ((0.6f - corr) / 1.2f);
            const float sideTarget = 1.0f - stereoAmt * (0.15f + 0.55f * phasey);
            sideGainSmooth += sideCoef * (sideTarget - sideGainSmooth);

            const float mid = 0.5f * (l + r);
            float side = 0.5f * (l - r);
            const float sideLow = sideLowPass.process (side);
            side = (side - stereoAmt * sideLow) * sideGainSmooth;   // mono bass + calmer stereo
            wet[0] = mid + side;
            wet[1] = mid - side;
        }

        // --- Punch (transient shaper) ---
        {
            float peak = std::abs (wet[0]);
            if (chans > 1) peak = std::max (peak, std::abs (wet[1]));
            envFast += (peak > envFast ? fastAtt : fastRel) * (peak - envFast);
            envSlow += (peak > envSlow ? slowAtt : slowRel) * (peak - envSlow);

            float targetGain = 1.0f;
            if (punchAmt > 0.0f && envSlow > 1.0e-4f)
            {
                const float diffDb = 20.0f * std::log10 ((envFast + 1.0e-9f) / (envSlow + 1.0e-9f));
                targetGain = juce::Decibels::decibelsToGain (punchAmt * clampf (diffDb, 0.0f, 12.0f) * 0.6f);
            }
            punchGainSmooth += punchCoef * (targetGain - punchGainSmooth);
            for (int c = 0; c < chans; ++c)
                wet[c] *= punchGainSmooth;
        }

        // --- Auto gain: keep the cleaned signal as loud as the original ---
        {
            float dryPow = 0.0f, wetPow = 0.0f;
            for (int c = 0; c < chans; ++c) { dryPow += dry[c] * dry[c]; wetPow += wet[c] * wet[c]; }
            msDry += msCoef * (dryPow - msDry);
            msWet += msCoef * (wetPow - msWet);

            float target = autoGainSmooth;
            if (! p.autoGain)
                target = 1.0f;
            else if (msDry > 1.0e-9f && msWet > 1.0e-12f)
                target = clampf (std::sqrt (msDry / msWet), 0.25f, 4.0f);
            autoGainSmooth += agCoef * (target - autoGainSmooth);
        }

        outGainSmooth += paramCoef * (targetOut - outGainSmooth);
        mixSmooth     += paramCoef * (targetMix - mixSmooth);
        bypassMix     += paramCoef * (targetBypass - bypassMix);

        for (int c = 0; c < chans; ++c)
        {
            float out;
            if (p.delta)
                out = (dry[c] - wet[c]) * outGainSmooth;   // only what the plugin removes
            else
                out = (wet[c] * autoGainSmooth * mixSmooth + dry[c] * (1.0f - mixSmooth)) * outGainSmooth;

            out = out * (1.0f - bypassMix) + dry[c] * bypassMix;
            ch[c][n] = std::isfinite (out) ? out : 0.0f;
        }
    }

    // Channels the engine does not handle (rare layouts) stay silent rather than garbage.
    for (int c = chans; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);
}

//==============================================================================
void CleanerEngine::updateFilters (const EngineParams& p, int numSamples)
{
    float targetLow, targetHigh;
    if (p.autoCuts) { targetLow = autoLow; targetHigh = autoHigh; }
    else            { targetLow = p.lowCut; targetHigh = p.highCut; }

    const float nyq = (float) sampleRate * 0.49f;
    targetLow  = clampf (targetLow, 10.0f, 2000.0f);
    targetHigh = clampf (targetHigh, 1000.0f, std::min (22000.0f, nyq));

    if (! filtersInitialised)
    {
        curLow = targetLow;
        curHigh = targetHigh;
        filtersInitialised = true;
    }
    else
    {
        // Glide in the log domain (~60 ms) so moving the cuts never clicks.
        const float k = onePole (0.06, numSamples / sampleRate);
        curLow  = std::exp (std::log (curLow)  + k * (std::log (targetLow)  - std::log (curLow)));
        curHigh = std::exp (std::log (curHigh) + k * (std::log (targetHigh) - std::log (curHigh)));
    }

    // Cuts parked at the extremes are switched out completely so they add no phase shift.
    highPassActive = curLow > 12.0f;
    lowPassActive  = curHigh < std::min (20000.0f, nyq * 0.95f);

    for (int c = 0; c < numChannels; ++c)
    {
        if (highPassActive) highPass[(size_t) c].setHighPass (sampleRate, curLow);
        if (lowPassActive)  lowPass[(size_t) c].setLowPass (sampleRate, curHigh);
    }

    uiLowCut.store (curLow);
    uiHighCut.store (curHigh);
}

void CleanerEngine::updateAutoCuts (const EngineParams& p)
{
    const auto& preset = stemPreset (juce::jlimit (0, numStems - 1, p.stem));

    double total = 0.0;
    const int kMin = std::max (1, (int) std::ceil (15.0f / binToHz (1)));
    for (int k = kMin; k < numBins; ++k)
        total += longP[(size_t) k];

    if (total < 1.0e-6)   // silence: keep the previous decision
        return;

    // Low cut: where the stem's own low end really starts (-35 dB of cumulative energy).
    double acc = 0.0;
    float lowCand = preset.autoLowMin;
    for (int k = kMin; k < numBins; ++k)
    {
        acc += longP[(size_t) k];
        if (acc > total * 3.0e-4) { lowCand = binToHz (k) * 0.8f; break; }
    }

    // High cut: where useful content ends; above it Suno mostly leaves hiss and "digital fizz".
    acc = 0.0;
    float highCand = preset.autoHighMax;
    for (int k = numBins - 1; k > kMin; --k)
    {
        acc += longP[(size_t) k];
        if (acc > total * 2.0e-5) { highCand = binToHz (k) * 1.1f; break; }
    }

    // Suno often leaves a band of digital "speckle" around 16-20 kHz that sits ABOVE a quieter
    // dip around 13-15.5 kHz. Natural recordings fall off instead. If we see that shelf, cut below it.
    {
        auto meanLong = [this] (float f0, float f1)
        {
            const int a = std::max (1, (int) (f0 / binToHz (1))), b = std::min (numBins - 1, (int) (f1 / binToHz (1)));
            double sum = 0.0;
            for (int k = a; k <= b; ++k) sum += longP[(size_t) k];
            return b >= a ? sum / (b - a + 1) : 0.0;
        };
        if (sampleRate > 40000.0)
        {
            const double dip = meanLong (13000.0f, 15500.0f), shelf = meanLong (16000.0f, 19500.0f);
            if (shelf > dip * 1.2 && shelf > 1.0e-14)
                highCand = std::min (highCand, 15500.0f);
        }
    }

    lowCand  = clampf (lowCand,  preset.autoLowMin,  preset.autoLowMax);
    highCand = clampf (highCand, preset.autoHighMin, preset.autoHighMax);

    autoLow  = std::exp (std::log (autoLow)  + 0.35f * (std::log (lowCand)  - std::log (autoLow)));
    autoHigh = std::exp (std::log (autoHigh) + 0.35f * (std::log (highCand) - std::log (autoHigh)));
}

//==============================================================================
void CleanerEngine::processFrame (const EngineParams& p)
{
    const int chans = numChannels;
    const float invChans = 1.0f / (float) chans;
    const double frameSec = (double) hopSize / sampleRate;

    // 1. Analysis: windowed FFT of the last fftSize samples of each channel.
    for (int c = 0; c < chans; ++c)
    {
        auto& ring = inRing[(size_t) c];
        auto& buf  = fftBuf[(size_t) c];
        for (int i = 0; i < fftSize; ++i)
            buf[(size_t) i] = ring[(size_t) ((writePos + i) % fftSize)] * window[(size_t) i];
        std::fill (buf.begin() + fftSize, buf.end(), 0.0f);
        fft->performRealOnlyForwardTransform (buf.data());
    }

    // 2. Linked power spectrum of the newest frame (keeps the stereo image intact),
    //    normalised so that a full-scale sine reads about 0 dB.
    const float norm = (float) (kPi * kPi) / ((float) fftSize * (float) fftSize);
    const int newSlot = (int) (frameCount % ringFrames);
    {
        auto& np = powRing[(size_t) newSlot];
        for (int k = 0; k < numBins; ++k)
        {
            float pw = 0.0f;
            for (int c = 0; c < chans; ++c)
            {
                const float re = fftBuf[(size_t) c][(size_t) (2 * k)];
                const float im = fftBuf[(size_t) c][(size_t) (2 * k + 1)];
                pw += re * re + im * im;
            }
            np[(size_t) k] = pw * invChans * norm;
        }
        auto& slot = specRing[(size_t) (frameCount % (lookFrames + 1))];
        for (int c = 0; c < chans; ++c)
            std::copy (fftBuf[(size_t) c].begin(), fftBuf[(size_t) c].end(), slot[(size_t) c].begin());
    }

    // The frame we actually clean is lookFrames hops old; everything below works on it.
    const long long centre = frameCount - lookFrames;
    ++frameCount;
    {
        const auto& cp = powRing[(size_t) ((centre + ringFrames) % ringFrames)];
        std::copy (cp.begin(), cp.end(), power.begin());
        auto& slot = specRing[(size_t) ((centre + (lookFrames + 1)) % (lookFrames + 1))];
        for (int c = 0; c < chans; ++c)
            std::copy (slot[(size_t) c].begin(), slot[(size_t) c].end(), fftBuf[(size_t) c].begin());
    }

    // Harmonic / percussive estimates (median filtering, Fitzgerald 2010):
    //  - along time: what is held (notes, tones)
    //  - along frequency: what is broadband and short (hits, clicks)
    {
        const bool percussiveStem = p.stem == drums || p.stem == percussion;
        // Tonal stems: centred window (4 before, 4 after) so a note's attack counts as held.
        // Drum stems: much longer look back (~170 ms), so only tones that really sustain count as held
        // (bass or synth notes), not the pitched ring of a tom or a conga.
        const int t0 = percussiveStem ? 0 : (pastFrames - lookFrames), t1 = ringFrames;
        const int nT = t1 - t0;
        const int oldest = (int) ((frameCount) % ringFrames);   // slot of frame (newest - ringFrames + 1)
        for (int k = 0; k < numBins; ++k)
        {
            for (int i = 0; i < nT; ++i)
                medScratch[(size_t) i] = powRing[(size_t) ((oldest + t0 + i) % ringFrames)][(size_t) k];
            std::nth_element (medScratch.begin(), medScratch.begin() + nT / 2, medScratch.begin() + nT);
            hMed[(size_t) k] = medScratch[(size_t) (nT / 2)];

            const int a = std::max (0, k - 8), b = std::min (numBins - 1, k + 8), n = b - a + 1;
            for (int i = 0; i < n; ++i)
                medScratch[(size_t) i] = power[(size_t) (a + i)];
            std::nth_element (medScratch.begin(), medScratch.begin() + n / 2, medScratch.begin() + n);
            pMed[(size_t) k] = medScratch[(size_t) (n / 2)];
        }
    }

    const float slowCoef = onePole (0.08, frameSec);
    const float longCoef = onePole (2.0, frameSec);
    const float dispCoef = onePole (0.06, frameSec);
    const float peakFall = std::pow (10.0f, -(float) (6.0 * frameSec) / 10.0f); // bleed reference falls 6 dB/s

    for (int k = 0; k < numBins; ++k)
    {
        const float pw = power[(size_t) k];
        if (! spectralPrimed)
        {
            slowP[(size_t) k] = pw;
            longP[(size_t) k] = pw;
            dispP[(size_t) k] = pw;
        }
        else
        {
            slowP[(size_t) k] += slowCoef * (pw - slowP[(size_t) k]);
            longP[(size_t) k] += longCoef * (pw - longP[(size_t) k]);
            dispP[(size_t) k] += dispCoef * (pw - dispP[(size_t) k]);
        }
    }
    spectralPrimed = true;

    // Prefix sums for fast local spectral envelopes.
    prefix[0] = 0.0f;
    for (int k = 0; k < numBins; ++k)
        prefix[(size_t) k + 1] = prefix[(size_t) k] + power[(size_t) k];

    auto localMean = [this] (int k, float octaveFraction)
    {
        const int w = std::max (2, (int) std::round ((float) k * (std::pow (2.0f, octaveFraction) - 1.0f)));
        const int a = std::max (0, k - w), b = std::min (numBins - 1, k + w);
        return (prefix[(size_t) b + 1] - prefix[(size_t) a]) / (float) (b - a + 1);
    };

    // Local maximum over +-3 bins (nearest harmonic peak), for the bleed "gap" rule.
    for (int k = 0; k < numBins; ++k)
    {
        float mx = 0.0f;
        for (int j = std::max (0, k - 3); j <= std::min (numBins - 1, k + 3); ++j)
            mx = std::max (mx, power[(size_t) j]);
        localMax[(size_t) k] = mx;
    }

    // Bleed reference: recent peak of each band (1/6 octave), falling slowly.
    // floorP holds this peak reference.
    for (int k = 0; k < numBins; ++k)
    {
        const float band = localMean (k, 1.0f / 12.0f);
        auto& pk = floorP[(size_t) k];
        pk = std::max (band, std::max (pk * peakFall, 1.0e-14f));
    }

    // 3. Band measurements for sibilance and mud.
    const float binHz = binToHz (1);
    auto bandSum = [this, binHz] (float f0, float f1)
    {
        const int a = std::max (1, (int) (f0 / binHz)), b = std::min (numBins - 1, (int) (f1 / binHz));
        if (b < a) return 0.0f;
        return prefix[(size_t) b + 1] - prefix[(size_t) a];
    };

    const float aMetal = p.amounts[metallic], aSib = p.amounts[sibilance], aRes = p.amounts[resonance];
    const float aMud = p.amounts[mud], aBleed = p.amounts[bleed], aRev = p.amounts[deverb];

    const float sibRatio = toDb (bandSum (4500.0f, 11000.0f)) - toDb (bandSum (80.0f, 16000.0f));
    sibRatioSmooth += 0.6f * (sibRatio - sibRatioSmooth);
    const float sibReduction = aSib * clampf ((sibRatioSmooth + 14.0f) * 1.0f, 0.0f, 12.0f);

    // Mud is judged on the long-term spectrum (~2 s), so it acts like a slowly adapting EQ
    // instead of thinning every note that lands in the low-mids.
    auto longBandMean = [this, binHz] (float f0, float f1)
    {
        const int a = std::max (1, (int) (f0 / binHz)), b = std::min (numBins - 1, (int) (f1 / binHz));
        double sum = 0.0;
        for (int k = a; k <= b; ++k) sum += longP[(size_t) k];
        return b >= a ? (float) (sum / (b - a + 1)) : 0.0f;
    };
    const float mudExcess = toDb (longBandMean (150.0f, 450.0f))
                          - toDb (0.5f * (longBandMean (60.0f, 150.0f) + longBandMean (450.0f, 1200.0f)));
    const float mudTarget = aMud * clampf ((mudExcess - 4.0f) * 0.5f, 0.0f, 5.0f);
    mudReductionSmooth += 0.05f * (mudTarget - mudReductionSmooth);

    // Deverb: late reverb estimate = power ~60 ms ago, decayed with an assumed RT60 of 0.5 s.
    auto& oldest = powerHistory[(size_t) historyPos];
    const float lateDecay = std::pow (10.0f, -6.0f * (float) (historyFrames * frameSec) / 0.5f);
    const float revFloor = std::pow (10.0f, -aRev * 8.0f / 10.0f);  // at most -8 dB

    // 4. Per-bin reduction in dB.
    for (int k = 0; k < numBins; ++k)
    {
        const float f = binToHz (k);
        const float pw = power[(size_t) k] + eps;
        float red = 0.0f;

        if (f > 30.0f)
        {
            // Metallic highs: unstable, spiky energy above ~3 kHz pulled back toward its average.
            const float wMetal = ramp (f, 2500.0f, 4000.0f);
            if (aMetal > 0.0f && wMetal > 0.0f)
            {
                // Only energy that jumps above its own recent average (stable notes are left alone).
                const float over = toDb (pw) - toDb (slowP[(size_t) k] * 1.6f);
                red += wMetal * aMetal * clampf (over * 0.8f, 0.0f, 12.0f);
            }

            // Resonance: narrow peaks sticking out of the local spectral envelope.
            const float wRes = ramp (f, 250.0f, 400.0f) * (1.0f - ramp (f, 12000.0f, 15000.0f));
            if (aRes > 0.0f && wRes > 0.0f)
            {
                const float over = toDb (pw) - toDb (localMean (k, 1.0f / 3.0f) * 4.0f);
                red += wRes * aRes * clampf (over * 0.5f, 0.0f, 8.0f);
            }

            // Sibilance: whole S band ducked when it dominates the frame.
            if (sibReduction > 0.0f)
                red += sibReduction * ramp (f, 4000.0f, 5000.0f) * (1.0f - ramp (f, 11000.0f, 13000.0f));

            // Mud: dynamic dip around 150-450 Hz.
            if (mudReductionSmooth > 0.0f)
            {
                const float o = std::log2 (f / 260.0f) / 0.55f;
                red += mudReductionSmooth * std::exp (-0.5f * o * o);
            }

            // Bleed: quiet content that never rises far above the bin's own floor is gated down.
            if (aBleed > 0.0f && f > 40.0f)
            {
                const float hpDb = toDb (hMed[(size_t) k]) - toDb (pMed[(size_t) k]);   // > 0: held, < 0: hit
                if (p.stem == drums || p.stem == percussion)
                {
                    // Drum / percussion stem: the bleed is held notes (bass, synth, voice) between the hits.
                    if (f > 150.0f && f < 8000.0f)
                        red += aBleed * 12.0f * clamp01 ((hpDb - 9.0f) / 12.0f);
                }
                else if (p.stem == fullMix)
                {
                    // Full mix: everything belongs to the song, only very low residue is lowered.
                    const float below = toDb (floorP[(size_t) k]) - toDb (pw);
                    red += aBleed * 8.0f * clamp01 ((below - 35.0f) / 15.0f);
                }
                else
                {
                    // Tonal stem: the bleed is drum hits (short, broadband).
                    red += aBleed * 10.0f * clamp01 ((-hpDb - 5.0f) / 12.0f);
                }
            }

            // Deverb: subtract the predicted late-reverb tail.
            // Only acts while the bin is decaying (a tail), never on a note that is starting or held.
            if (aRev > 0.0f && f > 250.0f && pw < prevPower[(size_t) k] * 0.9f)
            {
                const float late = oldest[(size_t) k] * lateDecay;
                const float g = std::max (1.0f - 0.45f * aRev * late / pw, revFloor);
                red += -toDb (g);
            }
        }

        targetDb[(size_t) k] = std::min (red, 36.0f);
    }

    // Remember this frame for the deverb estimate.
    std::copy (power.begin(), power.end(), oldest.begin());
    std::copy (power.begin(), power.end(), prevPower.begin());
    historyPos = (historyPos + 1) % historyFrames;

    // 5. Smooth gains across frequency then time (fast attack, gentle release) to avoid "musical noise".
    for (int k = 0; k < numBins; ++k)
    {
        auto at = [this] (int i) { return targetDb[(size_t) juce::jlimit (0, numBins - 1, i)]; };
        tmpDb[(size_t) k] = 0.1f * at (k - 2) + 0.2f * at (k - 1) + 0.4f * at (k) + 0.2f * at (k + 1) + 0.1f * at (k + 2);
    }
    const float attack = 0.5f, release = onePole (0.09, frameSec);
    for (int k = 0; k < numBins; ++k)
    {
        auto& s = gainDbSmooth[(size_t) k];
        const float t = tmpDb[(size_t) k];
        s += (t > s ? attack : release) * (t - s);
    }

    // 6. Apply gains, inverse FFT, overlap-add.
    const float olaScale = (float) hopSize / ((float) fftSize * 0.5f);
    for (int c = 0; c < chans; ++c)
    {
        auto& buf = fftBuf[(size_t) c];
        for (int k = 0; k < numBins; ++k)
        {
            const float g = std::pow (10.0f, -gainDbSmooth[(size_t) k] / 20.0f);
            buf[(size_t) (2 * k)] *= g;
            buf[(size_t) (2 * k + 1)] *= g;
            if (k > 0 && k < fftSize / 2)
            {
                const int m = fftSize - k;
                buf[(size_t) (2 * m)] *= g;
                buf[(size_t) (2 * m + 1)] *= g;
            }
        }
        fft->performRealOnlyInverseTransform (buf.data());

        auto& acc = outAccum[(size_t) c];
        for (int i = 0; i < fftSize; ++i)
            acc[(size_t) ((writePos + i) % fftSize)] += buf[(size_t) i] * window[(size_t) i] * olaScale;
    }

    // 7. Display spectrum and automatic cuts.
    {
        std::array<float, displayPoints> tmp {};
        const float logMin = std::log (displayMinHz), logMax = std::log (displayMaxHz);
        for (int i = 0; i < displayPoints; ++i)
        {
            const float f0 = std::exp (logMin + (logMax - logMin) * (float) i / displayPoints);
            const float f1 = std::exp (logMin + (logMax - logMin) * (float) (i + 1) / displayPoints);
            const int a = juce::jlimit (1, numBins - 1, (int) (f0 / binHz));
            const int b = juce::jlimit (a, numBins - 1, (int) (f1 / binHz));
            float mx = 0.0f;
            for (int k = a; k <= b; ++k)
                mx = std::max (mx, dispP[(size_t) k]);
            tmp[(size_t) i] = toDb (mx);
        }
        if (displayLock.tryEnter())
        {
            displaySpectrum = tmp;
            displayLock.exit();
        }
    }

    if (++framesSinceAuto * frameSec >= 0.25)
    {
        framesSinceAuto = 0;
        updateAutoCuts (p);
    }
}
}
