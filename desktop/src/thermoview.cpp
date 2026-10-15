// thermoview.cpp - see thermoview.hpp
#include "thermoview.hpp"
#include "win98.hpp"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>
#include <cstdio>

using irb::fmtNum;

static const Fl_Color HOT = fl_rgb_color(255, 40, 20);
static const Fl_Color COLD = fl_rgb_color(0, 120, 255);

static std::string fmtT(double v) { return fmtNum(v, 1) + " \xC2\xB0" "C"; }

/* ---------------- Doc / ranges ---------------- */
void Doc::compute(bool fit) {
    char k[128];
    std::snprintf(k, sizeof k, "%d|%.4f|%.3f", fit ? 1 : 0, eps, trefl);
    if (key == k) return;
    img.computeTemps(fit, eps, trefl, temps);
    st = irb::computeStats(temps, img.width());
    key = k;
}

bool cameraRange(const irb::Meta& m, float& lo, float& hi) {
    if (m.levelK > 150 && m.levelK < 700 && m.span > 0 && m.span < 1000) {
        lo = float(m.levelK - irb::KELVIN - m.span / 2);
        hi = float(m.levelK - irb::KELVIN + m.span / 2);
        return true;
    }
    return false;
}

void displayRange(const Doc& d, const ViewSettings& s, float& lo, float& hi) {
    bool ok = false;
    if (s.rangeMode == 1) ok = cameraRange(d.img.meta(), lo, hi);
    else if (s.rangeMode == 2 && std::isfinite(s.manLo) && std::isfinite(s.manHi)) {
        lo = float(std::min(s.manLo, s.manHi)); hi = float(std::max(s.manLo, s.manHi)); ok = true;
    }
    if (!ok) { lo = d.st.min; hi = d.st.max; }
    if (!(hi - lo > 0.05f)) { float c = (lo + hi) / 2; lo = c - 0.05f; hi = c + 0.05f; }
}

