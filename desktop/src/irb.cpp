// irb.cpp - see irb.hpp. File layout notes are in README.md.
#include "irb.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>

namespace irb {

static const double NaN = std::numeric_limits<double>::quiet_NaN();

/* ---------------- little-endian helpers ---------------- */
static uint16_t u16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
static int32_t i32(const uint8_t* p) { return int32_t(uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24)); }
static float f32(const uint8_t* p) { uint32_t v = uint32_t(i32(p)); float f; std::memcpy(&f, &v, 4); return f; }
static double f64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
    double d; std::memcpy(&d, &v, 8); return d;
}
static std::string str(const uint8_t* p, size_t n) {
    std::string s;
    for (size_t i = 0; i < n && p[i]; ++i) s += char(p[i]);
    size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// days since 1970-01-01 -> y/m/d  (H. Hinnant's civil_from_days)
static void civil(long z, int& y, int& m, int& d) {
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    long doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long yy = yoe + era * 400;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153;
    d = int(doy - (153 * mp + 2) / 5 + 1);
    m = int(mp < 10 ? mp + 3 : mp - 9);
    y = int(yy + (m <= 2));
}

/* ---------------- curves ---------------- */
double Curve::U(double tK) const {
    if (kind == Fit) return R / (std::exp(B / tK) - F);
    // linear: interpolate S(T), extrapolate with end segments
    size_t n = T.size();
    // T is sorted together with S (both ascending)
    size_t i = 1;
    if (tK <= T[0]) i = 1; else if (tK >= T[n - 1]) i = n - 1; else while (i < n - 1 && T[i] < tK) ++i;
    double s = S[i - 1] + (S[i] - S[i - 1]) * (tK - T[i - 1]) / (T[i] - T[i - 1]);
    return s - O;
}

double Curve::TfromU(double u) const {
    if (kind == Fit) {
        if (!(u > 0)) return NaN;
        double a = R / u + F;
        return a > 1 ? B / std::log(a) : NaN;
    }
    double s = u + O;
    size_t n = S.size(), i = 1;
    if (s <= S[0]) i = 1; else if (s >= S[n - 1]) i = n - 1; else while (i < n - 1 && S[i] < s) ++i;
    double t = T[i - 1] + (T[i] - T[i - 1]) * (s - S[i - 1]) / (S[i] - S[i - 1]);
    return t > 0 ? t : NaN;
}

namespace {
struct LS { double O, R, sse; bool ok; };

LS solveLinear(const std::vector<CalPoint>& p, double B, double F) {
    size_t n = p.size();
    double sg = 0, sgg = 0, ss = 0, sgs = 0;
    std::vector<double> g(n);
    for (size_t i = 0; i < n; ++i) {
        double d = std::exp(B / p[i].tK) - F;
        if (!(d > 0) || !std::isfinite(d)) return {0, 0, 0, false};
        g[i] = 1.0 / d;
        sg += g[i]; sgg += g[i] * g[i]; ss += p[i].s; sgs += g[i] * p[i].s;
    }
    double den = n * sgg - sg * sg;
    if (!(std::fabs(den) > 0)) return {0, 0, 0, false};
    double R = (n * sgs - sg * ss) / den, O = (ss - R * sg) / n;
    if (!(R > 0)) return {0, 0, 0, false};
    double sse = 0;
    for (size_t i = 0; i < n; ++i) { double e = p[i].s - O - R * g[i]; sse += e * e; }
    return {O, R, sse, true};
}

// 2-D Nelder-Mead
template <class Fn>
double nelderMead(Fn f, double x[2], const double step[2], int iters) {
    double P[3][2] = {{x[0], x[1]}, {x[0] + step[0], x[1]}, {x[0], x[1] + step[1]}};
    double V[3] = {f(P[0]), f(P[1]), f(P[2])};
    for (int k = 0; k < iters; ++k) {
        int o[3] = {0, 1, 2};
        std::sort(o, o + 3, [&](int a, int b) { return V[a] < V[b]; });
        double Q[3][2], W[3];
        for (int i = 0; i < 3; ++i) { Q[i][0] = P[o[i]][0]; Q[i][1] = P[o[i]][1]; W[i] = V[o[i]]; }
        std::memcpy(P, Q, sizeof P); std::memcpy(V, W, sizeof V);
        double size = 0;
        for (int i = 1; i < 3; ++i) for (int j = 0; j < 2; ++j)
            size = std::max(size, std::fabs(P[i][j] - P[0][j]) / (std::fabs(P[0][j]) + 1e-9));
        if (size < 1e-11) break;
        double c[2] = {(P[0][0] + P[1][0]) / 2, (P[0][1] + P[1][1]) / 2};
        double xr[2] = {2 * c[0] - P[2][0], 2 * c[1] - P[2][1]}, fr = f(xr);
        if (fr < V[0]) {
            double xe[2] = {3 * c[0] - 2 * P[2][0], 3 * c[1] - 2 * P[2][1]}, fe = f(xe);
            if (fe < fr) { P[2][0] = xe[0]; P[2][1] = xe[1]; V[2] = fe; }
            else { P[2][0] = xr[0]; P[2][1] = xr[1]; V[2] = fr; }
        } else if (fr < V[1]) {
            P[2][0] = xr[0]; P[2][1] = xr[1]; V[2] = fr;
        } else {
            double xc[2];
            if (fr < V[2]) { xc[0] = c[0] + 0.5 * (xr[0] - c[0]); xc[1] = c[1] + 0.5 * (xr[1] - c[1]); }
            else { xc[0] = c[0] + 0.5 * (P[2][0] - c[0]); xc[1] = c[1] + 0.5 * (P[2][1] - c[1]); }
            double fc = f(xc);
            if (fc < std::min(fr, V[2])) { P[2][0] = xc[0]; P[2][1] = xc[1]; V[2] = fc; }
            else for (int i = 1; i < 3; ++i) {
                P[i][0] = P[0][0] + 0.5 * (P[i][0] - P[0][0]);
                P[i][1] = P[0][1] + 0.5 * (P[i][1] - P[0][1]);
                V[i] = f(P[i]);
            }
        }
    }
    x[0] = P[0][0]; x[1] = P[0][1];
    return V[0];
}
} // namespace

static bool fitPlanck(const std::vector<CalPoint>& cal, Curve& c) {
    std::vector<CalPoint> p;
    for (auto& q : cal) if (q.tK > 20 && std::isfinite(q.s)) p.push_back(q);
    if (p.size() < 4) return false;
    auto obj = [&](const double* x) { LS r = solveLinear(p, x[0], x[1]); return r.ok ? r.sse : INFINITY; };
    double best[2] = {0, 0}, bestF = INFINITY;
    const double starts[4][2] = {{1400, 0}, {1500, -10}, {1300, 0.5}, {1800, -20}};
    for (auto& s : starts) {
        double x[2] = {s[0], s[1]};
        double st[2] = {150, 2};
        double fx = nelderMead(obj, x, st, 4000);
        for (int k = 0; k < 3; ++k) {
            double st2[2] = {std::fabs(x[0]) * 0.02 + 1, std::fabs(x[1]) * 0.05 + 0.1};
            fx = nelderMead(obj, x, st2, 4000);
        }
        if (fx < bestF) { bestF = fx; best[0] = x[0]; best[1] = x[1]; }
    }
    if (!std::isfinite(bestF)) return false;
    LS r = solveLinear(p, best[0], best[1]);
    c.kind = Curve::Fit; c.B = best[0]; c.F = best[1]; c.R = r.R; c.O = r.O;
    c.maxErr = 0;
    for (auto& q : p) c.maxErr = std::max(c.maxErr, std::fabs(c.TfromU(q.s - c.O) - q.tK));
    return c.maxErr < 0.25;
}

static void linearCurve(const std::vector<CalPoint>& cal, Curve& c) {
    std::vector<CalPoint> p = cal;
    std::sort(p.begin(), p.end(), [](const CalPoint& a, const CalPoint& b) { return a.s < b.s; });
    c.kind = Curve::Linear;
    c.S.clear(); c.T.clear();
    for (auto& q : p) { c.S.push_back(q.s); c.T.push_back(q.tK); }
    c.O = c.S[0];
    for (auto& q : p) if (q.tK < 1) { c.O = q.s; break; }
    c.maxErr = 0;
}

/* ---------------- loading ---------------- */
bool Image::load(const std::string& path, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "Datei kann nicht geöffnet werden."; return false; }
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return loadFromMemory(buf.data(), buf.size(), error);
}

