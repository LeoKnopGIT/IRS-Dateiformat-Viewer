// irb.hpp - Reader for Jenoptik VARIOSCAN .IRB thermograms (VS 3021-ST, "VARIOSCAN hr")
// and raw-value -> temperature conversion. No GUI dependencies.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace irb {

constexpr double KELVIN = 273.15;

struct CalPoint { double s; double tK; };   // raw signal, temperature in Kelvin

struct Meta {
    std::string fileType, firmware, device, model, serial, rangeName, calFile, calDate, manufacturer;
    int width = 0, height = 0;
    long dataOffset = 0;
    double emissivity = 1, distance = 0, envK = 293.15, pathK = 293.15;
    double calMinK = 0, calMaxK = 0, fovH = 0, fovV = 0, levelK = 0, span = 0;
    double ole = 0;                          // OLE automation date (days since 1899-12-30)
    bool hasExtended = false;
    std::vector<CalPoint> cal;
    // time stamp, camera local time
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, msec = 0;
    bool hasDate = false;
};

// Calibration curve: blackbody signal U(T) = S(T) - O and its inverse.
struct Curve {
    enum Kind { Fit, Linear } kind = Linear;
    double O = 0;
    // Fit: S = O + R / (exp(B/T) - F)
    double R = 0, B = 0, F = 0;
    double maxErr = 0;                       // K, at the calibration points
    std::vector<double> S, T;                // Linear: sorted table
    double U(double tK) const;               // signal above O for a blackbody at tK
    double TfromU(double u) const;           // Kelvin, NaN when out of model range
};

struct Stats {
    float min = 0, max = 0, mean = 0;
    int minX = 0, minY = 0, maxX = 0, maxY = 0;
    long valid = 0;
};

class Image {
public:
    bool load(const std::string& path, std::string& error);
    bool loadFromMemory(const uint8_t* data, size_t len, std::string& error);

    const Meta& meta() const { return meta_; }
    int width() const { return meta_.width; }
    int height() const { return meta_.height; }
    const std::vector<uint16_t>& raw() const { return raw_; }
    bool hasFit() const { return hasFit_; }
    const Curve& curve(bool preferFit) const { return preferFit && hasFit_ ? fit_ : lin_; }
    double rawLo() const { return rawLo_; }  // calibrated raw range
    double rawHi() const { return rawHi_; }

    // Temperatures in degC. eps: emissivity, treflC: reflected temperature
    void computeTemps(bool preferFit, double eps, double treflC, std::vector<float>& out) const;

private:
    Meta meta_;
    std::vector<uint16_t> raw_;
    Curve fit_, lin_;
    bool hasFit_ = false;
    double rawLo_ = 0, rawHi_ = 0;
};

Stats computeStats(const std::vector<float>& t, int w);

// Palettes
struct Palette { const char* name; uint8_t lut[256][3]; };
int paletteCount();
const Palette& palette(int i);

// Colorize temps -> RGB (3 bytes/pixel). steps>1 quantizes to N bands.
void colorize(const float* t, size_t n, float lo, float hi, int pal, int steps, uint8_t* rgb);

double niceStep(double range, int target);
std::string fmtNum(double v, int decimals);     // German decimal comma, unicode minus

} // namespace irb