/* ---------------- widget ---------------- */
ThermoView::ThermoView(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {
    box(FL_NO_BOX);
    color(w98::FACE);
}
ThermoView::~ThermoView() = default;

void ThermoView::computeLayout() {
    const int scaleW = 84, gap = 10;
    int availW = std::max(10, w() - scaleW - gap - 4), availH = std::max(10, h() - 4);
    double ar = doc_ ? double(doc_->img.width()) / doc_->img.height() : 1.5;
    int iw = availW, ih = int(iw / ar);
    if (ih > availH) { ih = availH; iw = int(ih * ar); }
    int total = iw + 4 + gap + scaleW;
    int x0 = x() + (w() - total) / 2;
    img_ = {x0 + 2, y() + 2 + (availH - ih) / 2, iw, ih};
    scale_ = {img_.x + iw + 2 + gap, img_.y - 2, scaleW, ih + 4};
}

void ThermoView::buildBuffer(int pw, int ph, std::vector<uchar>& out) const {
    const Doc& d = *doc_;
    const int sw = d.img.width(), sh = d.img.height();
    float lo, hi; displayRange(d, *set_, lo, hi);
    out.resize(size_t(pw) * ph * 3);
    std::vector<float> row(static_cast<size_t>(pw));
    std::vector<int> x0(static_cast<size_t>(pw)), x1(static_cast<size_t>(pw));
    std::vector<float> wx(static_cast<size_t>(pw));
    for (int x = 0; x < pw; ++x) {
        double fx = (x + 0.5) * sw / pw;
        if (set_->pixel) { x0[x] = std::min(sw - 1, int(fx)); continue; }
        fx -= 0.5;
        fx = std::clamp(fx, 0.0, double(sw - 1));
        x0[x] = int(fx); x1[x] = std::min(x0[x] + 1, sw - 1); wx[x] = float(fx - x0[x]);
    }
    const float* t = d.temps.data();
    for (int y = 0; y < ph; ++y) {
        double fy = (y + 0.5) * sh / ph;
        if (set_->pixel) {
            const float* r = t + size_t(std::min(sh - 1, int(fy))) * sw;
            for (int x = 0; x < pw; ++x) row[x] = r[x0[x]];
        } else {
            fy = std::clamp(fy - 0.5, 0.0, double(sh - 1));
            int y0 = int(fy), y1 = std::min(y0 + 1, sh - 1);
            float wy = float(fy - y0);
            const float* r0 = t + size_t(y0) * sw;
            const float* r1 = t + size_t(y1) * sw;
            for (int x = 0; x < pw; ++x) {
                float a = r0[x0[x]], b = r0[x1[x]], c = r1[x0[x]], e = r1[x1[x]];
                float v = (a * (1 - wx[x]) + b * wx[x]) * (1 - wy) + (c * (1 - wx[x]) + e * wx[x]) * wy;
                row[x] = std::isfinite(v) ? v : a;
            }
        }
        irb::colorize(row.data(), size_t(pw), lo, hi, set_->palette, set_->steps, out.data() + size_t(y) * pw * 3);
    }
}

void ThermoView::drawImage(const Rect& r, bool exact) {
    float s = exact || !window() ? 1.0f : Fl::screen_scale(window()->screen_num());
    int pw = std::max(1, int(std::lround(r.w * s))), ph = std::max(1, int(std::lround(r.h * s)));
    if (exact) {
        std::vector<uchar> tmp;
        buildBuffer(r.w, r.h, tmp);
        fl_draw_image(tmp.data(), r.x, r.y, r.w, r.h, 3);
        return;
    }
    if (dirty_ || pw != bufW_ || ph != bufH_) {
        buildBuffer(pw, ph, buf_);
        bufW_ = pw; bufH_ = ph;
        rgb_.reset(new Fl_RGB_Image(buf_.data(), pw, ph, 3));
        dirty_ = false;
    }
    if (pw == r.w && ph == r.h) fl_draw_image(buf_.data(), r.x, r.y, r.w, r.h, 3);
    else { rgb_->scale(r.w, r.h, 0, 1); rgb_->draw(r.x, r.y); }
}

static void crossAt(int X, int Y, Fl_Color c, int a) {
    int g = std::max(2, a / 3);
    auto segs = [&] {
        fl_line(X - a, Y, X - g, Y); fl_line(X + g, Y, X + a, Y);
        fl_line(X, Y - a, X, Y - g); fl_line(X, Y + g, X, Y + a);
    };
    // Note: on Windows (GDI) a line style sticks to the pen of the current colour,
    // so every style change is reset while that colour is still current.
    fl_color(w98::DARK); fl_line_style(FL_SOLID | FL_CAP_SQUARE, 4); segs(); fl_line_style(0);
    fl_color(c); fl_line_style(FL_SOLID | FL_CAP_SQUARE, 2); segs(); fl_line_style(0);
}

static void tag(int X, int Y, const std::string& txt, Fl_Color mark, int bx0, int by0, int bx1, int by1) {
    fl_font(FL_HELVETICA, 12);
    int tw = int(fl_width(txt.c_str()));
    int bh = fl_height() + 4, mw = mark ? 11 : 0;
    int bw = tw + 8 + mw;
    int bx = X + 9, by = Y - bh - 7;
    if (bx + bw > bx1 - 2) bx = X - 9 - bw;
    if (by < by0 + 2) by = Y + 7;
    bx = std::max(bx0 + 2, std::min(bx1 - bw - 2, bx));
    by = std::max(by0 + 2, std::min(by1 - bh - 2, by));
    fl_color(w98::INFO_BG); fl_rectf(bx, by, bw, bh);
    fl_color(w98::DARK); fl_rect(bx, by, bw, bh);
    if (mark) {
        fl_color(mark); fl_rectf(bx + 4, by + bh / 2 - 3, 7, 7);
        fl_color(w98::DARK); fl_rect(bx + 4, by + bh / 2 - 3, 7, 7);
    }
    fl_color(w98::DARK);
    fl_draw(txt.c_str(), bx + 4 + mw, by + bh - fl_descent() - 2);
}

void ThermoView::drawOverlay(const Rect& r, bool withHover) const {
    const Doc& d = *doc_;
    const double s = double(r.w) / d.img.width();
    auto P = [&](int px, int py, int& X, int& Y) {
        X = r.x + int((px + 0.5) * s); Y = r.y + int((py + 0.5) * s);
    };
    fl_push_clip(r.x, r.y, r.w, r.h);
    int X, Y;
    if (withHover && hoverOn_) {
        P(hx_, hy_, X, Y);
        fl_color(w98::DARK);
        fl_xyline(r.x, Y, r.x + r.w); fl_yxline(X, r.y, r.y + r.h);
        fl_color(w98::WHITE); fl_line_style(FL_DASH, 1);
        fl_xyline(r.x, Y, r.x + r.w); fl_yxline(X, r.y, r.y + r.h);
        fl_line_style(0);
    }
    if (set_->showMinMax && d.st.valid) {
        int a = std::clamp(int(s * 4), 8, 14);
        int nx, ny, mx, my;
        P(d.st.minX, d.st.minY, nx, ny); P(d.st.maxX, d.st.maxY, mx, my);
        crossAt(nx, ny, COLD, a);
        crossAt(mx, my, HOT, a);
        tag(nx, ny, "Min " + fmtT(d.st.min), COLD, r.x, r.y, r.x + r.w, r.y + r.h);
        tag(mx, my, "Max " + fmtT(d.st.max), HOT, r.x, r.y, r.x + r.w, r.y + r.h);
    }
    for (size_t i = 0; i < d.points.size(); ++i) {
        const Point& p = d.points[i];
        P(p.x, p.y, X, Y);
        int rr = std::clamp(int(s * 2.4), 5, 8);
        bool sel = int(i) == d.sel;
        fl_color(w98::DARK); fl_line_style(FL_SOLID, 3); fl_arc(X - rr, Y - rr, 2 * rr, 2 * rr, 0, 360); fl_line_style(0);
        fl_color(sel ? fl_rgb_color(255, 255, 0) : w98::WHITE); fl_arc(X - rr, Y - rr, 2 * rr, 2 * rr, 0, 360);
        fl_rectf(X - 1, Y - 1, 2, 2);
        tag(X, Y, "P" + std::to_string(i + 1) + "  " + fmtT(d.tAt(p.x, p.y)), 0, r.x, r.y, r.x + r.w, r.y + r.h);
    }
    if (withHover && hoverOn_) {
        P(hx_, hy_, X, Y);
        tag(X, Y, fmtT(d.tAt(hx_, hy_)), 0, r.x, r.y, r.x + r.w, r.y + r.h);
    }
    fl_pop_clip();
}

void ThermoView::drawScale(const Rect& r, Fl_Color bg) const {
    const Doc& d = *doc_;
    float lo, hi; displayRange(d, *set_, lo, hi);
    fl_color(bg); fl_rectf(r.x, r.y, r.w, r.h);
    const int top = r.y + 9, bot = r.y + r.h - 9, bh = bot - top;
    const int histW = 24, barX = r.x + histW + 6, barW = 14, labX = barX + barW + 9;
    const irb::Palette& P = irb::palette(set_->palette);
    // colour bar
    fl_draw_box(FL_DOWN_FRAME, barX - 2, top - 2, barW + 4, bh + 4, bg);
    for (int y = 0; y < bh; ++y) {
        double k = 1.0 - (y + 0.5) / bh;
        if (set_->steps > 1) k = double(std::min(set_->steps - 1, int(k * set_->steps))) / (set_->steps - 1);
        int i = int(k * 255 + 0.5);
        fl_color(P.lut[i][0], P.lut[i][1], P.lut[i][2]);
        fl_xyline(barX, top + y, barX + barW - 1);
    }
    // histogram (sqrt-scaled), drawn left of the bar
    int bins = std::clamp(bh / 4, 24, 120);
    std::vector<unsigned> hist(static_cast<size_t>(bins), 0u);
    float span = hi - lo;
    for (float v : d.temps) if (v >= lo && v <= hi) hist[size_t(std::min(bins - 1, int((v - lo) / span * bins)))]++;
    unsigned hmax = *std::max_element(hist.begin(), hist.end());
    fl_color(w98::SHADOW);
    double binH = double(bh) / bins;
    for (int i = 0; i < bins && hmax; ++i) {
        if (!hist[size_t(i)]) continue;
        int len = std::max(1, int(std::sqrt(double(hist[size_t(i)]) / hmax) * (histW - 1)));
        int y0 = int(bot - (i + 1) * binH + 0.5), y1 = int(bot - i * binH + 0.5);
        fl_rectf(barX - 4 - len, y0, len, std::max(1, y1 - y0 - (binH > 3 ? 1 : 0)));
    }
    // ticks and labels
    double step = irb::niceStep(span, std::max(3, bh / 40));
    int dec = step >= 1 ? 0 : step >= 0.1 ? 1 : 2;
    fl_font(FL_HELVETICA, 11);
    fl_color(w98::DARK);
    for (double t = std::ceil(lo / step - 1e-9) * step; t <= hi + 1e-9; t += step) {
        int y = int(bot - (t - lo) / span * bh + 0.5);
        fl_xyline(barX + barW + 2, y, barX + barW + 5);
        fl_draw(fmtNum(t, dec).c_str(), labX, y + fl_height() / 2 - fl_descent());
    }
    // pointers for image min / max
    auto tri = [&](float v, Fl_Color c) {
        if (!(v >= lo && v <= hi)) return;
        int y = int(bot - (v - lo) / span * bh + 0.5);
        fl_color(w98::DARK); fl_polygon(barX - 3, y, barX - 11, y - 5, barX - 11, y + 5);
        fl_color(c); fl_polygon(barX - 4, y, barX - 10, y - 4, barX - 10, y + 4);
    };
    tri(d.st.max, HOT); tri(d.st.min, COLD);
    fl_color(w98::DARK);
    fl_draw("\xC2\xB0" "C", labX, r.y + r.h);
}

void ThermoView::draw() {
    fl_color(color()); fl_rectf(x(), y(), w(), h());
    computeLayout();
    if (!doc_ || doc_->temps.empty()) {
        Rect r = img_;
        fl_draw_box(FL_DOWN_BOX, r.x - 2, r.y - 2, r.w + 4, r.h + 4, w98::SHADOW);
        fl_font(FL_HELVETICA_BOLD, 13);
        fl_color(w98::WHITE);
        fl_draw("Keine Datei geladen", r.x, r.y + r.h / 2 - 24, r.w, 20, FL_ALIGN_CENTER);
        fl_font(FL_HELVETICA, 12);
        fl_draw("Datei > \xC3\x96" "ffnen ... oder .IRB-Dateien hierher ziehen", r.x, r.y + r.h / 2, r.w, 20, FL_ALIGN_CENTER);
        return;
    }
    fl_draw_box(FL_DOWN_FRAME, img_.x - 2, img_.y - 2, img_.w + 4, img_.h + 4, color());
    drawImage(img_, false);
    drawOverlay(img_, true);
    drawScale(scale_, color());
}

void ThermoView::renderExport(int scale, int& outW, int& outH, bool doDraw) {
    const int pad = 16, gap = 14, scaleW = 90, capH = 44;
    int iw = doc_->img.width() * scale, ih = doc_->img.height() * scale;
    outW = pad + iw + gap + scaleW + pad;
    outH = pad + ih + capH;
    if (!doDraw) return;
    fl_color(w98::WHITE); fl_rectf(0, 0, outW, outH);
    Rect r{pad, pad, iw, ih};
    drawImage(r, true);
    drawOverlay(r, false);
    drawScale(Rect{pad + iw + gap, pad - 9, scaleW, ih + 18}, w98::WHITE);
    const irb::Meta& m = doc_->img.meta();
    char line[256];
    fl_font(FL_HELVETICA_BOLD, 13);
    fl_color(w98::DARK);
    if (m.hasDate)
        std::snprintf(line, sizeof line, "%s    %02d.%02d.%04d %02d:%02d:%02d", doc_->name.c_str(), m.day, m.month, m.year, m.hour, m.minute, m.second);
    else
        std::snprintf(line, sizeof line, "%s", doc_->name.c_str());
    fl_draw(line, pad, pad + ih + 20);
    fl_font(FL_HELVETICA, 12);
    std::string info = "\xCE\xB5 " + fmtNum(doc_->eps, 2);
    if (doc_->eps < 1) info += "   T refl " + fmtT(doc_->trefl);
    info += "   Max " + fmtT(doc_->st.max) + "   Min " + fmtT(doc_->st.min) + "   Mittel " + fmtT(doc_->st.mean);
    info += std::string("   Palette ") + irb::palette(set_->palette).name;
    fl_draw(info.c_str(), pad, pad + ih + 37);
}

bool ThermoView::toPixel(int mx, int my, int& px, int& py) const {
    if (!doc_ || !img_.in(mx, my)) return false;
    px = std::clamp(int(double(mx - img_.x) / img_.w * doc_->img.width()), 0, doc_->img.width() - 1);
    py = std::clamp(int(double(my - img_.y) / img_.h * doc_->img.height()), 0, doc_->img.height() - 1);
    return true;
}

int ThermoView::nearestPoint(int mx, int my) const {
    if (!doc_) return -1;
    double s = double(img_.w) / doc_->img.width();
    int best = -1; double bd = 10;
    for (size_t i = 0; i < doc_->points.size(); ++i) {
        double X = img_.x + (doc_->points[i].x + 0.5) * s, Y = img_.y + (doc_->points[i].y + 0.5) * s;
        double dd = std::hypot(X - mx, Y - my);
        if (dd < bd) { bd = dd; best = int(i); }
    }
    return best;
}

int ThermoView::handle(int e) {
    if (!doc_ || doc_->temps.empty()) return Fl_Widget::handle(e);
    int px, py;
    switch (e) {
    case FL_ENTER:
        if (window()) window()->cursor(FL_CURSOR_CROSS);
        return 1;
    case FL_LEAVE:
        if (window()) window()->cursor(FL_CURSOR_DEFAULT);
        if (hoverOn_ && !down_) { hoverOn_ = false; redraw(); if (onHover) onHover(); }
        return 1;
    case FL_MOVE:
    case FL_DRAG: {
        bool in = toPixel(Fl::event_x(), Fl::event_y(), px, py);
        if (e == FL_DRAG && down_) {
            if (std::abs(Fl::event_x() - downX_) + std::abs(Fl::event_y() - downY_) > 4) moved_ = true;
            if (!in) {   // keep dragging clamped to the image
                px = std::clamp(int(double(Fl::event_x() - img_.x) / img_.w * doc_->img.width()), 0, doc_->img.width() - 1);
                py = std::clamp(int(double(Fl::event_y() - img_.y) / img_.h * doc_->img.height()), 0, doc_->img.height() - 1);
                in = true;
            }
            if (dragPt_ >= 0 && moved_) { doc_->points[size_t(dragPt_)] = {px, py}; if (onPoints) onPoints(); }
        }
        if (in != hoverOn_ || (in && (px != hx_ || py != hy_))) {
            hoverOn_ = in; if (in) { hx_ = px; hy_ = py; }
            redraw();
            if (onHover) onHover();
        }
        return 1;
    }
    case FL_PUSH:
        if (Fl::event_button() != 1) return 1;
        if (!toPixel(Fl::event_x(), Fl::event_y(), px, py)) return 1;
        take_focus();
        down_ = true; moved_ = false;
        downX_ = Fl::event_x(); downY_ = Fl::event_y();
        dragPt_ = nearestPoint(Fl::event_x(), Fl::event_y());
        if (dragPt_ >= 0) { doc_->sel = dragPt_; redraw(); if (onPoints) onPoints(); }
        return 1;
    case FL_RELEASE:
        if (down_ && dragPt_ < 0 && !moved_ && toPixel(Fl::event_x(), Fl::event_y(), px, py)) {
            doc_->points.push_back({px, py});
            doc_->sel = int(doc_->points.size()) - 1;
            if (onPoints) onPoints();
        }
        down_ = false; dragPt_ = -1;
        redraw();
        return 1;
    case FL_FOCUS: case FL_UNFOCUS:
        return 1;
    case FL_KEYBOARD: {
        if (doc_->sel < 0 || doc_->sel >= int(doc_->points.size())) return 0;
        int k = Fl::event_key(), st = Fl::event_shift() ? 10 : 1;
        Point& p = doc_->points[size_t(doc_->sel)];
        if (k == FL_Left) p.x -= st; else if (k == FL_Right) p.x += st;
        else if (k == FL_Up) p.y -= st; else if (k == FL_Down) p.y += st;
        else if (k == FL_Delete || k == FL_BackSpace) {
            doc_->points.erase(doc_->points.begin() + doc_->sel);
            doc_->sel = std::min(doc_->sel, int(doc_->points.size()) - 1);
        } else if (k == FL_Escape) doc_->sel = -1;
        else return 0;
        if (!doc_->points.empty() && doc_->sel >= 0) {
            Point& q = doc_->points[size_t(doc_->sel)];
            q.x = std::clamp(q.x, 0, doc_->img.width() - 1);
            q.y = std::clamp(q.y, 0, doc_->img.height() - 1);
        }
        redraw();
        if (onPoints) onPoints();
        return 1;
    }
    }
    return Fl_Widget::handle(e);
}
