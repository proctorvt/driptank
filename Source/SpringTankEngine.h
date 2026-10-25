/*
    Driptank '63 — spring reverb engine
    ------------------------------------
    A model of a 1960s-style outboard tube spring-reverb unit:

      guitar ──┬──────────── dry path (12AX7 stage, subtle warmth) ──────────────┐
               │                                                                 │
               └─ DWELL ─ driver tube (2x oversampled) ─ transformer ─ TANK ─┐    MIXER ── OUTPUT
                                                                   recovery ─ TONE ─┘

    The tank is two springs of different lengths. Each spring is a feedback
    delay loop containing a cascade of "stretched" first-order allpass filters
    (after J. Parker / V. Välimäki's spring dispersion work). That cascade is
    what produces the chirpy "drip" — higher frequencies take longer to travel
    down a spring than low ones, and the effect compounds on every reflection.

    No JUCE plugin dependencies here except juce::dsp::Oversampling, so the
    engine can be tested offline.
*/
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace driptank
{
constexpr float kPi = 3.14159265358979f;

//==============================================================================
struct OnePoleLP
{
    float b = 1.0f, z = 0.0f;
    void set (float fc, float fs) { b = 1.0f - std::exp (-2.0f * kPi * fc / fs); }
    float process (float x) { z += b * (x - z); return z; }
    void reset() { z = 0.0f; }
};

struct OnePoleHP
{
    OnePoleLP lp;
    void set (float fc, float fs) { lp.set (fc, fs); }
    float process (float x) { return x - lp.process (x); }
    void reset() { lp.reset(); }
};

// RBJ biquad, transposed direct form II
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
    void setHighShelf (float fc, float gainDb, float fs)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 2.0f * kPi * fc / fs, c = std::cos (w), s = std::sin (w);
        const float al = s / 2.0f * std::sqrt (2.0f); // S = 1
        const float sq = 2.0f * std::sqrt (A) * al;
        norm (A * ((A + 1) + (A - 1) * c + sq),
              -2 * A * ((A - 1) + (A + 1) * c),
              A * ((A + 1) + (A - 1) * c - sq),
              (A + 1) - (A - 1) * c + sq,
              2 * ((A - 1) - (A + 1) * c),
              (A + 1) - (A - 1) * c - sq);
    }

private:
    void norm (float nb0, float nb1, float nb2, float na0, float na1, float na2)
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }
};

// Small, fast PRNG (xorshift) — returns -1..1
struct Noise
{
    uint32_t s = 0x9E3779B9u;
    float next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
};

// Schroeder allpass used as a light diffuser inside each spring loop
struct DiffuserAP
{
    std::vector<float> buf; int idx = 0; float g = 0.5f;
    void prepare (int len, float gain) { buf.assign ((size_t) std::max (1, len), 0.0f); idx = 0; g = gain; }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }
    float process (float x)
    {
        const float d = buf[(size_t) idx];
        const float w = x + g * d;
        buf[(size_t) idx] = w;
        if (++idx >= (int) buf.size()) idx = 0;
        return d - g * w;
    }
};

//==============================================================================
/** One spring: pre-delay (first arrival), then a feedback loop of
    delay -> dispersion (stretched allpass cascade) -> damping. */
class Spring
{
public:
    void prepare (double sampleRate, float delayMs, int numStages, float apCoeff,
                  float chirpHz, float lossHz, uint32_t seed)
    {
        fs = (float) sampleRate;
        N = numStages;
        a = apCoeff;
        K = std::max (1, (int) std::lround (fs / (2.0f * chirpHz)));
        ap.assign ((size_t) (N * K), 0.0f);
        apIdx = 0;

        preLen = std::max (1, (int) std::lround (delayMs * 0.001f * fs));
        pre.assign ((size_t) preLen, 0.0f);
        preIdx = 0;

        // round trip = twice the spring length
        fbDelay = 2.0f * (float) preLen;
        modDepth = 0.00018f * fs; // ~0.18 ms wobble
        fbLen = (int) std::ceil (fbDelay + modDepth + 4.0f);
        fb.assign ((size_t) fbLen, 0.0f);
        fbIdx = 0;

        loopLP.set (lossHz, fs);
        loopHP.set (60.0f, fs);
        diffA.prepare ((int) std::lround (0.0031f * fs), 0.45f);
        diffB.prepare ((int) std::lround (0.0013f * fs), 0.45f);

        rng.s = seed;
        modLP.set (1.2f, fs);

        // Average loop transit time used to set the feedback gain for a target T60.
        const float apDelay = (float) (N * K) * (1.0f - a) / (1.0f + a) * 2.0f; // rough mid-band estimate
        loopSeconds = (fbDelay + apDelay) / fs + 0.0044f;
        setDecay (decaySeconds);
        reset();
    }

