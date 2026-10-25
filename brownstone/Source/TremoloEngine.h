/*
    Brownstone 61 — tremolo engine
    -------------------------------
    Three classic amp tremolo circuits:

    HARMONIC  (brown/blonde amps, c.1959-63)
        The guitar is split into a low band and a high band. One LFO turns the
        low band up while it turns the high band down, and vice versa. The
        result is a phasey, swirling pulse rather than a simple volume wobble.

    BIAS      (tweed era)
        The LFO wiggles the bias of the power tubes, so the volume swells and
        dips smoothly with a soft, slightly lopsided shape.

    OPTO      (blackface era)
        The LFO flashes a neon lamp next to a light-dependent resistor. The
        lamp switches on fast and the resistor recovers slowly, which makes
        the classic choppy, throbbing pulse.

    No plugin-framework code here, so it can be tested offline.
*/
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

namespace brownstone
{
constexpr double kTwoPi = 6.283185307179586;
constexpr float kPi = 3.14159265358979f;

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, s1 = 0, s2 = 0;
    float process (float x)
    {
        const float y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { s1 = s2 = 0.0f; }
    void setLowPass (float fc, float q, float fs)
    {
        const float w = 2.0f * kPi * fc / fs, c = std::cos (w), al = std::sin (w) / (2.0f * q);
        norm ((1 - c) * 0.5f, 1 - c, (1 - c) * 0.5f, 1 + al, -2 * c, 1 - al);
    }
    void setHighPass (float fc, float q, float fs)
    {
        const float w = 2.0f * kPi * fc / fs, c = std::cos (w), al = std::sin (w) / (2.0f * q);
        norm ((1 + c) * 0.5f, -(1 + c), (1 + c) * 0.5f, 1 + al, -2 * c, 1 - al);
    }

private:
    void norm (float nb0, float nb1, float nb2, float na0, float na1, float na2)
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }
};

struct OnePoleHP
{
    float b = 0, z = 0;
    void set (float fc, float fs) { b = 1.0f - std::exp (-2.0f * kPi * fc / fs); }
    float process (float x) { z += b * (x - z); return x - z; }
    void reset() { z = 0; }
};

//==============================================================================
enum class Mode { harmonic = 0, bias = 1, opto = 2 };

struct EngineParams
{
    float speed = 4.5f;      // 0..10 (free-running)
    int division = 3;        // index into kDivisions (synced)
    float intensity = 6.0f;  // 0..10
    float crossover = 5.0f;  // 0..10  (harmonic mode split point)
    Mode mode = Mode::harmonic;
    bool sync = false;
    bool on = true;          // footswitch
    bool stereo = false;
    float outputDb = 0.0f;

    // host transport (for sync)
    double bpm = 120.0;
    bool playing = false;
    double ppqAtBlockStart = 0.0;
};

/** Beats per LFO cycle for each synced division. */
static constexpr double kDivisions[] = { 4.0, 2.0, 1.5, 1.0, 2.0 / 3.0, 0.75, 0.5, 1.0 / 3.0, 0.25 };
static constexpr int kNumDivisions = 9;
inline const char* divisionName (int i)
{
    static const char* names[] = { "1/1", "1/2", "1/4.", "1/4", "1/4T", "1/8.", "1/8", "1/8T", "1/16" };
    return names[juce::jlimit (0, kNumDivisions - 1, i)];
}

inline float speedToHz (float speed) { return 0.5f * std::pow (24.0f, juce::jlimit (0.0f, 10.0f, speed) / 10.0f); } // 0.5 .. 12 Hz
inline float crossoverToHz (float c) { return 250.0f * std::pow (12.0f, juce::jlimit (0.0f, 10.0f, c) / 10.0f); } // 250 .. 3000 Hz