bool Image::loadFromMemory(const uint8_t* d, size_t len, std::string& error) {
    Meta m;
    if (len < 0x60 || d[0] != 0xFF || std::memcmp(d + 1, "IRB", 3) != 0) {
        error = "Keine IRB-Datei (Kennung FF 'IRB' fehlt)."; return false;
    }
    m.fileType = str(d + 5, 16);
    int32_t boff = i32(d + 0x19), bcnt = i32(d + 0x1D);
    if (boff <= 0 || bcnt < 1 || bcnt > 4096 || size_t(boff) + 32u * size_t(bcnt) > len) {
        error = "Blockverzeichnis im Dateikopf ist unlesbar."; return false;
    }
    long imgOff = -1, imgSize = 0;
    for (int i = 0; i < bcnt; ++i) {
        const uint8_t* e = d + boff + 32 * i;
        if (i32(e) == 1) { imgOff = i32(e + 12); imgSize = i32(e + 16); break; }
    }
    if (imgOff < 0 || imgSize <= 0 || size_t(imgOff + imgSize) > len) { error = "Kein Bildblock (Typ 1) gefunden."; return false; }
    const uint8_t* b = d + imgOff;
    int bpp = u16(b), comp = u16(b + 2), w = u16(b + 4), h = u16(b + 6);
    if (bpp != 2 || comp != 0) {
        char msg[200];
        std::snprintf(msg, sizeof msg, "Nicht unterstützte IRB-Variante (%s, %d Byte/Pixel, komprimiert=%d).", m.fileType.c_str(), bpp, comp);
        error = msg; return false;
    }
    long hdr = imgSize - long(w) * h * bpp;
    if (w < 2 || h < 2 || hdr < 60) { error = "Bildgröße passt nicht zur Blockgröße."; return false; }
    m.width = w; m.height = h; m.dataOffset = imgOff + hdr;
    m.emissivity = f32(b + 24); m.distance = f32(b + 28); m.envK = f32(b + 32); m.pathK = f32(b + 40);
    int nCal = u16(b + 58);
    if (nCal >= 2 && nCal <= 64 && 60 + nCal * 8 <= hdr)
        for (int i = 0; i < nCal; ++i) m.cal.push_back({f32(b + 60 + 8 * i), f32(b + 64 + 8 * i)});
    if (m.cal.size() < 2) { error = "Keine Kalibriertabelle in der Datei gefunden."; return false; }
    if (hdr >= 744) {
        m.hasExtended = true;
        m.calMinK = f32(b + 222); m.calMaxK = f32(b + 226);
        m.firmware = str(b + 230, 22); m.device = str(b + 252, 32); m.model = str(b + 284, 32);
        m.serial = str(b + 316, 32); m.fovH = f32(b + 412); m.fovV = f32(b + 416);
        m.rangeName = str(b + 500, 40); m.calFile = str(b + 580, 16); m.calDate = str(b + 600, 24);
        m.levelK = f32(b + 632); m.span = f32(b + 636); m.ole = f64(b + 640);
        m.manufacturer = str(b + 654, 48);
        if (std::isfinite(m.ole) && m.ole > 1 && m.ole < 2958465) {
            long long ms = llround(m.ole * 86400000.0);
            long days = long(ms / 86400000LL);
            long long rem = ms - (long long)days * 86400000LL;
            civil(days - 25569, m.year, m.month, m.day);
            m.hour = int(rem / 3600000); m.minute = int(rem / 60000 % 60); m.second = int(rem / 1000 % 60); m.msec = int(rem % 1000);
            m.hasDate = true;
        }
    }
    raw_.resize(size_t(w) * h);
    const uint8_t* px = d + m.dataOffset;
    for (size_t i = 0; i < raw_.size(); ++i) raw_[i] = u16(px + 2 * i);
    meta_ = m;
    linearCurve(m.cal, lin_);
    hasFit_ = fitPlanck(m.cal, fit_);
    std::vector<double> s;
    for (auto& q : m.cal) if (q.tK > 20) s.push_back(q.s);
    std::sort(s.begin(), s.end());
    rawLo_ = s.empty() ? 0 : s.front(); rawHi_ = s.empty() ? 65535 : s.back();
    return true;
}

