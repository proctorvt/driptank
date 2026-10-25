// Offline test tool: renders demo audio, checks the modulation, and screenshots the GUI.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>

using namespace juce;
static constexpr double fs = 48000.0;

struct Pluck
{
    std::vector<float> buf; size_t idx = 0; float decay = 0.996f;
    void start (float freq, float amp, float bright, float dec, Random& r)
    {
        buf.assign ((size_t) std::max (2.0, fs / freq), 0.0f);
        float lp = 0;
        for (auto& b : buf) { lp += bright * ((r.nextFloat() * 2 - 1) - lp); b = lp * amp; }
        idx = 0; decay = dec;
    }
    float next()
    {
        if (buf.empty()) return 0;
        const float x = buf[idx];
        buf[idx] = decay * 0.5f * (x + buf[(idx + 1) % buf.size()]);
        idx = (idx + 1) % buf.size();
        return x;
    }
};

// slow chord changes that ring out, the way you'd play through a tremolo
static AudioBuffer<float> makeChords()
{
    Random r (3);
    AudioBuffer<float> b (1, (int) (fs * 10));
    b.clear();
    std::vector<Pluck> st (6);
    auto midi = [] (int n) { return 440.0f * std::pow (2.0f, (n - 69) / 12.0f); };
    const int chords[3][6] = { { 40, 47, 52, 56, 59, 64 }, { 45, 52, 57, 61, 64, 69 }, { 47, 54, 59, 63, 66, 71 } };
    const double times[] = { 0.1, 3.4, 6.7 };
    int ci = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double now = i / fs;
        if (ci < 3 && now >= times[ci])
        {
            for (int s = 0; s < 6; ++s) st[(size_t) s].start (midi (chords[ci][s]), 0.35f, 0.45f, 0.9993f, r);
            ++ci;
        }
        float v = 0;
        for (auto& p : st) v += p.next();
        b.setSample (0, i, v * 0.45f);
    }
    return b;
}

static void setParam (BrownstoneProcessor& p, const String& id, float v)
{
    auto* par = p.apvts.getParameter (id);
    par->setValueNotifyingHost (par->convertTo0to1 (v));
}

static AudioBuffer<float> run (BrownstoneProcessor& p, const AudioBuffer<float>& in, int block = 256)
{
    p.setPlayConfigDetails (1, 2, fs, block);
    p.prepareToPlay (fs, block);
    AudioBuffer<float> out (2, in.getNumSamples());
    MidiBuffer m;
    for (int pos = 0; pos < in.getNumSamples(); pos += block)
    {
        const int n = std::min (block, in.getNumSamples() - pos);
        AudioBuffer<float> b (2, n);
        b.clear();
        b.copyFrom (0, 0, in, 0, pos, n);
        p.processBlock (b, m);
        out.copyFrom (0, pos, b, 0, 0, n);
        out.copyFrom (1, pos, b, 1, 0, n);
    }
    return out;
}

static void writeWav (const File& f, const AudioBuffer<float>& b)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<OutputStream> os (f.createOutputStream());
    auto* w = wav.createWriterFor (os.get(), fs, (unsigned) b.getNumChannels(), 24, {}, 0);
    os.release();
    std::unique_ptr<AudioFormatWriter> writer (w);
    writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static float rms (const float* x, int n) { double s = 0; for (int i = 0; i < n; ++i) s += x[i] * x[i]; return (float) std::sqrt (s / std::max (1, n)); }

// envelope sampled every 10 ms
static std::vector<float> envelope (const float* x, int n, int hop)
{
    std::vector<float> e;
    for (int i = 0; i + hop <= n; i += hop) e.push_back (rms (x + i, hop));
    return e;
}

static float correlation (const std::vector<float>& a, const std::vector<float>& b)
{
    const size_t n = std::min (a.size(), b.size());
    double ma = 0, mb = 0;
    for (size_t i = 0; i < n; ++i) { ma += a[i]; mb += b[i]; }
    ma /= n; mb /= n;
    double sab = 0, saa = 0, sbb = 0;
    for (size_t i = 0; i < n; ++i) { sab += (a[i] - ma) * (b[i] - mb); saa += (a[i] - ma) * (a[i] - ma); sbb += (b[i] - mb) * (b[i] - mb); }
    return (float) (sab / std::sqrt (saa * sbb + 1e-30));
}

