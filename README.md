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

## Squelch

### Built-in step sequencer
Works like the original silver box's sequencer: 16 steps, and each step has
- **NOTE**: one of the 13 keys, C to C'. Click it for a menu, or drag or scroll up and down.
- **DOWN / UP**: octave transpose for that step.
- **ACCENT**: louder, snappier, brighter.
- **SLIDE**: glide into the next step. The gate stays open and the envelopes don't retrigger.
- **TIME**: click to cycle **N**ote, **T**ie (hold the previous note longer) or **R**est.

Notes are half a 16th long, the same as the original. Slides and ties hold the gate open.

Controls along the top:
| Control | What it does |
|---|---|
| **RUN** | Turns the sequencer on. |
| **Pattern 1–8** | 8 patterns per instance. Patterns 1–4 are demo lines and 5–8 are random. A pattern change waits for the downbeat. You can automate it in Ableton. |
| **Length** | 1–16 steps. |
| **Clock** | *Host sync* follows Ableton's transport and tempo; this is what you'll normally use. *Internal* runs at the Tempo setting whenever RUN is on, even with Ableton stopped, which is good for auditioning. |
| **Generate / Clear / < > / Copy / Paste** | New generated pattern, wipe, rotate and duplicate patterns. |

### Pattern generator
The **Pattern Generator** row writes acid lines into the selected pattern, and every pattern is in the key and scale you pick.

| Control | What it does |
|---|---|
| **Key / Scale** | Root note, and the scale to use: Minor, Major, Dorian, Phrygian, Harmonic Minor, Phrygian Dominant, Minor or Major Pentatonic, Blues, or Chromatic. |
| **Pattern #** | 9,999 repeatable patterns. Turn it to browse; the same number always gives the same line. **Generate** (sequencer bar) jumps to a random one. |
| **Density** | How many steps play a note versus rests and ties. |
| **Accents / Slides / Octaves** | How many accents, slides and octave jumps there are. |
| **Evolve** | Every 1–16 bars, part of the playing pattern is regenerated, so it changes endlessly. |
| **Variation** | How much each Evolve changes: low values give subtle mutations, 100% gives a brand-new line each time. |

Each control changes only its own part of the pattern. Turning up Accents adds accents and leaves the melody alone. Changing Key transposes the same line into the new key.

Moving any generator control rewrites the selected pattern, so copy anything you want to keep into another pattern slot first. Loading a saved Ableton set never regenerates; your saved patterns stay as they were.

**Transpose**: while the sequencer runs, playing a MIDI note transposes the pattern, and the transpose stays latched. C3 means no transpose. Put a one-note-per-bar clip on the track to sequence key changes.

Patterns are saved inside your Ableton set.

To use it in Ableton: put Squelch on a MIDI track, press **RUN** (with Clock on *Host sync*), and press play in Ableton.

### Playing it from a MIDI clip instead (RUN off)
- **Slide**: make a note overlap the start of the next one. The pitch glides (time set by *Slide Time*) and the envelopes don't retrigger.
- **Accent**: notes with velocity ≥ *Accent Vel* (default 100) are accented: louder, with a snappier, brighter filter sweep. Several accents in a row build up the resonant "wow".

The classic sound: Cutoff low, Resonance high, Env Mod around 50–70 %, Decay short, then tweak Cutoff and Env Mod live.

## Mira

### Presets
The preset bar at the top right has **<** / **>** to step through presets, a preset-name button that opens the browser, **Save**, and **Import Diva**.

- **Factory bank**: 25 patches, 5 each in Bass, Lead, Pad, Synth (keys, plucks, stabs) and FX. They also show up in Ableton's preset list for the device.
- **Save** writes your patch to `%APPDATA%\Homebrew\Mira\Presets`. Saved presets appear under **User** in the browser.

### Importing Diva presets
Click **Import Diva** and choose either some `.h2p` files or a whole folder. Choose the folder to import a full library; subfolders are kept. You can also drag files or folders onto the Mira window. Each imported preset is converted once and saved under **User › Diva Import › *folder name***.

Diva usually keeps its presets in `Documents\u-he\Diva.data\Presets\Diva`, and the file dialog opens there when that folder exists.

Imports are close approximations, not exact copies, because Diva has more under the hood than Mira:

| Carries over | Approximated | Dropped |
|---|---|---|
| Tuning, waveforms and levels for every oscillator model; noise; pulse width and PWM; filter cutoff, resonance, key tracking and envelope amount; both envelopes; glide; mono/legato/poly; drift; velocity; voice panning; chorus. | Diva's 3rd oscillator goes into the sub or replaces a quieter oscillator. Its 5 filter models map to Mira's 4 modes. Mira has one LFO, so the busiest Diva LFO is used. Tempo-synced LFOs assume 120 BPM. | High-pass filter, mod matrix extras (rectify, lag and so on), cross-mod/FM and sync, Diva's delay, reverb, phaser and rotary FX. Add those with Ableton effects. |

I tested the importer on 9,015 real Diva presets. All of them converted. When I played a random 400 through Mira, 397 were clean. The other three were two silent patches that make their sound from a self-oscillating filter with no oscillators, and one loud one.

### Playing it

- **Voice Mode**: *Poly* (8 voices), *Mono* (retriggers each note), or *Legato* (overlapping notes don't retrigger, good for leads with *Glide*).
- **Analog Drift** gives each voice slow random pitch and cutoff wander, which is a big part of the warm, alive sound. Turn it to 0 for a clean, digital sound.
- **Drive** pushes the filter's input saturation; pair it with Resonance for growl.
- Mod wheel adds vibrato; pitch bend is ±2 semitones; the sustain pedal works.

## Project layout

```
CMakeLists.txt              top-level build (fetches JUCE 8)
shared/dsp/                 reusable DSP: oscillators, ladder filter, envelopes, LFO, chorus
shared/ui/                  data-driven knob-panel editor + parameter helpers
plugins/Squelch/            acid bass + step sequencer (Sequencer.h, SequencerPanel.h) + generator (Generator.h)
plugins/Mira/               poly synth, presets (MiraPresets.cpp), Diva importer (DivaImporter.cpp)
tests/                      developer tools: DivaConvert, GeneratorTest (cmake -DHOMEBREW_BUILD_TESTS=ON)
.github/workflows/build.yml CI that builds the Windows VST3s
```

Adding a new synth: copy one of the `plugins/` folders, change the name and the 4-letter `PLUGIN_CODE` in its `CMakeLists.txt` (it must be unique), and add an `add_subdirectory` line to the top-level `CMakeLists.txt`.

**Changing parameters**: parameter IDs (`"cutoff"`, `"res"`, …) are what Ableton stores in your sets and presets. You can freely add new parameters, but renaming or removing an existing ID breaks saved automation for it.
