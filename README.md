# ApexAmp

A high-gain guitar amp simulator plugin (VST3 / AU / Standalone) built with JUCE and C++.
It targets the territory staked out by **Graphene** (PolychromeDSP) and **Thall Amp** (Odeholm
Audio) — dual-voiced high-gain tone with a smart pick-attack enhancer — and aims to go beyond
them with switchable amp voicings and a true power-amp feel.

> **Status: beta.** The full signal chain works and builds on macOS, Windows and Linux. Voicings
> are tuned by ear and will keep evolving — this is the version you can install and start playing.

---

## What's inside

```
Input → mono sum → Input HPF → Chug Enhancer → Dual-Channel Preamp → Tonestack → Low Dirt → Power Amp → Cab → Master
```

> **Mono in, dual-mono out.** A guitar is a mono source and may be plugged into any physical
> input on your interface (e.g. only input 2). ApexAmp sums the inputs to mono so it always
> hears the guitar regardless of which input it's on, then sends the processed signal to both
> output channels centred.

| Module | What it does | Why it beats the reference plugins |
|--------|--------------|------------------------------------|
| **Dual-Channel Preamp** | `Tight` (focused rhythm) and `Scoop` (aggressive lead) voicings, cascaded 12AX7-style triodes with asymmetric saturation | The mid scoop is applied **before** the tube stages, so the distortion texture itself differs per channel — not just a post-EQ |
| **Chug Enhancer** | Transient-aware upper-mid boost driven by a fast/slow envelope detector, behind a 200 Hz Linkwitz-Riley crossover | Adds pick clarity / palm-mute bite **only on attacks**, while sub-bass weight passes through completely untouched |
| **Low Dirt** | Parallel saturated low-band growl layer | Adds down-tuned growl without muddying the full-range signal |
| **Tonestack** | Four switchable voicings: Marshall, Fender, Mesa, Modern Metal | Neither Graphene nor Thall Amp lets you swap the underlying tonestack character |
| **Power Amp** | Bias-excursion **sag** + output-transformer saturation | The "give" and bloom under hard picking that most sims skip entirely |
| **Cab** | Smooth filter-based 4x12 speaker voicing by default (steep ~5 kHz roll-off tames fizz), or load your own WAV/AIFF IR for partitioned convolution | Sounds musical out of the box; load a real IR for the final 10% |
| **Oversampling** | 4× around the nonlinear amp stages | Keeps aliasing fizz above the audible range on high-gain tones |

The nonlinear DSP core (`Source/dsp/`) is **pure C++ with no JUCE dependency**, so it can be
unit-tested and iterated on in seconds via the offline harness.

---

## Building

You need **CMake ≥ 3.21** and a C++17 compiler. JUCE is downloaded automatically by CMake
(via `FetchContent`) — you do **not** need to install it separately.

