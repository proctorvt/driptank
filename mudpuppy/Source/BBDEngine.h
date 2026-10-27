/*
    Mudpuppy — bucket-brigade analog delay engine
    ----------------------------------------------
    Models a classic 3-knob BBD slapback pedal (20–300 ms):

      in ─┬───────────────────────────── dry (untouched, true-bypass style) ───────┐
          │                                                                         │
          └─► (+) ─► compressor ─► anti-alias LPF ─► BBD chip ─► reconstruction ─► expander ──► BLEND ──► OUTPUT
               ▲                                                     LPF               │
               └──────────── FEEDBACK (soft-clipping, can self-oscillate) ◄────────────┘

    Things that give an analog delay its sound, all modelled here:
      * The BBD clock slows down for longer delays, so long repeats get darker.
      * The compander (NE570-style) squeezes the signal into the chip and expands
        it back out, which adds "breathing" and grit when pushed.
      * The chip clips softly when driven, so high feedback saturates instead of exploding.
      * Turning the delay knob bends the pitch of the repeats, like a real pedal.
      * A slight clock wobble gives the repeats a tape-ish shimmer.
*/
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>

namespace mudpuppy
{
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
    void reset() { s1 = s2 = 0; }
    void setLowPass (float fc, float q, float fs)
    {
        fc = juce::jlimit (20.0f, 0.45f * fs, fc);
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

struct OnePole
{
    float b = 1, z = 0;
    void setTime (float seconds, float fs) { b = 1.0f - std::exp (-1.0f / std::max (1.0e-5f, seconds * fs)); }
    void setFreq (float fc, float fs) { b = 1.0f - std::exp (-2.0f * kPi * fc / fs); }
    float process (float x) { z += b * (x - z); return z; }
    void reset (float v = 0) { z = v; }
};

struct Noise
{
    uint32_t s = 0x2545F491u;
    float next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
};

//==============================================================================
struct EngineParams
{
    float delayMs = 110.0f;  // 20..300
    float feedback = 3.0f;   // 0..10
    float blend = 5.0f;      // 0..10
    float wobble = 2.0f;     // 0..10
    float outputDb = 0.0f;
    bool on = true;          // footswitch
    bool trails = false;     // repeats keep ringing after switching off
};

constexpr float kMinDelayMs = 20.0f, kMaxDelayMs = 300.0f;
constexpr int kBBDStages = 4096;

/** Repeat gain for a FEEDBACK knob position. Above ~8.5 the loop runs away into self-oscillation. */
inline float feedbackGain (float knob)
{
    const float k = juce::jlimit (0.0f, 10.0f, knob) / 10.0f;
    return 1.22f * std::pow (k, 1.15f);
}

/** Bandwidth of the BBD at a given delay time: the clock slows for long delays. */
inline float bbdBandwidth (float delayMs)
{
    const float clock = (float) kBBDStages / (2.0f * delayMs * 0.001f); // Hz
    return std::min (6800.0f, 0.42f * clock);                           // 20 ms: 6.8 kHz ... 300 ms: ~2.9 kHz
}

//==============================================================================
class BBDEngine
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        int len = 1;
        while (len < (int) (fs * (kMaxDelayMs * 0.001f * 1.05f)) + 16)
            len <<= 1;
        buf.assign ((size_t) len, 0.0f);
        mask = len - 1;
        writeIdx = 0;

        delaySm.setTime (0.07f, fs);       // knob turns bend the pitch, like a real BBD clock
        compEnv.setTime (0.012f, fs);
        expEnv.setTime (0.012f, fs);
        wobbleNoiseLP.setFreq (0.9f, fs);
        fbHP.setHighPass (70.0f, 0.6f, fs);
        inHP.setHighPass (30.0f, 0.6f, fs);

        for (auto* sv : { &blendSm, &outSm, &inEnableSm, &wetEnableSm, &fbSm })
            sv->reset (sampleRate, 0.02);

        reset();
        prime = true;
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (auto* f : { &aa1, &aa2, &rc1, &rc2, &fbHP, &inHP })
            f->reset();
        compEnv.reset (kRef); expEnv.reset (kRef); wobbleNoiseLP.reset();
        fbSample = 0.0f;
        lfoPhase = 0.0;
        clearCountdown = 0;
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const EngineParams& p)
    {
        const float targetDelay = juce::jlimit (kMinDelayMs, kMaxDelayMs, p.delayMs) * 0.001f * fs;
        const float blendGain = std::pow (juce::jlimit (0.0f, 10.0f, p.blend) / 10.0f, 1.3f) * 1.05f;

        blendSm.setTargetValue (blendGain);
        outSm.setTargetValue (juce::Decibels::decibelsToGain (p.outputDb, -60.0f));
        fbSm.setTargetValue (feedbackGain (p.feedback));
        inEnableSm.setTargetValue (p.on ? 1.0f : 0.0f);
        wetEnableSm.setTargetValue (p.on || p.trails ? 1.0f : 0.0f);

        if (prime)
        {
            for (auto* sv : { &blendSm, &outSm, &inEnableSm, &wetEnableSm, &fbSm })
                sv->setCurrentAndTargetValue (sv->getTargetValue());
            delaySm.reset (targetDelay);
            updateFilters (targetDelay);
            prime = false;
        }

        // when fully bypassed (no trails), flush the chip once the wet path has faded out
        if (! p.on && ! p.trails)
        {
            if (clearCountdown == 0 && ! wetEnableSm.isSmoothing() && wetEnableSm.getCurrentValue() <= 0.0f)
                clearCountdown = 1;
        }
        else
        {
            clearCountdown = 0;
        }
        if (clearCountdown == 1)
        {
            reset();
            clearCountdown = 2;
        }

        const float wob = juce::jlimit (0.0f, 10.0f, p.wobble) / 10.0f;
        const double lfoInc = 0.55 / fs;

        for (int s = 0; s < n; ++s)
        {
            const float dry = inR ? 0.5f * (inL[s] + inR[s]) : inL[s];

            // ---- clock: smoothed delay time + wobble
            const float d0 = delaySm.process (targetDelay);
            if ((s & 31) == 0)
                updateFilters (d0);

            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0) lfoPhase -= 1.0;
            const float lfo = std::sin ((float) (lfoPhase * 2.0 * juce::MathConstants<double>::pi));
            const float drift = wobbleNoiseLP.process (noise.next()) * 25.0f;
            const float modAmt = d0 * (0.0006f + wob * 0.0075f);
            const float d = juce::jlimit (4.0f, (float) mask - 4.0f, d0 + modAmt * (0.7f * lfo + 0.3f * juce::jlimit (-1.0f, 1.0f, drift)));

            // ---- read the chip (Hermite interpolation)
            const float r = readHermite (d);

            // ---- reconstruction filter + expander
            float wet = rc2.process (rc1.process (r));
            const float le = expEnv.process (std::abs (wet) + 1.0e-6f);
            wet *= juce::jlimit (0.02f, 12.0f, le / kRef);

            // ---- feedback: soft clip so it saturates instead of exploding
            const float fbg = fbSm.getNextValue();
            float fb = fbHP.process (wet * fbg);
            fb = std::tanh (fb * 0.9f) / 0.9f;

            // ---- into the chip: compressor -> anti-alias filter -> chip saturation
            const float inEn = inEnableSm.getNextValue();
            float x = inHP.process (dry) * inEn + fb;
            const float lc = compEnv.process (std::abs (x) + 1.0e-6f);
            x *= juce::jlimit (0.08f, 14.0f, std::sqrt (kRef / lc));
            x = aa2.process (aa1.process (x));
            x = kChipClip * std::tanh (x / kChipClip);
            x += noise.next() * 2.0e-5f; // a whisper of chip hiss (the expander keeps it down when quiet)

            buf[(size_t) writeIdx] = x;
            writeIdx = (writeIdx + 1) & mask;

            // ---- mixer
            const float wg = blendSm.getNextValue() * wetEnableSm.getNextValue();
            const float y = (dry + wet * wg) * outSm.getNextValue();
            outL[s] = y;
            if (outR) outR[s] = y;
        }
    }

