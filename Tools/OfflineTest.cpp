// Offline checks for the AI Cleaner engine (not shipped with the plugin).
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/CleanerEngine.h"
#include <cstdio>
#include <random>
#include <chrono>

using namespace aic;

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok) ++failures;
}

static juce::AudioBuffer<float> makeTestSignal (double sr, int seconds, unsigned seed)
{
    const int n = (int) (sr * seconds);
    juce::AudioBuffer<float> b (2, n);
    std::mt19937 rng (seed);
    std::normal_distribution<float> nd (0.0f, 1.0f);
    float lp = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        // "Vocal": harmonic tone with vibrato and syllable envelope
        const double f0 = 220.0 * (1.0 + 0.01 * std::sin (2 * M_PI * 5.5 * t));
        double v = 0;
        for (int h = 1; h <= 20; ++h)
            v += std::sin (2 * M_PI * f0 * h * t) / (h * 1.3);
        const double env = 0.5 + 0.5 * std::sin (2 * M_PI * 2.0 * t);
        // "Shimmer": high noise bursts, "bleed": quiet low thump
        lp += 0.02f * (nd (rng) - lp);
        const float hiss = nd (rng) * 0.02f;
        const float l = (float) (0.15 * v * env) + hiss + lp * 0.2f;
        const float r = (float) (0.15 * v * env) + nd (rng) * 0.02f + lp * 0.2f;
        b.setSample (0, i, l);
        b.setSample (1, i, r);
    }
    return b;
}

static double rms (const juce::AudioBuffer<float>& b, int start, int len)
{
    double s = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = start; i < start + len; ++i)
            s += (double) b.getSample (c, i) * b.getSample (c, i);
    return std::sqrt (s / (len * b.getNumChannels()));
}

static juce::AudioBuffer<float> run (CleanerEngine& e, const juce::AudioBuffer<float>& in, const EngineParams& p, int block)
{
    juce::AudioBuffer<float> out (in);
    for (int pos = 0; pos < out.getNumSamples(); pos += block)
    {
        const int len = std::min (block, out.getNumSamples() - pos);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), out.getNumChannels(), pos, len);
        e.process (view, p);
    }
    return out;
}