    void setDecay (float t60)
    {
        decaySeconds = t60;
        g = std::pow (10.0f, -3.0f * loopSeconds / std::max (0.1f, t60));
    }

    void reset()
    {
        std::fill (ap.begin(), ap.end(), 0.0f);
        std::fill (pre.begin(), pre.end(), 0.0f);
        std::fill (fb.begin(), fb.end(), 0.0f);
        loopLP.reset(); loopHP.reset(); diffA.reset(); diffB.reset(); modLP.reset();
    }

    float process (float x)
    {
        // first arrival
        const float xd = pre[(size_t) preIdx];
        pre[(size_t) preIdx] = x;
        if (++preIdx >= preLen) preIdx = 0;

        // slowly wandering round-trip time (springs are never perfectly still)
        const float m = modLP.process (rng.next()) * 6.0f;
        const float d = fbDelay + modDepth * juce::jlimit (-1.0f, 1.0f, m);
        float rp = (float) fbIdx - d;
        while (rp < 0.0f) rp += (float) fbLen;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % fbLen;
        const float fr = rp - (float) i0;
        const float echo = fb[(size_t) i0] + fr * (fb[(size_t) i1] - fb[(size_t) i0]);

        float s = xd + g * echo;

        // dispersion: N stretched first-order allpasses, H(z) = (a + z^-K) / (1 + a z^-K)
        float* buf = ap.data();
        for (int st = 0; st < N; ++st)
        {
            float& w = buf[st * K + apIdx];
            const float wk = w;
            const float wn = s - a * wk;
            s = a * wn + wk;
            w = wn;
        }
        if (++apIdx >= K) apIdx = 0;

        s = diffB.process (diffA.process (s));
        s = loopHP.process (loopLP.process (s));
        s = s / (1.0f + 0.08f * s * s); // never louder than its input -> loop always stable

        fb[(size_t) fbIdx] = s;
        if (++fbIdx >= fbLen) fbIdx = 0;
        return s;
    }

private:
    float fs = 44100.0f, a = 0.6f, g = 0.0f, fbDelay = 1.0f, modDepth = 0.0f;
    float loopSeconds = 0.07f, decaySeconds = 3.0f;
    int N = 1, K = 1, apIdx = 0, preLen = 1, preIdx = 0, fbLen = 1, fbIdx = 0;
    std::vector<float> ap, pre, fb;
    OnePoleLP loopLP, modLP;
    OnePoleHP loopHP;
    DiffuserAP diffA, diffB;
    Noise rng;
};

//==============================================================================
struct EngineParams
{
    float dwell = 6.0f;    // 0..10
    float mix = 4.0f;      // 0..10
    float tone = 6.0f;     // 0..10
    int tank = 2;          // 0 short, 1 medium, 2 long
    float outputDb = 0.0f; // -24..+12
    bool reverbOn = true;  // footswitch
    bool stereo = false;   // spread the two springs left/right
};

//==============================================================================
class SpringTankEngine
{
public:
    static constexpr int kChunk = 512;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            1, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampler->initProcessing ((size_t) kChunk);

        driveBuf.assign (kChunk, 0.0f);
        monoBuf.assign (kChunk, 0.0f);

        // Two springs, like a long-decay 2-spring tank. Slightly different
        // lengths, dispersion and damping keep them from sounding identical.
        springA.prepare (sampleRate, 29.0f, 90, 0.62f, 4300.0f, 5200.0f, 0x1234567u);
        springB.prepare (sampleRate, 37.5f, 80, 0.58f, 4600.0f, 4700.0f, 0x89ABCDEu);