// dominant modulation rate from an envelope (autocorrelation peak)
static float modRate (const std::vector<float>& e, float hopSec)
{
    double m = 0; for (auto v : e) m += v; m /= e.size();
    int best = 0; double bestV = -1;
    for (int lag = 4; lag < (int) e.size() / 2; ++lag)
    {
        double s = 0;
        for (size_t i = 0; i + (size_t) lag < e.size(); ++i) s += (e[i] - m) * (e[i + (size_t) lag] - m);
        if (s > bestV) { bestV = s; best = lag; }
    }
    return best > 0 ? 1.0f / (best * hopSec) : 0.0f;
}

static float modDepthDb (const std::vector<float>& e)
{
    auto sorted = e; std::sort (sorted.begin(), sorted.end());
    const float lo = sorted[sorted.size() / 20], hi = sorted[sorted.size() * 19 / 20];
    return Decibels::gainToDecibels (hi) - Decibels::gainToDecibels (lo + 1e-9f);
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File::getCurrentWorkingDirectory());
    outDir.createDirectory();
    const int hop = (int) (fs * 0.005);

    // ---- harmonic mode: low and high bands must pulse in opposite directions
    {
        AudioBuffer<float> lo (1, (int) (fs * 4)), hi (1, (int) (fs * 4));
        for (int i = 0; i < lo.getNumSamples(); ++i)
        {
            lo.setSample (0, i, 0.3f * std::sin (2 * MathConstants<double>::pi * 150.0 * i / fs));
            hi.setSample (0, i, 0.3f * std::sin (2 * MathConstants<double>::pi * 3500.0 * i / fs));
        }
        BrownstoneProcessor a, b;
        for (auto* p : { &a, &b }) { setParam (*p, "mode", 0); setParam (*p, "intensity", 8); setParam (*p, "speed", 4.5f); }
        auto oa = run (a, lo), ob = run (b, hi);
        const int skip = (int) fs / 2;
        auto ea = envelope (oa.getReadPointer (0) + skip, oa.getNumSamples() - skip, hop * 4);
        auto eb = envelope (ob.getReadPointer (0) + skip, ob.getNumSamples() - skip, hop * 4);
        std::cout << "harmonic: low/high envelope correlation = " << correlation (ea, eb)
                  << " (want strongly negative)\n"
                  << "harmonic: low band depth " << modDepthDb (ea) << " dB, high band depth " << modDepthDb (eb) << " dB\n"
                  << "harmonic: measured rate " << modRate (ea, hop * 4 / (float) fs) << " Hz (expected "
                  << brownstone::speedToHz (4.5f) << ")\n";
    }

    // ---- bias & opto depth/rate on noise, plus level steadiness across intensity
    {
        AudioBuffer<float> noise (1, (int) (fs * 4));
        Random r (5);
        for (int i = 0; i < noise.getNumSamples(); ++i) noise.setSample (0, i, (r.nextFloat() * 2 - 1) * 0.25f);
        const float inRms = rms (noise.getReadPointer (0), noise.getNumSamples());
        for (int mode = 0; mode < 3; ++mode)
            for (float inten : { 3.0f, 6.0f, 10.0f })
            {
                BrownstoneProcessor p;
                setParam (p, "mode", (float) mode); setParam (p, "intensity", inten); setParam (p, "speed", 6.0f);
                auto o = run (p, noise);
                auto e = envelope (o.getReadPointer (0) + 24000, o.getNumSamples() - 24000, hop);
                std::cout << "mode " << mode << " int " << inten << ": depth " << modDepthDb (e) << " dB, rate "
                          << modRate (e, hop / (float) fs) << " Hz, level vs input "
                          << Decibels::gainToDecibels (rms (o.getReadPointer (0), o.getNumSamples()) / inRms) << " dB\n";
            }

        BrownstoneProcessor off;
        setParam (off, "on", 0); setParam (off, "intensity", 10);
        auto o = run (off, noise);
        auto e = envelope (o.getReadPointer (0) + 24000, o.getNumSamples() - 24000, hop);
        std::cout << "footswitch off: depth " << modDepthDb (e) << " dB, level vs input "
                  << Decibels::gainToDecibels (rms (o.getReadPointer (0), o.getNumSamples()) / inRms) << " dB\n";
    }

    // ---- tempo sync on the engine directly (no host here): 120 bpm, 1/8 notes = 4 Hz, phase follows ppq
    {
        brownstone::TremoloEngine eng;
        eng.prepare (fs);
        brownstone::EngineParams ep;
        ep.mode = brownstone::Mode::bias; ep.intensity = 10; ep.sync = true; ep.division = 6; ep.bpm = 120; ep.playing = true;
        const int n = (int) fs * 4, block = 512;
        std::vector<float> in ((size_t) n, 0.25f), out ((size_t) n);
        for (int pos = 0; pos < n; pos += block)
        {
            ep.ppqAtBlockStart = (pos / fs) * 2.0; // 120 bpm = 2 beats per second
            eng.process (in.data() + pos, nullptr, out.data() + pos, nullptr, std::min (block, n - pos), ep);
        }
        auto e = envelope (out.data() + 4800, n - 4800, hop);
        std::cout << "sync 120bpm 1/8: rate " << modRate (e, hop / (float) fs) << " Hz (expected 4)\n";
    }

    // ---- stereo: left and right should pulse against each other in bias mode
    {
        AudioBuffer<float> dc (1, (int) (fs * 3));
        for (int i = 0; i < dc.getNumSamples(); ++i) dc.setSample (0, i, 0.2f * std::sin (2 * MathConstants<double>::pi * 440.0 * i / fs));
        BrownstoneProcessor p;
        setParam (p, "mode", 1); setParam (p, "stereo", 1); setParam (p, "intensity", 8);
        auto o = run (p, dc);
        auto l = envelope (o.getReadPointer (0) + 24000, o.getNumSamples() - 24000, hop);
        auto r = envelope (o.getReadPointer (1) + 24000, o.getNumSamples() - 24000, hop);
        std::cout << "stereo bias: L/R correlation " << correlation (l, r) << " (want negative)\n";
    }

    // ---- abuse
    {
        AudioBuffer<float> loud (1, (int) (fs * 2));
        Random r (9);
        for (int i = 0; i < loud.getNumSamples(); ++i) loud.setSample (0, i, r.nextFloat() * 2 - 1);
        bool ok = true; float pk = 0;
        for (int mode = 0; mode < 3; ++mode)
        {
            BrownstoneProcessor p;
            setParam (p, "mode", (float) mode); setParam (p, "intensity", 10); setParam (p, "speed", 10);
            setParam (p, "crossover", 0); setParam (p, "stereo", 1); setParam (p, "output", 12);
            auto o = run (p, loud, 33);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < o.getNumSamples(); ++i)
                {
                    const float v = o.getSample (c, i);
                    if (! std::isfinite (v)) ok = false;
                    pk = std::max (pk, std::abs (v));
                }
        }
        std::cout << "abuse: finite=" << ok << " peak=" << pk << "\n";
    }

    // ---- demo renders
    {
        auto chords = makeChords();
        AudioBuffer<float> dry (2, chords.getNumSamples());
        dry.copyFrom (0, 0, chords, 0, 0, chords.getNumSamples());
        dry.copyFrom (1, 0, chords, 0, 0, chords.getNumSamples());
        writeWav (outDir.getChildFile ("00_dry.wav"), dry);

        struct S { const char* name; int mode; float speed, inten, xover; bool stereo; };
        const S settings[] = {
            { "01_harmonic", 0, 4.0f, 7.0f, 5.0f, false },
            { "02_harmonic_fast_wide", 0, 6.0f, 8.0f, 4.0f, true },
            { "03_bias_tweed", 1, 4.0f, 6.0f, 5.0f, false },
            { "04_opto_blackface", 2, 5.0f, 8.0f, 5.0f, false },
        };
        for (auto& s : settings)
        {
            BrownstoneProcessor p;
            setParam (p, "mode", (float) s.mode); setParam (p, "speed", s.speed);
            setParam (p, "intensity", s.inten); setParam (p, "crossover", s.xover); setParam (p, "stereo", s.stereo ? 1.0f : 0.0f);
            writeWav (outDir.getChildFile (String (s.name) + ".wav"), run (p, chords));
        }
    }

    // ---- GUI snapshots (free and synced)
    for (int synced = 0; synced < 2; ++synced)
    {
        BrownstoneProcessor p;
        if (synced) setParam (p, "sync", 1);
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto f = outDir.getChildFile (synced ? "gui_sync.png" : "gui.png");
        f.deleteFile();
        FileOutputStream fo (f);
        PNGImageFormat().writeImageToStream (img, fo);
    }
    std::cout << "done\n";
    return 0;
}
