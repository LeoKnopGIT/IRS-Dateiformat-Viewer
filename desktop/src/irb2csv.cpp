// irb2csv - convert VARIOSCAN .IRB files to a temperature matrix (CSV, degC)
// usage: irb2csv [--eps 0.95] [--trefl 20] [--linear] [--en] file.IRB ...
#include "irb.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
    double eps = -1, trefl = NAN; bool fit = true, en = false; int done = 0;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--eps" && i + 1 < argc) { eps = std::atof(argv[++i]); continue; }
        if (a == "--trefl" && i + 1 < argc) { trefl = std::atof(argv[++i]); continue; }
        if (a == "--linear") { fit = false; continue; }
        if (a == "--en") { en = true; continue; }
        if (a == "-h" || a == "--help") {
            std::printf("irb2csv [--eps E] [--trefl GradC] [--linear] [--en] datei.IRB ...\n"
                        "Schreibt <datei>_temperatur.csv (Semikolon, Dezimalkomma; --en: Komma, Punkt).\n");
            return 0;
        }
        irb::Image img; std::string err;
        if (!img.load(a, err)) { std::fprintf(stderr, "%s: %s\n", a.c_str(), err.c_str()); continue; }
        const irb::Meta& m = img.meta();
        double e = eps > 0 ? eps : (m.emissivity > 0 && m.emissivity <= 1 ? m.emissivity : 1.0);
        double tr = std::isnan(trefl) ? m.envK - irb::KELVIN : trefl;
        std::vector<float> t;
        img.computeTemps(fit, e, tr, t);
        irb::Stats s = irb::computeStats(t, img.width());
        std::string out = a.substr(0, a.find_last_of('.')) + "_temperatur.csv";
        FILE* f = std::fopen(out.c_str(), "wb");
        if (!f) { std::fprintf(stderr, "%s: kann nicht geschrieben werden\n", out.c_str()); continue; }
        char sep = en ? ',' : ';';
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                char b[32]; std::snprintf(b, sizeof b, "%.2f", t[size_t(y) * img.width() + x]);
                if (!en) for (char* p = b; *p; ++p) if (*p == '.') *p = ',';
                std::fputs(b, f); if (x + 1 < img.width()) std::fputc(sep, f);
            }
            std::fputs("\r\n", f);
        }
        std::fclose(f);
        std::printf("%s  %dx%d  min %.2f  max %.2f  mittel %.2f degC  -> %s\n", a.c_str(), img.width(), img.height(), s.min, s.max, s.mean, out.c_str());
        ++done;
    }
    if (!done && argc < 2) { std::fprintf(stderr, "Aufruf: irb2csv datei.IRB ...\n"); return 1; }
    return 0;
}