private:
    static constexpr float kRef = 0.12f;      // compander reference level
    static constexpr float kChipClip = 0.55f; // where the chip starts to saturate (companded domain)

    float readHermite (float d) const
    {
        float rp = (float) writeIdx - d;
        while (rp < 0.0f) rp += (float) (mask + 1);
        const int i1 = (int) rp;
        const float t = rp - (float) i1;
        const float xm1 = buf[(size_t) ((i1 - 1) & mask)];
        const float x0 = buf[(size_t) (i1 & mask)];
        const float x1 = buf[(size_t) ((i1 + 1) & mask)];
        const float x2 = buf[(size_t) ((i1 + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    void updateFilters (float delaySamples)
    {
        const float bw = bbdBandwidth (delaySamples / fs * 1000.0f);
        aa1.setLowPass (bw, 0.54f, fs);
        aa2.setLowPass (bw, 1.31f, fs);   // 4th-order Butterworth pair
        rc1.setLowPass (bw * 1.05f, 0.54f, fs);
        rc2.setLowPass (bw * 1.05f, 1.31f, fs);
    }

    float fs = 44100.0f;
    std::vector<float> buf;
    int mask = 0, writeIdx = 0;
    Biquad aa1, aa2, rc1, rc2, fbHP, inHP;
    OnePole delaySm, compEnv, expEnv, wobbleNoiseLP;
    Noise noise;
    double lfoPhase = 0.0;
    float fbSample = 0.0f;
    int clearCountdown = 0;
    juce::SmoothedValue<float> blendSm, outSm, inEnableSm, wetEnableSm, fbSm;
    bool prime = true;
};

} // namespace mudpuppy
