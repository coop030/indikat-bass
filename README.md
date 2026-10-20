# INDIKAT Bass Synth / INDIKAT Bass FX

Zwei VST3/AU-Plugins (JUCE 8) für Basslines im Minimal-Dub- und Dub-Techno-Stil, aus einer Codebasis:

| Plugin | Typ | Wofür |
|---|---|---|
| **INDIKAT Bass Synth** | Instrument (MIDI) | Monophoner Bass-Synth mit 2 Oszillatoren + Sub und kompletter Effektkette |
| **INDIKAT Bass FX** | Audio-Effekt | Dieselbe Effektkette für vorhandene Basslines (Samples, andere Synths) |

![INDIKAT Bass Synth](docs/synth.png)
![INDIKAT Bass FX](docs/fx.png)

## Synth

- **OSC 1**: Sine / Triangle / Saw / Square / Pulse, Oktave, Level, plus **Sub** (Sinus eine Oktave tiefer)
- **OSC 2**: gleiche Wellenformen, Oktave, Halbtöne, Feinstimmung (für Quinten-/Septimen-Stabs oder Schwebung)
- **AMP**: ADSR mit exponentiellem Decay/Release
- **AMP**: Retrig/Legato-Modus
- **VOICE**: Glide (Portamento), Velocity-Empfindlichkeit, analoger Drift
- **CHORD** (Schalter + Typ): m7, m9, Moll, Quinte. Jeder Akkordton spielt OSC 1 + OSC 2, Glide zieht den ganzen Akkord mit, der Sub bleibt auf dem Grundton
- Saw/Square mit PolyBLEP (wenig Aliasing), Phasen-Reset bei jeder neuen Note für einen gleichbleibenden Punch
- Im Synth steuert eine eigene **Filter-Hüllkurve** (Attack/Decay) den Filter, im FX ein Envelope Follower

## Effektkette (beide Plugins)

```
[Synth | Input] → Low Cut → Mono-Bass → Sub Boost → Drive → Filter (+LFO) → Dub Delay → Reverb → Out
```

| Sektion | Wofür |
|---|---|
| **LOW END** | Low Cut gegen Rumpeln, alles unter „Mono“ wird mono (clubtauglich), Sub per Low-Shelf anheben |
| **DRIVE** | Asymmetrische Sättigung mit Tone, parallel mischbar |
| **FILTER** | Resonantes LP/BP/HP, von der Hüllkurve moduliert; ENV ist bipolar (violett = schließend). LFO: Sinus synchron zur DAW (1/4 bis 4 Takte), Tiefe bis ±2,5 Oktaven |
| **DUB DELAY** | Tempo-synchron, Ping Pong, gesättigtes und gedämpftes Feedback. Der Low Cut hält den Sub aus dem Echo |
| **REVERB** | Dunkler, modulierter FDN-Hall: Size (Raum + Nachhallzeit 0,7–14 s), Tone, Mix. LOW CUT (Standard: an) hält den Sub aus dem Hall |
| **OUT** | Level/Input, Dry/Wet, Output |

## Singularity (Schwarzes Loch)

Das Fenster im Header zeigt ein Schwarzes Loch, das in Echtzeit berechnet wird (Gravitationslinse,
Akkretionsscheibe, Photonenring). Es folgt den Parametern und dem Audiosignal
([Video](docs/blackhole.mp4)):

| Parameter | Wirkung |
|---|---|
| Sub Boost, Level/Input, Low Cut | Masse (Größe des Horizonts), federt nach |
| Ausgangspegel | Pulsieren mit dem Signal |
| Reverb Size/Mix/Tone + Nachhall | Nebel um das Loch: Ausdehnung, Dichte, Farbe (Violett bis Eis); glüht nach |
| Filter-LFO | Scheibe wird im LFO-Takt heller und dunkler |
| Chord | Ein zusätzlicher Ring pro Akkordton |
| Cutoff | Temperatur: tiefes Blau → Eis-Cyan → Weißglut |
| Resonanz | Schärfe und Helligkeit des Photonenrings |
| Drive | Crunch: Farbversatz, Körnung, Bitcrush-Stufen |
| Delay Feedback/Mix | Echo-Ringe um den Horizont |
| Mono | Neigung der Scheibe |
| Dry/Wet | Dichte der Scheibe |
| Synth: Wellenform, Drift, OSC 2 | Textur der Scheibe, Wackeln, Doppel-Photonenring |

**Jede Parameteränderung**, auch durch Automation, löst einen Effekt aus: Partikel spiralen ins Loch,
Schockwellen laufen nach außen, und Name und Wert werden kurz eingeblendet. Je nach Gruppe sieht der Effekt
anders aus: Drive glitcht, Delay erzeugt Echo-Wellen, Reverb lässt den Nebel aufblühen, Low End lässt das Loch
schlucken, Filter flackert auf. Die Darstellung läuft mit 30 fps und pausiert, wenn das Fenster nicht sichtbar ist.
Bei einem Preset-Wechsel kollabiert das Loch kurz und zeigt den Preset-Namen.

## Presets

**Synth (17):** Init Saw, Deep Sub Pluck, Dub Chord Stab, Minor Ninth Stab, Deep Space Chord, Rubber Bass, Warm Round,
Minimal Click Bass, Acid Glide, Detuned Wobble, Sub Only, Warehouse Bass, Echo Dub Bass, Hollow Pulse,
Tape Deep, Fifth Pluck, Night Drive. Die Pegel sind angeglichen (Bässe etwa −16 dBFS RMS, Stabs nach Spitzenpegel).

**FX (5):** Init, Deep Chord Dub, Rolling Sub, Warehouse Room, Echo Chamber

## Bedienung

- Ziehen = Wert ändern, **Doppelklick** = Standardwert, Mausrad funktioniert
- Das Fenster ist frei skalierbar (Ecke unten rechts)
- Mit `<` / `>` blätterst du durch die Presets, ein Klick auf den Namen öffnet die Liste
- Mit der Bildschirm-Tastatur (Synth) spielst du Presets auch ohne MIDI-Controller an

## Bauen

Voraussetzungen: CMake ≥ 3.22 und ein C++17-Compiler (Windows: Visual Studio 2022, macOS: Xcode).
JUCE wird automatisch heruntergeladen.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Ergebnis:
- `build/IndikatBassSynth_artefacts/Release/VST3/INDIKAT Bass Synth.vst3`
- `build/IndikatBassFX_artefacts/Release/VST3/INDIKAT Bass FX.vst3`

Installieren: Windows `C:\Program Files\Common Files\VST3\`, macOS `~/Library/Audio/Plug-Ins/VST3/`
(AU: `~/Library/Audio/Plug-Ins/Components/`), Linux `~/.vst3/`. Danach in der DAW neu scannen.

**Ohne Compiler:** Repository auf GitHub hochladen. Unter **Actions** baut `.github/workflows/build.yml`
automatisch Versionen für Windows, macOS und Linux zum Herunterladen.

**Offline-Test:** `cmake -B build -DINDIKAT_TESTS=ON` baut `IndikatRenderTest`. Das Programm rendert jedes
Synth-Preset mit einer Bassline als WAV und prüft auf NaN und Übersteuerung.

## Lizenzen

Schriften: Michroma, Questrial, Share Tech Mono (SIL Open Font License).
JUCE: AGPLv3 oder kommerzielle JUCE-Lizenz. Wer die Plugins verkaufen oder als Closed Source weitergeben will,
braucht eine JUCE-Lizenz.
