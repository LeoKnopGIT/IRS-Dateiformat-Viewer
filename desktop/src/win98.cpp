// win98.cpp - Windows 98 classic look for FLTK. See win98.hpp.
#include "win98.hpp"

#include <FL/Fl_Button.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Tooltip.H>
#include <FL/fl_draw.H>
#include <FL/platform.H>
#include <algorithm>
#include <cmath>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace w98 {

Fl_Color FACE, LIGHT, HILIGHT, SHADOW, DARK, NAVY, NAVY2, INFO_BG, WHITE;
Fl_Boxtype PRESSED_BOX = FL_FREE_BOXTYPE;
Fl_Boxtype WINDOW_BOX = Fl_Boxtype(FL_FREE_BOXTYPE + 1);

int frameWidth() { return 4; }
int captionHeight() { return 22; }

/* ------------------------------------------------------------------ */
/* bevel primitives                                                     */
/* ------------------------------------------------------------------ */
static void edge(int x, int y, int w, int h, Fl_Color tl, Fl_Color br) {
    if (w <= 0 || h <= 0) return;
    fl_color(tl);
    fl_xyline(x, y, x + w - 2);          // top
    fl_yxline(x, y, y + h - 2);          // left
    fl_color(br);
    fl_xyline(x, y + h - 1, x + w - 1);  // bottom
    fl_yxline(x + w - 1, y, y + h - 1);  // right
}
static Fl_Color act(Fl_Color c) { return Fl::draw_box_active() ? c : fl_inactive(c); }

static void raisedFrame(int x, int y, int w, int h, Fl_Color) {
    edge(x, y, w, h, HILIGHT, DARK);
    edge(x + 1, y + 1, w - 2, h - 2, LIGHT, SHADOW);
}
static void raised(int x, int y, int w, int h, Fl_Color c) {
    fl_color(act(c)); fl_rectf(x + 2, y + 2, w - 4, h - 4);
    raisedFrame(x, y, w, h, c);
}
static void sunkenFrame(int x, int y, int w, int h, Fl_Color) {
    edge(x, y, w, h, SHADOW, HILIGHT);
    edge(x + 1, y + 1, w - 2, h - 2, DARK, LIGHT);
}
static void sunken(int x, int y, int w, int h, Fl_Color c) {
    fl_color(act(c)); fl_rectf(x + 2, y + 2, w - 4, h - 4);
    sunkenFrame(x, y, w, h, c);
}
static void pressedBox(int x, int y, int w, int h, Fl_Color c) {
    fl_color(act(c)); fl_rectf(x + 2, y + 2, w - 4, h - 4);
    fl_color(DARK); fl_rect(x, y, w, h);
    fl_color(SHADOW); fl_rect(x + 1, y + 1, w - 2, h - 2);
}
static void thinUpF(int x, int y, int w, int h, Fl_Color) { edge(x, y, w, h, HILIGHT, SHADOW); }
static void thinDownF(int x, int y, int w, int h, Fl_Color) { edge(x, y, w, h, SHADOW, HILIGHT); }
static void thinUp(int x, int y, int w, int h, Fl_Color c) { fl_color(act(c)); fl_rectf(x + 1, y + 1, w - 2, h - 2); thinUpF(x, y, w, h, c); }
static void thinDown(int x, int y, int w, int h, Fl_Color c) { fl_color(act(c)); fl_rectf(x + 1, y + 1, w - 2, h - 2); thinDownF(x, y, w, h, c); }
static void engravedF(int x, int y, int w, int h, Fl_Color) {
    fl_color(SHADOW); fl_rect(x, y, w - 1, h - 1);
    fl_color(HILIGHT); fl_rect(x + 1, y + 1, w - 1, h - 1);
}
static void engraved(int x, int y, int w, int h, Fl_Color c) {
    fl_color(act(c)); fl_rectf(x + 2, y + 2, w - 4, h - 4);
    engravedF(x, y, w, h, c);
}
static void roundDown(int x, int y, int w, int h, Fl_Color c) {
    fl_color(act(c)); fl_pie(x + 1, y + 1, w - 2, h - 2, 0, 360);
    fl_color(SHADOW); fl_arc(x, y, w, h, 45, 225);
    fl_color(HILIGHT); fl_arc(x, y, w, h, 225, 405);
    fl_color(DARK); fl_arc(x + 1, y + 1, w - 2, h - 2, 45, 225);
    fl_color(LIGHT); fl_arc(x + 1, y + 1, w - 2, h - 2, 225, 405);
}
static void windowBox(int x, int y, int w, int h, Fl_Color c) {
    fl_color(c); fl_rectf(x, y, w, h);
    edge(x, y, w, h, LIGHT, DARK);
    edge(x + 1, y + 1, w - 2, h - 2, HILIGHT, SHADOW);
}