static void writeWav (const juce::AudioBuffer<float>& b, double sr, const char* name)
{
    juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile (name);
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, (unsigned) b.getNumChannels(), 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int main (int argc, char** argv)
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("\n== %.0f Hz ==\n", sr);
        auto in = makeTestSignal (sr, 4, 1);

        CleanerEngine e;
        e.prepare (sr, 2);
        const int L = e.getLatencySamples();

        // 1. Transparency: everything off, manual cuts wide open -> output == input delayed by latency.
        EngineParams off;
        off.autoCuts = false; off.lowCut = 10.0f; off.highCut = 22000.0f; off.autoGain = false;
        auto out = run (e, in, off, 512);
        double err = 0, ref = 0;
        for (int c = 0; c < 2; ++c)
            for (int i = L + (int) sr; i < in.getNumSamples(); ++i)
            {
                const double d = out.getSample (c, i) - in.getSample (c, i - L);
                err += d * d; ref += (double) in.getSample (c, i - L) * in.getSample (c, i - L);
            }
        const double nullDb = 10 * std::log10 (err / ref + 1e-30);
        std::printf ("     null test residual: %.1f dB (latency %d)\n", nullDb, L);
        check (nullDb < -40.0, "modules off = transparent (aligned with reported latency)");

        // 2. Full cleaning at several block sizes stays finite and sane.
        EngineParams full;
        const auto& pr = stemPreset (vocals);
        for (int m = 0; m < numModules; ++m) full.amounts[(size_t) m] = pr.amounts[(size_t) m] / 100.0f;
        bool finite = true;
        double outRms = 0;
        for (int block : { 1, 64, 441, 4096 })
        {
            e.reset();
            auto o = run (e, in, full, block);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < o.getNumSamples(); ++i)
                    if (! std::isfinite (o.getSample (c, i)) || std::abs (o.getSample (c, i)) > 4.0f) finite = false;
            outRms = rms (o, (int) sr, o.getNumSamples() - (int) sr);
            if (block == 441 && sr == 48000.0 && argc > 1) { writeWav (in, sr, "in.wav"); writeWav (o, sr, "out.wav"); }
        }
        const double inRms = rms (in, (int) sr, in.getNumSamples() - (int) sr);
        std::printf ("     in %.1f dBFS, cleaned (auto gain) %.1f dBFS\n", 20 * std::log10 (inRms), 20 * std::log10 (outRms));
        check (finite, "full cleaning: finite output at block sizes 1/64/441/4096");
        check (std::abs (20 * std::log10 (outRms / inRms)) < 3.0, "auto gain keeps loudness within 3 dB");

        // 3. Cleaning actually removes something (delta has energy) but not everything.
        e.reset();
        EngineParams d = full; d.delta = true; d.autoGain = false;
        auto del = run (e, in, d, 512);
        const double delRms = rms (del, (int) sr, del.getNumSamples() - (int) sr);
        std::printf ("     removed signal: %.1f dB relative to input\n", 20 * std::log10 (delRms / inRms));
        check (delRms > inRms * 0.01 && delRms < inRms, "delta: removes some content, not the whole signal");

        // 4. Bypass returns the dry signal (delayed).
        e.reset();
        EngineParams by = full; by.bypass = true;
        auto bo = run (e, in, by, 512);
        double berr = 0;
        for (int i = L + (int) sr; i < in.getNumSamples(); ++i)
            berr = std::max (berr, (double) std::abs (bo.getSample (0, i) - in.getSample (0, i - L)));
        check (berr < 1e-4, "bypass = dry signal, same latency");

        // 5. Auto cuts land inside the vocal ranges.
        std::printf ("     auto cuts: low %.0f Hz, high %.0f Hz\n", e.getCurrentLowCut(), e.getCurrentHighCut());

        // 5b. Per-module effect on a clean sustained harmonic tone (should stay mostly untouched).
        {
            juce::AudioBuffer<float> tone (2, (int) sr * 4);
            for (int i = 0; i < tone.getNumSamples(); ++i)
            {
                double v = 0; const double t = i / sr;
                for (int h = 1; h <= 20; ++h) v += std::sin (2 * M_PI * 220.0 * h * t) / (h * 1.3);
                tone.setSample (0, i, (float) (0.15 * v)); tone.setSample (1, i, (float) (0.15 * v));
            }
            const double toneRms = rms (tone, (int) sr, tone.getNumSamples() - (int) sr);
            for (int m = 0; m < numModules; ++m)
            {
                for (int which = 0; which < 2; ++which)
                {
                    const auto& src = which == 0 ? tone : in;
                    CleanerEngine one; one.prepare (sr, 2);
                    EngineParams q; q.autoGain = false; q.delta = true; q.autoCuts = false; q.lowCut = 10; q.highCut = 22000;
                    q.amounts[(size_t) m] = 1.0f;
                    auto r = run (one, src, q, 512);
                    const double rr = rms (r, (int) sr + 4096, r.getNumSamples() - (int) sr - 4096);
                    const double base = which == 0 ? toneRms : inRms;
                    if (sr == 48000.0)
                        std::printf ("     %-15s removes %6.1f dB on %s\n", moduleName (m), 20 * std::log10 (rr / base + 1e-9), which == 0 ? "clean tone " : "dirty test");
                    if (which == 0 && m != stereo && m != punch)
                        check (rr < base * 0.3, "module leaves a clean sustained tone mostly intact (< -10 dB change)");
                }
            }
        }

        // 6. Mono works.
        CleanerEngine mono;
        mono.prepare (sr, 1);
        juce::AudioBuffer<float> monoIn (1, in.getNumSamples());
        monoIn.copyFrom (0, 0, in, 0, 0, in.getNumSamples());
        auto mo = run (mono, monoIn, full, 256);
        bool mfin = true;
        for (int i = 0; i < mo.getNumSamples(); ++i) if (! std::isfinite (mo.getSample (0, i))) mfin = false;
        check (mfin, "mono input works");

        // 7. Speed.
        e.reset();
        auto t0 = std::chrono::steady_clock::now();
        run (e, in, full, 512);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("     CPU: %.1f%% of real time (stereo)\n", 100.0 * secs / 4.0);
        check (secs < 4.0 * 0.15, "uses less than 15% of one core");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL GOOD" : "PROBLEMS", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
