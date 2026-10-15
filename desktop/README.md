# IRB Thermoviewer 1.0

Desktop-Programm zum Ansehen von Thermogrammen der **Jenoptik VARIOSCAN** (getestet mit VARIOSCAN hr, VS 3021-ST, Firmware R2.04), im Stil einer Windows-98-Anwendung. Geschrieben in C++ mit FLTK und statisch gelinkt, deshalb ist keine Installation nötig. Läuft unter Windows und Linux.

## Funktionen

- `.IRB`-Dateien öffnen: Menü *Datei → Öffnen*, Dateien ins Fenster ziehen oder als Startparameter übergeben (auch mehrere auf einmal)
- Umrechnung der Rohwerte in °C über die Kalibriertabelle, die in jeder Datei steht (Planck-Fit oder linear)
- 8 Farbpaletten: Eisen, Regenbogen, Inferno, Viridis, Kalt-Warm, Arktis, Weiß-heiß, Schwarz-heiß
- Farbstufen (8, 16 oder 32)
- Temperaturbereich automatisch, wie an der Kamera bei der Aufnahme eingestellt, oder manuell
- Fadenkreuz mit Temperaturanzeige am Cursor, Kreuze am wärmsten und kältesten Punkt
- Messpunkte per Klick setzen, mit der Maus ziehen oder mit den Pfeiltasten verschieben (mit Shift um 10 Pixel)
- Korrektur über Emissionsgrad ε und reflektierte Temperatur
- Farbskala mit Histogramm
- Export als PNG (Bericht mit Farbskala und Kenndaten) oder CSV (Temperaturmatrix mit Semikolon und Dezimalkomma, öffnet direkt in Excel)
- Dialog *Dateiinfo* mit Kamera, Seriennummer, Aufnahmezeit, Sichtfeld, Pixelgröße am Objekt und Kalibrierpunkten
- Einstellungen werden gespeichert: Palette, Fenstergröße, zuletzt benutzter Ordner

### Tastenkürzel

| Taste | Funktion |
|---|---|
| Strg+O | Dateien öffnen |
| Strg+W | Datei schließen |
| Strg+S | Als PNG speichern |
| Strg+E | Als CSV speichern |
| Strg+I | Dateiinfo |
| Strg+M | Min/Max-Kreuze ein/aus |
| Strg+P | Pixel scharf ein/aus |
| Pfeiltasten | gewählten Messpunkt verschieben |
| Entf | gewählten Messpunkt löschen |

Titelleiste: Doppelklick maximiert das Fenster. Größe ändern über den Rahmen oder den Griff unten rechts. Unter *Ansicht → Systemrahmen verwenden* lässt sich auf den normalen Fensterrahmen des Systems umschalten.

### Kommandozeile

```
thermoviewer98 datei1.IRB datei2.IRB          # öffnet die Dateien
thermoviewer98 --png bericht.png datei.IRB     # erzeugt nur das PNG und beendet sich
irb2csv datei.IRB ...                          # Temperaturmatrix als CSV (ohne Oberfläche)
irb2csv --eps 0.95 --trefl 22 --en datei.IRB   # mit ε-Korrektur, englisches CSV-Format
```

## Start

**Windows:** `thermoviewer98.exe` starten. Weil die Datei nicht signiert ist, zeigt Windows beim ersten Start eventuell „Der Computer wurde durch Windows geschützt“. Dann auf *Weitere Informationen → Trotzdem ausführen* klicken. Tipp: Rechtsklick auf eine `.IRB`-Datei → *Öffnen mit* → `thermoviewer98.exe` wählen.

**Linux:** `./thermoviewer98`. Die mitgelieferte Datei braucht glibc ab 2.38 (z. B. Ubuntu 24.04, Debian 13, Fedora 39) und X11; unter Wayland läuft sie über XWayland. Auf älteren Systemen einfach aus den Quellen bauen.

## Bauen

Alle Befehle im Ordner `desktop/` ausführen. Benötigt: CMake ab 3.16, einen C++17-Compiler und beim ersten Bauen Internet, um FLTK 1.4.4 von GitHub zu holen. Alternativ FLTK nach `third_party/fltk` legen.

### Linux

```
sudo apt install build-essential cmake ninja-build libx11-dev libxext-dev libxft-dev \
     libxinerama-dev libxcursor-dev libxrender-dev libxfixes-dev libfontconfig-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows (MSYS2 / MinGW-w64)

```
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja git
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows-EXE von Linux aus (Cross-Compile)

```
sudo apt install mingw-w64
cmake -S . -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build-win
```

Die Builds der GitHub Action entstehen genau so (Ubuntu 24.04, GCC 13, MinGW-w64 13).

## Aufbau des Quellcodes

| Datei | Inhalt |
|---|---|
| `src/irb.cpp` | IRB-Parser, Kennlinie (Planck-Fit), Paletten. Ohne GUI, auch für eigene Tools nutzbar |
| `src/thermoview.cpp` | Bildanzeige: Skalierung, Overlays, Maus und Tastatur |
| `src/win98.cpp` | Windows-98-Optik: 3D-Kanten, Titelleiste, Rahmen, Gruppenrahmen, Combobox, Schieberegler |
| `src/main.cpp` | Hauptfenster, Menüs, Dialoge, Export, Einstellungen |
| `src/irb2csv.cpp` | Kommandozeilen-Konverter |

## Dateiformat

Siehe [`../docs/IRB-Dateiformat.md`](../docs/IRB-Dateiformat.md).

## Lizenz

Projekt: siehe [`LICENSE`](../LICENSE).

FLTK steht unter der LGPL mit Ausnahme für statisches Linken (siehe FLTK `COPYING`).
