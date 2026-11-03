#pragma once

#include <array>

// Shared definitions for the eight cleaning modules and the per-stem presets.
namespace aic
{
    enum Module
    {
        metallic = 0,
        sibilance,
        resonance,
        mud,
        bleed,
        deverb,
        stereo,
        punch,
        numModules
    };

    enum Stem
    {
        vocals = 0,
        drums,
        bass,
        instruments,
        fullMix,
        numStems
    };

    inline const char* moduleParamID (int m)
    {
        static const char* ids[numModules] = { "metallic", "sibilance", "resonance", "mud",
                                               "bleed", "deverb", "stereo", "punch" };
        return ids[m];
    }

    inline const char* moduleName (int m)
    {
        static const char* names[numModules] = { "Metallic Highs", "Sibilance", "Resonance", "Mud",
                                                  "Bleed", "Deverb", "Stereo Fix", "Punch" };
        return names[m];
    }

    inline const char* stemName (int s)
    {
        static const char* names[numStems] = { "Vocals", "Drums", "Bass", "Instruments", "Full Mix" };
        return names[s];
    }

    struct StemPreset
    {
        std::array<float, numModules> amounts; // 0..100 (module maximum, scaled by the heart)
        float lowCut;                          // Hz, manual default
        float highCut;                         // Hz, manual default
        float autoLowMin, autoLowMax;          // Hz, range the Auto mode may choose from
        float autoHighMin, autoHighMax;        // Hz
    };

    inline const StemPreset& stemPreset (int s)
    {
        static const StemPreset presets[numStems] = {
            //  Metal Sib  Res  Mud Bleed Dev  Ster Punch   low     high     autoLow      autoHigh
            { { 80,  90,  70,  60,  90,  70,  80,  20 },   90.0f, 16000.0f,  60.0f, 160.0f, 11000.0f, 18000.0f }, // Vocals
            { { 90,  20,  60,  50,  70,  40,  50,  90 },   30.0f, 18000.0f,  20.0f,  60.0f, 13000.0f, 20000.0f }, // Drums
            { { 30,   0,  50,  80,  80,  30, 100,  40 },   30.0f,  5000.0f,  20.0f,  45.0f,  2500.0f,  9000.0f }, // Bass
            { { 80,  40,  70,  60,  70,  50,  70,  40 },   60.0f, 16000.0f,  25.0f, 120.0f, 10000.0f, 18500.0f }, // Instruments
            { { 70,  60,  50,  50,   0,  40,  60,  30 },   25.0f, 17000.0f,  20.0f,  40.0f, 13000.0f, 19500.0f }  // Full Mix
        };
        return presets[s];
    }
}
