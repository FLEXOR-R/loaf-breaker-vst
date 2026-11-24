#!/bin/sh
# The VST's break bank: core/build_bank.sh OUT (breaks.bank): every break of the firmware's breaks/breaks.txt (its
# bank's * and its library's -), at 44.1 kHz in 16-bit PCM, sorted by BPM (the FM-1 holds eight, 22.05 kHz IMA ADPCM).
# The firmware reads it with LB_BIG (app.c lb_bank_ptr); the plugin puts it beside the core (VST3: Contents/Resources).
set -e
cd "$(dirname "$0")/.."
python3 firmware/tools/prep_breaks.py --list firmware/breaks/breaks.txt --all --pcm16 --rate 44100 --slots 128 \
    --capacity 200000000 --max-len 1048576 --break-slices 16 --transients -o "$1" | tail -2
# the breaks' categories, as the site's BREAKS page groups them (tools/break_lib.py FOLDER), and their full titles: one
# line a break, NAME<TAB>CATEGORY<TAB>TITLE (OUT's folder, breaks.index; the plugin's break browser reads it)
python3 - "$1" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, "firmware/tools")
import break_lib as bl
root = Path("firmware/breaks")
titles = {f"flexor/{k}": v for k, v in bl.titles(root / "flexor" / "README.md").items()}
lines = []
for built, file, bpm, bars, name, grid, loud, hz in bl.entries(root / "breaks.txt"):
    folder = file.split("/")[0]
    lines.append(f"{name.upper()[:11]}\t{bl.FOLDER.get(folder, folder.upper())}\t{titles.get(file, '')}")
out = Path(sys.argv[1]).with_name("breaks.index")
out.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"{out}: {len(lines)} breaks' categories")
PY
