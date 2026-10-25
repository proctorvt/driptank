#!/bin/bash
# Double-click (or right-click > Open) to install Driptank 63 from this folder.
cd "$(dirname "$0")"
VST3=~/Library/Audio/Plug-Ins/VST3
AU=~/Library/Audio/Plug-Ins/Components
mkdir -p "$VST3" "$AU" ~/Applications

install_one () {
    local src="$1" dest="$2"
    [ -e "$src" ] || return 0
    rm -rf "$dest/$src"
    cp -R "$src" "$dest/"
    xattr -dr com.apple.quarantine "$dest/$src" 2>/dev/null
    codesign --force --deep -s - "$dest/$src" 2>/dev/null
    echo "Installed $src -> $dest"
}

install_one "Driptank 63.vst3" "$VST3"
install_one "Driptank 63.component" "$AU"
install_one "Driptank 63.app" ~/Applications
killall -9 AudioComponentRegistrar 2>/dev/null

echo
echo "Done. Rescan plug-ins in your DAW and look for 'Driptank 63'."
read -n 1 -s -r -p "Press any key to close."
