#!/bin/bash
# Installiert INDIKAT Star Pad (VST3 + AU) systemweit nach
# Macintosh HD/Library/Audio/Plug-Ins, hebt die macOS-Quarantäne auf
# und signiert die Plugins lokal (ad hoc). Fragt einmal nach dem Mac-Passwort.
cd "$(dirname "$0")" || exit 1

VST3="/Library/Audio/Plug-Ins/VST3"
AU="/Library/Audio/Plug-Ins/Components"
echo "Installiere nach $VST3 und $AU (Mac-Passwort erforderlich) ..."
sudo mkdir -p "$VST3" "$AU" || exit 1

for p in *.vst3; do
    sudo rm -rf "$VST3/$p" && sudo cp -R "$p" "$VST3/" && echo "VST3: $p"
done
for p in *.component; do
    sudo rm -rf "$AU/$p" && sudo cp -R "$p" "$AU/" && echo "AU:   $p"
done

for p in "$VST3"/INDIKAT*.vst3 "$AU"/INDIKAT*.component; do
    [ -e "$p" ] || continue
    sudo xattr -cr "$p"
    sudo codesign --force --deep --sign - "$p" >/dev/null 2>&1
done

# Ältere Kopien im Benutzerordner entfernen, damit die DAW keine Doppelten findet
rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/"INDIKAT*.vst3 "$HOME/Library/Audio/Plug-Ins/Components/"INDIKAT*.component 2>/dev/null

# AU-Cache auffrischen, damit Logic/Ableton die neuen Plugins finden
killall -9 AudioComponentRegistrar >/dev/null 2>&1

echo ""
echo "Fertig. Bitte die DAW neu starten (in Logic ggf. Plug-in-Manager > Zurücksetzen & neu scannen)."
