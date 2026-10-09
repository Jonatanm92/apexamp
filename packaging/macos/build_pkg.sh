#!/usr/bin/env bash
# macOS installer for an Apex plug-in: one .pkg with the VST3, the AU and the
# standalone app as separate choices, and the EULA.
#
#   build_pkg.sh <artefacts-dir> <product> <slug> <bundle-id> <version> <out-dir>
#
# <artefacts-dir> is the JUCE artefacts folder (VST3/, AU/, Standalone/).
# Signing (all optional, from the environment):
#   APPLE_APP_IDENTITY        "Developer ID Application: ..." signs the bundles
#   APPLE_INSTALLER_IDENTITY  "Developer ID Installer: ..." signs the .pkg
#   APPLE_NOTARY_PROFILE      a notarytool keychain profile: notarise and staple
set -euo pipefail

art="$1"; product="$2"; slug="$3"; id="$4"; version="$5"; out="$6"
here="$(cd "$(dirname "$0")" && pwd)"
work="$(mktemp -d)"
mkdir -p "$out" "$work/res"

stage() {   # <kind> <bundle> <install-location>
    local kind="$1" bundle="$2" location="$3"
    mkdir -p "$work/$kind"
    cp -R "$bundle" "$work/$kind/"
    if [[ -n "${APPLE_APP_IDENTITY:-}" ]]; then
        codesign --force --deep --timestamp --options runtime --sign "$APPLE_APP_IDENTITY" "$work/$kind/$(basename "$bundle")"
    fi
    # install where we say, never into a copy the user moved elsewhere
    pkgbuild --analyze --root "$work/$kind" "$work/$kind.plist" > /dev/null
    plutil -replace BundleIsRelocatable -bool NO "$work/$kind.plist"
    pkgbuild --root "$work/$kind" --component-plist "$work/$kind.plist" \
             --identifier "$id.$kind" --version "$version" \
             --install-location "$location" "$work/$kind.pkg"
}

stage vst3 "$art/VST3/$product.vst3"        "/Library/Audio/Plug-Ins/VST3"
stage au   "$art/AU/$product.component"     "/Library/Audio/Plug-Ins/Components"
stage app  "$art/Standalone/$product.app"   "/Applications"

cp "$here/../EULA.txt" "$work/res/license.txt"

cat > "$work/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>$product $version</title>
    <license file="license.txt"/>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
        <line choice="app"/>
    </choices-outline>
    <choice id="vst3" title="VST3 plug-in" description="For Cubase, Studio One, Reaper, Ableton Live, FL Studio and other VST3 hosts.">
        <pkg-ref id="$id.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit plug-in" description="For Logic Pro, GarageBand and other Audio Unit hosts.">
        <pkg-ref id="$id.au"/>
    </choice>
    <choice id="app" title="Standalone app" description="Play without a DAW.">
        <pkg-ref id="$id.app"/>
    </choice>
    <pkg-ref id="$id.vst3" version="$version" onConclusion="none">vst3.pkg</pkg-ref>
    <pkg-ref id="$id.au" version="$version" onConclusion="none">au.pkg</pkg-ref>
    <pkg-ref id="$id.app" version="$version" onConclusion="none">app.pkg</pkg-ref>
</installer-gui-script>
XML

pkg="$out/$slug-$version-macOS.pkg"
if [[ -n "${APPLE_INSTALLER_IDENTITY:-}" ]]; then
    productbuild --distribution "$work/distribution.xml" --resources "$work/res" --package-path "$work" \
                 --sign "$APPLE_INSTALLER_IDENTITY" "$pkg"
else
    productbuild --distribution "$work/distribution.xml" --resources "$work/res" --package-path "$work" "$pkg"
fi

if [[ -n "${APPLE_NOTARY_PROFILE:-}" ]]; then
    xcrun notarytool submit "$pkg" --keychain-profile "$APPLE_NOTARY_PROFILE" --wait
    xcrun stapler staple "$pkg"
fi

echo "built $pkg"
