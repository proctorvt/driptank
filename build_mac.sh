#!/bin/bash
# Builds Driptank 63 on your Mac and installs the VST3 + AU.
# Needs: Apple's command line tools (this script will offer to install them) and CMake.
set -e
cd "$(dirname "$0")"

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Apple's command line tools aren't installed. A window will pop up to install them."
    echo "When that finishes, run this script again."
    xcode-select --install
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "CMake isn't installed. Easiest way:  brew install cmake"
    echo "(no Homebrew? get the macOS installer from https://cmake.org/download/ , then open"
    echo " CMake.app once and choose Tools > How to Install For Command Line Use)"
    exit 1
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j "$(sysctl -n hw.ncpu)" \
      --target Driptank_VST3 Driptank_AU Driptank_Standalone

A="build/Driptank_artefacts/Release"
VST3=~/Library/Audio/Plug-Ins/VST3
AU=~/Library/Audio/Plug-Ins/Components
mkdir -p "$VST3" "$AU" ~/Applications

rm -rf "$VST3/Driptank 63.vst3" "$AU/Driptank 63.component" ~/Applications/"Driptank 63.app"
cp -R "$A/VST3/Driptank 63.vst3" "$VST3/"
cp -R "$A/AU/Driptank 63.component" "$AU/"
cp -R "$A/Standalone/Driptank 63.app" ~/Applications/

for p in "$VST3/Driptank 63.vst3" "$AU/Driptank 63.component" ~/Applications/"Driptank 63.app"; do
    codesign --force --deep -s - "$p"
done
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo
echo "Installed:"
echo "  VST3 -> $VST3/Driptank 63.vst3"
echo "  AU   -> $AU/Driptank 63.component"
echo "  App  -> ~/Applications/Driptank 63.app"
echo "Rescan plug-ins in your DAW and look for 'Driptank 63' under Proctor."