void applyTheme() {
    Fl::scheme("base");
    Fl::background(192, 192, 192);
    Fl::background2(255, 255, 255);
    Fl::foreground(0, 0, 0);
    FACE = fl_rgb_color(192, 192, 192);
    LIGHT = fl_rgb_color(223, 223, 223);
    HILIGHT = fl_rgb_color(255, 255, 255);
    WHITE = HILIGHT;
    SHADOW = fl_rgb_color(128, 128, 128);
    DARK = fl_rgb_color(0, 0, 0);
    NAVY = fl_rgb_color(0, 0, 128);
    NAVY2 = fl_rgb_color(16, 132, 208);
    INFO_BG = fl_rgb_color(255, 255, 225);
    Fl::set_color(FL_SELECTION_COLOR, 0, 0, 128);
    Fl::set_color(FL_INACTIVE_COLOR, 128, 128, 128);

    Fl::set_boxtype(FL_UP_BOX, raised, 2, 2, 4, 4);
    Fl::set_boxtype(FL_DOWN_BOX, sunken, 2, 2, 4, 4);
    Fl::set_boxtype(FL_UP_FRAME, raisedFrame, 2, 2, 4, 4);
    Fl::set_boxtype(FL_DOWN_FRAME, sunkenFrame, 2, 2, 4, 4);
    Fl::set_boxtype(FL_THIN_UP_BOX, thinUp, 1, 1, 2, 2);
    Fl::set_boxtype(FL_THIN_DOWN_BOX, thinDown, 1, 1, 2, 2);
    Fl::set_boxtype(FL_THIN_UP_FRAME, thinUpF, 1, 1, 2, 2);
    Fl::set_boxtype(FL_THIN_DOWN_FRAME, thinDownF, 1, 1, 2, 2);
    Fl::set_boxtype(FL_ENGRAVED_FRAME, engravedF, 2, 2, 4, 4);
    Fl::set_boxtype(FL_ENGRAVED_BOX, engraved, 2, 2, 4, 4);
    Fl::set_boxtype(FL_ROUND_DOWN_BOX, roundDown, 2, 2, 4, 4);
    Fl::set_boxtype(PRESSED_BOX, pressedBox, 2, 2, 4, 4);
    Fl::set_boxtype(WINDOW_BOX, windowBox, 4, 4, 8, 8);

#ifdef _WIN32
    // Win98 used "MS Sans Serif"; its TrueType successor ships with every Windows.
    Fl::set_font(FL_HELVETICA, " Microsoft Sans Serif");
    Fl::set_font(FL_HELVETICA_BOLD, "BMicrosoft Sans Serif");
#endif
    FL_NORMAL_SIZE = 12;
    Fl_Tooltip::color(INFO_BG);
    Fl_Tooltip::textcolor(FL_BLACK);
    Fl_Tooltip::size(12);
    Fl_Tooltip::delay(0.6f);
}

void styleButton(Fl_Widget* b) {
    b->box(FL_UP_BOX);
    if (auto* btn = dynamic_cast<Fl_Button*>(b)) btn->down_box(PRESSED_BOX);
    b->color(FACE);
    b->labelsize(12);
}

/* ------------------------------------------------------------------ */
/* GroupBox                                                             */
/* ------------------------------------------------------------------ */
GroupBox::GroupBox(int x, int y, int w, int h, const char* l) : Fl_Group(x, y, w, h, l) {
    align(FL_ALIGN_INSIDE | FL_ALIGN_TOP_LEFT);
    color(FACE);
    labelsize(12);
}

