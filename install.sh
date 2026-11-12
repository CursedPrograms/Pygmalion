#!/usr/bin/env bash
# Installs Pygmalion for this user on Linux: ~/.local/share/Pygmalion, ~/.local/bin/pygmalion and a menu entry.
set -e
cd "$(dirname "$0")"
DEST="$HOME/.local/share/Pygmalion"
mkdir -p "$DEST" "$HOME/.local/bin" "$HOME/.local/share/applications"
cp -r build/Pygmalion build/assets build/data "$DEST/"
[ -d assets-mh ] && cp -r assets-mh "$DEST/"
printf '#!/bin/sh\nexec "%s/Pygmalion" "$@"\n' "$DEST" > "$HOME/.local/bin/pygmalion"
chmod +x "$HOME/.local/bin/pygmalion"
printf '[Desktop Entry]\nName=Pygmalion\nComment=People in 3D with sliders\nExec=%s/.local/bin/pygmalion %%F\nIcon=%s/assets/logo.png\nType=Application\nCategories=Graphics;3DGraphics;\n' "$HOME" "$DEST" > "$HOME/.local/share/applications/pygmalion.desktop"
echo "Installed: $DEST (run: pygmalion)"
