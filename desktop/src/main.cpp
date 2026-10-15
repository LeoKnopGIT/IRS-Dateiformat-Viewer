// IRB Thermoviewer 98 - desktop viewer for Jenoptik VARIOSCAN .IRB thermograms.
// FLTK, statically linked; Windows and Linux.
#include "irb.hpp"
#include "thermoview.hpp"
#include "win98.hpp"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Preferences.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Round_Button.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <FL/fl_utf8.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

using irb::fmtNum;
static const char* APP_NAME = "IRB Thermoviewer";
static const char* APP_VERSION = "1.0";

static std::string fmtT(double v) { return fmtNum(v, 1) + " \xC2\xB0" "C"; }
static std::string baseName(const std::string& p) {
    size_t s = p.find_last_of("/\\");
    return s == std::string::npos ? p : p.substr(s + 1);
}
static std::string stem(const std::string& p) {
    std::string b = baseName(p);
    size_t d = b.find_last_of('.');
    return d == std::string::npos ? b : b.substr(0, d);
}
static std::string dirName(const std::string& p) {
    size_t s = p.find_last_of("/\\");
    return s == std::string::npos ? std::string() : p.substr(0, s);
}
static std::string menuEscape(const std::string& s) {
    std::string o;
    for (char c : s) { if (c == '/' || c == '\\' || c == '_') o += '\\'; if (c == '&') o += '&'; o += c; }
    return o;
}
static bool parseNum(const char* s, double& v) {
    std::string t = s ? s : "";
    for (auto& c : t) if (c == ',') c = '.';
    char* end = nullptr;
    v = std::strtod(t.c_str(), &end);
    return end && end != t.c_str() && std::isfinite(v);
}
static bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = fl_fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > (64L << 20)) { std::fclose(f); return false; }
    out.resize(size_t(n));
    bool ok = std::fread(out.data(), 1, size_t(n), f) == size_t(n);
    std::fclose(f);
    return ok;
}
static std::string dateStr(const irb::Meta& m) {
    if (!m.hasDate) return "-";
    char b[64];
    std::snprintf(b, sizeof b, "%02d.%02d.%04d %02d:%02d:%02d", m.day, m.month, m.year, m.hour, m.minute, m.second);
    return b;
}

/* ------------------------------------------------------------------ */
class App;

class MainWindow : public w98::Window {
public:
    MainWindow(App* a, int w, int h) : w98::Window(w, h, APP_NAME, true), app_(a) {}
    void layout() override;
    int handle(int e) override;
private:
    App* app_;
};

/* Status bar size grip (diagonal hatching in the corner) */
class SizeGrip : public Fl_Widget {
public:
    SizeGrip(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}
    void draw() override {
        fl_color(w98::FACE); fl_rectf(x(), y(), w(), h());
        int X = x() + w() - 2, Y = y() + h() - 2;
        for (int i = 0; i < 3; ++i) {
            int o = 3 + i * 4;
            fl_color(w98::WHITE);  fl_line(X - o, Y, X, Y - o);
            fl_color(w98::SHADOW); fl_line(X - o + 1, Y, X, Y - o + 1);
            fl_line(X - o + 2, Y, X, Y - o + 2);
        }
    }
};

class App {
public:
    App();
    void run(int argc, char** argv);

    // actions
    void openDialog();
    bool openFile(const std::string& path, bool report);
    void closeCurrent();
    void select(int i);
    void exportPng();
    bool writePng(const std::string& path);
    void exportCsv();
    void showInfo();
    void showAbout();
    void dropped(const char* text);
    void quit();

    void layout();
    void refreshAll();
    void refreshReadouts();
    void refreshPoints();
    void refreshControls();
    void refreshFiles();
    void recompute();
    void savePrefs();

    MainWindow* win = nullptr;
    Fl_Menu_Bar* menu = nullptr;
    Fl_Box* lblFile = nullptr;
    w98::Combo* cbFile = nullptr;
    Fl_Button *btOpen = nullptr, *btClose = nullptr, *btInfo = nullptr, *btPng = nullptr, *btCsv = nullptr;
    ThermoView* view = nullptr;
    w98::GroupBox* gMeas = nullptr;
    Fl_Box *mName[4] = {}, *mVal[4] = {}, *mSub[4] = {};
    w98::GroupBox* gPts = nullptr;
    Fl_Hold_Browser* lsPts = nullptr;
    Fl_Button *btDelPt = nullptr, *btClrPts = nullptr;
    w98::GroupBox* gSet = nullptr;
    Fl_Box *lPal = nullptr, *lSteps = nullptr, *lRange = nullptr, *lMin = nullptr, *lMax = nullptr, *uMin = nullptr, *uMax = nullptr;
    w98::Combo *cbPal = nullptr, *cbSteps = nullptr, *cbCurve = nullptr;
    Fl_Group* grRange = nullptr;
    Fl_Round_Button* rbRange[3] = {};
    Fl_Input *inMin = nullptr, *inMax = nullptr, *inRefl = nullptr;
    Fl_Check_Button *ckMinMax = nullptr, *ckPixel = nullptr;
    Fl_Box *lEps = nullptr, *vEps = nullptr, *lRefl = nullptr, *uRefl = nullptr, *lCurve = nullptr;
    w98::Trackbar* tbEps = nullptr;
    Fl_Group* status = nullptr;
    Fl_Box* sb[4] = {};
    SizeGrip* grip = nullptr;

    std::vector<std::unique_ptr<Doc>> docs;
    int cur = -1;
    ViewSettings vs;
    std::string lastDir;
    bool nativeFrame = false;

    Doc* doc() { return cur >= 0 && cur < int(docs.size()) ? docs[size_t(cur)].get() : nullptr; }
};

static App* gApp = nullptr;

void MainWindow::layout() { if (app_) app_->layout(); }

int MainWindow::handle(int e) {
    switch (e) {
    case FL_DND_ENTER: case FL_DND_DRAG: case FL_DND_RELEASE: case FL_DND_LEAVE:
        return 1;
    case FL_PASTE:
        app_->dropped(Fl::event_text());
        return 1;
    }
    return w98::Window::handle(e);
}

