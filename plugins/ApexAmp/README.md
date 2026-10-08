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

ApexAmp is built from the **suite root** (see the [root README](../../README.md)), together
with the other Apex plugins. You need **CMake ≥ 3.21** and a C++20 compiler. JUCE is downloaded
automatically by CMake (via `FetchContent`) — you do **not** need to install it separately.

```bash
git clone <your-repo-url> apexamp
cd apexamp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Build artefacts land in `build/plugins/ApexAmp/ApexAmp_artefacts/Release/`:

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

## v0.10: The Legion

Play a riff; a band plays it with you. **The Legion** (the BAND block in the chain) listens to the
DI before the amp and adds, live:

- **Kick** on every chug. In **Chugs** mode a pick attack only gets a kick when its low band
  confirms a low-string note, so the lead notes on the high strings stay kick-free; **All Notes**
  kicks every attack. **Feel** sets how light an attack still counts, **Tone** goes from deep and
  round to tight and clicky. Velocity follows your picking. The hit strip shows every hit.
- **Bass**: the riff an octave under the guitar (plus any drop), voiced like a modern metal bass:
  a clean sub under a driven, low-passed mid band (**Grit**). It follows the input trim, not the
  amp gain, so cranking the amp doesn't change the bass.

The Legion is mixed in after the effects and before the output level. Off by default.

**Riff capture.** Whether or not the DAW is recording, the plugin keeps the last 30 seconds of the
DI, the bass and the kick hits. Drag **RIFF DI**, **BASS** or **KICK MIDI** from the Legion panel
straight into the DAW (or click a tile to show the file). You get a 24-bit WAV of the clean DI to
re-amp later, the bass as audio, and a MIDI file with a GM kick (note 36, channel 10) on every hit
at the host tempo, placed on the pick rather than when the detector fired. Silence before and after
the riff is trimmed. Until you play something new, all three tiles hand out the same take. Files go
to `Music/Apex Riffs/`.

Tested on a 150 bpm riff of 27 low chugs and 12 lead notes: Chugs mode kicks 26 of the chugs (25
within 5 ms of the pick), none of the lead notes and nothing between notes; you hear the kick about
4.5 ms after the pick (worst 11 ms), and the recorded kick lands within 6 ms of it. The bass tracks
an octave down within 2 cents. The Legion costs about 5 % of one core at 48 kHz.

## v0.8: Thallbyssal

The editor is now **Thallbyssal**, built in the abyss visual language of apex-ui: black basalt with
ember-lit cracks, thorned gothic frames, Cinzel lettering. All of it is rendered in code
(procedural stone, vector ornaments, glow passes), and every control is a live component bound to
a parameter with undo, crisp at any window size.

- **Input Match**: DI level relative to the target, a calibration radar (the closer to the centre,
  the closer your DI sits to the target), match status, **Target Zone** (open / modern / hot) and
  **Calibrate DI** (Auto Input: listens for ~3 s and sets an input trim kept out of presets).
- **Abyssal Core** (amp): Gain, Bass, Mid, Treble, Presence, **Depth** (new: power-amp resonance
  around 85 Hz, flat at 0 dB), Tight, Master; rigs Bite / Body / Edge / Blend (+ trims) / User.
- **Signal chain**: Drop, Gate, Boost, Amp, Shape, Cab, FX. Click a block to edit it in the centre
  panel, click its bar to switch it on or off; the links crackle with the signal. The output ring
  is the power switch.
- **Shape** (Chug Forge): Chug (pick-attack punch read from the DI before the amp, applied after
  it), Frequency, Growl (parallel growl on the lows), with a live punch meter.
- **FX** (The Void): Echo (time or host-synced division, tap, feedback, duck, mix) and Abyss reverb
  (decay, octave-down shimmer depth, tone, mix). Both spill over when switched off.
- **Cab Chamber**: IR slot (prev / next / load), cab mix, low cut and **High Cut** (new, off at
  the top); the portal glows with the output.
- **Status**: amp pressure, output scope with DSP load and latency, drop depth, output level and a
  hot-signal warning.

- **Fizz Tamer** (v0.9, Cab Chamber): sixteen narrow detector bands from 1.6 to 9.5 kHz follow the
  treble; a band is cut only while it stands out from the spectral trend of its neighbours (the
  whistling resonances of a cranked amp through a cab), by as much as it sticks out. Brightness
  and pick attack stay; a measured +14 dB resonance comes down 13.6 dB with the mids untouched.
- **Boost** is now 4x oversampled around its clipper, so its harmonics no longer fold back.
- The chain blocks show each module's key setting (tuning, gate threshold, rig, IR, FX), and the
  first open guides you to Calibrate DI.

New effects are off by default and Depth / High Cut / Fizz are neutral, so earlier sessions and
presets sound the same.

## v0.6: the rig

(Superseded in the UI by Thallbyssal; the DSP below is unchanged.) The editor was a full rig built
from the Apex design system: the head on a 4x12 with a pedalboard in front of it.

- **Drop** (new): apex-dsp's Live pitch engine in front of the amp, -12..+12 semitones with a
  seven-segment display, **Body** (keeps the pickup / body resonances where a real drop tuning
  keeps them) and **Sub** (octave-down layer). Adds ~8 ms latency while on, reported to the host.
- **Amp EQ** (new): Bass (110 Hz shelf), Mid (700 Hz), Treble (2.6 kHz shelf). Flat at 0 dB, so
  sessions and presets from earlier versions sound identical.
- **Rig selector**: Bite / Body / Edge / Blend (with blend trims) / User (.nam).
- **Gate, Boost, Cab** (IR browser + load) as pedals; **Power** toggle with pilot jewel.
- **Strobe tuner** with output mute, presets (factory + user), A/B, undo / redo, resizable UI.

## Controls (legacy section)

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
- [ ] IR mic blending / morphing, dual-IR
- [ ] Per-channel independent knob sets
- [ ] Neural capture mode (RTNeural) for user amp captures
- [ ] Resizable / skinned UI with custom LookAndFeel
- [ ] Tuner utility

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
