#!/usr/bin/env bash
# Linux archive for an Apex plug-in: the VST3 and the standalone app with an
# install script for the current user.
#
#   package.sh <artefacts-dir> <product> <slug> <version> <out-dir>
set -euo pipefail

art="$1"; product="$2"; slug="$3"; version="$4"; out="$5"
here="$(cd "$(dirname "$0")" && pwd)"
name="$slug-$version-Linux"
work="$(mktemp -d)/$name"
mkdir -p "$work" "$out"

cp -R "$art/VST3/$product.vst3" "$work/"
cp "$art/Standalone/$product" "$work/"
cp "$here/../EULA.txt" "$here/../THIRD_PARTY.txt" "$work/"

cat > "$work/install.sh" <<SH
#!/usr/bin/env bash
# Installs $product for the current user: the VST3 into ~/.vst3 and the
# standalone app into ~/.local/bin.
set -euo pipefail
cd "\$(dirname "\$0")"
mkdir -p "\$HOME/.vst3" "\$HOME/.local/bin"
rm -rf "\$HOME/.vst3/$product.vst3"
cp -R "$product.vst3" "\$HOME/.vst3/"
cp "$product" "\$HOME/.local/bin/"
echo "Installed $product: VST3 in ~/.vst3, app in ~/.local/bin"
SH
chmod +x "$work/install.sh" "$work/$product"

tar -C "$(dirname "$work")" -czf "$out/$name.tar.gz" "$name"
echo "built $out/$name.tar.gz"
