// Offline test tool: renders demo audio, measures decay/stability, and screenshots the GUI.
// Usage: driptank_tool <outdir>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>

using namespace juce;

static constexpr double fs = 48000.0;

// ---- Karplus-Strong guitar, close enough to a DI'd single-coil for testing
struct Pluck
{
    std::vector<float> buf; size_t idx = 0; float decay = 0.996f, last = 0;
    void start (float freq, float amp, float bright, float dec, Random& r)
    {
        buf.assign ((size_t) std::max (2.0, fs / freq), 0.0f);
        float lp = 0;
        for (auto& b : buf) { lp += bright * ((r.nextFloat() * 2 - 1) - lp); b = lp * amp; }
        idx = 0; decay = dec; last = 0;
    }
    float next()
    {
        if (buf.empty()) return 0;
        const float x = buf[idx];
        const float nx = buf[(idx + 1) % buf.size()];
        buf[idx] = decay * 0.5f * (x + nx);
        idx = (idx + 1) % buf.size();
        return x;
    }
};

static AudioBuffer<float> makeRiff()
{
    Random r (7);
    AudioBuffer<float> b (1, (int) (fs * 9.0));
    b.clear();
    std::vector<Pluck> strings (6);
    auto midi = [] (int n) { return 440.0f * std::pow (2.0f, (n - 69) / 12.0f); };

    struct Ev { double t; int str; int note; float amp; float dec; };
    std::vector<Ev> ev;
    // surf-style tremolo-picked low E line, palm-muted-ish
    const int line[] = { 40, 40, 40, 40, 41, 41, 40, 40, 43, 43, 40, 40, 45, 45, 44, 43 };
    double t = 0.1;
    for (int i = 0; i < 16; ++i, t += 0.11)
        ev.push_back ({ t, 0, line[i], 0.55f, 0.985f });
    // a ringing E major chord
    t += 0.25;
    const int chord[] = { 40, 47, 52, 56, 59, 64 };
    for (int s = 0; s < 6; ++s)
        ev.push_back ({ t + s * 0.012, s, chord[s], 0.35f, 0.9975f });
    // a single stabbed high note to hear the drip
    ev.push_back ({ t + 2.6, 5, 71, 0.6f, 0.993f });

    size_t e = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double now = i / fs;
        while (e < ev.size() && ev[e].t <= now)
        {
            strings[(size_t) ev[e].str].start (midi (ev[e].note), ev[e].amp, 0.45f, ev[e].dec, r);
            ++e;
        }
        float s = 0;
        for (auto& p : strings) s += p.next();
        b.setSample (0, i, s * 0.45f);
    }
    return b;
}

static void setParam (DriptankProcessor& p, const String& id, float v)
{
    auto* par = p.apvts.getParameter (id);
    par->setValueNotifyingHost (par->convertTo0to1 (v));
}