void Image::computeTemps(bool preferFit, double eps, double treflC, std::vector<float>& out) const {
    const Curve& c = curve(preferFit);
    eps = std::min(1.0, std::max(0.01, eps));
    double uRefl = eps < 1 ? c.U(treflC + KELVIN) : 0;
    out.resize(raw_.size());
    // Fast path: build a LUT over the raw values present (16-bit domain)
    uint16_t lo = 65535, hi = 0;
    for (uint16_t v : raw_) { lo = std::min(lo, v); hi = std::max(hi, v); }
    std::vector<float> lut(size_t(hi) - lo + 1);
    for (size_t r = lo; r <= hi; ++r) {
        double u = double(r) - c.O;
        if (eps < 1) u = (u - (1 - eps) * uRefl) / eps;
        lut[r - lo] = float(c.TfromU(u) - KELVIN);
    }
    for (size_t i = 0; i < raw_.size(); ++i) out[i] = lut[raw_[i] - lo];
}

Stats computeStats(const std::vector<float>& t, int w) {
    Stats s; s.min = INFINITY; s.max = -INFINITY;
    double sum = 0; long iMin = -1, iMax = -1;
    for (size_t i = 0; i < t.size(); ++i) {
        float v = t[i];
        if (!std::isfinite(v)) continue;
        if (v < s.min) { s.min = v; iMin = long(i); }
        if (v > s.max) { s.max = v; iMax = long(i); }
        sum += v; ++s.valid;
    }
    s.mean = s.valid ? float(sum / s.valid) : NAN;
    if (iMin >= 0) { s.minX = int(iMin % w); s.minY = int(iMin / w); }
    if (iMax >= 0) { s.maxX = int(iMax % w); s.maxY = int(iMax / w); }
    return s;
}

