# apex-dsp

Framework-free C++17 DSP shared by the Apex plugins. No JUCE, no allocations after `prepare()`,
every class processes the same output whatever the host block size.

## Pitch engine

`apex::dsp::PitchShifter` is what plugins use. It picks one of two engines, maps **Body** onto
each engine's formant control and blends a latency-aligned dry signal.

| Class | Mode | How it works |
|---|---|---|
| `SplicingShifter` | Live | Ring buffer read at the pitch ratio. When the read head must jump, the jump is chosen by normalised cross-correlation over one longest period of history, taking the *shortest* jump that is nearly as good as the best one (≈ one period of the note being played), so latency follows the note instead of being sized for the lowest one. Pick attacks detected at the input make a lagging read head jump straight to the attack. Splice decisions are made on the mono sum and shared by all channels. |
| `LpcFormantCorrector` | Live (Body) | Zero-latency formant correction: fits an all-pole envelope to the unshifted content under the read head and to the shifted output, whitens with one and re-colours with a blend of the two. Lattice filters with per-sample interpolated reflection coefficients. Pick attacks pass uncorrected while the fits catch up (correction fades back in over ~25 ms); a peak-envelope guard keeps the corrected signal within ~3.5 dB of the uncorrected one. |
| `SpectralShifter` | Studio | Peak-shifting phase vocoder (Laroche & Dolson 1999): each spectral peak and its region of influence moves to the target frequency as a block with identity phase locking; peaks are tracked across frames for phase continuity; phases reset on pick attacks; the cepstral envelope is measured every frame so formants can be held or moved. ~85 ms window (scales with sample rate). |
| `Fft` | — | In-place radix-2 complex FFT, tables built in `prepare()`. |

## Tests and measurements

`tests/pitch_tests.cpp` uses a Karplus-Strong guitar (pick-position comb, resonant pickup), so the
right answer at any tuning can be rendered directly: an E-standard riff shifted down 7 semitones
is compared against the same riff rendered natively in A.

```bash
cmake -S libs/apex-dsp -B build-dsp -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp
./build-dsp/apex_pitch_tests listening/      # also runs under ctest as "pitch_engine"
```

Hard checks (fail the build): 0-semitone null against the input delayed by the reported latency,
dry/wet alignment, identical output for block sizes 1 / 67 / 1024, pitch accuracy, Body moving or
holding a resonance, stability on noise / DC / square waves from -24 to +24 semitones.

Results at 48 kHz on the synthetic guitar (first version):

| | Live | Studio |
|---|---|---|
| Pitch error, E2–E4 shifted -12…+12 st | ≤ 4.1 cents (≤ 1.6 below E4) | ≤ 0.07 cents |
| 0 st null | exact | 9e-8 |
| Latency | 8.1 ms reported; attacks measured at 9–13 ms (Body on, -7 / -2 / +5 st) | 85.3 ms, compensated by the host |
| 2.5 kHz resonance after -7 st, Body 100 % / 0 % | 2643 Hz / 1688 Hz | 2496 Hz / 1688 Hz |
| Distance to the natively tuned riff, Body 100 % / 0 % (third-octave dB RMS) | 2.10 / 5.55 | 2.24 / 5.70 |
| CPU, stereo, Body on | ~3 % of one core | ~3 % of one core |

The Body rows are the point of the design: holding the pickup/body resonance in place makes a
shifted guitar measurably closer to a guitar that is really tuned down. These are synthetic-signal
numbers; the listening files the test writes (`riff_E_standard`, `riff_A_native_ground_truth`,
`riff_E_to_A_<mode>_body<0|100>`) and real DI recordings are the next check.

## Render tool

```bash
./build-dsp/apex_pitch_render di.wav dropped.wav --semitones -7 --mode studio --body 1 --trim
```

Options: `--semitones N`, `--mode live|studio`, `--body 0..1`, `--mix 0..1`, `--trim` (remove the
latency so the output lines up with the input in a DAW).

## Known limits / next steps

- Live upshifts read the buffer faster than real time without an anti-alias filter, so content
  above `fs / (2 × ratio)` folds back (audible only on bright material at large upshifts).
- Studio uses one window size; a multi-resolution version (long windows for the lows, short for
  the highs) would cut pre-echo on attacks.
- Benchmark against Signalsmith Stretch / Rubber Band in the same harness, and run blind A/B on
  real DI recordings.
- Octave-down blend and MIDI-controlled dives (whammy) for the plugin.