/* ------------------------------------------------------------------ */
/* construction                                                         */
/* ------------------------------------------------------------------ */
static Fl_Box* mkLabel(const char* t, Fl_Align a = FL_ALIGN_LEFT | FL_ALIGN_INSIDE) {
    Fl_Box* b = new Fl_Box(0, 0, 10, 10, t);
    b->align(a | FL_ALIGN_CLIP);
    b->labelsize(12);
    return b;
}
static Fl_Button* mkButton(const char* t) {
    Fl_Button* b = new Fl_Button(0, 0, 10, 10, t);
    w98::styleButton(b);
    return b;
}
static Fl_Input* mkInput() {
    Fl_Input* i = new Fl_Input(0, 0, 10, 10);
    i->box(FL_DOWN_BOX);
    i->textsize(12);
    i->when(FL_WHEN_ENTER_KEY | FL_WHEN_RELEASE);
    return i;
}

App::App() {
    gApp = this;
    win = new MainWindow(this, 1080, 790);
    win->minSize(920, 660);

    menu = new Fl_Menu_Bar(0, 0, 10, 20);
    menu->box(FL_FLAT_BOX);
    menu->down_box(FL_FLAT_BOX);
    menu->color(w98::FACE);
    menu->selection_color(w98::NAVY);
    menu->textsize(12);

    lblFile = mkLabel("Datei:");
    cbFile = new w98::Combo(0, 0, 10, 10);
    btOpen = mkButton("\xC3\x96" "ffnen...");
    btClose = mkButton("Schlie\xC3\x9F" "en");
    btInfo = mkButton("Dateiinfo...");
    btPng = mkButton("Als PNG speichern...");
    btCsv = mkButton("Als CSV speichern...");

    view = new ThermoView(0, 0, 10, 10);

    gMeas = new w98::GroupBox(0, 0, 10, 10, "Messwerte");
    const char* names[4] = {"Cursor", "Maximum", "Minimum", "Mittelwert"};
    for (int i = 0; i < 4; ++i) {
        mName[i] = mkLabel(names[i]);
        mVal[i] = mkLabel("-", FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
        mVal[i]->labelfont(FL_HELVETICA_BOLD); mVal[i]->labelsize(15);
        mSub[i] = mkLabel("");
        mSub[i]->labelsize(11); mSub[i]->labelcolor(w98::SHADOW);
    }
    mVal[1]->labelcolor(fl_rgb_color(176, 0, 0));
    mVal[2]->labelcolor(fl_rgb_color(0, 64, 176));
    gMeas->end();

    gPts = new w98::GroupBox(0, 0, 10, 10, "Messpunkte");
    lsPts = new Fl_Hold_Browser(0, 0, 10, 10);
    lsPts->box(FL_DOWN_BOX);
    lsPts->color(FL_BACKGROUND2_COLOR);
    lsPts->selection_color(w98::NAVY);
    lsPts->textsize(12);
    lsPts->has_scrollbar(Fl_Browser_::VERTICAL);
    static int cols[] = {34, 86, 0};
    lsPts->column_widths(cols);
    lsPts->format_char(0);
    btDelPt = mkButton("L\xC3\xB6schen");
    btClrPts = mkButton("Alle l\xC3\xB6schen");
    gPts->end();

    gSet = new w98::GroupBox(0, 0, 10, 10, "Einstellungen");
    lPal = mkLabel("Farbpalette");
    cbPal = new w98::Combo(0, 0, 10, 10);
    for (int i = 0; i < irb::paletteCount(); ++i) cbPal->add(irb::palette(i).name);
    lSteps = mkLabel("Farbstufen");
    cbSteps = new w98::Combo(0, 0, 10, 10);
    cbSteps->add("Stufenlos"); cbSteps->add("8 Stufen"); cbSteps->add("16 Stufen"); cbSteps->add("32 Stufen");
    lRange = mkLabel("Bereich");
    grRange = new Fl_Group(0, 0, 10, 10);
    const char* rn[3] = {"Auto", "Kamera", "Manuell"};
    for (int i = 0; i < 3; ++i) {
        rbRange[i] = new Fl_Round_Button(0, 0, 10, 10, rn[i]);
        rbRange[i]->type(FL_RADIO_BUTTON);
        rbRange[i]->labelsize(12);
        rbRange[i]->selection_color(w98::DARK);
        rbRange[i]->tooltip(i == 0 ? "Farbskala von Bild-Minimum bis Bild-Maximum"
                          : i == 1 ? "Farbskala wie bei der Aufnahme an der Kamera eingestellt"
                                   : "Farbskala mit eigenen Grenzen");
    }
    grRange->end();
    lMin = mkLabel("von"); inMin = mkInput(); uMin = mkLabel("\xC2\xB0" "C");
    lMax = mkLabel("bis"); inMax = mkInput(); uMax = mkLabel("\xC2\xB0" "C");
    ckMinMax = new Fl_Check_Button(0, 0, 10, 10, "Min/Max markieren");
    ckMinMax->tooltip("Kreuz am w\xC3\xA4rmsten und k\xC3\xA4ltesten Punkt des Bildes");
    ckPixel = new Fl_Check_Button(0, 0, 10, 10, "Pixel scharf anzeigen");
    ckMinMax->labelsize(12); ckPixel->labelsize(12);

    lEps = mkLabel("Emissionsgrad");
    tbEps = new w98::Trackbar(0, 0, 10, 10);
    tbEps->bounds(0.10, 1.00); tbEps->step(0.01); tbEps->value(1.0);
    tbEps->ticks(9); tbEps->endLabels("0,1", "0,55", "1,0");
    vEps = mkLabel("1,00", FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    vEps->box(FL_DOWN_BOX); vEps->color(FL_BACKGROUND2_COLOR);
    lRefl = mkLabel("Refl. Temperatur"); inRefl = mkInput(); uRefl = mkLabel("\xC2\xB0" "C");
    inRefl->tooltip("Temperatur der Umgebung, die sich im Objekt spiegelt (wirkt nur bei \xCE\xB5 < 1)");
    lCurve = mkLabel("Kennlinie");
    cbCurve = new w98::Combo(0, 0, 10, 10);
    cbCurve->add("Planck-Fit durch St\xC3\xBCtzpunkte");
    cbCurve->add("Linear zwischen St\xC3\xBCtzpunkten");
    gSet->end();

    status = new Fl_Group(0, 0, 10, 10);
    for (int i = 0; i < 4; ++i) {
        sb[i] = new Fl_Box(0, 0, 10, 10);
        sb[i]->box(FL_THIN_DOWN_BOX);
        sb[i]->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
        sb[i]->labelsize(12);
    }
    grip = new SizeGrip(0, 0, 10, 10);
    status->end();
    win->end();

    // --- menu ---
    menu->add("&Datei/\xC3\x96" "&ffnen...", FL_COMMAND + 'o', [](Fl_Widget*, void*) { gApp->openDialog(); });
    menu->add("&Datei/&Schlie\xC3\x9F" "en", FL_COMMAND + 'w', [](Fl_Widget*, void*) { gApp->closeCurrent(); }, nullptr, FL_MENU_DIVIDER);
    menu->add("&Datei/Als &PNG speichern...", FL_COMMAND + 's', [](Fl_Widget*, void*) { gApp->exportPng(); });
    menu->add("&Datei/Als &CSV speichern...", FL_COMMAND + 'e', [](Fl_Widget*, void*) { gApp->exportCsv(); }, nullptr, FL_MENU_DIVIDER);
    menu->add("&Datei/&Beenden", FL_ALT + FL_F + 4, [](Fl_Widget*, void*) { gApp->quit(); });
    for (int i = 0; i < irb::paletteCount(); ++i) {
        std::string p = std::string("&Ansicht/&Farbpalette/") + irb::palette(i).name;
        menu->add(p.c_str(), 0, [](Fl_Widget*, void* d) {
            gApp->vs.palette = int(reinterpret_cast<intptr_t>(d));
            gApp->refreshControls(); gApp->view->invalidate(); gApp->savePrefs();
        }, reinterpret_cast<void*>(intptr_t(i)), FL_MENU_RADIO);
    }
    menu->add("&Ansicht/&Min- und Max-Kreuze", FL_COMMAND + 'm', [](Fl_Widget*, void*) {
        gApp->vs.showMinMax = !gApp->vs.showMinMax; gApp->refreshControls(); gApp->view->redraw(); gApp->savePrefs();
    }, nullptr, FL_MENU_TOGGLE);
    menu->add("&Ansicht/&Pixel scharf", FL_COMMAND + 'p', [](Fl_Widget*, void*) {
        gApp->vs.pixel = !gApp->vs.pixel; gApp->refreshControls(); gApp->view->invalidate(); gApp->savePrefs();
    }, nullptr, FL_MENU_TOGGLE | FL_MENU_DIVIDER);
    menu->add("&Ansicht/&Systemrahmen verwenden", 0, [](Fl_Widget*, void*) {
        gApp->nativeFrame = !gApp->nativeFrame; gApp->win->useSystemFrame(gApp->nativeFrame); gApp->refreshControls(); gApp->savePrefs();
    }, nullptr, FL_MENU_TOGGLE);
    menu->add("&Messpunkte/Gew\xC3\xA4hlten &l\xC3\xB6schen", FL_Delete, [](Fl_Widget*, void*) { gApp->btDelPt->do_callback(); });
    menu->add("&Messpunkte/&Alle l\xC3\xB6schen", 0, [](Fl_Widget*, void*) { gApp->btClrPts->do_callback(); });
    menu->add("&Hilfe/&Dateiinfo...", FL_COMMAND + 'i', [](Fl_Widget*, void*) { gApp->showInfo(); }, nullptr, FL_MENU_DIVIDER);
    menu->add("&Hilfe/\xC3\x9C" "&ber IRB Thermoviewer...", 0, [](Fl_Widget*, void*) { gApp->showAbout(); });

    // --- callbacks ---
    btOpen->callback([](Fl_Widget*, void*) { gApp->openDialog(); });
    btClose->callback([](Fl_Widget*, void*) { gApp->closeCurrent(); });
    btInfo->callback([](Fl_Widget*, void*) { gApp->showInfo(); });
    btPng->callback([](Fl_Widget*, void*) { gApp->exportPng(); });
    btCsv->callback([](Fl_Widget*, void*) { gApp->exportCsv(); });
    cbFile->callback([](Fl_Widget* w, void*) { gApp->select(static_cast<Fl_Choice*>(w)->value()); });
    cbPal->callback([](Fl_Widget* w, void*) {
        gApp->vs.palette = static_cast<Fl_Choice*>(w)->value(); gApp->refreshControls(); gApp->view->invalidate(); gApp->savePrefs();
    });
    cbSteps->callback([](Fl_Widget* w, void*) {
        static const int st[4] = {0, 8, 16, 32};
        gApp->vs.steps = st[std::clamp(static_cast<Fl_Choice*>(w)->value(), 0, 3)];
        gApp->view->invalidate(); gApp->savePrefs();
    });
    for (int i = 0; i < 3; ++i)
        rbRange[i]->callback([](Fl_Widget*, void* d) {
            int m = int(reinterpret_cast<intptr_t>(d));
            Doc* dc = gApp->doc();
            if (m == 2 && gApp->vs.rangeMode != 2 && dc) {
                float lo, hi; displayRange(*dc, gApp->vs, lo, hi);
                gApp->vs.manLo = std::round(lo * 10) / 10; gApp->vs.manHi = std::round(hi * 10) / 10;
            }
            gApp->vs.rangeMode = m; gApp->refreshControls(); gApp->view->invalidate();
        }, reinterpret_cast<void*>(intptr_t(i)));
    auto rangeCb = [](Fl_Widget*, void*) {
        Doc* dc = gApp->doc(); if (!dc) return;
        float lo, hi; displayRange(*dc, gApp->vs, lo, hi);
        double a, b;
        gApp->vs.manLo = parseNum(gApp->inMin->value(), a) ? a : lo;
        gApp->vs.manHi = parseNum(gApp->inMax->value(), b) ? b : hi;
        gApp->vs.rangeMode = 2;
        gApp->refreshControls(); gApp->view->invalidate();
    };
    inMin->callback(rangeCb); inMax->callback(rangeCb);
    ckMinMax->callback([](Fl_Widget* w, void*) {
        gApp->vs.showMinMax = static_cast<Fl_Check_Button*>(w)->value(); gApp->refreshControls(); gApp->view->redraw(); gApp->savePrefs();
    });
    ckPixel->callback([](Fl_Widget* w, void*) {
        gApp->vs.pixel = static_cast<Fl_Check_Button*>(w)->value(); gApp->refreshControls(); gApp->view->invalidate(); gApp->savePrefs();
    });
    tbEps->callback([](Fl_Widget* w, void*) {
        Doc* dc = gApp->doc(); if (!dc) return;
        dc->eps = std::round(static_cast<w98::Trackbar*>(w)->value() * 100) / 100;
        gApp->recompute();
    });
    inRefl->callback([](Fl_Widget* w, void*) {
        Doc* dc = gApp->doc(); if (!dc) return;
        double v; if (parseNum(static_cast<Fl_Input*>(w)->value(), v)) dc->trefl = v;
        gApp->recompute();
    });
    cbCurve->callback([](Fl_Widget* w, void*) {
        gApp->vs.fit = static_cast<Fl_Choice*>(w)->value() == 0;
        gApp->recompute(); gApp->savePrefs();
    });
    lsPts->callback([](Fl_Widget* w, void*) {
        Doc* dc = gApp->doc(); if (!dc) return;
        dc->sel = static_cast<Fl_Browser*>(w)->value() - 1;
        gApp->view->redraw(); gApp->refreshReadouts();
    });
    btDelPt->callback([](Fl_Widget*, void*) {
        Doc* dc = gApp->doc(); if (!dc || dc->sel < 0 || dc->sel >= int(dc->points.size())) return;
        dc->points.erase(dc->points.begin() + dc->sel);
        dc->sel = std::min(dc->sel, int(dc->points.size()) - 1);
        gApp->refreshPoints(); gApp->view->redraw(); gApp->refreshReadouts();
    });
    btClrPts->callback([](Fl_Widget*, void*) {
        Doc* dc = gApp->doc(); if (!dc) return;
        dc->points.clear(); dc->sel = -1;
        gApp->refreshPoints(); gApp->view->redraw(); gApp->refreshReadouts();
    });
    view->settings(&vs);
    view->onHover = [] { gApp->refreshReadouts(); };
    view->onPoints = [] { gApp->refreshPoints(); gApp->refreshReadouts(); };

    win->onClose = [] { gApp->quit(); };
    win->callback([](Fl_Widget*, void*) {
        if (Fl::event() == FL_SHORTCUT && Fl::event_key() == FL_Escape) return;  // Esc does not close
        gApp->quit();
    });
}

/* ------------------------------------------------------------------ */
/* layout                                                               */
/* ------------------------------------------------------------------ */
void App::layout() {
    const int X0 = win->cx(), Y0 = win->cy(), W = win->cw(), H = win->ch();
    const int pad = 6, bh = 23;
    int y = Y0;
    menu->resize(X0, y, W, 20); y += 20;
    // toolbar row
    y += 5;
    int x = X0 + pad;
    lblFile->resize(x, y, 38, bh); x += 40;
    cbFile->resize(x, y, 290, 22); x += 290 + 8;
    btOpen->resize(x, y, 82, bh); x += 82 + 4;
    btClose->resize(x, y, 82, bh); x += 82 + 4;
    btInfo->resize(x, y, 90, bh); x += 90 + 14;
    btPng->resize(x, y, 128, bh); x += 128 + 4;
    btCsv->resize(x, y, 128, bh);
    y += bh + 7;

    const int statusH = 20, setH = 140, sideW = 244;
    int statusY = Y0 + H - statusH - 1;
    int setY = statusY - 4 - setH;
    int viewH = setY - 6 - y;
    int viewW = W - 2 * pad - sideW - pad;
    view->resize(X0 + pad, y, viewW, viewH);

    // side panel
    int sx = X0 + pad + viewW + pad;
    int measH = 4 * 40 + 26;
    gMeas->resize(sx, y - 6, sideW, measH);
    for (int i = 0; i < 4; ++i) {
        int ry = y - 6 + 20 + i * 40;
        mName[i]->resize(sx + 12, ry, 90, 18);
        mVal[i]->resize(sx + 100, ry, sideW - 112, 20);
        mSub[i]->resize(sx + 12, ry + 19, sideW - 24, 15);
    }
    int py = y - 6 + measH + 6;
    gPts->resize(sx, py, sideW, setY - 6 - py);
    lsPts->resize(sx + 10, py + 20, sideW - 20, gPts->h() - 20 - 10 - bh - 6);
    btDelPt->resize(sx + 10, py + gPts->h() - 10 - bh, 90, bh);
    btClrPts->resize(sx + 106, py + gPts->h() - 10 - bh, 100, bh);

    // settings group box
    gSet->resize(X0 + pad, setY, W - 2 * pad, setH);
    int gx = X0 + pad + 14, gy = setY + 22, row = 28;
    lPal->resize(gx, gy, 86, 22);         cbPal->resize(gx + 90, gy, 140, 22);
    lSteps->resize(gx + 248, gy, 70, 22); cbSteps->resize(gx + 320, gy, 110, 22);
    lRange->resize(gx, gy + row, 86, 22);
    grRange->resize(gx + 90, gy + row, 290, 22);
    rbRange[0]->resize(gx + 90, gy + row, 60, 22);
    rbRange[1]->resize(gx + 158, gy + row, 75, 22);
    rbRange[2]->resize(gx + 240, gy + row, 80, 22);
    lMin->resize(gx + 90, gy + 2 * row, 30, 22); inMin->resize(gx + 120, gy + 2 * row, 64, 22); uMin->resize(gx + 186, gy + 2 * row, 24, 22);
    lMax->resize(gx + 218, gy + 2 * row, 24, 22); inMax->resize(gx + 244, gy + 2 * row, 64, 22); uMax->resize(gx + 310, gy + 2 * row, 24, 22);
    ckMinMax->resize(gx + 88, gy + 3 * row, 150, 22);
    ckPixel->resize(gx + 246, gy + 3 * row, 180, 22);

    int rx = X0 + pad + std::max(540, (W - 2 * pad) / 2 + 20);
    lEps->resize(rx, gy, 104, 22);
    tbEps->resize(rx + 108, gy - 2, 220, 44);
    vEps->resize(rx + 336, gy, 46, 22);
    lRefl->resize(rx, gy + 2 * row, 104, 22); inRefl->resize(rx + 108, gy + 2 * row, 64, 22); uRefl->resize(rx + 174, gy + 2 * row, 24, 22);
    lCurve->resize(rx, gy + 3 * row, 104, 22); cbCurve->resize(rx + 108, gy + 3 * row, 228, 22);

    // status bar
    status->resize(X0 + 2, statusY, W - 4, statusH);
    int sw[3] = {120, 90, 120};
    int sxp = X0 + 2;
    for (int i = 0; i < 3; ++i) { sb[i]->resize(sxp, statusY, sw[i], statusH); sxp += sw[i] + 2; }
    sb[3]->resize(sxp, statusY, X0 + W - 2 - 16 - sxp, statusH);
    grip->resize(X0 + W - 2 - 15, statusY + 2, 15, statusH - 2);
}

/* ------------------------------------------------------------------ */
/* state -> widgets                                                     */
/* ------------------------------------------------------------------ */
void App::recompute() {
    Doc* d = doc();
    if (d) d->compute(vs.fit);
    view->invalidate();
    refreshControls(); refreshReadouts(); refreshPoints();
}

void App::refreshFiles() {
    cbFile->clear();
    for (auto& d : docs) {
        std::string l = d->name;
        if (d->img.meta().hasDate) l += "   " + dateStr(d->img.meta()).substr(0, 10);
        cbFile->add(menuEscape(l).c_str());
    }
    if (cur >= 0) cbFile->value(cur);
    cbFile->redraw();
    bool any = doc() != nullptr;
    for (Fl_Widget* w : std::initializer_list<Fl_Widget*>{cbFile, btClose, btInfo, btPng, btCsv, gSet, gPts})
        any ? w->activate() : w->deactivate();
    std::string cap = std::string(APP_NAME) + " v" + APP_VERSION;
    if (any) cap += " - " + doc()->name;
    win->caption(cap);
}

void App::refreshControls() {
    Doc* d = doc();
    cbPal->value(vs.palette);
    static const int st[4] = {0, 8, 16, 32};
    for (int i = 0; i < 4; ++i) if (st[i] == vs.steps) cbSteps->value(i);
    float clo, chi;
    bool cam = d && cameraRange(d->img.meta(), clo, chi);
    cam ? rbRange[1]->activate() : rbRange[1]->deactivate();
    if (vs.rangeMode == 1 && !cam) vs.rangeMode = 0;
    for (int i = 0; i < 3; ++i) rbRange[i]->value(i == vs.rangeMode);
    if (d && !d->temps.empty()) {
        float lo, hi; displayRange(*d, vs, lo, hi);
        if (Fl::focus() != inMin) inMin->value(fmtNum(lo, 1).c_str());
        if (Fl::focus() != inMax) inMax->value(fmtNum(hi, 1).c_str());
        tbEps->value(d->eps);
        vEps->copy_label(fmtNum(d->eps, 2).c_str());
        if (Fl::focus() != inRefl) inRefl->value(fmtNum(d->trefl, 1).c_str());
        d->eps < 1 ? inRefl->activate() : inRefl->deactivate();
        cbCurve->value(d->img.hasFit() && vs.fit ? 0 : 1);
        cbCurve->mode(0, d->img.hasFit() ? 0 : FL_MENU_INACTIVE);
    }
    ckMinMax->value(vs.showMinMax);
    ckPixel->value(vs.pixel);
    // menu state
    for (int i = 0; i < irb::paletteCount(); ++i) {
        std::string p = std::string("&Ansicht/&Farbpalette/") + irb::palette(i).name;
        if (auto* it = const_cast<Fl_Menu_Item*>(menu->find_item(p.c_str()))) { if (i == vs.palette) it->set(); else it->clear(); }
    }
    if (auto* it = const_cast<Fl_Menu_Item*>(menu->find_item("&Ansicht/&Min- und Max-Kreuze"))) vs.showMinMax ? it->set() : it->clear();
    if (auto* it = const_cast<Fl_Menu_Item*>(menu->find_item("&Ansicht/&Pixel scharf"))) vs.pixel ? it->set() : it->clear();
    if (auto* it = const_cast<Fl_Menu_Item*>(menu->find_item("&Ansicht/&Systemrahmen verwenden"))) nativeFrame ? it->set() : it->clear();
    gSet->redraw();
}

void App::refreshReadouts() {
    Doc* d = doc();
    char b[128];
    if (!d || d->temps.empty()) {
        for (int i = 0; i < 4; ++i) { mVal[i]->copy_label("-"); mSub[i]->copy_label(""); }
        sb[0]->copy_label(""); sb[1]->copy_label(""); sb[2]->copy_label("");
        sb[3]->copy_label(" Bereit. Datei > \xC3\x96" "ffnen oder .IRB-Datei ins Fenster ziehen.");
        gMeas->redraw(); status->redraw();
        return;
    }
    const irb::Stats& s = d->st;
    mVal[1]->copy_label(fmtT(s.max).c_str());
    std::snprintf(b, sizeof b, "bei x %d, y %d", s.maxX, s.maxY); mSub[1]->copy_label(b);
    mVal[2]->copy_label(fmtT(s.min).c_str());
    std::snprintf(b, sizeof b, "bei x %d, y %d", s.minX, s.minY); mSub[2]->copy_label(b);
    mVal[3]->copy_label(fmtT(s.mean).c_str());
    std::snprintf(b, sizeof b, "\xC3\xBC" "ber %ld Pixel", s.valid); mSub[3]->copy_label(b);

    int hx, hy;
    bool hov = view->hover(hx, hy);
    const char* who = "";
    std::string pn;
    if (!hov && d->sel >= 0 && d->sel < int(d->points.size())) {
        hx = d->points[size_t(d->sel)].x; hy = d->points[size_t(d->sel)].y; hov = true;
        pn = "P" + std::to_string(d->sel + 1) + ", "; who = pn.c_str();
    }
    if (hov) {
        float t = d->tAt(hx, hy);
        unsigned raw = d->img.raw()[size_t(hy) * d->img.width() + hx];
        mVal[0]->copy_label(fmtT(t).c_str());
        std::snprintf(b, sizeof b, "%sx %d, y %d%s", who, hx, hy, d->rawOutside(hx, hy) ? "  (au\xC3\x9F" "erhalb Kalibrierung)" : "");
        mSub[0]->copy_label(b);
        std::snprintf(b, sizeof b, " x %d   y %d", hx, hy); sb[0]->copy_label(b);
        sb[1]->copy_label((" " + fmtT(t)).c_str());
        std::snprintf(b, sizeof b, " Rohwert %u", raw); sb[2]->copy_label(b);
    } else {
        mVal[0]->copy_label("-");
        mSub[0]->copy_label("Maus \xC3\xBC" "ber das Bild bewegen");
        sb[0]->copy_label(""); sb[1]->copy_label(""); sb[2]->copy_label("");
    }
    const irb::Meta& m = d->img.meta();
    std::string info = " " + d->name + "   " + dateStr(m) + "   " + std::to_string(m.width) + " x " + std::to_string(m.height) +
                       "   \xCE\xB5 " + fmtNum(d->eps, 2);
    sb[3]->copy_label(info.c_str());
    gMeas->redraw(); status->redraw();
}

void App::refreshPoints() {
    Doc* d = doc();
    lsPts->clear();
    if (d) {
        for (size_t i = 0; i < d->points.size(); ++i) {
            char b[128];
            std::snprintf(b, sizeof b, "P%zu\t%d / %d\t%s", i + 1, d->points[i].x, d->points[i].y, fmtT(d->tAt(d->points[i].x, d->points[i].y)).c_str());
            lsPts->add(b);
        }
        if (d->sel >= 0 && d->sel < int(d->points.size())) lsPts->select(d->sel + 1);
    }
    bool has = d && !d->points.empty();
    has ? btClrPts->activate() : btClrPts->deactivate();
    (d && d->sel >= 0) ? btDelPt->activate() : btDelPt->deactivate();
}

void App::refreshAll() {
    refreshFiles();
    Doc* d = doc();
    if (d) d->compute(vs.fit);
    view->doc(d);
    refreshControls(); refreshReadouts(); refreshPoints();
    win->redraw();
}

/* ------------------------------------------------------------------ */
/* actions                                                              */
/* ------------------------------------------------------------------ */
bool App::openFile(const std::string& path, bool report) {
    std::vector<uint8_t> buf;
    if (!readFile(path, buf)) {
        if (report) fl_alert("Die Datei kann nicht gelesen werden:\n%s", path.c_str());
        return false;
    }
    auto d = std::make_unique<Doc>();
    std::string err;
    if (!d->img.loadFromMemory(buf.data(), buf.size(), err)) {
        if (report) fl_alert("%s\n\n%s", baseName(path).c_str(), err.c_str());
        return false;
    }
    d->path = path;
    d->name = baseName(path);
    const irb::Meta& m = d->img.meta();
    d->eps = m.emissivity > 0.05 && m.emissivity <= 1 ? m.emissivity : 1.0;
    d->trefl = m.envK > 150 && m.envK < 450 ? std::round((m.envK - irb::KELVIN) * 10) / 10 : 20.0;
    docs.push_back(std::move(d));
    lastDir = dirName(path);
    return true;
}

void App::openDialog() {
    Fl_Native_File_Chooser fc;
    fc.title("IRB-Dateien \xC3\xB6" "ffnen");
    fc.type(Fl_Native_File_Chooser::BROWSE_MULTI_FILE);
    fc.filter("IRB-Thermogramme\t*.{irb,IRB}\nAlle Dateien\t*");
    if (!lastDir.empty()) fc.directory(lastDir.c_str());
    if (fc.show() != 0) return;
    int added = 0;
    for (int i = 0; i < fc.count(); ++i) if (openFile(fc.filename(i), true)) ++added;
    if (added) { cur = int(docs.size()) - 1; refreshAll(); savePrefs(); }
}

void App::dropped(const char* text) {
    if (!text) return;
    std::string all = text;
    int added = 0;
    size_t pos = 0;
    while (pos < all.size()) {
        size_t nl = all.find_first_of("\r\n", pos);
        std::string line = all.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? all.size() : nl + 1;
        if (line.empty()) continue;
        if (line.rfind("file://", 0) == 0) {
            line = line.substr(7);
            std::string dec;
            for (size_t i = 0; i < line.size(); ++i) {
                if (line[i] == '%' && i + 2 < line.size()) { dec += char(std::strtol(line.substr(i + 1, 2).c_str(), nullptr, 16)); i += 2; }
                else dec += line[i];
            }
            line = dec;
        }
        if (openFile(line, true)) ++added;
    }
    if (added) { cur = int(docs.size()) - 1; refreshAll(); savePrefs(); }
}

void App::closeCurrent() {
    if (!doc()) return;
    docs.erase(docs.begin() + cur);
    if (cur >= int(docs.size())) cur = int(docs.size()) - 1;
    refreshAll();
}

void App::select(int i) {
    if (i < 0 || i >= int(docs.size())) return;
    cur = i;
    refreshAll();
}

static bool askSavePath(const char* title, const char* filter, const std::string& preset, const std::string& dir, std::string& out) {
    Fl_Native_File_Chooser fc;
    fc.title(title);
    fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    fc.filter(filter);
    fc.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM | Fl_Native_File_Chooser::USE_FILTER_EXT);
    if (!dir.empty()) fc.directory(dir.c_str());
    fc.preset_file(preset.c_str());
    if (fc.show() != 0) return false;
    out = fc.filename();
    return !out.empty();
}

void App::exportPng() {
    Doc* d = doc(); if (!d) return;
    std::string path;
    if (!askSavePath("Als PNG speichern", "PNG-Bild\t*.png", stem(d->name) + ".png", dirName(d->path), path)) return;
    if (path.size() < 4 || fl_utf_strcasecmp(path.c_str() + path.size() - 4, ".png") != 0) path += ".png";
    if (!writePng(path)) fl_alert("Das PNG konnte nicht gespeichert werden:\n%s", path.c_str());
    else { sb[3]->copy_label((" Gespeichert: " + path).c_str()); status->redraw(); }
}

bool App::writePng(const std::string& path) {
    int W, H;
    view->renderExport(2, W, H, false);
    Fl_Image_Surface surf(W, H, 0);
    Fl_Surface_Device::push_current(&surf);
    view->renderExport(2, W, H, true);
    Fl_RGB_Image* img = surf.image();
    Fl_Surface_Device::pop_current();
    int rc = img ? fl_write_png(path.c_str(), img) : -1;
    delete img;
    return rc == 0;
}

void App::exportCsv() {
    Doc* d = doc(); if (!d) return;
    std::string path;
    if (!askSavePath("Temperaturen als CSV speichern", "CSV (Semikolon, Dezimalkomma)\t*.csv", stem(d->name) + "_temperatur.csv", dirName(d->path), path)) return;
    if (path.size() < 4 || fl_utf_strcasecmp(path.c_str() + path.size() - 4, ".csv") != 0) path += ".csv";
    FILE* f = fl_fopen(path.c_str(), "wb");
    if (!f) { fl_alert("Die Datei kann nicht geschrieben werden:\n%s", path.c_str()); return; }
    const int w = d->img.width(), h = d->img.height();
    std::string line;
    char b[32];
    for (int y = 0; y < h; ++y) {
        line.clear();
        for (int x = 0; x < w; ++x) {
            float v = d->temps[size_t(y) * w + x];
            if (std::isfinite(v)) { std::snprintf(b, sizeof b, "%.2f", v); for (char* p = b; *p; ++p) if (*p == '.') *p = ','; line += b; }
            if (x + 1 < w) line += ';';
        }
        line += "\r\n";
        std::fwrite(line.data(), 1, line.size(), f);
    }
    std::fclose(f);
    sb[3]->copy_label((" Gespeichert: " + path).c_str());
    status->redraw();
}

/* simple modal dialog in the same Win98 style */
static void runDialog(w98::Window* dlg, Fl_Button* ok) {
    ok->callback([](Fl_Widget* w, void*) { w->window()->hide(); });
    dlg->onClose = [dlg] { dlg->hide(); };
    dlg->set_modal();
    dlg->position(gApp->win->x() + (gApp->win->w() - dlg->w()) / 2, gApp->win->y() + (gApp->win->h() - dlg->h()) / 2);
    dlg->showWindow();
    while (dlg->shown()) Fl::wait();
}

void App::showInfo() {
    Doc* d = doc(); if (!d) return;
    const irb::Meta& m = d->img.meta();
    auto* dlg = new w98::Window(500, 596, "Dateiinfo", false);
    dlg->caption("Dateiinfo - " + d->name);
    int X = dlg->cx() + 10, Y = dlg->cy() + 8, W = dlg->cw() - 20;
    auto* g1 = new w98::GroupBox(X, Y, W, 300, "Aufnahme");
    auto* b1 = new Fl_Browser(X + 10, Y + 20, W - 20, 270);
    b1->box(FL_DOWN_BOX); b1->color(FL_BACKGROUND2_COLOR); b1->textsize(12); b1->format_char(0);
    static int c1[] = {150, 0}; b1->column_widths(c1);
    auto add = [&](const std::string& k, const std::string& v) { if (!v.empty()) b1->add((k + "\t" + v).c_str()); };
    char t[160];
    add("Datei", d->name);
    add("Aufnahmezeit", dateStr(m) + (m.hasDate && m.year <= 2000 && m.month == 1 && m.day == 1 ? "  (Kamera-Uhr nicht gestellt)" : ""));
    add("Kamera", m.device + (m.model.empty() ? "" : "  (" + m.model + ")"));
    add("Hersteller", m.manufacturer);
    add("Seriennummer", m.serial);
    add("Firmware", m.firmware);
    add("Aufl\xC3\xB6sung", std::to_string(m.width) + " x " + std::to_string(m.height) + " Pixel");
    if (m.fovH > 1 && m.fovH < 180) {
        double ifov = m.fovH / m.width * 3.14159265358979 / 180 * 1000;
        add("Sichtfeld", fmtNum(m.fovH, 0) + "\xC2\xB0 x " + fmtNum(m.fovV, 0) + "\xC2\xB0,  IFOV " + fmtNum(ifov, 2) + " mrad");
        if (m.distance > 0) add("Pixel am Objekt", "ca. " + fmtNum(m.distance * ifov, 1) + " mm bei " + fmtNum(m.distance, 1) + " m");
    }
    add("Messbereich", m.rangeName);
    if (m.calMaxK > 0) add("Kalibriert", fmtNum(m.calMinK - irb::KELVIN, 1) + " ... " + fmtT(m.calMaxK - irb::KELVIN));
    add("Kalibrierdatei", m.calFile + (m.calDate.empty() ? "" : "   " + m.calDate));
    add("\xCE\xB5 in Datei", fmtNum(m.emissivity, 2));
    add("Umgebung in Datei", fmtT(m.envK - irb::KELVIN));
    float lo, hi;
    if (cameraRange(m, lo, hi)) add("Anzeige bei Aufnahme", fmtNum(lo, 1) + " ... " + fmtT(hi));
    std::snprintf(t, sizeof t, "0x%lX (Kopf %ld Byte)", m.dataOffset, m.dataOffset - 128);
    add("Pixeldaten ab", t);
    g1->end();

    int y2 = Y + 308;
    auto* g2 = new w98::GroupBox(X, y2, W, 206, "Kalibrierung");
    auto* b2 = new Fl_Browser(X + 10, y2 + 20, W - 20, 112);
    b2->box(FL_DOWN_BOX); b2->color(FL_BACKGROUND2_COLOR); b2->textsize(12); b2->format_char(0);
    static int c2[] = {100, 100, 110, 0}; b2->column_widths(c2);
    b2->add("Rohwert\tSoll \xC2\xB0" "C\tKennlinie \xC2\xB0" "C\tAbw. K");
    const irb::Curve& c = d->img.curve(vs.fit);
    std::vector<irb::CalPoint> cal = m.cal;
    std::sort(cal.begin(), cal.end(), [](const irb::CalPoint& a, const irb::CalPoint& b) { return a.s < b.s; });
    for (auto& p : cal) {
        bool anchor = p.tK < 20;
        double tk = anchor ? 0 : c.TfromU(p.s - c.O);
        std::string l = fmtNum(p.s, 1) + "\t" + fmtNum(p.tK - irb::KELVIN, 2) + "\t" + (anchor ? "Nullpunkt" : fmtNum(tk - irb::KELVIN, 2)) +
                        "\t" + (anchor ? "" : fmtNum(tk - p.tK, 3));
        b2->add(l.c_str());
    }
    auto* f = new Fl_Box(X + 10, y2 + 138, W - 20, 60);
    f->align(FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    f->labelsize(11);
    std::string ft;
    if (d->img.hasFit()) {
        const irb::Curve& q = d->img.curve(true);
        std::snprintf(t, sizeof t, "S = O + R / (exp(B/T) - F)   O = %.1f   R = %.0f   B = %.2f K   F = %.4f", q.O, q.R, q.B, q.F);
        ft = std::string(t) + "\nPlanck-Fit trifft die St\xC3\xBCtzpunkte auf " + fmtNum(q.maxErr, 3) + " K. Der Punkt bei 0,15 K ist der Nullpunkt der Rohwerte.";
    } else ft = "Planck-Fit nicht stabil, es wird linear zwischen den St\xC3\xBCtzpunkten interpoliert.";
    f->copy_label(ft.c_str());
    g2->end();
    auto* ok = new Fl_Button(dlg->cx() + dlg->cw() - 10 - 86, dlg->cy() + dlg->ch() - 10 - 23, 86, 23, "OK");
    w98::styleButton(ok);
    dlg->end();
    runDialog(dlg, ok);
    Fl::delete_widget(dlg);
}

void App::showAbout() {
    auto* dlg = new w98::Window(400, 220, "\xC3\x9C" "ber", false);
    dlg->caption("\xC3\x9C" "ber IRB Thermoviewer");
    int X = dlg->cx(), Y = dlg->cy();
    auto* ic = new Fl_Box(X + 18, Y + 18, 36, 36);
    ic->box(FL_DOWN_BOX); ic->color(fl_rgb_color(239, 105, 13));
    auto* t = new Fl_Box(X + 70, Y + 14, dlg->cw() - 86, 120);
    t->align(FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    t->labelsize(12);
    std::string s = std::string(APP_NAME) + "  Version " + APP_VERSION +
                    "\n\nBetrachter f\xC3\xBCr Thermogramme der Jenoptik VARIOSCAN (.IRB)."
                    "\nRohwerte werden \xC3\xBC" "ber die Kalibriertabelle jeder Datei in Temperaturen umgerechnet."
                    "\n\nGebaut mit FLTK " + std::to_string(FL_MAJOR_VERSION) + "." + std::to_string(FL_MINOR_VERSION) + "." + std::to_string(FL_PATCH_VERSION) + ".";
    t->copy_label(s.c_str());
    auto* ok = new Fl_Button(X + dlg->cw() - 10 - 86, Y + dlg->ch() - 10 - 23, 86, 23, "OK");
    w98::styleButton(ok);
    dlg->end();
    runDialog(dlg, ok);
    Fl::delete_widget(dlg);
}

void App::savePrefs() {
    Fl_Preferences p(Fl_Preferences::USER_L, "thermoviewer98", "settings");
    p.set("palette", vs.palette);
    p.set("steps", vs.steps);
    p.set("minmax", vs.showMinMax ? 1 : 0);
    p.set("pixel", vs.pixel ? 1 : 0);
    p.set("fit", vs.fit ? 1 : 0);
    p.set("nativeFrame", nativeFrame ? 1 : 0);
    p.set("lastDir", lastDir.c_str());
    if (!win->isMaximized()) { p.set("x", win->x()); p.set("y", win->y()); p.set("w", win->w()); p.set("h", win->h()); }
}

void App::quit() {
    savePrefs();
    while (Fl::first_window()) Fl::first_window()->hide();
}

static Fl_RGB_Image* makeIcon() {
    static uchar px[32 * 32 * 4];
    static const uchar bands[7][3] = {{12, 8, 60}, {29, 11, 92}, {101, 10, 158}, {216, 53, 61}, {239, 105, 13}, {249, 167, 6}, {253, 225, 73}};
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            uchar* p = px + (y * 32 + x) * 4;
            bool border = x == 1 || y == 1 || x == 30 || y == 30;
            bool out = x < 1 || y < 1 || x > 30 || y > 30;
            if (out) { p[0] = p[1] = p[2] = 0; p[3] = 0; continue; }
            if (border) { p[0] = p[1] = p[2] = 0; p[3] = 255; continue; }
            double dd = std::hypot(x - 19.0, y - 12.0);
            int k = std::clamp(6 - int(dd / 3.2), 0, 6);
            p[0] = bands[k][0]; p[1] = bands[k][1]; p[2] = bands[k][2]; p[3] = 255;
        }
    return new Fl_RGB_Image(px, 32, 32, 4);
}

void App::run(int argc, char** argv) {
    Fl_Preferences p(Fl_Preferences::USER_L, "thermoviewer98", "settings");
    int v;
    p.get("palette", v, 0); vs.palette = std::clamp(v, 0, irb::paletteCount() - 1);
    p.get("steps", v, 0); vs.steps = v;
    p.get("minmax", v, 1); vs.showMinMax = v != 0;
    p.get("pixel", v, 0); vs.pixel = v != 0;
    p.get("fit", v, 1); vs.fit = v != 0;
    p.get("nativeFrame", v, 0); nativeFrame = v != 0;
    char* dir = nullptr;
    p.get("lastDir", dir, "");
    if (dir) { lastDir = dir; free(dir); }
    int x, y, w, h;
    p.get("w", w, 1080); p.get("h", h, 790); p.get("x", x, -1); p.get("y", y, -1);
    w = std::max(w, 920); h = std::max(h, 660);
    int sx, sy, sw, sh;
    Fl::screen_work_area(sx, sy, sw, sh);
    w = std::min(w, sw); h = std::min(h, sh);
    if (x < sx || y < sy || x + 40 > sx + sw || y + 40 > sy + sh) { x = sx + (sw - w) / 2; y = sy + (sh - h) / 2; }
    win->resize(x, y, w, h);

    // files from the command line
    std::vector<std::string> files;
#ifdef _WIN32
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    for (int i = 1; wargv && i < wargc; ++i) {
        unsigned n = fl_utf8fromwc(nullptr, 0, reinterpret_cast<const wchar_t*>(wargv[i]), unsigned(wcslen(wargv[i])));
        std::string s(n, '\0');
        fl_utf8fromwc(&s[0], n + 1, reinterpret_cast<const wchar_t*>(wargv[i]), unsigned(wcslen(wargv[i])));
        files.push_back(s);
    }
    if (wargv) LocalFree(wargv);
#else
    for (int i = 1; i < argc; ++i) files.push_back(argv[i]);
#endif
    (void)argc; (void)argv;
    std::string pngOut;
    for (size_t i = 0; i < files.size(); ++i) {
        const std::string& f = files[i];
        if ((f == "--png" || f == "-png") && i + 1 < files.size()) { pngOut = files[++i]; continue; }
        if (!f.empty() && f[0] != '-') openFile(f, pngOut.empty());
    }
    cur = docs.empty() ? -1 : 0;
    if (!pngOut.empty()) {             // batch: thermoviewer98 --png out.png datei.IRB
        if (!doc()) return;
        doc()->compute(vs.fit);
        view->doc(doc());
        win->show();                   // a window (display connection) is needed for offscreen drawing
        win->hide();
        writePng(pngOut);
        return;
    }

    Fl_Window::default_icon(makeIcon());
    if (nativeFrame) win->useSystemFrame(true);
    layout();
    refreshAll();
    win->showWindow();
    Fl::run();
}

int main(int argc, char** argv) {
    w98::applyTheme();
    fl_message_title_default(APP_NAME);
    Fl::option(Fl::OPTION_VISIBLE_FOCUS, true);
    App app;
    app.run(argc, argv);
    return 0;
}
