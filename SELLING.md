# Selling ApexAmp — release checklist

This document tracks what is required to legally and technically ship ApexAmp
as a commercial product. It is informational, not legal advice.

## Licensing

- **JUCE**: ApexAmp is built on JUCE. A commercial product requires a paid JUCE
  licence (Indie or Pro) unless it qualifies for the free/GPL tier. The GPL
  option is not compatible with a closed-source paid plugin, so a commercial
  JUCE licence is needed before selling. See https://juce.com/get-juce.
- **NeuralAmpModelerCore**: MIT licensed — free to use commercially. Keep the
  MIT licence text in the distribution / about box attribution.
- **Bundled NAM rigs (Bite / Body / Edge)**: these are the owner's own trained
  captures and are cleared for commercial distribution.
- **Bundled cabinet IRs (Ashen / Meshuggah / PDI-09)**: confirm distribution
  rights for each IR before shipping. Replace any that are not cleared.
- **ASIO SDK** (Windows): the Steinberg ASIO SDK is compiled against but not
  redistributed; the resulting binary may be distributed. Review Steinberg's
  licensing terms.

## Code signing & notarization

- **Windows**: sign the VST3 and Standalone with an EV/OV code-signing
  certificate (signtool). Unsigned plugins trigger SmartScreen warnings.
- **macOS**: sign with a Developer ID Application certificate, then notarize
  the .vst3/.component/.app with `notarytool` and staple the ticket. Required
  for Gatekeeper on modern macOS. Build universal (x86_64 + arm64).

## Installers

- **Windows**: an installer (e.g. Inno Setup) that places the VST3 in
  `C:\Program Files\Common Files\VST3` and the Standalone in Program Files.
- **macOS**: a signed/notarized `.pkg` installing to `/Library/Audio/Plug-Ins/`.

## Pre-release QA

- Validate the VST3 with `pluginval` (strictness 10) on Win/Mac.
- Test in major DAWs: Reaper, Logic, Ableton Live, Cubase, FL Studio.
- Verify at 44.1 / 48 / 88.2 / 96 kHz and at 32 / 64 / 128 / 256 / 512 / 1024
  block sizes. Confirm latency reporting (resampler) is correct in-DAW.
- Confirm state save/recall (presets, parameters) round-trips in a session.
- Check CPU usage with all features engaged.

## Product

- App icon / branding for the Standalone and installer.
- About box with version + attributions (in app — see the editor's About).
- EULA, privacy note, and a simple license/activation strategy if desired.
- Versioned changelog.

## Current status

- Cross-platform CI builds (Win/Mac/Linux) with audio + resampler guards: DONE.
- NAM engine at native 48 kHz with host resampling: DONE.
- Custom UI, presets, tone controls: DONE / ongoing.
- Signing, notarization, installers, pluginval pass: TODO before sale.