        inputHP.set (110.0f, fs);           // coupling cap into the driver
        xfmrLP.setLowPass (6500.0f, 0.6f, fs); // transformer top end
        tankHP_L.setHighPass (190.0f, 0.7f, fs);
        tankHP_R.setHighPass (190.0f, 0.7f, fs);
        tankLP_L.setLowPass (4800.0f, 0.75f, fs);
        tankLP_R.setLowPass (4800.0f, 0.75f, fs);
        dryDC.set (8.0f, fs);
        wetDC_L.set (12.0f, fs);
        wetDC_R.set (12.0f, fs);

        const double ramp = 0.03;
        for (auto* sv : { &dwellGain, &dryGain, &wetGain, &outGain, &toneSm, &brightSm })
            sv->reset (sampleRate, ramp);

        kickDecay = std::exp (-1.0f / (0.015f * fs)); // ~15 ms burst
        lastTank = -1;
        reset();
        primeSmoothers = true;
    }

    void reset()
    {
        if (oversampler) oversampler->reset();
        springA.reset(); springB.reset();
        inputHP.reset(); xfmrLP.reset();
        tankHP_L.reset(); tankHP_R.reset(); tankLP_L.reset(); tankLP_R.reset();
        toneLP_L.reset(); toneLP_R.reset(); toneHS_L.reset(); toneHS_R.reset();
        dryDC.reset(); wetDC_L.reset(); wetDC_R.reset();
        kickEnv = 0.0f;
    }

    /** Thread-safe: call from any thread (GUI button, automation). */
    void triggerKick() { kickRequested.store (true); }

    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples,
                  const EngineParams& p)
    {
        updateTargets (p);

        int pos = 0;
        while (pos < numSamples)
        {
            const int n = std::min (kChunk, numSamples - pos);
            processChunk (inL + pos, inR ? inR + pos : nullptr, outL + pos, outR ? outR + pos : nullptr, n, p.stereo);
            pos += n;
        }
    }