void GroupBox::draw() {
    fl_color(color());
    fl_rectf(x(), y(), w(), h());
    fl_font(labelfont(), labelsize());
    int lh = fl_height();
    int ty = y() + lh / 2;
    engravedF(x(), ty, w(), h() - (ty - y()), color());
    if (label() && *label()) {
        int tw = int(fl_width(label()));
        fl_color(color());
        fl_rectf(x() + 7, y(), tw + 5, lh);
        fl_color(active_r() ? labelcolor() : fl_inactive(labelcolor()));
        fl_draw(label(), x() + 9, y() + lh - fl_descent());
    }
    draw_children();
}

/* ------------------------------------------------------------------ */
/* Combo                                                                */
/* ------------------------------------------------------------------ */
Combo::Combo(int x, int y, int w, int h, const char* l) : Fl_Choice(x, y, w, h, l) {
    box(FL_DOWN_BOX);
    down_box(FL_FLAT_BOX);
    color(FL_BACKGROUND2_COLOR);
    selection_color(FL_SELECTION_COLOR);
    textsize(12);
    labelsize(12);
}

void Combo::draw() {
    int X = x(), Y = y(), W = w(), H = h();
    bool on = active_r();
    draw_box(FL_DOWN_BOX, X, Y, W, H, on ? FL_BACKGROUND2_COLOR : FACE);
    int bw = std::max(12, H - 4);
    int bx = X + W - 2 - bw, by = Y + 2;
    draw_box(FL_UP_BOX, bx, by, bw, H - 4, FACE);
    fl_color(on ? DARK : SHADOW);
    int cx = bx + bw / 2, cy = by + (H - 4) / 2 - 1;
    for (int i = 0; i < 4; ++i) fl_xyline(cx - 3 + i, cy - 1 + i, cx + 3 - i);
    const Fl_Menu_Item* m = mvalue();
    if (m && m->label()) {
        fl_font(textfont(), textsize());
        bool foc = Fl::focus() == this;
        int tw = std::min(int(fl_width(m->label())) + 4, bx - X - 5);
        if (foc && on) { fl_color(NAVY); fl_rectf(X + 3, Y + 3, tw, H - 6); }
        fl_color(foc && on ? HILIGHT : on ? DARK : SHADOW);
        fl_push_clip(X + 3, Y + 2, bx - X - 4, H - 4);
        fl_draw(m->label(), X + 5, Y + 1, bx - X - 6, H - 2, FL_ALIGN_LEFT);
        fl_pop_clip();
        if (foc && on) { fl_color(HILIGHT); fl_line_style(FL_DOT); fl_rect(X + 3, Y + 3, tw, H - 6); fl_line_style(0); }
    }
    draw_label();
}

/* ------------------------------------------------------------------ */
/* Trackbar                                                             */
/* ------------------------------------------------------------------ */
Trackbar::Trackbar(int x, int y, int w, int h, const char* l) : Fl_Valuator(x, y, w, h, l) {
    box(FL_NO_BOX);
    color(FACE);
    labelsize(12);
    align(FL_ALIGN_LEFT);
    when(FL_WHEN_CHANGED);
}

int Trackbar::thumbX() const {
    double r = maximum() > minimum() ? (value() - minimum()) / (maximum() - minimum()) : 0;
    r = std::clamp(r, 0.0, 1.0);
    return trackX0() + int(std::lround(r * (trackX1() - trackX0())));
}

double Trackbar::valueFromX(int mx) const {
    double r = double(mx - trackX0()) / std::max(1, trackX1() - trackX0());
    r = std::clamp(r, 0.0, 1.0);
    return minimum() + r * (maximum() - minimum());
}

