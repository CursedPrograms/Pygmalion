#!/usr/bin/env bash
# Builds Pygmalion on Linux (cmake, a C++17 compiler, libsdl2-dev); MakeHuman's asset pack (CC0) is fetched once.
set -e
cd "$(dirname "$0")"
if [ ! -d assets-mh/clothes ]; then
    mkdir -p external/mh-assets
    curl -L -o external/makehuman_system_assets_cc0.zip https://mirror1.makehuman.net/asset_packs/makehuman_system_assets/makehuman_system_assets_cc0.zip
    unzip -q -o external/makehuman_system_assets_cc0.zip -d external/mh-assets
    if [ -d external/mh-assets/system ]; then cp -r external/mh-assets/system assets-mh; else cp -r external/mh-assets assets-mh; fi
fi
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
echo "Built: build/Pygmalion"
