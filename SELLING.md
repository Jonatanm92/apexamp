# Selling ApexAmp — commercial release checklist

This is the practical roadmap to turn ApexAmp into a product you can sell. None of
it changes the DSP; it's the business/distribution layer.

## 1. Licensing you MUST clear first
- **JUCE**: closed-source commercial plugins require a paid JUCE licence (Indie or
  Pro). Get it at https://juce.com/get-juce before selling. (AGPLv3 is only for
  open-source releases.)
- **Steinberg ASIO SDK** and **VST3 SDK**: comply with Steinberg's licence terms
  when distributing.
- **IRs**: ship only the built-in cab voicings (original) — never bundle
  third-party/commercial IRs without a redistribution licence.

## 2. Code signing & notarization (so it installs without scary warnings)
- **Windows**: buy an OV/EV code-signing certificate; sign the `.vst3` and installer
  with `signtool`. EV certs avoid SmartScreen warnings.
- **macOS**: enrol in the Apple Developer Program ($99/yr). Sign with a Developer ID
  certificate (`codesign`), then **notarize** with `notarytool` and `staple`. Build
  a **universal binary** (arm64 + x86_64) so it runs on Intel and Apple Silicon.

## 3. Installers
- **Windows**: Inno Setup or WiX to place `ApexAmp.vst3` in
  `C:\Program Files\Common Files\VST3`.
- **macOS**: `pkgbuild`/`productbuild` to install `.vst3` and `.component` into the
  standard plug-in folders; notarize the `.pkg`.

## 4. Copy protection / licensing (optional but common)
- Simple: serial-key activation (offline key check).
- Hosted: services like PACE/iLok or a custom license server. Keep it light — heavy
  DRM annoys customers.

## 5. Versioned releases
- Tag releases (e.g. `v1.0.0`) and attach signed installers to a **GitHub Release**
  (permanent download, unlike CI artifacts which expire).
- The CI already builds Win/Mac/Linux on every push; extend it to run on tags and
  upload the installers as release assets.

## 6. Branding & store assets
- Logo, product shots/GIFs of the UI in a DAW, a 30–60s demo riff video.
- A one-line hook + feature bullets (dual-channel, IR loader + dual-IR blend, gate,
  TS boost, tuner, presets).

## 7. Where to sell
- Your own site via **Gumroad / Lemon Squeezy / FastSpring / Paddle** (Paddle &
  FastSpring act as merchant of record and handle VAT/tax).
- Marketplaces: **Plugin Boutique**, **Reverb**, etc.
- Free **demo build** (e.g. periodic noise or save-disabled) to drive conversions.

## 8. Pricing (for context, not advice)
Indie high-gain amp sims typically sell in the ~$25–$80 range, often with intro
discounts and bundle deals.

## 9. Pre-launch QA checklist
- Validate in **pluginval** (strict) for VST3/AU.
- Test in major DAWs (Reaper, Ableton, Logic, FL, Cubase) at 44.1/48/96 kHz and
  several buffer sizes.
- Check automation, preset recall, state save/restore, and CPU on a full mix.
- Confirm no clicks on parameter sweeps and silence-in → silence-out.