void Trackbar::draw() {
    bool on = active_r();
    fl_color(color()); fl_rectf(x(), y(), w(), h());
    int ty = y() + 2;
    // groove
    sunken(trackX0() - 3, ty + 7, trackX1() - trackX0() + 7, 4, FACE);
    // ticks
    int yt = ty + 23;
    fl_color(on ? DARK : SHADOW);
    for (int i = 0; i <= ticks_; ++i) {
        int xx = trackX0() + (trackX1() - trackX0()) * i / std::max(1, ticks_);
        fl_yxline(xx, yt, yt + 3);
    }
    // end labels
    fl_font(FL_HELVETICA, 10);
    fl_color(on ? DARK : SHADOW);
    int ly = yt + 5 + fl_height() - fl_descent();
    if (!l_.empty()) fl_draw(l_.c_str(), trackX0() - 4, ly);
    if (!m_.empty()) fl_draw(m_.c_str(), (trackX0() + trackX1()) / 2 - int(fl_width(m_.c_str())) / 2, ly);
    if (!r_.empty()) fl_draw(r_.c_str(), trackX1() + 4 - int(fl_width(r_.c_str())), ly);
    // pointed thumb
    int tx = thumbX();
    fl_color(FACE);
    fl_begin_complex_polygon();
    fl_vertex(tx - 5, ty); fl_vertex(tx + 5, ty); fl_vertex(tx + 5, ty + 14);
    fl_vertex(tx, ty + 19); fl_vertex(tx - 5, ty + 14);
    fl_end_complex_polygon();
    fl_color(HILIGHT);
    fl_line(tx - 5, ty + 14, tx - 5, ty); fl_line(tx - 5, ty, tx + 4, ty);
    fl_color(LIGHT);
    fl_line(tx - 4, ty + 14, tx - 4, ty + 1); fl_line(tx - 4, ty + 1, tx + 3, ty + 1);
    fl_color(DARK);
    fl_line(tx + 5, ty, tx + 5, ty + 14); fl_line(tx + 5, ty + 14, tx, ty + 19);
    fl_color(SHADOW);
    fl_line(tx + 4, ty + 1, tx + 4, ty + 14); fl_line(tx + 4, ty + 14, tx, ty + 18);
    fl_color(HILIGHT);
    fl_line(tx - 5, ty + 14, tx - 1, ty + 18);
    if (Fl::focus() == this && Fl::visible_focus()) {
        fl_color(DARK); fl_line_style(FL_DOT);
        fl_rect(x(), y(), w(), yt - y() + 4);
        fl_line_style(0);
    }
    draw_label();
}

int Trackbar::handle(int e) {
    switch (e) {
    case FL_PUSH:
        if (Fl::visible_focus()) take_focus();
        handle_push();
        /* fall through */
    case FL_DRAG: {
        double v = round(valueFromX(Fl::event_x()));
        handle_drag(clamp(v));
        return 1;
    }
    case FL_RELEASE:
        handle_release();
        return 1;
    case FL_KEYBOARD: {
        double st = step() > 0 ? step() : (maximum() - minimum()) / 100;
        double v = value();
        switch (Fl::event_key()) {
        case FL_Left: case FL_Down: v -= st; break;
        case FL_Right: case FL_Up: v += st; break;
        case FL_Page_Down: v -= st * 10; break;
        case FL_Page_Up: v += st * 10; break;
        case FL_Home: v = minimum(); break;
        case FL_End: v = maximum(); break;
        default: return 0;
        }
        handle_drag(clamp(round(v)));
        return 1;
    }
    case FL_FOCUS: case FL_UNFOCUS:
        redraw();
        return Fl::visible_focus() ? 1 : 0;
    case FL_ENTER: case FL_LEAVE:
        return 1;
    }
    return Fl_Valuator::handle(e);
}

/* ------------------------------------------------------------------ */
/* Title bar                                                            */
/* ------------------------------------------------------------------ */
class TitleBar : public Fl_Widget {
public:
    explicit TitleBar(Window* w) : Fl_Widget(0, 0, 10, captionHeight()), win_(w) {}
    int handle(int e) override;
protected:
    void draw() override;
private:
    Window* win_;
    int pressed_ = -1, armed_ = -1;
    bool dragging_ = false;
    int dx_ = 0, dy_ = 0;
    int buttons() const { return win_->sizable_ ? 3 : 1; }
    void btnRect(int i, int& X, int& Y, int& W, int& H) const {
        W = 16; H = 14; Y = y() + (h() - H) / 2;
        int r = x() + w() - 2;
        X = i == 0 ? r - W : i == 1 ? r - 2 - 2 * W : r - 2 - 3 * W;
    }
    int btnAt(int mx, int my) const {
        for (int i = 0; i < buttons(); ++i) {
            int X, Y, W, H; btnRect(i, X, Y, W, H);
            if (mx >= X && mx < X + W && my >= Y && my < Y + H) return i;
        }
        return -1;
    }
    void glyph(int i, int X, int Y, bool down);
};