private:
    static float dwellToGain (float dwell)
    {
        const float d = juce::jlimit (0.0f, 10.0f, dwell) / 10.0f;
        return 12.0f * d * d;
    }

    void updateTargets (const EngineParams& p)
    {
        if (p.tank != lastTank)
        {
            static constexpr float t60[] = { 2.3f, 3.4f, 4.8f };
            const float t = t60[juce::jlimit (0, 2, p.tank)];
            springA.setDecay (t);
            springB.setDecay (t * 0.93f);
            lastTank = p.tank;
        }

        const float m = juce::jlimit (0.0f, 10.0f, p.mix) / 10.0f;
        const float dryT = std::cos (m * kPi * 0.5f);
        const float wetT = p.reverbOn ? std::sin (m * kPi * 0.5f) * kWetMakeup : 0.0f;
        const float outT = juce::Decibels::decibelsToGain (p.outputDb, -60.0f);
        const float toneN = juce::jlimit (0.0f, 10.0f, p.tone) / 10.0f;

        dwellGain.setTargetValue (dwellToGain (p.dwell));
        dryGain.setTargetValue (dryT);
        wetGain.setTargetValue (wetT);
        outGain.setTargetValue (outT);
        toneSm.setTargetValue (900.0f * std::pow (14.0f, toneN));                   // 900 Hz .. 12.6 kHz
        brightSm.setTargetValue (toneN > 0.6f ? (toneN - 0.6f) / 0.4f * 4.5f : 0.0f); // up to +4.5 dB sparkle

        if (primeSmoothers)
        {
            for (auto* sv : { &dwellGain, &dryGain, &wetGain, &outGain, &toneSm, &brightSm })
                sv->setCurrentAndTargetValue (sv->getTargetValue());
            updateToneFilters (toneSm.getCurrentValue(), brightSm.getCurrentValue());
            primeSmoothers = false;
        }
    }

    void updateToneFilters (float fc, float brightDb)
    {
        toneLP_L.set (fc, fs);
        toneLP_R.set (fc, fs);
        toneHS_L.setHighShelf (2500.0f, brightDb, fs);
        toneHS_R.setHighShelf (2500.0f, brightDb, fs);
    }

    // Single-ended pentode-ish driver: asymmetric soft clip
    static float driverTube (float x)
    {
        constexpr float bias = 0.18f;
        return (std::tanh (x + bias) - std::tanh (bias)) * 1.15f;
    }

    // 12AX7 recovery stage: gentle, only clips when the tank is slammed
    static float recoveryTube (float x)
    {
        return std::tanh (x * 0.7f) / 0.7f;
    }

    void processChunk (const float* inL, const float* inR, float* outL, float* outR, int n, bool stereo)
    {
        // ---- input: sum to mono (a guitar is mono anyway)
        for (int i = 0; i < n; ++i)
            monoBuf[(size_t) i] = inR ? 0.5f * (inL[i] + inR[i]) : inL[i];

        // ---- DWELL -> driver tube, 2x oversampled to keep the clipping clean
        for (int i = 0; i < n; ++i)
            driveBuf[(size_t) i] = inputHP.process (monoBuf[(size_t) i]) * dwellGain.getNextValue();

        {
            float* chans[] = { driveBuf.data() };
            juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
            auto up = oversampler->processSamplesUp (block);
            float* u = up.getChannelPointer (0);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                u[i] = driverTube (u[i]);
            oversampler->processSamplesDown (block);
        }

        for (int i = 0; i < n; ++i)
        {
            if ((i & 31) == 0)
            {
                // tone filters are re-tuned every 32 samples while the knob moves
                if (toneSm.isSmoothing() || brightSm.isSmoothing())
                {
                    const float fc = toneSm.skip (32);
                    const float bd = brightSm.skip (32);
                    updateToneFilters (fc, bd);
                }
            }

            const float dry = monoBuf[(size_t) i];

            // ---- transformer into the tank
            float t = xfmrLP.process (driveBuf[(size_t) i]) * 0.55f;

            // ---- "kick the amp"
            if (kickRequested.exchange (false))
                kickEnv = 1.0f;
            if (kickEnv > 1.0e-4f)
            {
                t += kickNoise.next() * kickEnv * 5.0f;
                kickEnv *= kickDecay;
            }

            // ---- the springs
            const float a = springA.process (t);
            const float b = springB.process (t);

            float wl, wr;
            if (stereo) { wl = a + 0.3f * b; wr = b + 0.3f * a; }
            else        { wl = wr = 0.5f * (a + b) * 1.3f; }

            // ---- pickup transducer band-limit, recovery tube, TONE
            wl = tankLP_L.process (tankHP_L.process (wl));
            wl = toneHS_L.process (toneLP_L.process (recoveryTube (wl * kRecoveryGain)));
            wl = wetDC_L.process (wl);

            if (stereo)
            {
                wr = tankLP_R.process (tankHP_R.process (wr));
                wr = toneHS_R.process (toneLP_R.process (recoveryTube (wr * kRecoveryGain)));
                wr = wetDC_R.process (wr);
            }
            else
            {
                wr = wl;
            }

            // ---- dry path through a 12AX7 stage: a touch of warmth, nothing more
            float d = dry + 0.035f * dry * dry;
            d = std::tanh (d * 0.6f) / 0.6f;
            d = dryDC.process (d);

            // ---- MIXER + output
            const float dg = dryGain.getNextValue();
            const float wg = wetGain.getNextValue();
            const float og = outGain.getNextValue();

            outL[i] = (d * dg + wl * wg) * og;
            if (outR)
                outR[i] = (d * dg + wr * wg) * og;
        }
    }

    // calibrated offline so Mixer ~5 / Dwell ~6 gives a balanced blend
    static constexpr float kWetMakeup = 0.24f;
    static constexpr float kRecoveryGain = 1.6f;

    float fs = 44100.0f;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::vector<float> driveBuf, monoBuf;

    Spring springA, springB;
    OnePoleHP inputHP, dryDC, wetDC_L, wetDC_R;
    Biquad xfmrLP, tankHP_L, tankHP_R, tankLP_L, tankLP_R, toneHS_L, toneHS_R;
    OnePoleLP toneLP_L, toneLP_R;

    juce::SmoothedValue<float> dwellGain, dryGain, wetGain, outGain, toneSm, brightSm;
    bool primeSmoothers = true;
    int lastTank = -1;

    std::atomic<bool> kickRequested { false };
    float kickEnv = 0.0f;
    float kickDecay = 0.9985f;
    Noise kickNoise;
};

} // namespace driptank
