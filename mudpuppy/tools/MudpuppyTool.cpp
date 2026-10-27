// Offline test tool: checks echo timing, feedback behaviour and bypass; renders demos; screenshots the GUI.
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

// rockabilly-ish single-note line: short, staccato notes are where slapback shines
static AudioBuffer<float> makeLick()
{
    Random r (11);
    AudioBuffer<float> b (1, (int) (fs * 8));
    b.clear();
    std::vector<Pluck> st (2);
    auto midi = [] (int n) { return 440.0f * std::pow (2.0f, (n - 69) / 12.0f); };
    struct Ev { double t; int note; float dec; };
    std::vector<Ev> ev;
    const int line[] = { 52, 55, 56, 57, 59, 57, 55, 52, 50, 52, 0, 45, 47, 49, 52, 0 };
    double t = 0.15;
    for (int rep = 0; rep < 2; ++rep)
        for (int i = 0; i < 16; ++i, t += 0.19)
            if (line[i] > 0) ev.push_back ({ t, line[i], i == 9 || i == 14 ? 0.996f : 0.982f });
    ev.push_back ({ t + 0.2, 64, 0.997f });
    size_t e = 0; int voice = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double now = i / fs;
        while (e < ev.size() && ev[e].t <= now)
        {
            st[(size_t) voice].start (midi (ev[e].note), 0.6f, 0.5f, ev[e].dec, r);
            voice ^= 1;
            ++e;
        }
        b.setSample (0, i, (st[0].next() + st[1].next()) * 0.5f);
    }
    return b;
}

static void setParam (MudpuppyProcessor& p, const String& id, float v)
{
    auto* par = p.apvts.getParameter (id);
    par->setValueNotifyingHost (par->convertTo0to1 (v));
}

