#!/bin/bash
# Installiert INDIKAT Bass Synth & FX (VST3 + AU) für den aktuellen Benutzer,
# hebt die macOS-Quarantäne auf und signiert die Plugins lokal (ad hoc).
cd "$(dirname "$0")" || exit 1

VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"

for p in *.vst3; do
    rm -rf "$VST3/$p" && cp -R "$p" "$VST3/" && echo "VST3: $p"
done
for p in *.component; do
    rm -rf "$AU/$p" && cp -R "$p" "$AU/" && echo "AU:   $p"
done

for p in "$VST3"/INDIKAT*.vst3 "$AU"/INDIKAT*.component; do
    [ -e "$p" ] || continue
    xattr -cr "$p"
    codesign --force --deep --sign - "$p" >/dev/null 2>&1
done

# AU-Cache auffrischen, damit Logic/Ableton die neuen Plugins finden
killall -9 AudioComponentRegistrar >/dev/null 2>&1

echo ""
echo "Fertig. Bitte die DAW neu starten (in Logic ggf. Plug-in-Manager > Zurücksetzen & neu scannen)."