/* ---------------- palettes ---------------- */
namespace {
struct Stop { double p; uint32_t c; };
struct PalDef { const char* name; std::vector<Stop> stops; };
const std::vector<PalDef>& defs() {
    static const std::vector<PalDef> d = {
        {"Eisen", {{0, 0x000008}, {0.12, 0x1d0b5c}, {0.26, 0x650a9e}, {0.40, 0xae1281}, {0.52, 0xd8353d}, {0.64, 0xef690d}, {0.78, 0xf9a706}, {0.90, 0xfde149}, {1, 0xfffff2}}},
        {"Regenbogen", {{0, 0x14004a}, {0.14, 0x2428d6}, {0.30, 0x009dff}, {0.45, 0x00cf86}, {0.60, 0x93e000}, {0.73, 0xffe100}, {0.87, 0xff6200}, {1, 0xe8002f}}},
        {"Inferno", {{0, 0x000004}, {0.1, 0x160b39}, {0.2, 0x420a68}, {0.3, 0x6a176e}, {0.4, 0x932667}, {0.5, 0xbc3754}, {0.6, 0xdd513a}, {0.7, 0xf37819}, {0.8, 0xfca50a}, {0.9, 0xf6d746}, {1, 0xfcffa4}}},
        {"Viridis", {{0, 0x440154}, {1 / 9., 0x482878}, {2 / 9., 0x3e4989}, {3 / 9., 0x31688e}, {4 / 9., 0x26828e}, {5 / 9., 0x1f9e89}, {6 / 9., 0x35b779}, {7 / 9., 0x6ece58}, {8 / 9., 0xb5de2b}, {1, 0xfde725}}},
        {"Kalt-Warm", {{0, 0x3b4cc0}, {0.25, 0x7b9ff9}, {0.5, 0xdddcdc}, {0.75, 0xf49a7b}, {1, 0xb40426}}},
        {"Arktis", {{0, 0x0a0a3c}, {0.30, 0x1e46a0}, {0.55, 0x5aa0dc}, {0.70, 0xd2e6f0}, {0.84, 0xf0b450}, {1, 0xfff08c}}},
        {"Weiss-heiss", {{0, 0x000000}, {1, 0xffffff}}},
        {"Schwarz-heiss", {{0, 0xffffff}, {1, 0x000000}}},
    };
    return d;
}
std::vector<Palette>& pals() {
    static std::vector<Palette> p;
    if (p.empty()) {
        for (auto& d : defs()) {
            Palette pal; pal.name = d.name;
            for (int i = 0; i < 256; ++i) {
                double x = i / 255.0; size_t j = 1;
                while (j < d.stops.size() - 1 && d.stops[j].p < x) ++j;
                const Stop &a = d.stops[j - 1], &b = d.stops[j];
                double f = b.p > a.p ? std::clamp((x - a.p) / (b.p - a.p), 0.0, 1.0) : 0;
                for (int k = 0; k < 3; ++k) {
                    int ca = (a.c >> (16 - 8 * k)) & 255, cb = (b.c >> (16 - 8 * k)) & 255;
                    pal.lut[i][k] = uint8_t(std::lround(ca + (cb - ca) * f));
                }
            }
            p.push_back(pal);
        }
    }
    return p;
}
} // namespace

