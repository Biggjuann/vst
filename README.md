# Homebrew Synths

Soft synths (VST3) for Ableton Live, built with [JUCE](https://juce.com) and CMake.

| Plugin | What it is |
|---|---|
| **Squelch** | Monophonic acid bass in the spirit of the classic silver bass box: saw/square oscillator, resonant 4-pole filter, accent and slide. |
| **Mira** | 8-voice virtual-analog polysynth going for a thick, warm "expensive analog" sound: 2 oscillators + sub + noise, saturating zero-delay-feedback ladder filter, analog-style envelopes, per-voice drift. |

Both are original code (no third-party synth code or assets), so they're yours to change however you like.

## Getting the plugins onto your Windows PC

### Option A: download a build from GitHub (no tools needed)
1. On GitHub, open the **Actions** tab, then the latest green **Build plugins** run.
2. Download the **synths-windows** artifact and unzip it.
3. Copy `Squelch.vst3` and `Mira.vst3` into `C:\Program Files\Common Files\VST3\`.
4. In Ableton: **Options → Preferences → Plug-Ins**, turn on **Use VST3 Plug-In System Folders**, then click **Rescan**.
5. They show up in the browser under **Plug-Ins → VST3 → Homebrew**.

The `.exe` files in the artifact are standalone versions, handy for a quick test outside Ableton.

### Option B: build it yourself
Install [Visual Studio 2022](https://visualstudio.microsoft.com/) (Community is free; pick the *Desktop development with C++* workload, which includes CMake), and [Git](https://git-scm.com/). Then, from a terminal in this folder:

```bash
cmake -B build
```

```bash
cmake --build build --config Release
```

The plugins end up in `build\plugins\<Name>\<Name>_artefacts\Release\VST3\`. The first configure downloads JUCE, which takes a minute.

## Playing Squelch from Ableton

- **Slide**: make a note overlap the start of the next one. The pitch glides (time set by *Slide Time*) and the envelopes don't retrigger.
- **Accent**: notes with velocity ≥ *Accent Vel* (default 100) are accented: louder, with a snappier, brighter filter sweep. Several accents in a row build up the resonant "wow".
- The classic sound: Cutoff low, Resonance high, Env Mod around 50–70 %, Decay short, then tweak Cutoff and Env Mod live.

## Playing Mira

- **Voice Mode**: *Poly* (8 voices), *Mono* (retriggers each note), or *Legato* (overlapping notes don't retrigger, good for leads with *Glide*).
- **Analog Drift** gives each voice slow random pitch and cutoff wander, which is a big part of the warm, alive sound. Turn it to 0 for a clean, digital sound.
- **Drive** pushes the filter's input saturation; pair it with Resonance for growl.
- Mod wheel adds vibrato; pitch bend is ±2 semitones; the sustain pedal works.

## Project layout

```
CMakeLists.txt              top-level build (fetches JUCE 8)
shared/dsp/                 reusable DSP: oscillators, ladder filter, envelopes, LFO
shared/ui/                  data-driven knob-panel editor + parameter helpers
plugins/Squelch/            acid bass
plugins/Mira/               poly synth
.github/workflows/build.yml CI that builds the Windows VST3s
```

Adding a new synth: copy one of the `plugins/` folders, change the name and the 4-letter `PLUGIN_CODE` in its `CMakeLists.txt` (it must be unique), and add an `add_subdirectory` line to the top-level `CMakeLists.txt`.

**Changing parameters**: parameter IDs (`"cutoff"`, `"res"`, …) are what Ableton stores in your sets and presets. You can freely add new parameters, but renaming or removing an existing ID breaks saved automation for it.
