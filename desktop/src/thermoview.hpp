// thermoview.hpp - thermogram display widget: colour-mapped image, scale bar with
// histogram, min/max crosses, measurement points and cursor readout.
#pragma once
#include "irb.hpp"

#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Widget.H>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct Point { int x, y; };

struct Doc {
    std::string path, name;
    irb::Image img;
    std::vector<float> temps;
    irb::Stats st;
    double eps = 1.0, trefl = 20.0;
    std::vector<Point> points;
    int sel = -1;
    std::string key;                     // parameters temps were computed with
    void compute(bool fit);
    float tAt(int x, int y) const { return temps[size_t(y) * img.width() + x]; }
    bool rawOutside(int x, int y) const {
        double r = img.raw()[size_t(y) * img.width() + x];
        return r < img.rawLo() || r > img.rawHi();
    }
};

struct ViewSettings {
    int palette = 0;
    int steps = 0;                       // 0 = continuous
    int rangeMode = 0;                   // 0 auto, 1 camera, 2 manual
    double manLo = 0, manHi = 30;
    bool showMinMax = true;
    bool pixel = false;                  // nearest neighbour instead of bilinear
    bool fit = true;                     // Planck fit vs. linear table
};

bool cameraRange(const irb::Meta& m, float& lo, float& hi);
void displayRange(const Doc& d, const ViewSettings& s, float& lo, float& hi);

class ThermoView : public Fl_Widget {
public:
    ThermoView(int x, int y, int w, int h);
    ~ThermoView() override;

    void doc(Doc* d) { doc_ = d; hoverOn_ = false; invalidate(); }
    Doc* doc() const { return doc_; }
    void settings(ViewSettings* s) { set_ = s; invalidate(); }
    void invalidate() { dirty_ = true; redraw(); }

    bool hover(int& x, int& y) const { x = hx_; y = hy_; return hoverOn_ && doc_; }

    std::function<void()> onHover;       // cursor moved / left
    std::function<void()> onPoints;      // points added, moved, removed or selected

    int handle(int e) override;

    // Render a report image (thermogram at `scale` px per pixel, scale bar, caption)
    // with the current drawing device; returns the size needed when w/h are null.
    void renderExport(int scale, int& outW, int& outH, bool draw);

    void resize(int X, int Y, int W, int H) override { Fl_Widget::resize(X, Y, W, H); dirty_ = true; }

protected:
    void draw() override;

private:
    struct Rect { int x = 0, y = 0, w = 0, h = 0; bool in(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; } };
    Doc* doc_ = nullptr;
    ViewSettings* set_ = nullptr;
    Rect img_, scale_;
    bool dirty_ = true;
    std::vector<uchar> buf_;
    int bufW_ = 0, bufH_ = 0;
    std::unique_ptr<Fl_RGB_Image> rgb_;
    bool hoverOn_ = false;
    int hx_ = 0, hy_ = 0;
    int dragPt_ = -1, downX_ = 0, downY_ = 0;
    bool down_ = false, moved_ = false;

    void computeLayout();
    void buildBuffer(int pw, int ph, std::vector<uchar>& out) const;
    void drawImage(const Rect& r, bool exact);
    void drawOverlay(const Rect& r, bool withHover) const;
    void drawScale(const Rect& r, Fl_Color bg) const;
    bool toPixel(int mx, int my, int& px, int& py) const;
    int nearestPoint(int mx, int my) const;
};
