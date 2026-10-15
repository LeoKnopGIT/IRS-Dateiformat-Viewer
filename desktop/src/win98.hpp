// win98.hpp - Windows 98 "classic" look for FLTK: box types, colours, fonts and a
// handful of widgets (custom title bar + frame, group box, combo box, trackbar).
#pragma once
#include <FL/Fl.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Valuator.H>
#include <FL/Fl_Widget.H>
#include <functional>
#include <string>

namespace w98 {

// Classic system colours
extern Fl_Color FACE, LIGHT, HILIGHT, SHADOW, DARK, NAVY, NAVY2, INFO_BG, WHITE;
// Pressed push button (black + dark grey frame)
extern Fl_Boxtype PRESSED_BOX;
// Window frame (4 px raised border)
extern Fl_Boxtype WINDOW_BOX;

void applyTheme();                         // call once before creating widgets

/* Group box: etched rectangle with the label set into the top edge. */
class GroupBox : public Fl_Group {
public:
    GroupBox(int x, int y, int w, int h, const char* l = nullptr);
protected:
    void draw() override;
};

/* Combo box: white sunken field with a raised drop-down button. */
class Combo : public Fl_Choice {
public:
    Combo(int x, int y, int w, int h, const char* l = nullptr);
protected:
    void draw() override;
};

/* Trackbar (Win98 slider): thin sunken groove, pointed thumb, ticks and end labels. */
class Trackbar : public Fl_Valuator {
public:
    Trackbar(int x, int y, int w, int h, const char* l = nullptr);
    void ticks(int n) { ticks_ = n; }
    void endLabels(const char* left, const char* mid, const char* right) { l_ = left; m_ = mid; r_ = right; }
    int handle(int e) override;
protected:
    void draw() override;
private:
    int ticks_ = 10;
    std::string l_, m_, r_;
    int trackX0() const { return x() + 7; }
    int trackX1() const { return x() + w() - 8; }
    int thumbX() const;
    double valueFromX(int mx) const;
};

/* Push button with Win98 bevels and pressed look. */
void styleButton(Fl_Widget* b);

class TitleBar;

/* Window with a hand-drawn Win98 frame and caption bar (falls back to the system frame). */
class Window : public Fl_Double_Window {
public:
    Window(int w, int h, const char* title, bool resizableFrame);
    ~Window() override;

    void caption(const std::string& s);
    const std::string& caption() const { return caption_; }

    void useSystemFrame(bool on);          // switch between custom and native frame
    bool systemFrame() const { return native_; }
    bool isMaximized() const { return maximized_; }
    void toggleMaximize();
    void minimize();
    void showWindow();                     // show() + platform fix-ups

    // client rectangle (inside frame + caption)
    int cx() const; int cy() const; int cw() const; int ch() const;

    std::function<void()> onClose;         // default: hide()
    virtual void layout() {}               // place children inside the client rect

    int handle(int e) override;
    void resize(int X, int Y, int W, int H) override;
    void draw() override;

    void minSize(int w, int h) { minW_ = w; minH_ = h; size_range(w, h); }

private:
    friend class TitleBar;
    class TitleBar* bar_;
    std::string caption_;
    bool native_ = false, sizable_, maximized_ = false;
    int normX_ = 0, normY_ = 0, normW_ = 0, normH_ = 0;
    int minW_ = 300, minH_ = 200;
    // frame resize state
    int edge_ = 0, dragMX_ = 0, dragMY_ = 0, startX_ = 0, startY_ = 0, startW_ = 0, startH_ = 0;
    Fl_Cursor cur_ = FL_CURSOR_DEFAULT;
    int edgeAt(int ex, int ey) const;
};

int frameWidth();                          // 4
int captionHeight();                       // 22

} // namespace w98
