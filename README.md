# Apex — plugins for thall & djent

One repository for the Apex plugin suite and the DSP the plugins share.

| | What it is | Status |
|---|---|---|
| [**ApexAmp**](plugins/ApexAmp/README.md) | NAM-powered high-gain amp: Bite / Body / Edge rigs, Screamer boost, gate, cab IRs | beta |
| [**Apex Drop**](#apex-drop) | Drop-tuning pitch shifter: Live (play through it) and Studio (mix quality) engines, formant-correct "Body" | v0.1, generic UI |
| [**apex-dsp**](libs/apex-dsp/README.md) | Framework-free C++ DSP shared by the plugins, with offline tests and tools | — |

```
CMakeLists.txt          # suite: apex-dsp + every plugin, JUCE via FetchContent
libs/apex-dsp/          # pure C++ DSP (no JUCE): pitch engine, tests, render tool
plugins/ApexAmp/        # amp plugin (Source/, assets/, tools/)
plugins/ApexDrop/       # pitch shifter plugin (thin wrapper around apex-dsp)
third_party/            # NeuralAmpModelerCore
```

## Building

You need **CMake ≥ 3.21** and a C++20 compiler. JUCE is downloaded automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Artefacts land in `build/plugins/<Plugin>/<Plugin>_artefacts/Release/` (VST3, AU on macOS,
Standalone). On Linux install the JUCE dev packages first:
`libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libfreetype6-dev`.

Working only on DSP? Skip the JUCE download entirely:

```bash
cmake -S libs/apex-dsp -B build-dsp -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp && ./build-dsp/apex_pitch_tests listening/   # writes WAVs to listen to
```

Windows ASIO for the Standalone apps: see [ApexAmp's README](plugins/ApexAmp/README.md#low-latency-asio-windows-standalone)
(`-DAPEXAMP_ENABLE_ASIO=ON` applies to every plugin).

## Apex Drop

Drop tuning without the "pitch-shifted" sound. Two engines share one set of controls:

| Mode | Engine | Latency (48 kHz) | Use it for |
|---|---|---|---|
| **Live** | Time-domain, splices chosen by correlation, attack catch-up | attacks land ~9–13 ms after the input | playing through it while tracking |
| **Studio** | Phase vocoder that moves spectral peaks with phase locking, phase reset on pick attacks | 85 ms, reported to the host and compensated | re-amping, mixing, bouncing |

**Body** keeps the guitar's pickup / body resonances where they are while the strings drop,
which is what a really down-tuned guitar does. At 0 % the resonances move with the pitch (the
classic shifter sound). Put Apex Drop **before** the amp: shift the clean DI, never the
distorted signal.

Controls: Shift (±24 st) · Fine (±100 ct) · Mode · Body · Mix (latency-aligned dry) · Output.
Bypass fades to the aligned dry signal so the host's delay compensation never breaks.

The editor is JUCE's generic one for now; the Apex design system (Monolith) replaces it once
the mockups are signed off. Measured results and how they are measured:
[libs/apex-dsp/README.md](libs/apex-dsp/README.md).

## Selling

See [SELLING.md](SELLING.md) for licensing, signing and QA before release.
