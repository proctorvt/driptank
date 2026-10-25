# Driptank 63 — tube spring reverb plug-in

A model of a 1960s outboard tube spring-reverb unit (the kind surf players
parked on top of the amp). Plug your guitar into your interface, put Driptank
on the track as an insert, and play.

Formats: **VST3** (Ableton Live, FL Studio, Cubase, Reaper, Studio One…),
**AU** (Logic, GarageBand, Ableton) and a **standalone app** for playing
without a DAW. Runs natively on Apple Silicon and Intel Macs.

## The controls

| Control | What it does |
|---|---|
| **DWELL** | How hard the driver tube hits the springs. Low = polite ambience; past 6 the tube starts to saturate and the tank splashes; 8–10 is full surf drip. Also makes the reverb louder, like the real thing. |
| **MIXER** | Blend of dry guitar and reverb. 0 = dry only, 10 = reverb only. |
| **TONE** | Treble on the reverb return. Low = dark and dubby, high = glassy with extra sparkle above 7. |
| **TANK** | Spring length: SHORT (~2 s), MED (~2.8 s), LONG (~3.8 s decay). |
| **OUTPUT** | Final level, −24 to +12 dB. |
| **SPRINGS** WIDE/MONO | Mono (down) is like the original box. Wide spreads the two springs left/right. |
| **REVERB** footswitch + jewel | Turns the reverb on/off; the lamp lights when it's on. The tail is cut, like grounding the return on the real unit. |
| **KICK** | Bump the cabinet — the springs crash. Also an automatable parameter ("Kick Tank"), so you can map it to a MIDI pad. |

Double-click any knob to reset it. Drag the window corner to resize.
Starting point for surf: Dwell 8, Mixer 5, Tone 7, Tank LONG.

## Getting it onto your Mac

### Option A — let GitHub build it (no developer tools needed)
1. Make a free GitHub account, create a new repository, and upload everything
   in this folder (keep the `.github` folder — it holds the build recipe).
2. Open the repo's **Actions** tab. The "Build Driptank for Mac" job runs by
   itself (about 5–10 minutes) and also runs Apple's own Audio Unit validator.
3. When it's green, download **Driptank63-mac** from the bottom of the run
   page, unzip it (twice — GitHub wraps it in an extra zip).
4. Right-click `install_mac.command` → **Open** → Open. (Right-click is needed
   the first time because the file is from "an unidentified developer".)

### Option B — build it on your Mac
1. Open Terminal in this folder and run `./build_mac.sh`.
2. The first time it may ask to install Apple's command line tools and CMake
   (`brew install cmake`). The first build takes a few minutes because it
   downloads the JUCE framework.
3. It installs the plug-ins for you.

Either way the files end up in:
- `~/Library/Audio/Plug-Ins/VST3/Driptank 63.vst3`
- `~/Library/Audio/Plug-Ins/Components/Driptank 63.component`
- `~/Applications/Driptank 63.app`

## In your DAW
- **Ableton Live:** Settings → Plug-Ins → turn on "Use VST3 Plug-in System
  Folders" (and/or "Use Audio Units"), click Rescan. Find it under Plug-Ins →
  Proctor → Driptank 63.
- **FL Studio (Mac):** Options → Manage plugins → Find plugins. It'll show in
  the effects list.
- **Cubase:** restart it; Cubase scans VST3 on launch. Insert it from Reverb.

Set your interface buffer to 128 samples or lower so playing through it feels
instant. The plug-in itself adds no latency.

## Under the hood
`Source/SpringTankEngine.h` is the whole sound:
- dry path through a light 12AX7-style stage
- Dwell → asymmetric "driver tube" saturation, 2× oversampled → transformer
  band-limit
- two springs (29 ms and 37.5 ms), each a feedback loop with an 80–90 stage
  "stretched allpass" dispersion cascade — that's what makes the chirpy drip —
  plus damping, light diffusion and slow random wobble
- pickup band-limit (≈190 Hz–4.8 kHz) → recovery tube → Tone → Mixer

Built with JUCE 8 (GPLv3 / JUCE personal licence — fine for your own use).
"Driptank" is an original name; it isn't affiliated with any amp maker.