static AudioBuffer<float> run (DriptankProcessor& p, const AudioBuffer<float>& in, int block = 256,
                               int kickAt = -1)
{
    p.setPlayConfigDetails (1, 2, fs, block);
    p.prepareToPlay (fs, block);
    AudioBuffer<float> out (2, in.getNumSamples());
    MidiBuffer m;
    for (int pos = 0; pos < in.getNumSamples(); pos += block)
    {
        const int n = std::min (block, in.getNumSamples() - pos);
        if (kickAt >= 0)
            setParam (p, "kick", (pos <= kickAt && kickAt < pos + n) ? 1.0f : 0.0f);
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
    if (w == nullptr) { std::cerr << "wav fail\n"; return; }
    os.release();
    std::unique_ptr<AudioFormatWriter> writer (w);
    writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static bool finiteAll (const AudioBuffer<float>& b, float& peak)
{
    peak = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample (c, i);
            if (! std::isfinite (v)) return false;
            peak = std::max (peak, std::abs (v));
        }
    return true;
}

static float rms (const AudioBuffer<float>& b, int ch, int start, int len)
{
    double s = 0;
    for (int i = start; i < start + len && i < b.getNumSamples(); ++i) s += b.getSample (ch, i) * b.getSample (ch, i);
    return (float) std::sqrt (s / std::max (1, len));
}

// Schroeder backward integration; T60 from the -5..-25 dB slope
static float measureT60 (const AudioBuffer<float>& ir)
{
    const int n = ir.getNumSamples();
    std::vector<double> edc ((size_t) n);
    double acc = 0;
    for (int i = n - 1; i >= 0; --i) { acc += (double) ir.getSample (0, i) * ir.getSample (0, i); edc[(size_t) i] = acc; }
    const double total = edc[0];
    int i5 = -1, i25 = -1;
    for (int i = 0; i < n; ++i)
    {
        const double db = 10 * std::log10 (edc[(size_t) i] / total + 1e-30);
        if (i5 < 0 && db <= -5) i5 = i;
        if (i25 < 0 && db <= -25) { i25 = i; break; }
    }
    if (i5 < 0 || i25 < 0) return -1;
    return (float) ((i25 - i5) / fs * 3.0);
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    auto riff = makeRiff();
    {
        AudioBuffer<float> dry2 (2, riff.getNumSamples());
        dry2.copyFrom (0, 0, riff, 0, 0, riff.getNumSamples());
        dry2.copyFrom (1, 0, riff, 0, 0, riff.getNumSamples());
        writeWav (outDir.getChildFile ("00_dry_guitar.wav"), dry2);
        std::cout << "dry riff peak " << riff.getMagnitude (0, riff.getNumSamples()) << "\n";
    }

    struct Setting { const char* name; float dwell, mix, tone; int tank; bool stereo; };
    const Setting settings[] = {
        { "01_default", 6, 4, 6, 2, false },
        { "02_surf_dwell9_mix6", 9, 6, 7, 2, false },
        { "03_subtle_short", 4, 3, 5, 0, false },
        { "04_dark_wide", 7, 5, 2.5f, 2, true },
    };

    for (auto& s : settings)
    {
        DriptankProcessor p;
        setParam (p, "dwell", s.dwell); setParam (p, "mix", s.mix); setParam (p, "tone", s.tone);
        setParam (p, "tank", (float) s.tank); setParam (p, "stereo", s.stereo ? 1.0f : 0.0f);
        auto out = run (p, riff);
        float pk; const bool ok = finiteAll (out, pk);
        writeWav (outDir.getChildFile (String (s.name) + ".wav"), out);
        std::cout << s.name << ": finite=" << ok << " peak=" << pk
                  << " rms(riff)=" << rms (out, 0, (int) (0.1 * fs), (int) (2 * fs))
                  << " rms(dry)=" << rms (riff, 0, (int) (0.1 * fs), (int) (2 * fs)) << "\n";
    }

    // wet-only vs dry loudness balance at mixer 5
    {
        DriptankProcessor pw, pd;
        setParam (pw, "mix", 10); setParam (pw, "dwell", 6);
        setParam (pd, "mix", 0);
        auto w = run (pw, riff), d = run (pd, riff);
        std::cout << "loudness  wet-only(mix10,dwell6)=" << Decibels::gainToDecibels (rms (w, 0, 0, w.getNumSamples()))
                  << " dB   dry-only(mix0)=" << Decibels::gainToDecibels (rms (d, 0, 0, d.getNumSamples())) << " dB\n";
    }

    // decay time of each tank
    for (int tank = 0; tank < 3; ++tank)
    {
        DriptankProcessor p;
        setParam (p, "mix", 10); setParam (p, "dwell", 5); setParam (p, "tank", (float) tank);
        AudioBuffer<float> imp (1, (int) (fs * 8));
        imp.clear();
        // short band-limited click, like a pick attack
        for (int i = 0; i < 48; ++i) imp.setSample (0, i, 0.5f * std::sin (MathConstants<float>::pi * i / 48.0f));
        auto ir = run (p, imp);
        std::cout << "tank " << tank << " T60 ~ " << measureT60 (ir) << " s\n";
        if (tank == 2) writeWav (outDir.getChildFile ("05_impulse_long_tank.wav"), ir);
    }

    // abuse: loud noise, max dwell & mix, kicked, odd block sizes, 44.1k
    {
        DriptankProcessor p;
        setParam (p, "mix", 10); setParam (p, "dwell", 10); setParam (p, "tone", 10);
        AudioBuffer<float> noise (1, (int) (fs * 4));
        Random r (1);
        for (int i = 0; i < noise.getNumSamples(); ++i) noise.setSample (0, i, i < fs * 2 ? (r.nextFloat() * 2 - 1) : 0.0f);
        auto out = run (p, noise, 37, (int) (fs * 1.0));
        float pk; const bool ok = finiteAll (out, pk);
        std::cout << "abuse test: finite=" << ok << " peak=" << pk << " tailRMS(last 0.5s)="
                  << rms (out, 0, out.getNumSamples() - (int) (fs / 2), (int) (fs / 2)) << "\n";
    }

    // kick the tank in silence
    {
        DriptankProcessor p;
        setParam (p, "mix", 5); setParam (p, "dwell", 6);
        AudioBuffer<float> sil (1, (int) (fs * 4));
        sil.clear();
        auto out = run (p, sil, 256, (int) (fs * 0.2));
        float pk; finiteAll (out, pk);
        writeWav (outDir.getChildFile ("06_kick_the_tank.wav"), out);
        std::cout << "kick peak=" << pk << "\n";
    }

    // footswitch off: wet must disappear, dry stays
    {
        DriptankProcessor p;
        setParam (p, "mix", 5); setParam (p, "reverb", 0);
        auto out = run (p, riff);
        std::cout << "reverb off: tail rms after riff = " << rms (out, 0, (int) (fs * 8.0), (int) (fs * 0.9)) << "\n";
    }

    // GUI screenshot
    {
        DriptankProcessor p;
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        PNGImageFormat png;
        auto f = outDir.getChildFile ("gui.png");
        f.deleteFile();
        FileOutputStream fo (f);
        png.writeImageToStream (img, fo);
        std::cout << "gui snapshot " << img.getWidth() << "x" << img.getHeight() << "\n";
    }
    return 0;
}
