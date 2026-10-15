#!/usr/bin/env python3
"""
irb_reader.py - Reader und Konverter fuer .IRB-Dateien der Jenoptik VARIOSCAN
(getestet mit VARIOSCAN hr, VS 3021-ST, Firmware R2.04).

Kommandozeile
-------------
    python irb_reader.py *.IRB --csv --png
    python irb_reader.py bild.IRB --png --eps 0.95 --trefl 22 --palette inferno
    python irb_reader.py bild.IRB --csv --csv-en          # Komma/Punkt statt Semikolon/Komma
    python irb_reader.py bild.IRB --info                  # nur Kopfdaten anzeigen

Als Modul
---------
    from irb_reader import read_irb
    img = read_irb("20923002.IRB")
    T = img.temperatures(eps=1.0)      # numpy-Array (240, 360) in Grad Celsius
    img.raw                            # Rohwerte uint16
    img.meta                           # Kopfdaten (dict)

Abhaengigkeiten: numpy. Fuer --png zusaetzlich matplotlib.

Dateiformat (little-endian)
---------------------------
Dateikopf
  0x00  5 B   Kennung  FF 'I' 'R' 'B' 00
  0x05 16 B   Typ-String, hier "VARIOSCAN hr"
  0x19  int32 Offset des Blockverzeichnisses (0x40)
  0x1D  int32 Anzahl Bloecke (2)
Blockverzeichnis, je 32 Byte: int32 Typ, ?, Frame, Offset, Groesse, ?, ?, ?
  Typ 1 = Messbild, Typ 2 = Vorschaubild 59 x 39 mit 8 Bit
Bildblock (Offsets relativ zum Blockanfang)
  +0   uint16 Byte/Pixel (2)      +2  uint16 komprimiert (0)
  +4   uint16 Breite (360)        +6  uint16 Hoehe (240)
  +14  uint16 Breite-1            +18 uint16 Hoehe-1
  +24  float  Emissionsgrad       +28 float  Objektabstand [m]
  +32  float  Umgebungstemp. [K]  +40 float  Pfadtemperatur [K]
  +48  float  12.0, +52 float 4.0 (vermutl. Spektralband 8-12 um)
  +58  uint16 Anzahl Kalibrier-Stuetzpunkte (6)
  +60  n x (float Rohwert, float Temperatur [K])
       erster Punkt bei 0,15 K = Rohwert-Nullpunkt, dann -39,3 / -9,8 / 10,4 / 40,1 / 100,5 C
  +222 float  Kalibrierbereich min [K]   +226 float max [K]
  +230 str    Firmware "R2.04"           +252 str Geraet "VARIOSCAN hr"
  +284 str    Modell "VS: 3021"          +316 str Seriennummer
  +412 float  Sichtfeld horizontal [Grad] +416 float vertikal
  +500 str    Messbereich "B1:-40..100"  +580 str Kalibrierdatei, +600 Kalibrierdatum
  +632 float  Anzeigemitte bei Aufnahme [K] +636 float Anzeigespanne [K]
  +640 double Aufnahmezeit (OLE-Datum, Tage seit 30.12.1899)
  +648 int32  Millisekunden              +654 str Hersteller
  +744 Pixel: uint16, zeilenweise von links oben, Breite x Hoehe

Umrechnung Rohwert -> Temperatur
--------------------------------
Jede Datei hat ihre eigene Stuetzpunkttabelle (die Rohwerte verschieben sich mit
dem Kamerazustand). Durch die Punkte wird eine Planck-artige Kennlinie gelegt:
    S = O + R / (exp(B/T) - F)
Sie trifft die Stuetzpunkte auf etwa 0,01 K. Alternativ: lineare Interpolation
(--curve linear), die zwischen den Punkten bis gut 1 K niedriger liegen kann.
Emissionsgrad-Korrektur: U = S - O,  U_obj = (U - (1 - eps) * U(T_refl)) / eps.
Atmosphaerische Daempfung wird nicht korrigiert.
"""
import argparse
import datetime as _dt
import math
import os
import struct
import sys

import numpy as np

KELVIN = 273.15


def _str(buf, off, n):
    s = buf[off:off + n].split(b"\0", 1)[0]
    return s.decode("latin-1").strip()


