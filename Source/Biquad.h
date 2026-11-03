#pragma once

#include <cmath>

namespace aic
{
    // Minimal transposed direct form II biquad (RBJ cookbook coefficients).
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;

        void reset() noexcept { z1 = z2 = 0; }

        inline float process (float xIn) noexcept
        {
            const double x = xIn;
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }

        void setLowPass (double sampleRate, double freq, double q) noexcept
        {
            const double w = 2.0 * M_PI * clampFreq (freq, sampleRate) / sampleRate;
            const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
            const double a0 = 1.0 + alpha;
            b0 = (1.0 - cw) * 0.5 / a0;
            b1 = (1.0 - cw) / a0;
            b2 = b0;
            a1 = -2.0 * cw / a0;
            a2 = (1.0 - alpha) / a0;
        }

        void setHighPass (double sampleRate, double freq, double q) noexcept
        {
            const double w = 2.0 * M_PI * clampFreq (freq, sampleRate) / sampleRate;
            const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
            const double a0 = 1.0 + alpha;
            b0 = (1.0 + cw) * 0.5 / a0;
            b1 = -(1.0 + cw) / a0;
            b2 = b0;
            a1 = -2.0 * cw / a0;
            a2 = (1.0 - alpha) / a0;
        }

        static double clampFreq (double f, double sr) noexcept
        {
            const double maxF = sr * 0.49;
            return f < 5.0 ? 5.0 : (f > maxF ? maxF : f);
        }
    };

    // 24 dB/octave Butterworth made of two biquads.
    struct Butterworth4
    {
        Biquad s1, s2;
        void reset() noexcept { s1.reset(); s2.reset(); }
        void setHighPass (double sr, double f) noexcept { s1.setHighPass (sr, f, 0.54119610); s2.setHighPass (sr, f, 1.30656296); }
        void setLowPass  (double sr, double f) noexcept { s1.setLowPass  (sr, f, 0.54119610); s2.setLowPass  (sr, f, 1.30656296); }
        inline float process (float x) noexcept { return s2.process (s1.process (x)); }
    };
}