```bash
git clone <your-repo-url> ApexAmp
cd ApexAmp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Build artefacts land in `build/ApexAmp_artefacts/Release/`:

- **VST3** — `VST3/ApexAmp.vst3`
- **AU** (macOS only) — `AU/ApexAmp.component`
- **Standalone app** — `Standalone/ApexAmp`

`COPY_PLUGIN_AFTER_BUILD` is on, so the plugin is also copied into your user plugin folder
automatically. Restart your DAW and rescan if it doesn't appear.

### Platform notes
- **macOS**: AU + VST3 + Standalone all build. Xcode command-line tools required.
- **Windows**: VST3 + Standalone. Use the Visual Studio generator or Ninja.
- **Linux**: VST3 + Standalone. Install dev packages first:
  `libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libfreetype6-dev`

### Low-latency ASIO (Windows Standalone)

ASIO gives the lowest round-trip latency for live playing. The Steinberg ASIO SDK can't be
redistributed, so it's opt-in:

1. Download the **ASIO SDK** from Steinberg and unzip it (you'll get a folder containing `common/`).
2. Configure with the SDK path:
   ```
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DAPEXAMP_ENABLE_ASIO=ON -DASIO_SDK_DIR=C:/path/to/asiosdk
   cmake --build build --config Release --parallel
   ```
The Standalone app's audio settings will then offer your ASIO device.

> Tip: if you run ApexAmp **inside a DAW** (Reaper, etc.), the DAW already provides ASIO and
> input routing, so you don't need this — it only matters for the Standalone app.

---

## Offline DSP test (no DAW needed)

A standalone harness compiles only the pure-C++ DSP core, runs a synthetic guitar signal
through several presets, checks the output for NaN/Inf, and writes WAV files you can listen to:

```bash
cmake --build build --target apexamp_offline_test
./build/apexamp_offline_test          # writes out_*.wav in the working directory
```

This is the fastest way to iterate on tone — change a coefficient, rebuild this one target,
listen, repeat.

---

## Controls

**Presets:** factory preset menu (Chug Machine, Djent Tight, Modern Lead, Tight Rhythm, Clean…) plus **Save/Load** for your own `.apreset` files (parameters *and* the loaded IR path are saved).
**Preamp:** Channel (Tight/Scoop) · Input · Gain · Push · Tight · Super Cut · Bias
**Tone:** Tonestack model (Marshall/Fender/Mesa/Modern Metal) · Bass · Mid · Treble
**Dynamics:** Chug · Low Dirt Drive · Low Dirt Mix · **Gate** (input noise gate threshold)
**Cab:** Cab on/off · **Cab Type** (Modern V30 / Vintage Greenback / Tight 4x12 / American Scooped) · **Load IR** (any WAV/AIFF; the name is shown and it persists with your project) · **Built-in** (revert to the filter cab)
**Power / Output:** Sag · Power Drive · Master

### Noise gate
High-gain amps amplify the noise floor between notes. The **Gate** gates the DI *before* the
preamp, so hiss/hum never gets amplified — giving tight, silent chugs. Turn it up (toward -20 dB)
for more aggressive gating; down toward -80 dB to disable.

### Cabinet
Use the built-in **Cab Type** voicings for an instant usable sound, or **Load IR** to use any
impulse response (a real IR is the single biggest tone upgrade). Loading an IR bypasses the
built-in voicing; **Built-in** switches back.

---

## Starting-point presets (to A/B against the references)

Use the built-in preset menu, or dial these by hand:

### "Tight Crunch" — Marshall-style rhythm
`Channel=Tight · Tonestack=Marshall · Gain≈0.6 · Tight≈0.4 · Bass≈0.55 · Mid≈0.6 · Treble≈0.55 · Chug≈0.3 · Sag≈0.3 · Power≈0.4`

### "Scoop Metal" — modern down-tuned lead/chug
`Channel=Scoop · Tonestack=Modern Metal · Gain≈0.85 · Push≈0.5 · Super Cut≈0.6 · Tight≈0.6 · Bass≈0.6 · Mid≈0.35 · Treble≈0.6 · Chug≈0.6 · Low Drv≈0.5 · Low Mix≈0.3 · Sag≈0.5 · Power≈0.6`

### "Clean-ish Fender"
`Channel=Tight · Tonestack=Fender · Gain≈0.25 · Bass≈0.6 · Mid≈0.45 · Treble≈0.6 · Sag≈0.2 · Power≈0.2`

---

## Roadmap

- [x] Preset manager (save/recall, factory bank)
- [x] Built-in cab voicing library + user IR loading
- [x] Noise gate
- [x] 8x oversampling, ASIO (Windows)
- [x] Punch + Loud output stage (alias-free transient gain + limiter)
- [x] Custom dark/amber UI (LookAndFeel)
- [x] Dual-IR + mic blend
- [x] Built-in tuner
- [ ] Per-channel independent knob sets
- [ ] Neural capture mode (RTNeural) for user amp captures
- [ ] Resizable UI

## Project layout

```
CMakeLists.txt          # JUCE via FetchContent, builds plugin + offline test
Source/
  PluginProcessor.*     # AudioProcessor + APVTS parameter layout
  PluginEditor.*        # UI
  Presets.h             # factory presets + apply logic
  AmpEngine.h           # JUCE oversampling + cab (filter cab / IR convolution) wrapper
  dsp/                  # pure-C++ DSP core (no JUCE)
    Biquad.h            # RBJ biquads, DC blocker, envelope follower
    NoiseGate.h         # input noise gate
    TubeStage.h         # asymmetric triode stage
    DualChannelPreamp.h # Tight / Scoop cascaded preamp
    Tonestack.h         # 4 amp voicings
    ChugEnhancer.h      # transient enhancer + Low Dirt
    PowerAmp.h          # sag + transformer saturation
    CabSim.h            # filter-based speaker/cab voicings (selectable)
    AmpCore.h           # full chain
tests/
  offline_test.cpp      # DAW-free DSP harness
```


## Metering

The header shows live **IN / OUT** level meters (with peak-hold) and a **tuner**
(note + cents). The output is also scrubbed for non-finite samples as a safety net,
so a bad IR or extreme setting can never blast NaNs to your speakers.

## License

ApexAmp's code is proprietary — see [LICENSE](LICENSE). **Before selling**, note that
the frameworks it builds on have their own terms you must satisfy:

- **JUCE** is dual-licensed (AGPLv3 / commercial). Closed-source commercial sale
  requires a paid JUCE licence — https://juce.com/get-juce
- **Steinberg ASIO SDK** and **VST3 SDK** distribution are governed by Steinberg's
  licences.
- Ship only your **own** cab IRs; don't redistribute commercial/copyrighted IRs.

The ApexAmp source is yours; just secure a JUCE commercial licence and comply with
the Steinberg terms before commercial release.
