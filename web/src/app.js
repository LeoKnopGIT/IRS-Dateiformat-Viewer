(function () {
  'use strict';
  const $ = id => document.getElementById(id);
  const K = IRB.KELVIN;

  const store = {
    get(k, d) { try { const v = localStorage.getItem('irbv.' + k); return v === null ? d : JSON.parse(v); } catch (e) { return d; } },
    set(k, v) { try { localStorage.setItem('irbv.' + k, JSON.stringify(v)); } catch (e) { /* storage unavailable */ } }
  };

  const S = {
    files: [], cur: -1,
    palette: store.get('palette', 'eisen'),
    rangeMode: 'auto', manLo: null, manHi: null,
    steps: store.get('steps', 0),
    showMinMax: store.get('showMinMax', true),
    pixel: store.get('pixel', false),
    curve: 'fit',
    hover: null
  };
  if (!IRB.PALETTES[S.palette]) S.palette = 'eisen';

  const css = name => getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  const TOK = {};
  function readTokens() {
    ['--bg', '--panel', '--fg', '--muted', '--dim', '--line', '--accent', '--hot', '--cold', '--warn', '--font-num', '--font-ui'].forEach(t => { TOK[t] = css(t); });
  }
  readTokens();

  /* ---------- formatting ---------- */
  const fmtNum = (v, d = 1) => isFinite(v) ? v.toFixed(d).replace('.', ',').replace(/^-/, '−') : '–';
  const fmtT = (v, d = 1) => isFinite(v) ? fmtNum(v, d) + ' °C' : '–';
  const pad2 = n => String(n).padStart(2, '0');
  const fmtDay = dt => dt ? pad2(dt.getUTCDate()) + '.' + pad2(dt.getUTCMonth() + 1) + '.' + dt.getUTCFullYear() : '';
  const fmtDate = dt => dt ? fmtDay(dt) + ' ' + pad2(dt.getUTCHours()) + ':' + pad2(dt.getUTCMinutes()) + ':' + pad2(dt.getUTCSeconds()) : '–';
  const groupInt = n => String(n).replace(/\B(?=(\d{3})+(?!\d))/g, ' ');
  const esc = s => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const baseName = n => n.replace(/\.[^.]+$/, '');

  let toastTimer = 0;
  function toast(msg) {
    const t = $('toast');
    t.textContent = msg; t.hidden = false;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => { t.hidden = true; }, 7000);
  }

  /* ---------- files ---------- */
  const cur = () => S.files[S.cur] || null;

  function addFile(buf, name) {
    let p;
    try { p = IRB.parseIRB(buf); }
    catch (e) { toast(name + ': ' + e.message); return false; }
    const m = p.meta;
    const curves = IRB.buildCurves(m.cal);
    const envC = m.envK > 150 && m.envK < 450 ? m.envK - K : 20;
    const eps = m.emissivity > 0.05 && m.emissivity <= 1 ? m.emissivity : 1;
    S.files.push({ name, meta: m, raw: p.raw, w: p.w, h: p.h, curves, eps, trefl: Math.round(envC * 10) / 10, points: [], sel: -1, temps: null, st: null, key: '' });
    return true;
  }

  function curveOf(f) { return S.curve === 'fit' && f.curves.fit ? f.curves.fit : f.curves.linear; }

  function compute(f) {
    const c = curveOf(f);
    const key = c.kind + '|' + f.eps + '|' + f.trefl;
    if (f.key === key) return;
    f.temps = IRB.computeTemps(f.raw, c, f.eps, f.trefl);
    f.st = IRB.stats(f.temps, f.w);
    f.key = key;
    // raw limits of the calibrated range
    const pts = f.meta.cal.filter(p => p.t > 20).sort((a, b) => a.s - b.s);
    f.rawLo = pts[0].s; f.rawHi = pts[pts.length - 1].s;
  }

  function cameraRange(f) {
    const m = f.meta;
    if (m.levelK > 150 && m.levelK < 700 && m.span > 0 && m.span < 1000) return [m.levelK - K - m.span / 2, m.levelK - K + m.span / 2];
    return null;
  }

  function range(f) {
    let lo, hi;
    if (S.rangeMode === 'camera' && cameraRange(f)) [lo, hi] = cameraRange(f);
    else if (S.rangeMode === 'manual' && isFinite(S.manLo) && isFinite(S.manHi)) { lo = Math.min(S.manLo, S.manHi); hi = Math.max(S.manLo, S.manHi); }
    else { lo = f.st.min; hi = f.st.max; }
    if (!(hi - lo > 0.05)) { const c = (lo + hi) / 2; lo = c - 0.05; hi = c + 0.05; }
    return [lo, hi];
  }

  /* ---------- canvases ---------- */
  const stage = $('stage'), imgC = $('img'), ovl = $('ovl'), scaleC = $('scale');
  let imgData = null;

  function renderImage(f, lo, hi) {
    if (imgC.width !== f.w || imgC.height !== f.h) { imgC.width = f.w; imgC.height = f.h; imgData = null; }
    const ctx = imgC.getContext('2d');
    if (!imgData) imgData = ctx.createImageData(f.w, f.h);
    IRB.colorize(f.temps, lo, hi, S.palette, S.steps, imgData.data);
    ctx.putImageData(imgData, 0, 0);
  }

  function roundRect(ctx, x, y, w, h, r) {
    ctx.beginPath();
    ctx.moveTo(x + r, y); ctx.lineTo(x + w - r, y); ctx.quadraticCurveTo(x + w, y, x + w, y + r);
    ctx.lineTo(x + w, y + h - r); ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
    ctx.lineTo(x + r, y + h); ctx.quadraticCurveTo(x, y + h, x, y + h - r);
    ctx.lineTo(x, y + r); ctx.quadraticCurveTo(x, y, x + r, y);
    ctx.closePath();
  }

  function drawLabel(ctx, X, Y, text, dot, W, H, fs) {
    ctx.font = '500 ' + fs + 'px ' + TOK['--font-num'];
    const tw = ctx.measureText(text).width;
    const px = Math.round(fs * 0.5), bh = Math.round(fs * 1.7), dotW = dot ? Math.round(fs * 0.85) : 0;
    const bw = tw + px * 2 + dotW;
    let bx = X + fs * 0.9, by = Y - bh - fs * 0.6;
    if (bx + bw > W - 2) bx = X - fs * 0.9 - bw;
    if (by < 2) by = Y + fs * 0.6;
    bx = Math.max(2, Math.min(W - bw - 2, bx)); by = Math.max(2, Math.min(H - bh - 2, by));
    ctx.fillStyle = 'rgba(11,13,17,0.86)';
    roundRect(ctx, bx, by, bw, bh, 4); ctx.fill();
    ctx.strokeStyle = 'rgba(255,255,255,0.16)'; ctx.lineWidth = 1; ctx.stroke();
    if (dot) { ctx.fillStyle = dot; ctx.beginPath(); ctx.arc(bx + px + fs * 0.28, by + bh / 2, fs * 0.28, 0, Math.PI * 2); ctx.fill(); }
    ctx.fillStyle = TOK['--fg']; ctx.textBaseline = 'middle'; ctx.textAlign = 'left';
    ctx.fillText(text, bx + px + dotW, by + bh / 2 + 0.5);
  }

  function drawCross(ctx, X, Y, color, a) {
    const g = a * 0.35;
    const seg = [[X - a, Y, X - g, Y], [X + g, Y, X + a, Y], [X, Y - a, X, Y - g], [X, Y + g, X, Y + a]];
    ctx.lineCap = 'round';
    ctx.strokeStyle = 'rgba(0,0,0,0.85)'; ctx.lineWidth = Math.max(4, a * 0.42);
    ctx.beginPath(); seg.forEach(s => { ctx.moveTo(s[0], s[1]); ctx.lineTo(s[2], s[3]); }); ctx.stroke();
    ctx.strokeStyle = color; ctx.lineWidth = Math.max(2, a * 0.2);
    ctx.beginPath(); seg.forEach(s => { ctx.moveTo(s[0], s[1]); ctx.lineTo(s[2], s[3]); }); ctx.stroke();
  }

  function drawPoint(ctx, X, Y, selected, r) {
    ctx.lineWidth = 3; ctx.strokeStyle = 'rgba(0,0,0,0.85)';
    ctx.beginPath(); ctx.arc(X, Y, r, 0, Math.PI * 2); ctx.stroke();
    ctx.lineWidth = 1.6; ctx.strokeStyle = selected ? TOK['--accent'] : '#ffffff';
    ctx.beginPath(); ctx.arc(X, Y, r, 0, Math.PI * 2); ctx.stroke();
    ctx.fillStyle = selected ? TOK['--accent'] : '#ffffff';
    ctx.beginPath(); ctx.arc(X, Y, 1.6, 0, Math.PI * 2); ctx.fill();
  }

  // Draws markers in a box of W×H where one image pixel = s units
  function drawMarkers(ctx, f, s, W, H, hover) {
    const fs = Math.max(11, Math.min(14, W / 60));
    const P = (x, y) => [(x + 0.5) * s, (y + 0.5) * s];
    const t = (x, y) => f.temps[y * f.w + x];
    if (hover) {
      const [X, Y] = P(hover.x, hover.y);
      ctx.strokeStyle = 'rgba(255,255,255,0.38)'; ctx.lineWidth = 1; ctx.setLineDash([3, 4]);
      ctx.beginPath(); ctx.moveTo(0, Y); ctx.lineTo(W, Y); ctx.moveTo(X, 0); ctx.lineTo(X, H); ctx.stroke();
      ctx.setLineDash([]);
    }
    if (S.showMinMax && f.st.valid) {
      const a = Math.max(8, Math.min(14, s * 4));
      const [mx, my] = P(f.st.maxX, f.st.maxY), [nx, ny] = P(f.st.minX, f.st.minY);
      drawCross(ctx, nx, ny, TOK['--cold'], a);
      drawCross(ctx, mx, my, TOK['--hot'], a);
      drawLabel(ctx, nx, ny, 'Min ' + fmtT(f.st.min), TOK['--cold'], W, H, fs);
      drawLabel(ctx, mx, my, 'Max ' + fmtT(f.st.max), TOK['--hot'], W, H, fs);
    }
    f.points.forEach((p, i) => {
      const [X, Y] = P(p.x, p.y);
      drawPoint(ctx, X, Y, i === f.sel, Math.max(5, Math.min(8, s * 2.4)));
      drawLabel(ctx, X, Y, 'P' + (i + 1) + ' ' + fmtT(t(p.x, p.y)), null, W, H, fs);
    });
    if (hover) {
      const [X, Y] = P(hover.x, hover.y);
      drawLabel(ctx, X, Y, fmtT(t(hover.x, hover.y)), TOK['--accent'], W, H, fs + 1);
    }
  }

  function drawOverlay() {
    const f = cur();
    const r = stage.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    const w = Math.max(1, Math.round(r.width * dpr)), h = Math.max(1, Math.round(r.height * dpr));
    if (ovl.width !== w || ovl.height !== h) { ovl.width = w; ovl.height = h; }
    const ctx = ovl.getContext('2d');
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.clearRect(0, 0, w, h);
    if (!f) return;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    drawMarkers(ctx, f, r.width / f.w, r.width, r.height, S.hover);
  }

  // Scale bar with histogram, drawn into a box (x0,y0,W,H)
  function drawScaleBox(ctx, f, lo, hi, x0, y0, W, H) {
    const top = y0 + 8, bot = y0 + H - 8, bh = bot - top;
    const histW = Math.round(W * 0.28), barX = x0 + histW + 4, barW = Math.max(10, Math.round(W * 0.16));
    const labX = barX + barW + 8;
    const lut = IRB.paletteLUT(S.palette);
    const steps = S.steps;
    for (let y = 0; y < bh; y++) {
      let k = 1 - (y + 0.5) / bh;
      if (steps > 1) k = Math.min(steps - 1, Math.floor(k * steps)) / (steps - 1);
      const i = Math.round(k * 255) * 3;
      ctx.fillStyle = 'rgb(' + lut[i] + ',' + lut[i + 1] + ',' + lut[i + 2] + ')';
      ctx.fillRect(barX, top + y, barW, 1.2);
    }
    ctx.strokeStyle = TOK['--line']; ctx.lineWidth = 1; ctx.strokeRect(barX - 0.5, top - 0.5, barW + 1, bh + 1);
    // histogram
    const bins = Math.max(24, Math.min(120, Math.round(bh / 4)));
    const hist = new Uint32Array(bins);
    const span = hi - lo;
    for (let i = 0; i < f.temps.length; i++) {
      const v = f.temps[i]; if (!(v >= lo && v <= hi)) continue;
      hist[Math.min(bins - 1, Math.floor((v - lo) / span * bins))]++;
    }
    let hmax = 0; for (let i = 0; i < bins; i++) hmax = Math.max(hmax, hist[i]);
    ctx.fillStyle = TOK['--dim'];
    const binH = bh / bins;
    for (let i = 0; i < bins; i++) {
      if (!hist[i]) continue;
      const len = Math.max(1, Math.sqrt(hist[i] / hmax) * (histW - 2));
      ctx.fillRect(x0 + histW - len, bot - (i + 1) * binH + 0.3, len, Math.max(0.8, binH - 0.6));
    }
    // ticks
    const step = IRB.niceStep(span, Math.max(3, Math.floor(bh / 42)));
    const dec = step >= 1 ? 0 : step >= 0.1 ? 1 : 2;
    const fs = W < 80 ? 11 : 12;
    ctx.font = '400 ' + fs + 'px ' + TOK['--font-num'];
    ctx.textBaseline = 'middle'; ctx.textAlign = 'left';
    for (let t = Math.ceil(lo / step - 1e-9) * step; t <= hi + 1e-9; t += step) {
      const y = bot - (t - lo) / span * bh;
      ctx.strokeStyle = TOK['--muted']; ctx.beginPath(); ctx.moveTo(barX + barW, Math.round(y) + 0.5); ctx.lineTo(barX + barW + 4, Math.round(y) + 0.5); ctx.stroke();
      ctx.fillStyle = TOK['--fg']; ctx.fillText(fmtNum(t, dec), labX, y);
    }
    // min/max pointers
    const tri = (v, color) => {
      if (!(v >= lo && v <= hi)) return;
      const y = bot - (v - lo) / span * bh;
      ctx.fillStyle = 'rgba(0,0,0,0.8)';
      ctx.beginPath(); ctx.moveTo(barX - 1, y); ctx.lineTo(barX - 9, y - 5.5); ctx.lineTo(barX - 9, y + 5.5); ctx.closePath(); ctx.fill();
      ctx.fillStyle = color;
      ctx.beginPath(); ctx.moveTo(barX - 2, y); ctx.lineTo(barX - 8, y - 4); ctx.lineTo(barX - 8, y + 4); ctx.closePath(); ctx.fill();
    };
    tri(f.st.max, TOK['--hot']); tri(f.st.min, TOK['--cold']);
    ctx.fillStyle = TOK['--muted']; ctx.textAlign = 'right'; ctx.font = '400 ' + (fs - 1) + 'px ' + TOK['--font-num'];
    ctx.textBaseline = 'bottom'; ctx.fillText('°C', x0 + W - 2, y0 + H);
  }

  function drawScale() {
    const f = cur();
    const r = stage.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    const W = scaleC.clientWidth, H = Math.round(r.height);
    scaleC.style.height = H + 'px';
    const pw = Math.round(W * dpr), ph = Math.round(H * dpr);
    if (scaleC.width !== pw || scaleC.height !== ph) { scaleC.width = pw; scaleC.height = ph; }
    const ctx = scaleC.getContext('2d');
    ctx.setTransform(1, 0, 0, 1, 0, 0); ctx.clearRect(0, 0, pw, ph);
    if (!f) return;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    const [lo, hi] = range(f);
    drawScaleBox(ctx, f, lo, hi, 0, 0, W, H);
  }

  /* ---------- UI rendering ---------- */
  function renderFiles() {
    const box = $('fileList');
    box.innerHTML = '';
    S.files.forEach((f, i) => {
      const c = document.createElement('div');
      c.className = 'chip'; c.setAttribute('role', 'tab');
      c.setAttribute('aria-current', i === S.cur ? 'true' : 'false');
      c.setAttribute('aria-selected', i === S.cur ? 'true' : 'false');
      c.tabIndex = 0;
      c.innerHTML = '<span class="nm">' + esc(f.name) + '</span>' + (f.meta.date ? '<span class="dt">' + fmtDay(f.meta.date) + '</span>' : '') + '<button type="button" class="x" aria-label="' + esc(f.name) + ' schließen">×</button>';
      c.addEventListener('click', e => {
        if (e.target.closest('.x')) { removeFile(i); return; }
        select(i);
      });
      c.addEventListener('keydown', e => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); select(i); } });
      box.appendChild(c);
    });
  }

  function renderPalettes() {
    const box = $('pal');
    box.innerHTML = '';
    Object.keys(IRB.PALETTES).forEach(k => {
      const b = document.createElement('button');
      b.type = 'button';
      b.setAttribute('aria-pressed', k === S.palette ? 'true' : 'false');
      b.innerHTML = '<i style="background:' + IRB.paletteCSS(k) + '"></i>' + esc(IRB.PALETTES[k].name);
      b.addEventListener('click', () => { S.palette = k; store.set('palette', k); renderPalettes(); render(); });
      box.appendChild(b);
    });
  }

  function setReadout(id, v, sub, warn) {
    $(id).innerHTML = v;
    const s = $(id + 'S'); s.textContent = sub || ''; s.classList.toggle('warn', !!warn);
  }
  const tHTML = v => isFinite(v) ? fmtNum(v) + '<small>°C</small>' : '–';

  function pixelInfo(f, x, y) {
    const i = y * f.w + x, raw = f.raw[i];
    const out = raw < f.rawLo || raw > f.rawHi;
    return { t: f.temps[i], raw, out };
  }

  function renderReadouts() {
    const f = cur();
    if (!f) { ['roCur', 'roMax', 'roMin', 'roMean'].forEach(id => setReadout(id, '–', '')); return; }
    const st = f.st;
    setReadout('roMax', tHTML(st.max), 'x ' + st.maxX + ' · y ' + st.maxY);
    setReadout('roMin', tHTML(st.min), 'x ' + st.minX + ' · y ' + st.minY);
    setReadout('roMean', tHTML(st.mean), 'über ' + groupInt(st.valid) + ' Pixel');
    renderCursor();
  }

  function renderCursor() {
    const f = cur(); if (!f) return;
    let p = S.hover, label = '';
    if (!p && f.sel >= 0) { p = f.points[f.sel]; label = 'P' + (f.sel + 1) + ' · '; }
    if (!p) { setReadout('roCur', '–', 'Maus über das Bild bewegen'); return; }
    const info = pixelInfo(f, p.x, p.y);
    setReadout('roCur', tHTML(info.t), info.out ? label + 'x ' + p.x + ' · y ' + p.y + ' · außerhalb Kalibrierung' : label + 'x ' + p.x + ' · y ' + p.y + ' · Rohwert ' + info.raw, info.out);
  }

  function renderPoints() {
    const f = cur();
    const ul = $('pts');
    ul.innerHTML = '';
    const pts = f ? f.points : [];
    $('ptsEmpty').hidden = pts.length > 0;
    $('clearPts').disabled = !pts.length;
    pts.forEach((p, i) => {
      const li = document.createElement('li');
      li.setAttribute('aria-selected', i === f.sel ? 'true' : 'false');
      li.tabIndex = 0;
      li.innerHTML = '<span class="tag">P' + (i + 1) + '</span><span class="xy">x ' + p.x + ' · y ' + p.y + '</span><span class="tv">' + fmtT(f.temps[p.y * f.w + p.x]) + '</span><button type="button" class="del" aria-label="P' + (i + 1) + ' löschen">×</button>';
      li.addEventListener('click', e => {
        if (e.target.closest('.del')) { f.points.splice(i, 1); f.sel = Math.min(f.sel, f.points.length - 1); }
        else f.sel = i;
        renderPoints(); drawOverlay(); renderCursor();
      });
      ul.appendChild(li);
    });
  }

  function renderControls() {
    const f = cur();
    const cam = f && cameraRange(f);
    document.querySelectorAll('#rangeSeg button').forEach(b => {
      b.setAttribute('aria-pressed', b.dataset.mode === S.rangeMode ? 'true' : 'false');
      if (b.dataset.mode === 'camera') b.disabled = !cam;
    });
    $('rangeNote').textContent = S.rangeMode === 'camera' ? 'wie bei der Aufnahme' : S.rangeMode === 'auto' ? 'Bild-Min bis -Max' : '';
    if (f) {
      const [lo, hi] = range(f);
      if (document.activeElement !== $('lo')) $('lo').value = (Math.round(lo * 10) / 10).toFixed(1);
      if (document.activeElement !== $('hi')) $('hi').value = (Math.round(hi * 10) / 10).toFixed(1);
      $('epsRange').value = f.eps;
      $('epsVal').textContent = fmtNum(f.eps, 2);
      if (document.activeElement !== $('trefl')) $('trefl').value = f.trefl;
      $('trefl').disabled = f.eps >= 1;
      $('curve').querySelector('option[value="fit"]').disabled = !f.curves.fit;
      $('curve').value = f.curves.fit ? S.curve : 'linear';
    }
    $('steps').value = String(S.steps);
    $('showMinMax').checked = S.showMinMax;
    $('pixel').checked = S.pixel;
    stage.classList.toggle('pixel', S.pixel);
    $('expPng').disabled = $('expCsv').disabled = !f;
  }

  function renderMeta() {
    const f = cur();
    const dl = $('meta');
    if (!f) { dl.innerHTML = ''; $('calBody').innerHTML = ''; return; }
    const m = f.meta;
    const rows = [];
    const add = (k, v, warn) => { if (v !== undefined && v !== null && v !== '') rows.push('<dt>' + k + '</dt><dd' + (warn ? ' class="warn"' : '') + '>' + v + '</dd>'); };
    add('Datei', esc(f.name));
    if (m.date) {
      const unset = m.date.getUTCFullYear() <= 2000 && m.date.getUTCMonth() === 0 && m.date.getUTCDate() === 1;
      add('Aufnahme', fmtDate(m.date) + (unset ? '<br>Kamera-Uhr war nicht gestellt' : ''), unset);
    }
    add('Kamera', esc([m.device, m.model].filter(Boolean).join(' · ')));
    add('Hersteller', esc(m.manufacturer || ''));
    add('Seriennummer', esc(m.serial || ''));
    add('Firmware', esc(m.firmware || ''));
    add('Auflösung', f.w + ' × ' + f.h + ' px');
    if (m.fovH > 1 && m.fovH < 180) {
      const ifov = m.fovH / f.w * Math.PI / 180 * 1000;
      add('Sichtfeld', fmtNum(m.fovH, 0) + '° × ' + fmtNum(m.fovV, 0) + '°, IFOV ' + fmtNum(ifov, 2) + ' mrad');
      if (m.distance > 0) add('Pixel am Objekt', '≈ ' + fmtNum(m.distance * ifov, m.distance * ifov < 10 ? 1 : 0) + ' mm bei ' + fmtNum(m.distance, m.distance < 10 ? 1 : 0) + ' m');
    } else if (m.distance > 0) add('Objektabstand', fmtNum(m.distance, 1) + ' m');
    add('Messbereich', esc(m.rangeName || '') + (m.calMinK ? '<br>' + fmtNum(m.calMinK - K) + ' … ' + fmtNum(m.calMaxK - K) + ' °C kalibriert' : ''));
    add('Kalibrierdatei', esc([m.calFile, (m.calDate || '').replace(/\s*\/\s*/g, '/')].filter(Boolean).join(' · ')));
    add('ε in Datei', fmtNum(m.emissivity, 2));
    add('Umgebung in Datei', fmtT(m.envK - K));
    const cam = cameraRange(f);
    if (cam) add('Anzeige bei Aufnahme', fmtNum(cam[0]) + ' … ' + fmtT(cam[1]));
    dl.innerHTML = rows.join('');

    // calibration
    const c = curveOf(f);
    const sorted = m.cal.slice().sort((a, b) => a.s - b.s);
    let html = '<div class="tbl-wrap"><table><thead><tr><th>Rohwert</th><th>Soll °C</th><th>Kennl. °C</th><th>Δ K</th></tr></thead><tbody>';
    sorted.forEach(p => {
      const isAnchor = p.t < 20;
      const tc = isAnchor ? NaN : c.T(p.s - c.O);
      html += '<tr><td>' + fmtNum(p.s, 1) + '</td><td>' + fmtNum(p.t - K, 2) + '</td><td>' + (isAnchor ? 'Nullpunkt' : fmtNum(tc - K, 2)) + '</td><td>' + (isAnchor ? '' : fmtNum(tc - p.t, 3)) + '</td></tr>';
    });
    html += '</tbody></table></div>';
    if (f.curves.fit) {
      const q = f.curves.fit;
      html += '<div class="formula">S = O + R / (exp(B/T) − F)<br>O = ' + fmtNum(q.O, 1) + ', R = ' + groupInt(Math.round(q.R)) + '<br>B = ' + fmtNum(q.B, 2) + ' K, F = ' + fmtNum(q.F, 4) + '</div>';
      const tl = IRB.computeTemps(f.raw, f.curves.linear, f.eps, f.trefl);
      const tf = IRB.computeTemps(f.raw, f.curves.fit, f.eps, f.trefl);
      let dmax = 0; for (let i = 0; i < tl.length; i++) { const d = Math.abs(tl[i] - tf[i]); if (d > dmax) dmax = d; }
      html += '<p class="note">Jede Datei bringt ihre eigenen 6 Stützpunkte mit (die Rohwerte verschieben sich mit dem Kamerazustand). Der Planck-Fit trifft sie auf ' + fmtNum(q.maxErr, 2) + ' K genau. Lineare Interpolation weicht in diesem Bild um bis zu ' + fmtNum(dmax, 2) + ' K ab, weil die Strahlung nicht linear mit der Temperatur wächst.</p>';
    } else {
      html += '<p class="note">Der Planck-Fit war für diese Tabelle nicht stabil, daher wird linear zwischen den Stützpunkten interpoliert.</p>';
    }
    $('calBody').innerHTML = html;
  }

  function render() {
    const f = cur();
    $('empty').hidden = !!f;
    if (!f) {
      imgC.getContext('2d').clearRect(0, 0, imgC.width, imgC.height);
      renderFiles(); renderControls(); renderReadouts(); renderPoints(); renderMeta(); drawOverlay(); drawScale();
      return;
    }
    compute(f);
    stage.style.setProperty('--ar', String(f.w / f.h));
    const [lo, hi] = range(f);
    renderImage(f, lo, hi);
    renderControls(); renderReadouts(); renderPoints(); renderMeta();
    drawOverlay(); drawScale();
  }

  function select(i) {
    S.cur = i; S.hover = null;
    if (S.rangeMode === 'camera' && !cameraRange(cur())) S.rangeMode = 'auto';
    renderFiles(); render();
  }

  function removeFile(i) {
    S.files.splice(i, 1);
    if (S.cur >= S.files.length) S.cur = S.files.length - 1;
    else if (i < S.cur) S.cur--;
    renderFiles(); render();
  }

  /* ---------- interaction: image ---------- */
  function toPx(e) {
    const f = cur(); const r = stage.getBoundingClientRect();
    const x = Math.floor((e.clientX - r.left) / r.width * f.w), y = Math.floor((e.clientY - r.top) / r.height * f.h);
    return { x: Math.max(0, Math.min(f.w - 1, x)), y: Math.max(0, Math.min(f.h - 1, y)) };
  }
  function nearestPoint(e) {
    const f = cur(); const r = stage.getBoundingClientRect(); const s = r.width / f.w;
    let best = -1, bd = 12;
    f.points.forEach((p, i) => {
      const d = Math.hypot(r.left + (p.x + 0.5) * s - e.clientX, r.top + (p.y + 0.5) * s - e.clientY);
      if (d < bd) { bd = d; best = i; }
    });
    return best;
  }

  let down = null;
  stage.addEventListener('pointerdown', e => {
    if (!cur() || e.button > 0) return;
    const f = cur();
    const hit = nearestPoint(e);
    down = { x: e.clientX, y: e.clientY, drag: hit, moved: false };
    if (hit >= 0) { f.sel = hit; renderPoints(); }
    try { stage.setPointerCapture(e.pointerId); } catch (err) { /* ignore */ }
    S.hover = toPx(e); drawOverlay(); renderCursor();
  });
  stage.addEventListener('pointermove', e => {
    const f = cur(); if (!f) return;
    const p = toPx(e);
    if (down) {
      if (Math.hypot(e.clientX - down.x, e.clientY - down.y) > 5) down.moved = true;
      if (down.drag >= 0 && down.moved) { f.points[down.drag] = p; renderPoints(); }
    }
    if (!S.hover || S.hover.x !== p.x || S.hover.y !== p.y) { S.hover = p; drawOverlay(); renderCursor(); }
  });
  stage.addEventListener('pointerup', e => {
    const f = cur(); if (!f || !down) return;
    if (down.drag < 0 && !down.moved) {
      f.points.push(toPx(e)); f.sel = f.points.length - 1;
    }
    down = null;
    if (e.pointerType !== 'mouse') S.hover = null;
    renderPoints(); drawOverlay(); renderCursor();
    stage.focus({ preventScroll: true });
  });
  stage.addEventListener('pointercancel', () => { down = null; S.hover = null; drawOverlay(); renderCursor(); });
  stage.addEventListener('pointerleave', e => { if (e.pointerType === 'mouse' && !down) { S.hover = null; drawOverlay(); renderCursor(); } });

  document.addEventListener('keydown', e => {
    const f = cur(); if (!f || f.sel < 0) return;
    const tag = (document.activeElement && document.activeElement.tagName) || '';
    if (/INPUT|SELECT|TEXTAREA/.test(tag)) return;
    const d = e.shiftKey ? 10 : 1, p = f.points[f.sel];
    let nx = p.x, ny = p.y;
    if (e.key === 'ArrowLeft') nx -= d; else if (e.key === 'ArrowRight') nx += d;
    else if (e.key === 'ArrowUp') ny -= d; else if (e.key === 'ArrowDown') ny += d;
    else if (e.key === 'Delete' || e.key === 'Backspace') { f.points.splice(f.sel, 1); f.sel = Math.min(f.sel, f.points.length - 1); renderPoints(); drawOverlay(); renderCursor(); e.preventDefault(); return; }
    else if (e.key === 'Escape') { f.sel = -1; renderPoints(); drawOverlay(); renderCursor(); return; }
    else return;
    e.preventDefault();
    f.points[f.sel] = { x: Math.max(0, Math.min(f.w - 1, nx)), y: Math.max(0, Math.min(f.h - 1, ny)) };
    renderPoints(); drawOverlay(); renderCursor();
  });

  /* ---------- interaction: controls ---------- */
  document.querySelectorAll('#rangeSeg button').forEach(b => b.addEventListener('click', () => {
    const f = cur(); if (!f) return;
    if (b.dataset.mode === 'manual' && S.rangeMode !== 'manual') {
      const [lo, hi] = range(f); S.manLo = Math.round(lo * 10) / 10; S.manHi = Math.round(hi * 10) / 10;
    }
    S.rangeMode = b.dataset.mode; render();
  }));
  const num = el => parseFloat(String(el.value).replace(',', '.'));
  ['lo', 'hi'].forEach(id => $(id).addEventListener('change', () => {
    const f = cur(); if (!f) return;
    const [lo, hi] = range(f);
    const a = num($('lo')), b = num($('hi'));
    S.manLo = isFinite(a) ? a : lo; S.manHi = isFinite(b) ? b : hi;
    S.rangeMode = 'manual'; render();
  }));
  $('steps').addEventListener('change', () => { S.steps = parseInt($('steps').value, 10) || 0; store.set('steps', S.steps); render(); });
  $('showMinMax').addEventListener('change', () => { S.showMinMax = $('showMinMax').checked; store.set('showMinMax', S.showMinMax); drawOverlay(); });
  $('pixel').addEventListener('change', () => { S.pixel = $('pixel').checked; store.set('pixel', S.pixel); stage.classList.toggle('pixel', S.pixel); });
  $('epsRange').addEventListener('input', () => { const f = cur(); if (!f) return; f.eps = Math.round(parseFloat($('epsRange').value) * 100) / 100; render(); });
  $('trefl').addEventListener('change', () => { const f = cur(); if (!f) return; const v = num($('trefl')); if (isFinite(v)) f.trefl = v; render(); });
  $('curve').addEventListener('change', () => { S.curve = $('curve').value; render(); });
  $('clearPts').addEventListener('click', () => { const f = cur(); if (!f) return; f.points = []; f.sel = -1; renderPoints(); drawOverlay(); renderCursor(); });

  /* ---------- files in ---------- */
  async function readFiles(list) {
    let added = 0;
    for (const file of list) {
      if (!/\.irb$/i.test(file.name)) { toast(file.name + ': keine .IRB-Datei.'); continue; }
      try { if (addFile(await file.arrayBuffer(), file.name)) added++; }
      catch (e) { toast(file.name + ': Datei konnte nicht gelesen werden.'); }
    }
    if (added) { S.cur = S.files.length - 1; renderFiles(); select(S.cur); }
  }
  $('fileInput').addEventListener('change', e => { readFiles(Array.from(e.target.files)); e.target.value = ''; });
  let dragDepth = 0;
  const hasFiles = e => e.dataTransfer && Array.from(e.dataTransfer.types || []).includes('Files');
  window.addEventListener('dragenter', e => { if (!hasFiles(e)) return; e.preventDefault(); dragDepth++; $('drop').hidden = false; });
  window.addEventListener('dragover', e => { if (!hasFiles(e)) return; e.preventDefault(); e.dataTransfer.dropEffect = 'copy'; });
  window.addEventListener('dragleave', e => { if (!hasFiles(e)) return; dragDepth = Math.max(0, dragDepth - 1); if (!dragDepth) $('drop').hidden = true; });
  window.addEventListener('drop', e => { if (!hasFiles(e)) return; e.preventDefault(); dragDepth = 0; $('drop').hidden = true; readFiles(Array.from(e.dataTransfer.files)); });

  /* ---------- export ---------- */
  const dlPromise = (window.claude && typeof window.claude.use === 'function') ? window.claude.use('downloads').catch(() => null) : Promise.resolve(null);
  function status(msg, err) { const s = $('expStatus'); s.textContent = msg; s.classList.toggle('err', !!err); }
  async function saveFile(name, data) {
    const dl = await dlPromise;
    if (dl) {
      try { await dl.save({ filename: name, data }); status('Gespeichert: ' + name); }
      catch (e) { status(e && e.code === 'declined' ? 'Speichern abgebrochen.' : 'Speichern war nicht möglich (' + ((e && (e.code || e.message)) || 'unbekannt') + ').', e && e.code !== 'declined'); }
      return;
    }
    if (!window.claude) {
      const blob = data instanceof Blob ? data : new Blob([data], { type: 'text/csv' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a'); a.href = url; a.download = name; document.body.appendChild(a); a.click(); a.remove();
      setTimeout(() => URL.revokeObjectURL(url), 4000);
      status('Gespeichert: ' + name); return;
    }
    status('Speichern ist in dieser Ansicht nicht verfügbar.', true);
  }

  $('expCsv').addEventListener('click', () => {
    const f = cur(); if (!f) return;
    const lines = [];
    for (let y = 0; y < f.h; y++) {
      const row = new Array(f.w);
      for (let x = 0; x < f.w; x++) { const v = f.temps[y * f.w + x]; row[x] = isFinite(v) ? v.toFixed(2).replace('.', ',') : ''; }
      lines.push(row.join(';'));
    }
    saveFile(baseName(f.name) + '_temperatur.csv', lines.join('\r\n') + '\r\n');
  });

  $('expPng').addEventListener('click', () => {
    const f = cur(); if (!f) return;
    const sc = 2, W = f.w * sc, H = f.h * sc, pad = 20, gap = 14, barW = 92, capH = 46;
    const c = document.createElement('canvas');
    c.width = pad + W + gap + barW + pad; c.height = pad + H + capH;
    const ctx = c.getContext('2d');
    ctx.fillStyle = TOK['--bg']; ctx.fillRect(0, 0, c.width, c.height);
    ctx.imageSmoothingEnabled = !S.pixel; ctx.imageSmoothingQuality = 'high';
    ctx.drawImage(imgC, pad, pad, W, H);
    ctx.save(); ctx.translate(pad, pad); ctx.beginPath(); ctx.rect(0, 0, W, H); ctx.clip();
    drawMarkers(ctx, f, sc, W, H, null); ctx.restore();
    const [lo, hi] = range(f);
    drawScaleBox(ctx, f, lo, hi, pad + W + gap, pad - 8, barW, H + 16);
    ctx.textBaseline = 'alphabetic'; ctx.textAlign = 'left';
    ctx.fillStyle = TOK['--fg']; ctx.font = '500 14px ' + TOK['--font-num'];
    ctx.fillText(f.name + (f.meta.date ? '   ' + fmtDate(f.meta.date) : ''), pad, pad + H + 22);
    ctx.fillStyle = TOK['--muted']; ctx.font = '400 12px ' + TOK['--font-num'];
    ctx.fillText('ε ' + fmtNum(f.eps, 2) + (f.eps < 1 ? ' · T refl ' + fmtT(f.trefl) : '') + ' · Max ' + fmtT(f.st.max) + ' · Min ' + fmtT(f.st.min) + ' · Mittel ' + fmtT(f.st.mean) + ' · ' + IRB.PALETTES[S.palette].name, pad, pad + H + 40);
    c.toBlob(blob => { if (blob) saveFile(baseName(f.name) + '.png', blob); else status('PNG konnte nicht erzeugt werden.', true); }, 'image/png');
  });

  /* ---------- boot ---------- */
  function b64ToBuf(b64) {
    const bin = atob(b64); const u = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) u[i] = bin.charCodeAt(i);
    return u.buffer;
  }
  renderPalettes();
  (window.IRB_SAMPLES || []).forEach(s => addFile(b64ToBuf(s.b64), s.name));
  S.cur = S.files.length ? (window.IRB_SAMPLES_SELECT || 0) : -1;
  if (S.cur >= S.files.length) S.cur = 0;
  renderFiles();
  render();

  let raf = 0;
  const relayout = () => { cancelAnimationFrame(raf); raf = requestAnimationFrame(() => { drawOverlay(); drawScale(); }); };
  if (window.ResizeObserver) new ResizeObserver(relayout).observe(stage); else window.addEventListener('resize', relayout);
  if (document.fonts && document.fonts.ready) document.fonts.ready.then(() => { readTokens(); relayout(); });
})();
