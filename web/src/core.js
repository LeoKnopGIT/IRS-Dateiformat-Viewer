/* IRB core: parsing, calibration curve, temperature conversion, palettes.
   Format reverse-engineered from Jenoptik VARIOSCAN hr (VS 3021-ST) files,
   block layout per InfraTec IRBIS (see tomsoftware/Irbis-File-Format). */
(function (root) {
  'use strict';

  const KELVIN = 273.15;

  function readStr(u8, o, n) {
    let s = '';
    for (let i = 0; i < n && o + i < u8.length; i++) {
      const c = u8[o + i];
      if (!c) break;
      s += String.fromCharCode(c);
    }
    return s.trim();
  }

  // OLE automation date (days since 1899-12-30) -> {date: Date (UTC fields = camera local time)}
  function oleToDate(days) {
    if (!isFinite(days) || days < 1 || days > 2958465) return null;
    return new Date(Date.UTC(1899, 11, 30) + Math.round(days * 86400000));
  }

  function parseIRB(buffer) {
    const u8 = new Uint8Array(buffer);
    const dv = new DataView(buffer);
    if (u8.length < 0x60 || u8[0] !== 0xFF || readStr(u8, 1, 3) !== 'IRB') {
      throw new Error('Die Datei beginnt nicht mit der IRB-Kennung (FF 49 52 42).');
    }
    const fileType = readStr(u8, 5, 16);
    const blockOffset = dv.getInt32(0x19, true);
    const blockCount = dv.getInt32(0x1D, true);
    if (blockOffset <= 0 || blockCount < 1 || blockCount > 4096 || blockOffset + 32 * blockCount > u8.length) {
      throw new Error('Das Blockverzeichnis im Dateikopf ist unlesbar.');
    }
    const blocks = [];
    for (let i = 0; i < blockCount; i++) {
      const o = blockOffset + 32 * i;
      blocks.push({
        type: dv.getInt32(o, true),
        frame: dv.getInt32(o + 8, true),
        offset: dv.getInt32(o + 12, true),
        size: dv.getInt32(o + 16, true)
      });
    }
    const img = blocks.find(b => b.type === 1);
    if (!img) throw new Error('Kein Bildblock (Typ 1) in der Datei gefunden.');
    const b = img.offset;
    const bpp = dv.getUint16(b, true);
    const compressed = dv.getUint16(b + 2, true);
    const w = dv.getUint16(b + 4, true);
    const h = dv.getUint16(b + 6, true);
    if (compressed !== 0 || bpp !== 2) {
      throw new Error('Diese IRB-Variante (' + (fileType || 'unbekannt') + ', ' + bpp + ' Byte/Pixel, komprimiert=' + compressed + ') wird nicht unterstützt. Gelesen werden unkomprimierte 16-Bit-Dateien der VARIOSCAN-Reihe.');
    }
    const headerLen = img.size - w * h * bpp;
    if (w < 2 || h < 2 || headerLen < 60 || b + img.size > u8.length) {
      throw new Error('Bildgröße und Blockgröße passen nicht zusammen.');
    }
    const dataOff = b + headerLen;
    const raw = new Uint16Array(w * h);
    for (let i = 0; i < raw.length; i++) raw[i] = dv.getUint16(dataOff + 2 * i, true);

    const f32 = o => dv.getFloat32(b + o, true);
    const meta = {
      fileType,
      width: w, height: h,
      dataOffset: dataOff,
      headerLength: headerLen,
      blocks,
      emissivity: f32(24),
      distance: f32(28),
      envK: f32(32),
      pathK: f32(40),
      waveA: f32(48),
      waveB: f32(52)
    };

    // Calibration table: uint16 at +58 = number of points, pairs (raw, Kelvin) from +60
    const nCal = dv.getUint16(b + 58, true);
    const cal = [];
    if (nCal >= 2 && nCal <= 64 && 60 + nCal * 8 <= headerLen) {
      for (let i = 0; i < nCal; i++) cal.push({ s: f32(60 + 8 * i), t: f32(64 + 8 * i) });
    }
    if (cal.length < 2) throw new Error('In der Datei wurde keine Kalibriertabelle gefunden.');
    meta.cal = cal;

    if (headerLen >= 744) {
      meta.calMinK = f32(222);
      meta.calMaxK = f32(226);
      meta.firmware = readStr(u8, b + 230, 22);
      meta.device = readStr(u8, b + 252, 32);
      meta.model = readStr(u8, b + 284, 32);
      meta.serial = readStr(u8, b + 316, 32);
      meta.fovH = f32(412);
      meta.fovV = f32(416);
      meta.rangeName = readStr(u8, b + 500, 40);
      meta.calFile = readStr(u8, b + 580, 16);
      meta.calVersion = readStr(u8, b + 596, 4);
      meta.calDate = readStr(u8, b + 600, 24);
      meta.levelK = f32(632);
      meta.span = f32(636);
      meta.ole = dv.getFloat64(b + 640, true);
      meta.date = oleToDate(meta.ole);
      meta.manufacturer = readStr(u8, b + 654, 48);
    }

    // Preview block (type 2): 8-bit thumbnail
    const pv = blocks.find(x => x.type === 2);
    if (pv) {
      const pb = pv.offset;
      const pbpp = dv.getUint16(pb, true), pw = dv.getUint16(pb + 4, true), ph = dv.getUint16(pb + 6, true);
      meta.preview = { width: pw, height: ph, bpp: pbpp };
    }
    return { meta, raw, w, h };
  }

  /* ---------- calibration curve ---------- */
  // Model (Planck-like, Sakuma–Hattori form): S = O + R / (exp(B/T) - F)
  function nelderMead(f, x0, step, iters) {
    const n = x0.length;
    let P = [x0.slice()];
    for (let i = 0; i < n; i++) { const p = x0.slice(); p[i] += step[i]; P.push(p); }
    let V = P.map(f);
    for (let k = 0; k < iters; k++) {
      const order = P.map((_, i) => i).sort((a, c) => V[a] - V[c]);
      P = order.map(i => P[i]); V = order.map(i => V[i]);
      let size = 0;
      for (let i = 1; i <= n; i++) for (let j = 0; j < n; j++) size = Math.max(size, Math.abs(P[i][j] - P[0][j]) / (Math.abs(P[0][j]) + 1e-9));
      if (size < 1e-11) break;
      const c = new Array(n).fill(0);
      for (let i = 0; i < n; i++) for (let j = 0; j < n; j++) c[j] += P[i][j] / n;
      const worst = P[n];
      const xr = c.map((cj, j) => cj + (cj - worst[j])); const fr = f(xr);
      if (fr < V[0]) {
        const xe = c.map((cj, j) => cj + 2 * (cj - worst[j])); const fe = f(xe);
        if (fe < fr) { P[n] = xe; V[n] = fe; } else { P[n] = xr; V[n] = fr; }
      } else if (fr < V[n - 1]) {
        P[n] = xr; V[n] = fr;
      } else {
        const xc = fr < V[n] ? c.map((cj, j) => cj + 0.5 * (xr[j] - cj)) : c.map((cj, j) => cj + 0.5 * (worst[j] - cj));
        const fc = f(xc);
        if (fc < Math.min(fr, V[n])) { P[n] = xc; V[n] = fc; }
        else {
          for (let i = 1; i <= n; i++) { P[i] = P[i].map((v, j) => P[0][j] + 0.5 * (v - P[0][j])); V[i] = f(P[i]); }
        }
      }
    }
    return { x: P[0], fx: V[0] };
  }

  function fitPlanck(cal) {
    const pts = cal.filter(p => p.t > 20 && isFinite(p.s));
    if (pts.length < 4) return null;
    const S = pts.map(p => p.s), T = pts.map(p => p.t), n = pts.length;
    function solve(B, F) {
      const g = new Array(n);
      for (let i = 0; i < n; i++) {
        const d = Math.exp(B / T[i]) - F;
        if (!(d > 0) || !isFinite(d)) return null;
        g[i] = 1 / d;
      }
      let sg = 0, sgg = 0, ss = 0, sgs = 0;
      for (let i = 0; i < n; i++) { sg += g[i]; sgg += g[i] * g[i]; ss += S[i]; sgs += g[i] * S[i]; }
      const den = n * sgg - sg * sg;
      if (!(Math.abs(den) > 0)) return null;
      const R = (n * sgs - sg * ss) / den;
      const O = (ss - R * sg) / n;
      if (!(R > 0)) return null;
      let sse = 0;
      for (let i = 0; i < n; i++) { const e = S[i] - O - R * g[i]; sse += e * e; }
      return { R, O, sse };
    }
    const obj = x => { const r = solve(x[0], x[1]); return r ? r.sse : Infinity; };
    let best = null;
    const starts = [[1400, 0], [1500, -10], [1300, 0.5], [1800, -20]];
    for (const s of starts) {
      let r = nelderMead(obj, s, [150, 2], 4000);
      for (let k = 0; k < 3; k++) r = nelderMead(obj, r.x, [Math.abs(r.x[0]) * 0.02 + 1, Math.abs(r.x[1]) * 0.05 + 0.1], 4000);
      if (!best || r.fx < best.fx) best = r;
    }
    if (!best || !isFinite(best.fx)) return null;
    const [B, F] = best.x;
    const { R, O } = solve(B, F);
    const curve = { kind: 'fit', B, F, R, O };
    curve.U = tK => R / (Math.exp(B / tK) - F);
    curve.T = u => { const a = R / u + F; return u > 0 && a > 1 ? B / Math.log(a) : NaN; };
    curve.residuals = pts.map(p => ({ s: p.s, t: p.t, fit: curve.T(p.s - O) }));
    curve.maxErr = Math.max(...curve.residuals.map(r => Math.abs(r.fit - r.t)));
    return curve;
  }

  function interp(x, xs, ys) {
    const n = xs.length;
    let i = 1;
    if (x <= xs[0]) i = 1;
    else if (x >= xs[n - 1]) i = n - 1;
    else while (i < n - 1 && xs[i] < x) i++;
    const x0 = xs[i - 1], x1 = xs[i];
    return ys[i - 1] + (ys[i] - ys[i - 1]) * (x - x0) / (x1 - x0);
  }

  function linearCurve(cal) {
    const pts = cal.slice().filter(p => isFinite(p.s) && isFinite(p.t)).sort((a, c) => a.s - c.s);
    const S = pts.map(p => p.s), T = pts.map(p => p.t);
    const anchor = pts.find(p => p.t < 1);
    const O = anchor ? anchor.s : S[0] - (S[1] - S[0]) * T[0] / (T[1] - T[0]);
    const curve = { kind: 'linear', O };
    curve.U = tK => interp(tK, T, S) - O;
    curve.T = u => { const t = interp(u + O, S, T); return t > 0 ? t : NaN; };
    curve.residuals = pts.filter(p => p.t > 20).map(p => ({ s: p.s, t: p.t, fit: p.t }));
    curve.maxErr = 0;
    return curve;
  }

  function buildCurves(cal) {
    const lin = linearCurve(cal);
    const fit = fitPlanck(cal);
    return { fit: fit && fit.maxErr < 0.25 ? fit : null, linear: lin };
  }

  // Temperatures in °C. eps: emissivity, treflC: reflected temperature (°C)
  function computeTemps(raw, curve, eps, treflC) {
    const out = new Float32Array(raw.length);
    const e = Math.min(1, Math.max(0.01, eps || 1));
    const uRefl = e < 1 ? curve.U(treflC + KELVIN) : 0;
    const O = curve.O;
    for (let i = 0; i < raw.length; i++) {
      const u = raw[i] - O;
      const uo = e < 1 ? (u - (1 - e) * uRefl) / e : u;
      out[i] = curve.T(uo) - KELVIN;
    }
    return out;
  }

  function stats(temps, w) {
    let min = Infinity, max = -Infinity, iMin = -1, iMax = -1, sum = 0, n = 0;
    for (let i = 0; i < temps.length; i++) {
      const v = temps[i];
      if (!isFinite(v)) continue;
      if (v < min) { min = v; iMin = i; }
      if (v > max) { max = v; iMax = i; }
      sum += v; n++;
    }
    return {
      min, max, mean: n ? sum / n : NaN, valid: n,
      minX: iMin % w, minY: Math.floor(iMin / w),
      maxX: iMax % w, maxY: Math.floor(iMax / w)
    };
  }

  /* ---------- palettes ---------- */
  const PALETTES = {
    eisen: { name: 'Eisen', stops: [[0, '#000008'], [0.12, '#1d0b5c'], [0.26, '#650a9e'], [0.40, '#ae1281'], [0.52, '#d8353d'], [0.64, '#ef690d'], [0.78, '#f9a706'], [0.90, '#fde149'], [1, '#fffff2']] },
    regenbogen: { name: 'Regenbogen', stops: [[0, '#14004a'], [0.14, '#2428d6'], [0.30, '#009dff'], [0.45, '#00cf86'], [0.60, '#93e000'], [0.73, '#ffe100'], [0.87, '#ff6200'], [1, '#e8002f']] },
    inferno: { name: 'Inferno', stops: [[0, '#000004'], [0.1, '#160b39'], [0.2, '#420a68'], [0.3, '#6a176e'], [0.4, '#932667'], [0.5, '#bc3754'], [0.6, '#dd513a'], [0.7, '#f37819'], [0.8, '#fca50a'], [0.9, '#f6d746'], [1, '#fcffa4']] },
    viridis: { name: 'Viridis', stops: [[0, '#440154'], [1 / 9, '#482878'], [2 / 9, '#3e4989'], [3 / 9, '#31688e'], [4 / 9, '#26828e'], [5 / 9, '#1f9e89'], [6 / 9, '#35b779'], [7 / 9, '#6ece58'], [8 / 9, '#b5de2b'], [1, '#fde725']] },
    kaltwarm: { name: 'Kalt–Warm', stops: [[0, '#3b4cc0'], [0.25, '#7b9ff9'], [0.5, '#dddcdc'], [0.75, '#f49a7b'], [1, '#b40426']] },
    arktis: { name: 'Arktis', stops: [[0, '#0a0a3c'], [0.30, '#1e46a0'], [0.55, '#5aa0dc'], [0.70, '#d2e6f0'], [0.84, '#f0b450'], [1, '#fff08c']] },
    grau: { name: 'Weiß-heiß', stops: [[0, '#000000'], [1, '#ffffff']] },
    grauinv: { name: 'Schwarz-heiß', stops: [[0, '#ffffff'], [1, '#000000']] }
  };

  function hexToRgb(hx) { const v = parseInt(hx.slice(1), 16); return [(v >> 16) & 255, (v >> 8) & 255, v & 255]; }
  const lutCache = {};
  function paletteLUT(key) {
    if (lutCache[key]) return lutCache[key];
    const st = PALETTES[key].stops.map(([p, c]) => [p, hexToRgb(c)]);
    const lut = new Uint8ClampedArray(256 * 3);
    for (let i = 0; i < 256; i++) {
      const x = i / 255;
      let j = 1; while (j < st.length - 1 && st[j][0] < x) j++;
      const [p0, c0] = st[j - 1], [p1, c1] = st[j];
      const f = p1 > p0 ? Math.min(1, Math.max(0, (x - p0) / (p1 - p0))) : 0;
      for (let k = 0; k < 3; k++) lut[i * 3 + k] = Math.round(c0[k] + (c1[k] - c0[k]) * f);
    }
    return (lutCache[key] = lut);
  }
  function paletteCSS(key) {
    return 'linear-gradient(90deg,' + PALETTES[key].stops.map(([p, c]) => c + ' ' + (p * 100).toFixed(1) + '%').join(',') + ')';
  }

  // Map temps to RGBA
  function colorize(temps, lo, hi, key, steps, out) {
    const lut = paletteLUT(key);
    const span = hi - lo || 1;
    for (let i = 0; i < temps.length; i++) {
      const v = temps[i];
      let r, g, b;
      if (!isFinite(v)) { r = 255; g = 0; b = 255; }
      else {
        let k = (v - lo) / span;
        k = k < 0 ? 0 : k > 1 ? 1 : k;
        if (steps > 1) k = Math.min(steps - 1, Math.floor(k * steps)) / (steps - 1);
        const idx = Math.round(k * 255) * 3;
        r = lut[idx]; g = lut[idx + 1]; b = lut[idx + 2];
      }
      const o = i * 4;
      out[o] = r; out[o + 1] = g; out[o + 2] = b; out[o + 3] = 255;
    }
    return out;
  }

  function niceStep(range, target) {
    const raw = range / Math.max(1, target);
    const mag = Math.pow(10, Math.floor(Math.log10(raw)));
    const n = raw / mag;
    const m = n < 1.5 ? 1 : n < 3.5 ? 2 : n < 7.5 ? 5 : 10;
    return m * mag;
  }

  root.IRB = { KELVIN, parseIRB, buildCurves, computeTemps, stats, PALETTES, paletteLUT, paletteCSS, colorize, niceStep };
})(typeof window !== 'undefined' ? window : globalThis);