def _nelder_mead(f, x0, step, iters=4000):
    n = len(x0)
    P = [list(x0)] + [[x0[j] + (step[j] if j == i else 0) for j in range(n)] for i in range(n)]
    V = [f(p) for p in P]
    for _ in range(iters):
        order = sorted(range(n + 1), key=lambda i: V[i])
        P = [P[i] for i in order]
        V = [V[i] for i in order]
        size = max(abs(P[i][j] - P[0][j]) / (abs(P[0][j]) + 1e-9) for i in range(1, n + 1) for j in range(n))
        if size < 1e-11:
            break
        c = [sum(P[i][j] for i in range(n)) / n for j in range(n)]
        w = P[n]
        xr = [c[j] + (c[j] - w[j]) for j in range(n)]
        fr = f(xr)
        if fr < V[0]:
            xe = [c[j] + 2 * (c[j] - w[j]) for j in range(n)]
            fe = f(xe)
            P[n], V[n] = (xe, fe) if fe < fr else (xr, fr)
        elif fr < V[n - 1]:
            P[n], V[n] = xr, fr
        else:
            xc = [c[j] + 0.5 * ((xr[j] if fr < V[n] else w[j]) - c[j]) for j in range(n)]
            fc = f(xc)
            if fc < min(fr, V[n]):
                P[n], V[n] = xc, fc
            else:
                for i in range(1, n + 1):
                    P[i] = [P[0][j] + 0.5 * (P[i][j] - P[0][j]) for j in range(n)]
                    V[i] = f(P[i])
    return P[0], V[0]


class PlanckCurve:
    """S = O + R / (exp(B/T) - F), fitted through the calibration points."""
    kind = "fit"

    def __init__(self, cal):
        pts = [(s, t) for s, t in cal if t > 20]
        if len(pts) < 4:
            raise ValueError("zu wenige Stuetzpunkte fuer den Fit")
        S = np.array([p[0] for p in pts])
        T = np.array([p[1] for p in pts])

        def solve(B, F):
            d = np.exp(B / T) - F
            if not np.all(d > 0) or not np.all(np.isfinite(d)):
                return None
            g = 1.0 / d
            A = np.vstack([np.ones_like(g), g]).T
            (O, R), *_ = np.linalg.lstsq(A, S, rcond=None)
            if R <= 0:
                return None
            return O, R, float(np.sum((S - O - R * g) ** 2))

        def obj(x):
            r = solve(x[0], x[1])
            return r[2] if r else math.inf

        best = None
        for s0 in ([1400, 0], [1500, -10], [1300, 0.5], [1800, -20]):
            x, fx = _nelder_mead(obj, s0, [150, 2])
            for _ in range(3):
                x, fx = _nelder_mead(obj, x, [abs(x[0]) * 0.02 + 1, abs(x[1]) * 0.05 + 0.1])
            if best is None or fx < best[1]:
                best = (x, fx)
        if not math.isfinite(best[1]):
            raise ValueError("Fit nicht konvergiert")
        self.B, self.F = best[0]
        self.O, self.R, _ = solve(self.B, self.F)
        self.max_err = float(np.max(np.abs(self.T(S - self.O) - T)))

    def U(self, t_kelvin):
        return self.R / (np.exp(self.B / np.asarray(t_kelvin, float)) - self.F)

    def T(self, u):
        u = np.asarray(u, float)
        with np.errstate(divide="ignore", invalid="ignore"):
            a = self.R / u + self.F
            return np.where((u > 0) & (a > 1), self.B / np.log(a), np.nan)


class LinearCurve:
    """Piecewise linear through the table (incl. the 0 K anchor)."""
    kind = "linear"

    def __init__(self, cal):
        pts = sorted(cal)
        self.S = np.array([p[0] for p in pts])
        self.Tk = np.array([p[1] for p in pts])
        anchor = [s for s, t in pts if t < 1]
        self.O = anchor[0] if anchor else self.S[0]
        self.max_err = 0.0

    @staticmethod
    def _interp(x, xs, ys):
        x = np.asarray(x, float)
        y = np.interp(x, xs, ys)
        lo, hi = x < xs[0], x > xs[-1]
        y = np.where(lo, ys[0] + (x - xs[0]) * (ys[1] - ys[0]) / (xs[1] - xs[0]), y)
        return np.where(hi, ys[-1] + (x - xs[-1]) * (ys[-1] - ys[-2]) / (xs[-1] - xs[-2]), y)

    def U(self, t_kelvin):
        return self._interp(t_kelvin, self.Tk, self.S) - self.O

    def T(self, u):
        t = self._interp(np.asarray(u, float) + self.O, self.S, self.Tk)
        return np.where(t > 0, t, np.nan)