static AudioBuffer<float> run (MudpuppyProcessor& p, const AudioBuffer<float>& in, int block = 256,
                               std::function<void (int)> perBlock = {})
{
    p.setPlayConfigDetails (1, 2, fs, block);
    p.prepareToPlay (fs, block);
    AudioBuffer<float> out (2, in.getNumSamples());
    MidiBuffer m;
    for (int pos = 0; pos < in.getNumSamples(); pos += block)
    {
        if (perBlock) perBlock (pos);
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

// a short pick-like burst
static AudioBuffer<float> burst (double seconds)
{
    AudioBuffer<float> b (1, (int) (fs * seconds));
    b.clear();
    Random r (2);
    for (int i = 0; i < 240; ++i)
        b.setSample (0, i, 0.5f * std::sin (MathConstants<float>::pi * i / 240.0f) * (r.nextFloat() * 2 - 1));
    return b;
}

static float windowRms (const AudioBuffer<float>& b, int start, int len)
{
    double s = 0; int c = 0;
    for (int i = std::max (0, start); i < std::min (b.getNumSamples(), start + len); ++i, ++c) s += b.getSample (0, i) * b.getSample (0, i);
    return (float) std::sqrt (s / std::max (1, c));
}

// energy-weighted arrival time of the first echo (skip the dry burst)
static float firstEchoMs (const AudioBuffer<float>& b, float expectMs)
{
    const int lo = (int) (fs * (expectMs * 0.5f) / 1000.0), hi = (int) (fs * (expectMs * 1.5f) / 1000.0);
    double num = 0, den = 0;
    for (int i = std::max (lo, 400); i < std::min (hi, b.getNumSamples()); ++i)
    {
        const double e = b.getSample (0, i) * b.getSample (0, i);
        num += e * i; den += e;
    }
    return den > 0 ? (float) (num / den / fs * 1000.0) : -1.0f;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    // ---- echo timing accuracy
    for (float ms : { 20.0f, 80.0f, 110.0f, 150.0f, 300.0f })
    {
        MudpuppyProcessor p;
        setParam (p, "delay", ms); setParam (p, "feedback", 0); setParam (p, "blend", 10); setParam (p, "wobble", 0);
        auto o = run (p, burst (1.0));
        std::cout << "delay " << ms << " ms -> measured first echo " << firstEchoMs (o, ms) << " ms\n";
    }

    // ---- echo level vs dry: 200 ms tone, 300 ms delay, blend 10
    {
        MudpuppyProcessor p;
        setParam (p, "delay", 300); setParam (p, "feedback", 0); setParam (p, "blend", 10); setParam (p, "wobble", 0);
        AudioBuffer<float> in (1, (int) fs);
        in.clear();
        for (int i = 0; i < (int) (0.2 * fs); ++i) in.setSample (0, i, 0.2f * std::sin (2 * MathConstants<double>::pi * 330.0 * i / fs));
        auto o = run (p, in);
        std::cout << "echo vs dry at blend 10: " << Decibels::gainToDecibels (windowRms (o, (int) (0.35 * fs), (int) (0.1 * fs)) / windowRms (o, (int) (0.05 * fs), (int) (0.1 * fs))) << " dB\n";
    }

    // ---- repeat decay vs feedback knob (ratio between echo 2 and echo 1)
    for (float fb : { 0.0f, 3.0f, 6.0f, 8.0f })
    {
        MudpuppyProcessor p;
        setParam (p, "delay", 100); setParam (p, "feedback", fb); setParam (p, "blend", 10); setParam (p, "wobble", 0);
        auto o = run (p, burst (1.5));
        const int d = (int) (0.1 * fs), w = (int) (0.02 * fs);
        const float e1 = windowRms (o, d - w / 2, w), e2 = windowRms (o, 2 * d - w / 2, w), e3 = windowRms (o, 3 * d - w / 2, w);
        std::cout << "feedback " << fb << ": echo1 " << Decibels::gainToDecibels (e1) << " dB, echo2 "
                  << Decibels::gainToDecibels (e2) << " dB, echo3 " << Decibels::gainToDecibels (e3) << " dB\n";
    }

    // ---- self-oscillation must stay bounded and finite
    {
        MudpuppyProcessor p;
        setParam (p, "delay", 60); setParam (p, "feedback", 10); setParam (p, "blend", 10); setParam (p, "wobble", 10);
        AudioBuffer<float> in (1, (int) (fs * 8));
        Random r (4);
        for (int i = 0; i < in.getNumSamples(); ++i) in.setSample (0, i, i < fs * 0.5 ? (r.nextFloat() * 2 - 1) : 0.0f);
        // sweep the delay knob while it screams
        auto o = run (p, in, 128, [&] (int pos) { setParam (p, "delay", 40.0f + 250.0f * (float) (0.5 + 0.5 * std::sin (pos / fs * 1.3))); });
        float pk = 0; bool ok = true;
        for (int i = 0; i < o.getNumSamples(); ++i) { const float v = o.getSample (0, i); ok &= std::isfinite (v); pk = std::max (pk, std::abs (v)); }
        std::cout << "self-oscillation: finite=" << ok << " peak=" << pk << " still ringing at end (rms) "
                  << windowRms (o, o.getNumSamples() - 24000, 24000) << "\n";
        writeWav (outDir.getChildFile ("05_self_oscillation_sweep.wav"), o);
    }

    // ---- footswitch: bypass must equal dry; trails keeps the echoes
    {
        auto lick = makeLick();
        const int switchAt = (int) (fs * 3.0);
        for (int tr = 0; tr < 2; ++tr)
        {
            MudpuppyProcessor p;
            setParam (p, "delay", 120); setParam (p, "feedback", 5); setParam (p, "blend", 7); setParam (p, "trails", (float) tr);
            AudioBuffer<float> in (1, lick.getNumSamples());
            in.clear();
            in.copyFrom (0, 0, lick, 0, 0, switchAt); // play, then stop playing at the switch
            auto o = run (p, in, 256, [&] (int pos) { setParam (p, "on", pos >= switchAt ? 0.0f : 1.0f); });
            std::cout << (tr ? "trails ON" : "trails OFF") << ": level 0.1-0.5 s after switching off = "
                      << Decibels::gainToDecibels (windowRms (o, switchAt + 4800, 19200) + 1e-9f) << " dB\n";
        }
        MudpuppyProcessor p;
        setParam (p, "on", 0);
        auto o = run (p, lick);
        double err = 0;
        for (int i = 0; i < o.getNumSamples(); ++i) err = std::max (err, (double) std::abs (o.getSample (0, i) - lick.getSample (0, i)));
        std::cout << "bypassed: max difference from dry = " << err << "\n";
    }

    // ---- abuse
    {
        AudioBuffer<float> loud (1, (int) (fs * 2));
        Random r (9);
        for (int i = 0; i < loud.getNumSamples(); ++i) loud.setSample (0, i, r.nextFloat() * 2 - 1);
        MudpuppyProcessor p;
        setParam (p, "delay", 20); setParam (p, "feedback", 10); setParam (p, "blend", 10); setParam (p, "output", 12);
        auto o = run (p, loud, 31);
        float pk = 0; bool ok = true;
        for (int i = 0; i < o.getNumSamples(); ++i) { const float v = o.getSample (0, i); ok &= std::isfinite (v); pk = std::max (pk, std::abs (v)); }
        std::cout << "abuse: finite=" << ok << " peak=" << pk << "\n";
    }

    // ---- demo renders
    {
        auto lick = makeLick();
        AudioBuffer<float> dry (2, lick.getNumSamples());
        dry.copyFrom (0, 0, lick, 0, 0, lick.getNumSamples());
        dry.copyFrom (1, 0, lick, 0, 0, lick.getNumSamples());
        writeWav (outDir.getChildFile ("00_dry.wav"), dry);
        struct S { const char* name; float d, fb, bl, wob; };
        const S settings[] = {
            { "01_slapback_110ms", 110, 0.5f, 6, 2 },
            { "02_rockabilly_140ms_two_repeats", 140, 3.5f, 6, 2 },
            { "03_doubling_35ms", 35, 1.0f, 5, 3 },
            { "04_long_dark_300ms", 300, 6.0f, 6, 4 },
        };
        for (auto& s : settings)
        {
            MudpuppyProcessor p;
            setParam (p, "delay", s.d); setParam (p, "feedback", s.fb); setParam (p, "blend", s.bl); setParam (p, "wobble", s.wob);
            writeWav (outDir.getChildFile (String (s.name) + ".wav"), run (p, lick));
        }
    }

    // ---- GUI
    {
        MudpuppyProcessor p;
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto f = outDir.getChildFile ("gui.png");
        f.deleteFile();
        FileOutputStream fo (f);
        PNGImageFormat().writeImageToStream (img, fo);
    }
    std::cout << "done\n";
    return 0;
}
