#!/usr/bin/env python3
"""Builds the single-file web viewer irb-thermoviewer.html from src/.

    python build.py                 # with the example files from ../samples embedded
    python build.py --no-samples    # empty viewer, files are loaded by the user
    python build.py --fragment      # page body only (for hosts that add <html>/<head> themselves)
"""
import base64
import json
import pathlib
import sys

here = pathlib.Path(__file__).resolve().parent
src = here / "src"
samples = []
if "--no-samples" not in sys.argv:
    for p in sorted((here.parent / "samples").glob("*.[Ii][Rr][Bb]")):
        samples.append({"name": p.name, "b64": base64.b64encode(p.read_bytes()).decode()})
sel = next((i for i, s in enumerate(samples) if s["name"].startswith("2")), 0)
samples_js = "window.IRB_SAMPLES = " + json.dumps(samples) + ";\nwindow.IRB_SAMPLES_SELECT = %d;" % sel

page = (src / "template.html").read_text(encoding="utf-8")
page = page.replace("/*STYLE*/", (src / "style.css").read_text(encoding="utf-8"))
page = page.replace("/*CORE*/", (src / "core.js").read_text(encoding="utf-8"))
page = page.replace("/*SAMPLES*/", samples_js)
page = page.replace("/*APP*/", (src / "app.js").read_text(encoding="utf-8"))

if "--fragment" not in sys.argv:
    page = ('<!doctype html><html lang="de"><head><meta charset="utf-8">'
            '<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"></head><body>\n'
            + page + "\n</body></html>\n")

out = here / "irb-thermoviewer.html"
out.write_text(page, encoding="utf-8")
print("geschrieben:", out, "(%d kB, %d Beispieldateien)" % (len(page) // 1024, len(samples)))