class IRBImage:
    def __init__(self, path):
        self.path = path
        buf = open(path, "rb").read()
        if len(buf) < 0x60 or buf[0] != 0xFF or buf[1:4] != b"IRB":
            raise ValueError("keine IRB-Datei (Kennung FF 'IRB' fehlt)")
        file_type = _str(buf, 5, 16)
        boff, bcnt = struct.unpack_from("<2i", buf, 0x19)
        blocks = [dict(zip(("type", "x1", "frame", "offset", "size"), struct.unpack_from("<5i", buf, boff + 32 * i)))
                  for i in range(bcnt)]
        img = next((b for b in blocks if b["type"] == 1), None)
        if img is None:
            raise ValueError("kein Bildblock (Typ 1)")
        b = img["offset"]
        bpp, comp, w, h = struct.unpack_from("<4H", buf, b)
        if bpp != 2 or comp != 0:
            raise ValueError("nicht unterstuetzte Variante: %d Byte/Pixel, komprimiert=%d" % (bpp, comp))
        hdr = img["size"] - w * h * bpp
        f32 = lambda o: struct.unpack_from("<f", buf, b + o)[0]
        n = struct.unpack_from("<H", buf, b + 58)[0]
        cal = [struct.unpack_from("<2f", buf, b + 60 + 8 * i) for i in range(n)]
        m = dict(file_type=file_type, width=w, height=h, data_offset=b + hdr, blocks=blocks,
                 emissivity=f32(24), distance_m=f32(28), ambient_C=f32(32) - KELVIN,
                 path_C=f32(40) - KELVIN, calibration=cal)
        if hdr >= 744:
            ole = struct.unpack_from("<d", buf, b + 640)[0]
            m.update(
                cal_range_C=(f32(222) - KELVIN, f32(226) - KELVIN),
                firmware=_str(buf, b + 230, 22), device=_str(buf, b + 252, 32),
                model=_str(buf, b + 284, 32), serial=_str(buf, b + 316, 32),
                fov_deg=(f32(412), f32(416)), range_name=_str(buf, b + 500, 40),
                cal_file=_str(buf, b + 580, 16), cal_date=_str(buf, b + 600, 24),
                display_C=(f32(632) - KELVIN - f32(636) / 2, f32(632) - KELVIN + f32(636) / 2),
                timestamp=_dt.datetime(1899, 12, 30) + _dt.timedelta(days=ole),
                manufacturer=_str(buf, b + 654, 48))
        self.meta = m
        self.width, self.height = w, h
        self.raw = np.frombuffer(buf, "<u2", w * h, b + hdr).reshape(h, w).copy()
        self.linear = LinearCurve(cal)
        try:
            self.fit = PlanckCurve(cal)
            if self.fit.max_err > 0.25:
                self.fit = None
        except ValueError:
            self.fit = None

    def curve(self, kind="fit"):
        return self.fit if kind == "fit" and self.fit is not None else self.linear

    def temperatures(self, eps=1.0, trefl_C=None, curve="fit"):
        """Temperaturen in Grad Celsius als float32-Array (Hoehe x Breite)."""
        c = self.curve(curve)
        eps = min(1.0, max(0.01, float(eps)))
        if trefl_C is None:
            trefl_C = self.meta["ambient_C"]
        u = self.raw.astype(float) - c.O
        if eps < 1:
            u = (u - (1 - eps) * c.U(trefl_C + KELVIN)) / eps
        return (c.T(u) - KELVIN).astype(np.float32)


def read_irb(path):
    return IRBImage(path)


def _save_csv(T, path, english=False):
    sep, dec = (",", ".") if english else (";", ",")
    with open(path, "w", newline="") as fh:
        for row in T:
            fh.write(sep.join("" if not np.isfinite(v) else ("%.2f" % v).replace(".", dec) for v in row) + "\r\n")