void TitleBar::glyph(int i, int X, int Y, bool down) {
    if (down) { X += 1; Y += 1; }
    fl_color(DARK);
    if (i == 0) {            // close: X
        for (int k = 0; k < 2; ++k) {
            fl_line(X + 4 + k, Y + 3, X + 10 + k, Y + 9);
            fl_line(X + 10 + k, Y + 3, X + 4 + k, Y + 9);
        }
    } else if (i == 1) {     // maximize / restore
        if (win_->maximized_) {
            fl_rect(X + 5, Y + 2, 6, 5); fl_xyline(X + 5, Y + 3, X + 10);
            fl_color(FACE); fl_rectf(X + 3, Y + 5, 6, 5); fl_color(DARK);
            fl_rect(X + 3, Y + 5, 6, 5); fl_xyline(X + 3, Y + 6, X + 8);
        } else {
            fl_rect(X + 3, Y + 2, 9, 9); fl_xyline(X + 3, Y + 3, X + 11);
        }
    } else {                 // minimize
        fl_rectf(X + 4, Y + 9, 6, 2);
    }
}

void TitleBar::draw() {
    // horizontal gradient navy -> light blue
    uchar r0, g0, b0, r1, g1, b1;
    Fl::get_color(NAVY, r0, g0, b0); Fl::get_color(NAVY2, r1, g1, b1);
    int W = std::max(1, w() - 1);
    for (int i = 0; i < w(); ++i) {
        double t = double(i) / W;
        fl_color(uchar(r0 + (r1 - r0) * t), uchar(g0 + (g1 - g0) * t), uchar(b0 + (b1 - b0) * t));
        fl_yxline(x() + i, y(), y() + h() - 1);
    }
    // icon: tiny thermogram
    int ix = x() + 3, iy = y() + (h() - 16) / 2;
    static const uchar bands[6][3] = {{29, 11, 92}, {101, 10, 158}, {216, 53, 61}, {239, 105, 13}, {249, 167, 6}, {253, 225, 73}};
    fl_color(DARK); fl_rect(ix, iy, 16, 16);
    for (int yy = 1; yy < 15; ++yy)
        for (int xx = 1; xx < 15; ++xx) {
            double d = std::hypot(xx - 9.5, yy - 6.5);
            int k = std::clamp(5 - int(d / 1.6), 0, 5);
            fl_color(bands[k][0], bands[k][1], bands[k][2]);
            fl_point(ix + xx, iy + yy);
        }
    // caption text
    int bX, bY, bW, bH; btnRect(buttons() - 1, bX, bY, bW, bH);
    fl_font(FL_HELVETICA_BOLD, 12);
    fl_color(HILIGHT);
    fl_push_clip(x() + 22, y(), bX - x() - 26, h());
    fl_draw(win_->caption_.c_str(), x() + 23, y(), bX - x() - 26, h(), FL_ALIGN_LEFT);
    fl_pop_clip();
    // caption buttons
    for (int i = 0; i < buttons(); ++i) {
        int X, Y, Wb, Hb; btnRect(i, X, Y, Wb, Hb);
        bool down = pressed_ == i && armed_ == i;
        fl_draw_box(down ? PRESSED_BOX : FL_UP_BOX, X, Y, Wb, Hb, FACE);
        glyph(i, X, Y, down);
    }
}

