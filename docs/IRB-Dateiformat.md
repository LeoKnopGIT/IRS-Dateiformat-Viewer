# IRB-Dateiformat der Jenoptik VARIOSCAN

Das Format wurde anhand von Beispieldateien einer **VARIOSCAN hr, VS 3021-ST** (Firmware R2.04, Messbereich B1: −40 … 100 °C) entschlüsselt. Den Aufbau aus Dateikopf und Blöcken teilt sie mit dem IRBIS-Format von InfraTec ([tomsoftware/Irbis-File-Format](https://github.com/tomsoftware/Irbis-File-Format)), aber Bildkopf und Kalibrierung sind anders aufgebaut.

Alle Zahlen sind **little-endian**. `float` heißt IEEE-754 mit 32 Bit, `double` mit 64 Bit.

## Dateikopf

| Offset | Typ | Inhalt | Beispiel |
|---|---|---|---|
| `0x00` | 5 Byte | Kennung `FF 49 52 42 00` | `ÿIRB` |
| `0x05` | 16 Byte | Typ-String, mit Nullbytes aufgefüllt | `VARIOSCAN hr` |
| `0x15` | int32 | unbekannt | 100 |
| `0x19` | int32 | Offset des Blockverzeichnisses | `0x40` |
| `0x1D` | int32 | Anzahl der Blöcke | 2 |

## Blockverzeichnis

Jeder Eintrag ist 32 Byte lang und besteht aus 8 × int32:

| Feld | Inhalt |
|---|---|
| 0 | Blocktyp: 1 = Messbild, 2 = Vorschaubild |
| 1 | unbekannt (100) |
| 2 | Bildnummer (1) |
| 3 | Offset des Blocks in der Datei |
| 4 | Größe des Blocks |
| 5–7 | unbekannt (0, 1, 0) |

Beispiel: Das Messbild liegt bei `0x80` mit 173 544 Byte, das Vorschaubild bei `0x2A668` mit 3 045 Byte.

## Bildblock

Alle Offsets sind relativ zum Anfang des Blocks. Der Kopf ist 744 Byte lang, danach kommen die Pixel. Allgemein gilt: Kopflänge = Blockgröße − Breite × Höhe × Byte/Pixel.

| Offset | Typ | Inhalt | Beispiel |
|---|---|---|---|
| `+0` | uint16 | Byte pro Pixel | 2 |
| `+2` | uint16 | komprimiert | 0 |
| `+4` | uint16 | Breite | 360 |
| `+6` | uint16 | Höhe | 240 |
| `+14` | uint16 | Breite − 1 | 359 |
| `+18` | uint16 | Höhe − 1 | 239 |
| `+24` | float | Emissionsgrad | 1,0 |
| `+28` | float | Objektabstand in m | 2,0 |
| `+32` | float | Umgebungstemperatur in K | 293,15 |
| `+40` | float | Pfadtemperatur in K | 293,15 |
| `+48` | float | vermutlich obere Wellenlänge in µm | 12,0 |
| `+52` | float | vermutlich Bandbreite in µm | 4,0 |
| `+56` | uint16 | unbekannt | 2 |
| `+58` | uint16 | Anzahl Kalibrier-Stützpunkte n | 6 |
| `+60` | n × (float, float) | Stützpunkte (Rohwert, Temperatur in K) | siehe unten |
| `+222` | float | Kalibrierbereich min in K | 233,85 |
| `+226` | float | Kalibrierbereich max in K | 373,65 |
| `+230` | String | Firmware | `R2.04` |
| `+252` | String | Gerät | `VARIOSCAN hr` |
| `+284` | String | Modell | `VS: 3021` |
| `+316` | String | Seriennummer | `104402` |
| `+412` | float | Sichtfeld horizontal in Grad | 30,0 |
| `+416` | float | Sichtfeld vertikal in Grad | 20,0 |
| `+500` | String | Messbereich | `B1:-40..100` |
| `+580` | String | Kalibrierdatei | `04402A1.SPC` |
| `+596` | String | Kalibrier-Version | `1.0` |
| `+600` | String | Kalibrierdatum | `2000/ 2/ 3 1:17` |
| `+632` | float | Mitte der Anzeige bei der Aufnahme in K | 288,15 |
| `+636` | float | Spanne der Anzeige bei der Aufnahme in K | 40,0 |
| `+640` | double | Aufnahmezeit als OLE-Datum (Tage seit 30.12.1899) | 44827,70136 = 23.09.2022 16:49:57,92 |
| `+648` | int32 | Millisekunden der Aufnahmezeit | 920 |
| `+654` | String | Hersteller | `JENOPTIK L.O.S.` |
| `+744` | Breite × Höhe × uint16 | Pixel-Rohwerte, Zeile für Zeile, links oben beginnend | |

Die Aufnahme liegt als normales Zeilen-Spalten-Raster vor. Der Scanner muss also nicht als Winkel umgerechnet werden. Bei 30° auf 360 Pixel beträgt der IFOV 1,45 mrad, das entspricht 2,9 mm Pixelgröße bei 2 m Abstand.

## Vorschaubild

Es hat denselben Kopf mit 744 Byte. Danach folgen 59 × 39 Pixel mit 8 Bit als Graustufen.

## Rohwert → Temperatur

In jeder Datei steht eine eigene Stützpunkttabelle. Die Rohwerte verschieben sich je nach Zustand der Kamera, die Temperaturen bleiben gleich:

| Temperatur | Rohwert in 00101003 | Rohwert in 20923002 |
|---|---|---|
| 0,15 K (Nullpunkt) | 18 714,8 | 15 333,3 |
| −39,3 °C | 22 699,1 | 19 079,5 |
| −9,8 °C | 26 112,8 | 22 289,3 |
| 10,4 °C | 29 352,3 | 25 335,2 |
| 40,1 °C | 35 392,4 | 31 014,5 |
| 100,5 °C | 51 347,5 | 46 171,9 |

Durch die Punkte oberhalb von 0 K wird eine Planck-artige Kennlinie gelegt:

```
S(T) = O + R / (exp(B / T) − F)          T in Kelvin
T(S) = B / ln(R / (S − O) + F)
```

Für jede Datei passen die Parameter durch einen Fit in zwei Stufen: Für feste Werte von B und F sind O und R ein lineares Ausgleichsproblem. B und F selbst werden mit dem Nelder-Mead-Verfahren gesucht. Die Kurve trifft die 5 Stützpunkte auf 0,01 bis 0,02 K. Beispiel für 00101003: O = 19 369,5, R = 2 152 046, B = 1 509,53 K, F = −10,474.

Alternativ kann man **linear zwischen den Stützpunkten** interpolieren. Das liegt in den Beispielbildern um bis zu etwa 1,2 K niedriger als die Kennlinie, weil die Strahlung nicht linear mit der Temperatur wächst. Welche Methode die Original-Software verwendet, ist unbekannt.

**Emissionsgrad-Korrektur** (ohne Atmosphäre):

```
U      = S − O                                  Signal oberhalb des Nullpunkts
U_obj  = (U − (1 − ε) · U(T_refl)) / ε
T_obj  = T(U_obj + O)
```

## Prüfwerte

Damit lassen sich eigene Implementierungen prüfen (Planck-Fit, ε = 1):

| Datei | Min | Max | Mittel | Pixel (250, 60) |
|---|---|---|---|---|
| `00101003.IRB` | 10,33 °C bei (192, 54) | 17,62 °C bei (343, 125) | 13,78 °C | |
| `20923002.IRB` | −16,62 °C bei (104, 38) | 26,27 °C bei (195, 210) | 13,81 °C | 7,34 °C (Rohwert 24 828) |

Die C++-, JavaScript- und Python-Implementierung in diesem Repository stimmen untereinander auf 2 · 10⁻⁶ K überein.