int paletteCount() { return int(pals().size()); }
const Palette& palette(int i) { return pals()[size_t(std::clamp(i, 0, paletteCount() - 1))]; }

void colorize(const float* t, size_t n, float lo, float hi, int pal, int steps, uint8_t* rgb) {
    const Palette& P = palette(pal);
    float span = hi - lo; if (!(span != 0)) span = 1;
    for (size_t i = 0; i < n; ++i) {
        float v = t[i];
        uint8_t* o = rgb + 3 * i;
        if (!std::isfinite(v)) { o[0] = 255; o[1] = 0; o[2] = 255; continue; }
        float k = (v - lo) / span;
        k = k < 0 ? 0 : k > 1 ? 1 : k;
        if (steps > 1) k = float(std::min(steps - 1, int(k * steps))) / float(steps - 1);
        int idx = int(k * 255 + 0.5f);
        o[0] = P.lut[idx][0]; o[1] = P.lut[idx][1]; o[2] = P.lut[idx][2];
    }
}

double niceStep(double range, int target) {
    double raw = range / std::max(1, target);
    double mag = std::pow(10, std::floor(std::log10(raw)));
    double n = raw / mag;
    double m = n < 1.5 ? 1 : n < 3.5 ? 2 : n < 7.5 ? 5 : 10;
    return m * mag;
}

std::string fmtNum(double v, int dec) {
    if (!std::isfinite(v)) return "-";
    char b[64];
    std::snprintf(b, sizeof b, "%.*f", dec, v);
    std::string s = b;
    for (auto& ch : s) if (ch == '.') ch = ',';
    if (s == "-0" || s == "-0,0" || s == "-0,00") s.erase(0, 1);
    return s;
}

} // namespace irb