int TitleBar::handle(int e) {
    switch (e) {
    case FL_PUSH: {
        if (Fl::event_button() != 1) return 1;
        int b = btnAt(Fl::event_x(), Fl::event_y());
        if (b >= 0) { pressed_ = armed_ = b; redraw(); return 1; }
        if (Fl::event_clicks() && win_->sizable_) { Fl::event_clicks(0); win_->toggleMaximize(); return 1; }
        if (!win_->native_ && !win_->maximized_) {
            dragging_ = true;
            dx_ = Fl::event_x_root() - win_->x();
            dy_ = Fl::event_y_root() - win_->y();
        }
        return 1;
    }
    case FL_DRAG:
        if (pressed_ >= 0) {
            int a = btnAt(Fl::event_x(), Fl::event_y()) == pressed_ ? pressed_ : -1;
            if (a != armed_) { armed_ = a; redraw(); }
        } else if (dragging_) {
            win_->position(Fl::event_x_root() - dx_, Fl::event_y_root() - dy_);
        }
        return 1;
    case FL_RELEASE: {
        int p = pressed_, a = armed_;
        pressed_ = armed_ = -1; dragging_ = false;
        redraw();
        if (p >= 0 && p == a) {
            if (p == 0) { if (win_->onClose) win_->onClose(); else win_->hide(); }
            else if (p == 1) win_->toggleMaximize();
            else win_->minimize();
        }
        return 1;
    }
    case FL_ENTER: case FL_LEAVE: case FL_MOVE:
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Window                                                               */
/* ------------------------------------------------------------------ */
Window::Window(int w, int h, const char* title, bool resizableFrame)
    : Fl_Double_Window(w, h, title), caption_(title ? title : ""), sizable_(resizableFrame) {
    box(WINDOW_BOX);
    color(FACE);
    border(0);
    bar_ = new TitleBar(this);
    bar_->resize(frameWidth(), frameWidth(), w - 2 * frameWidth(), captionHeight());
    resizable(nullptr);
    if (sizable_) size_range(minW_, minH_);
    else size_range(w, h, w, h);
}

Window::~Window() = default;

void Window::caption(const std::string& s) {
    caption_ = s;
    copy_label(s.c_str());
    bar_->redraw();
}

int Window::cx() const { return native_ ? 0 : frameWidth(); }
int Window::cy() const { return native_ ? 0 : frameWidth() + captionHeight(); }
int Window::cw() const { return w() - 2 * cx(); }
int Window::ch() const { return h() - cy() - (native_ ? 0 : frameWidth()); }

void Window::resize(int X, int Y, int W, int H) {
    bool sizeChanged = W != w() || H != h();
    Fl_Double_Window::resize(X, Y, W, H);
    if (!native_) {
        int f = frameWidth();
        bar_->resize(f, f, W - 2 * f, captionHeight());
    }
    if (sizeChanged) { layout(); redraw(); }
}

void Window::draw() {
    Fl_Double_Window::draw();
}

int Window::edgeAt(int ex, int ey) const {
    if (native_ || !sizable_ || maximized_) return 0;
    const int t = frameWidth() + 2, c = 16;
    int e = 0;
    if (ex < t) e |= 1; else if (ex >= w() - t) e |= 2;
    if (ey < t) e |= 4; else if (ey >= h() - t) e |= 8;
    if (e & 3) { if (ey < c) e |= 4; else if (ey >= h() - c) e |= 8; }
    if (e & 12) { if (ex < c) e |= 1; else if (ex >= w() - c) e |= 2; }
    // size grip in the bottom-right corner
    if (ex >= w() - 18 && ey >= h() - 18) e = 2 | 8;
    return e;
}

static Fl_Cursor cursorFor(int e) {
    switch (e) {
    case 1: case 2: return FL_CURSOR_WE;
    case 4: case 8: return FL_CURSOR_NS;
    case 1 | 4: case 2 | 8: return FL_CURSOR_NWSE;
    case 2 | 4: case 1 | 8: return FL_CURSOR_NESW;
    }
    return FL_CURSOR_DEFAULT;
}

int Window::handle(int e) {
    if (!native_ && sizable_ && !maximized_) {
        switch (e) {
        case FL_MOVE: {
            int ed = edgeAt(Fl::event_x(), Fl::event_y());
            if (ed) {
                Fl_Cursor c = cursorFor(ed);
                if (c != cur_) { cursor(c); cur_ = c; }
                return 1;
            }
            if (cur_ != FL_CURSOR_DEFAULT) { cursor(FL_CURSOR_DEFAULT); cur_ = FL_CURSOR_DEFAULT; }
            break;
        }
        case FL_PUSH: {
            int ed = edgeAt(Fl::event_x(), Fl::event_y());
            if (ed && Fl::event_button() == 1) {
                edge_ = ed;
                dragMX_ = Fl::event_x_root(); dragMY_ = Fl::event_y_root();
                startX_ = x(); startY_ = y(); startW_ = w(); startH_ = h();
                return 1;
            }
            break;
        }
        case FL_DRAG:
            if (edge_) {
                int dx = Fl::event_x_root() - dragMX_, dy = Fl::event_y_root() - dragMY_;
                int nx = startX_, ny = startY_, nw = startW_, nh = startH_;
                if (edge_ & 1) { nw = std::max(minW_, startW_ - dx); nx = startX_ + startW_ - nw; }
                if (edge_ & 2) nw = std::max(minW_, startW_ + dx);
                if (edge_ & 4) { nh = std::max(minH_, startH_ - dy); ny = startY_ + startH_ - nh; }
                if (edge_ & 8) nh = std::max(minH_, startH_ + dy);
                resize(nx, ny, nw, nh);
                return 1;
            }
            break;
        case FL_RELEASE:
            if (edge_) { edge_ = 0; return 1; }
            break;
        case FL_LEAVE:
            if (!edge_ && cur_ != FL_CURSOR_DEFAULT) { cursor(FL_CURSOR_DEFAULT); cur_ = FL_CURSOR_DEFAULT; }
            break;
        }
    }
    return Fl_Double_Window::handle(e);
}

void Window::toggleMaximize() {
    if (!sizable_) return;
    if (native_) {
        if (maximize_active()) un_maximize(); else maximize();
        return;
    }
    if (!maximized_) {
        normX_ = x(); normY_ = y(); normW_ = w(); normH_ = h();
        int X, Y, W, H;
        Fl::screen_work_area(X, Y, W, H, screen_num());
        maximized_ = true;
        resize(X, Y, W, H);
    } else {
        maximized_ = false;
        resize(normX_, normY_, normW_, normH_);
    }
    cursor(FL_CURSOR_DEFAULT); cur_ = FL_CURSOR_DEFAULT;
    bar_->redraw();
}

void Window::minimize() { iconize(); }

void Window::useSystemFrame(bool on) {
    if (on == native_) return;
    bool wasShown = shown();
    if (maximized_) toggleMaximize();
    int X = x(), Y = y(), W = w(), H = h();
    if (wasShown) hide();
    native_ = on;
    border(on ? 1 : 0);
    box(on ? FL_FLAT_BOX : WINDOW_BOX);
    if (on) bar_->hide(); else bar_->show();
    if (sizable_) size_range(minW_, minH_);
    Fl_Double_Window::resize(X, Y, W, H);
    if (!native_) bar_->resize(frameWidth(), frameWidth(), W - 2 * frameWidth(), captionHeight());
    layout();
    if (wasShown) showWindow();
}

#ifdef _WIN32
// FLTK creates border-less top-level windows as tool windows (no taskbar button, no
// Alt+Tab entry). While our own window is being created we patch its styles so it
// becomes a normal application window. Done before the first ShowWindow, so the
// shell sees the final style.
static HHOOK g_cbtHook = nullptr;
static LRESULT CALLBACK cbtProc(int code, WPARAM wp, LPARAM lp) {
    if (code == HCBT_CREATEWND) {
        auto* cw = reinterpret_cast<CBT_CREATEWNDW*>(lp);
        HWND hw = reinterpret_cast<HWND>(wp);
        if (cw && cw->lpcs && !cw->lpcs->hwndParent && (cw->lpcs->style & WS_POPUP)) {
            cw->lpcs->dwExStyle = (cw->lpcs->dwExStyle & ~DWORD(WS_EX_TOOLWINDOW)) | WS_EX_APPWINDOW;
            cw->lpcs->style |= WS_MINIMIZEBOX | WS_SYSMENU;
            SetWindowLongPtrW(hw, GWL_EXSTYLE, LONG_PTR(cw->lpcs->dwExStyle));
            SetWindowLongPtrW(hw, GWL_STYLE, LONG_PTR(cw->lpcs->style));
        }
    }
    return CallNextHookEx(g_cbtHook, code, wp, lp);
}
#endif

void Window::showWindow() {
#ifdef _WIN32
    if (!native_ && !shown() && !getenv("TV98_NOFIX")) {
        g_cbtHook = SetWindowsHookExW(WH_CBT, cbtProc, nullptr, GetCurrentThreadId());
        show();
        if (g_cbtHook) UnhookWindowsHookEx(g_cbtHook);
        g_cbtHook = nullptr;
        return;
    }
#endif
    show();
}

} // namespace w98
