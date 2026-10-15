# irb_reader.py

Python-Bibliothek und Kommandozeilen-Konverter für VARIOSCAN-`.IRB`-Dateien. Sie braucht nur `numpy`, für `--png` zusätzlich `matplotlib`.

## Kommandozeile

```
python irb_reader.py ../samples/*.IRB --csv --png        # CSV (Semikolon/Komma) und PNG neben die Dateien
python irb_reader.py bild.IRB --info                     # alle Kopfdaten und Kennlinie anzeigen
python irb_reader.py bild.IRB --png --eps 0.95 --trefl 22 --palette magma --range camera
python irb_reader.py bild.IRB --csv-en --out export/     # CSV mit Komma und Dezimalpunkt
python irb_reader.py bild.IRB --npy --raw                # NumPy-Array und Rohwerte
```

| Option | Bedeutung |
|---|---|
| `--csv` / `--csv-en` | Temperaturmatrix als CSV, deutsches oder englisches Format |
| `--png` | Thermogramm mit Farbskala und Min/Max-Markern |
| `--npy` | Temperaturen als `.npy` |
| `--raw` | Rohwerte als CSV |
| `--info` | Kopfdaten ausgeben |
| `--eps`, `--trefl` | Emissionsgrad und reflektierte Temperatur in °C |
| `--curve fit\|linear` | Kennlinie |
| `--palette` | matplotlib-Colormap |
| `--range auto\|camera\|LO:HI` | Farbbereich für das PNG |
| `--out` | Zielordner |

## Als Modul

```python
from irb_reader import read_irb

img = read_irb("../samples/20923002.IRB")
img.meta["timestamp"]          # datetime(2022, 9, 23, 16, 49, 57, 920000)
img.raw                        # uint16-Array (240, 360)
T = img.temperatures()         # °C, Planck-Fit, ε aus der Datei
T95 = img.temperatures(eps=0.95, trefl_C=21)
Tlin = img.temperatures(curve="linear")
```

Den Dateiaufbau beschreibt [`../docs/IRB-Dateiformat.md`](../docs/IRB-Dateiformat.md).