//==============================================================================
class TremoloEngine
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        for (auto* sv : { &depthSm, &outSm, &onSm, &xoverSm })
            sv->reset (sampleRate, 0.03);
        dc.set (8.0f, fs);
        dcR.set (8.0f, fs);
        // neon lamp / LDR timing for opto mode
        ldrAttack = 1.0f - std::exp (-1.0f / (0.003f * fs));
        ldrRelease = 1.0f - std::exp (-1.0f / (0.045f * fs));
        reset();
        prime = true;
    }

    void reset()
    {
        lpL.reset(); hpL.reset(); dc.reset(); dcR.reset();
        phase = 0.0;
        ldrL = ldrR = 0.0f;
    }

    /** 0..1 — how "lit" the pilot lamp should be right now (GUI reads this). */
    std::atomic<float> lampLevel { 0.0f };

    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const EngineParams& p)
    {
        // ---- targets
        const float i = juce::jlimit (0.0f, 10.0f, p.intensity) / 10.0f;
        float depth = 0.0f;
        switch (p.mode)
        {
            case Mode::harmonic: depth = 0.95f * std::pow (i, 1.2f); makeupK = 0.38f; break;
            case Mode::bias:     depth = 0.90f * std::pow (i, 1.3f); makeupK = 0.40f; break;
            case Mode::opto:     depth = 0.94f * std::pow (i, 1.1f); makeupK = 0.42f; break;
        }
        depthSm.setTargetValue (depth);
        onSm.setTargetValue (p.on ? 1.0f : 0.0f);
        outSm.setTargetValue (juce::Decibels::decibelsToGain (p.outputDb, -60.0f));
        xoverSm.setTargetValue (crossoverToHz (p.crossover));
        if (prime)
        {
            for (auto* sv : { &depthSm, &outSm, &onSm, &xoverSm })
                sv->setCurrentAndTargetValue (sv->getTargetValue());
            setCrossover (xoverSm.getCurrentValue());
            prime = false;
        }

        // ---- LFO rate (and phase lock to the song when synced and playing)
        double cyclesPerSample;
        if (p.sync)
        {
            const double beats = kDivisions[juce::jlimit (0, kNumDivisions - 1, p.division)];
            const double bpm = juce::jlimit (20.0, 400.0, p.bpm);
            cyclesPerSample = (bpm / 60.0) / beats / (double) fs;
            if (p.playing)
            {
                const double cyc = p.ppqAtBlockStart / beats;
                phase = cyc - std::floor (cyc);
            }
        }
        else
        {
            cyclesPerSample = (double) speedToHz (p.speed) / (double) fs;
        }

        float lampAcc = 0.0f;

        for (int s = 0; s < n; ++s)
        {
            if ((s & 31) == 0 && xoverSm.isSmoothing())
                setCrossover (xoverSm.skip (32));

            const float x = inR ? 0.5f * (inL[s] + inR[s]) : inL[s];

            const float d = depthSm.getNextValue() * onSm.getNextValue();
            const float mk = 1.0f / (1.0f - makeupK * d); // keeps the average level steady as intensity rises
            const float og = outSm.getNextValue();

            const double th = kTwoPi * phase;
            phase += cyclesPerSample;
            if (phase >= 1.0) phase -= 1.0;

            float yl = 0.0f, yr = 0.0f, lamp = 0.0f;

            switch (p.mode)
            {
                case Mode::harmonic:
                {
                    // tube phase-shift oscillator: a sine with rounded, slightly flattened peaks
                    const float m = tubeLfo ((float) std::sin (th));
                    const float lo = lpL.process (x);
                    const float hi = hpL.process (x);
                    const float gLo = 1.0f - d * 0.5f * (1.0f + m);
                    const float gHi = 1.0f - d * 0.5f * (1.0f - m);
                    yl = lo * gLo + hi * gHi;
                    yr = p.stereo ? lo * gHi + hi * gLo : yl; // wide: bands swap sides -> swirl
                    lamp = 0.5f + 0.5f * m;
                    break;
                }
                case Mode::bias:
                {
                    // power-tube bias wobble: soft, slightly lopsided swell
                    const auto biasShape = [] (double t)
                    {
                        const float v = (float) (std::sin (t) + 0.18 * std::sin (2.0 * t + 0.6));
                        return std::tanh (1.3f * v) / std::tanh (1.3f * 1.1f);
                    };
                    const float mL = biasShape (th);
                    const float mR = p.stereo ? biasShape (th + juce::MathConstants<double>::pi) : mL;
                    yl = x * (1.0f - d * 0.5f * (1.0f + juce::jlimit (-1.0f, 1.0f, mL)));
                    yr = x * (1.0f - d * 0.5f * (1.0f + juce::jlimit (-1.0f, 1.0f, mR)));
                    lamp = 0.5f - 0.5f * mL;
                    break;
                }
                case Mode::opto:
                {
                    // neon lamp fires on the upper part of the cycle; LDR is quick to dim, slow to recover
                    const auto lampOn = [] (double t) { return std::sin (t) > -0.15 ? 1.0f : 0.0f; };
                    const float targetL = lampOn (th);
                    const float targetR = p.stereo ? lampOn (th + juce::MathConstants<double>::pi) : targetL;
                    ldrL += (targetL > ldrL ? ldrAttack : ldrRelease) * (targetL - ldrL);
                    ldrR += (targetR > ldrR ? ldrAttack : ldrRelease) * (targetR - ldrR);
                    yl = x * (1.0f - d * ldrL);
                    yr = x * (1.0f - d * (p.stereo ? ldrR : ldrL));
                    lamp = ldrL;
                    break;
                }
            }

            // a little tube warmth from the recovery stage, then DC block
            yl = dc.process (warm (yl * mk));
            yr = (p.stereo ? warmR (yr * mk) : yl);

            outL[s] = yl * og;
            if (outR) outR[s] = yr * og;

            lampAcc += lamp;
        }

        lampLevel.store (n > 0 ? lampAcc / (float) n : 0.0f);
    }

private:
    void setCrossover (float fc)
    {
        // gentle, overlapping RC-style filters like the original: the bands don't sum
        // perfectly flat, which is part of the phasey harmonic-vibrato character
        lpL.setLowPass (fc * 0.8f, 0.5f, fs);
        hpL.setHighPass (fc * 1.25f, 0.5f, fs);
    }

    static float tubeLfo (float s)
    {
        return std::tanh (1.8f * s) / std::tanh (1.8f);
    }

    static float warm (float x) { return std::tanh (x * 0.8f) / 0.8f; }
    float warmR (float x) { return dcR.process (warm (x)); }

    float fs = 44100.0f;
    double phase = 0.0;
    Biquad lpL, hpL;
    OnePoleHP dc, dcR;
    float ldrL = 0.0f, ldrR = 0.0f, ldrAttack = 0.1f, ldrRelease = 0.01f;
    juce::SmoothedValue<float> depthSm, outSm, onSm, xoverSm;
    float makeupK = 0.38f;
    bool prime = true;
};

} // namespace brownstone