def _save_png(img, T, path, palette, rng, title):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        sys.exit("Fuer --png wird matplotlib benoetigt:  pip install matplotlib")
    lo, hi = rng
    fig, ax = plt.subplots(figsize=(9, 6.3), dpi=110)
    im = ax.imshow(T, cmap=palette, vmin=lo, vmax=hi, interpolation="bilinear")
    iy, ix = np.unravel_index(np.nanargmax(T), T.shape)
    ny, nx = np.unravel_index(np.nanargmin(T), T.shape)
    for (x, y, col, lab) in ((ix, iy, "#ff5233", "Max"), (nx, ny, "#3fa6ff", "Min")):
        ax.plot(x, y, marker="+", ms=16, mew=4, color="black")
        ax.plot(x, y, marker="+", ms=14, mew=2, color=col)
        ax.annotate("%s %.1f °C" % (lab, T[y, x]), (x, y), xytext=(8, -12), textcoords="offset points",
                    color="white", fontsize=9, bbox=dict(boxstyle="round,pad=0.25", fc="black", alpha=0.75, lw=0))
    ax.set_title(title, fontsize=10)
    ax.set_xlabel("x [px]")
    ax.set_ylabel("y [px]")
    fig.colorbar(im, ax=ax, label="Temperatur [°C]", fraction=0.035, pad=0.02)
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Jenoptik VARIOSCAN .IRB nach CSV/PNG/NPY umwandeln")
    ap.add_argument("files", nargs="+")
    ap.add_argument("--csv", action="store_true", help="Temperaturmatrix als CSV")
    ap.add_argument("--csv-en", action="store_true", help="CSV mit Komma und Dezimalpunkt")
    ap.add_argument("--png", action="store_true", help="Thermogramm mit Farbskala als PNG")
    ap.add_argument("--npy", action="store_true", help="Temperaturen als NumPy-Datei")
    ap.add_argument("--raw", action="store_true", help="zusaetzlich Rohwerte als CSV")
    ap.add_argument("--info", action="store_true", help="Kopfdaten ausgeben")
    ap.add_argument("--eps", type=float, default=None, help="Emissionsgrad (Standard: Wert aus Datei)")
    ap.add_argument("--trefl", type=float, default=None, help="reflektierte Temperatur in Grad C (Standard: Umgebung aus Datei)")
    ap.add_argument("--curve", choices=("fit", "linear"), default="fit")
    ap.add_argument("--palette", default="inferno", help="matplotlib-Colormap, z. B. inferno, magma, jet, gray")
    ap.add_argument("--range", default="auto", help="auto, camera oder LO:HI in Grad C")
    ap.add_argument("--out", default=None, help="Zielordner (Standard: neben der Datei)")
    a = ap.parse_args(argv)

    for fn in a.files:
        try:
            img = read_irb(fn)
        except (OSError, ValueError, struct.error) as e:
            print("%s: %s" % (fn, e), file=sys.stderr)
            continue
        m = img.meta
        eps = a.eps if a.eps is not None else (m["emissivity"] if 0 < m["emissivity"] <= 1 else 1.0)
        T = img.temperatures(eps=eps, trefl_C=a.trefl, curve=a.curve)
        c = img.curve(a.curve)
        stamp = m.get("timestamp")
        print("%s  %dx%d  %s  min %.2f  max %.2f  mittel %.2f °C  (eps %.2f, Kennlinie %s)" % (
            os.path.basename(fn), img.width, img.height, stamp.strftime("%d.%m.%Y %H:%M:%S") if stamp else "",
            np.nanmin(T), np.nanmax(T), np.nanmean(T), eps, c.kind))
        if a.info:
            for k, v in m.items():
                if k not in ("blocks",):
                    print("   %-14s %s" % (k, v))
            if img.fit is not None:
                print("   Kennlinie      S = O + R/(exp(B/T)-F): O=%.2f R=%.1f B=%.4f F=%.5f  max. Abw. %.3f K" % (
                    img.fit.O, img.fit.R, img.fit.B, img.fit.F, img.fit.max_err))
        out_dir = a.out or os.path.dirname(os.path.abspath(fn))
        os.makedirs(out_dir, exist_ok=True)
        base = os.path.join(out_dir, os.path.splitext(os.path.basename(fn))[0])
        if a.csv or a.csv_en:
            _save_csv(T, base + "_temperatur.csv", english=a.csv_en)
        if a.raw:
            np.savetxt(base + "_rohwerte.csv", img.raw, fmt="%d", delimiter=";")
        if a.npy:
            np.save(base + "_temperatur.npy", T)
        if a.png:
            if a.range == "auto":
                rng = (float(np.nanmin(T)), float(np.nanmax(T)))
            elif a.range == "camera" and "display_C" in m:
                rng = m["display_C"]
            else:
                lo, hi = (float(x.replace(",", ".")) for x in a.range.split(":"))
                rng = (lo, hi)
            title = "%s   %s   eps %.2f" % (os.path.basename(fn), stamp.strftime("%d.%m.%Y %H:%M:%S") if stamp else "", eps)
            _save_png(img, T, base + ".png", a.palette, rng, title)


if __name__ == "__main__":
    main()
