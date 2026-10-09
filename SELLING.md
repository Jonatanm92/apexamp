# Selling ApexAmp — release checklist

This document tracks what is required to legally and technically ship ApexAmp
as a commercial product. It is informational, not legal advice.

## Licensing

- **JUCE 8**: the free Starter licence covers closed-source commercial products
  while revenue over the last 12 months stays at or below $20,000 (no splash
  screen required); above that, Indie ($800 perpetual, up to $300,000) or Pro.
  Confirm against the current JUCE EULA before release.
- **VST3 SDK**: MIT licensed since VST 3.8 (October 2025); keep the notice.
- **NeuralAmpModelerCore**: MIT licensed — free to use commercially. Keep the
  MIT licence text in the distribution / about box attribution.
- **Bundled NAM rigs (Bite / Body / Edge)**: these are the owner's own trained
  captures and are cleared for commercial distribution.
- **Bundled cabinet IRs (shown as Cinder / Iron / Obsidian; files
  ir_ashen / ir_meshuggah / ir_pdi09)**: confirm distribution rights for each IR
  before shipping and replace any that are not cleared. Product-facing names
  avoid band, album and competitor names.
- **Names**: no third-party trademarks in product, feature or preset names
  (bands, albums, competitors' feature names). The company string in the
  plugin metadata ("PolychromeNext") is too close to PolyChrome DSP and must be
  replaced with the final company name before release.
- **ASIO SDK** (Windows): the Steinberg ASIO SDK is compiled against but not
  redistributed; the resulting binary may be distributed. Review Steinberg's
  licensing terms.

## Licence keys and the trial (built in)

ApexAmp has a 14-day trial with everything working. After it, without a key,
the plugin passes the guitar through untouched; sessions keep their settings.
Keys are Ed25519-signed and checked offline (no server, no activation limit to
run). One key unlocks every DAW on the computer.

1. **Make your own key pair once** (on your own computer, not in CI):
   `apex_keygen keypair ~/apex-secret.key`. Keep the secret key offline with a
   backup (password manager + USB stick). Lose it and you cannot make keys for
   builds that carry its public key; leak it and anyone can.
2. **Build releases with the public key** it prints:
   `-DAPEX_LICENCE_PUBLIC_KEY=<hex>` (add it as a repository variable for CI).
   Builds without it use the development key, whose secret is in the repo, and
   say "DEVELOPMENT BUILD · NOT FOR SALE" on the unlock screen. Never sell one.
3. **Buy button**: `-DAPEX_STORE_URL=https://...` (your store page).
4. **Keys per sale**: `apex_keygen issue ~/apex-secret.key --owner "Buyer Name"`
   for one buyer (the name shows in the plugin), or
   `apex_keygen batch ~/apex-secret.key 500 > keys.txt` for a list of keys to
   upload to a store that hands out one key per sale.
5. `apex_keygen check <key>` verifies a key (support requests).

The trial is a file in the Apex folder; deleting it restarts the trial. That is
accepted: a trial only has to keep honest people honest.

## Installers (built by CI)

Every CI run uploads an `Installers-<platform>` artifact:

- **Windows**: `ApexAmp-<version>-Windows-Setup.exe` and `ApexDrop-...` (Inno
  Setup, `packaging/windows/apex.iss`). The VST3 goes to
  `C:\Program Files\Common Files\VST3`, the standalone app to
  `Program Files\Apex\<product>`; EULA page, uninstaller, 64-bit only.
- **macOS**: `ApexAmp-<version>-macOS.pkg` (`packaging/macos/build_pkg.sh`) with
  the VST3, the Audio Unit and the app as separate choices. Universal binaries
  (Apple silicon and Intel), macOS 10.15 and later. CI also runs Apple's
  `auval` on both Audio Units.
- **Linux**: `.tar.gz` with an `install.sh` for the current user.

The version comes from the `VERSION` file. The EULA (`packaging/EULA.txt`) and
third-party notices (`packaging/THIRD_PARTY.txt`) ship with every installer;
have the EULA reviewed for your country before selling.

## Release settings and signing

Set these in GitHub under Settings → Secrets and variables → Actions. Without
them CI still builds everything, unsigned and with the development key.

Variables:
- `APEX_LICENCE_PUBLIC_KEY`: your public key (see above). Required for sale.
- `APEX_STORE_URL`: the store page for the Buy button.
- `APEX_PUBLISHER`: the company name shown by the Windows installer.

Secrets, macOS (Apple Developer Program, $99/year):
- `MACOS_CERT_P12`: base64 of a .p12 holding both your "Developer ID
  Application" and "Developer ID Installer" certificates;
  `MACOS_CERT_PASSWORD`: its password.
- `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD` (an app-specific password)
  for notarisation. With these set, CI signs the plug-ins and the app, signs
  the .pkg, notarises it and staples the ticket. Unsigned packages are blocked
  by Gatekeeper on customers' Macs.

Secrets, Windows (an OV or EV code-signing certificate):
- `WINDOWS_CERT_PFX`: base64 of the .pfx; `WINDOWS_CERT_PASSWORD`. CI signs
  the plug-ins, the app and the installer. Unsigned installers trigger
  SmartScreen warnings. (EV certificates on hardware tokens need a cloud
  signing service instead; adapt the signing step to it.)

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
- EULA: drafted in `packaging/EULA.txt` (have it reviewed). Privacy note: the
  plugins collect nothing and never go online.
- Versioned changelog.

## Current status

- Cross-platform CI builds (Win/Mac/Linux) with audio, resampler, licence,
  gate and bypass tests: DONE.
- pluginval strictness 10 (both VST3s, Linux) and auval (both AUs, CI): DONE.
- Licence keys and 14-day trial: DONE (needs your own key pair).
- Installers for Windows, macOS and Linux: DONE (unsigned until the signing
  secrets are set).
- Before the first sale: your key pair, company name (replace
  "PolychromeNext"), IR rights, signing certificates, EULA review, testing in
  the DAWs above.
