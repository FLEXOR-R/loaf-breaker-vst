#!/bin/sh
# The VST's break bank: core/build_bank.sh OUT (breaks.bank): every break of the firmware's breaks/breaks.txt (its
# bank's * and its library's -), at 44.1 kHz in 16-bit PCM, sorted by BPM (the FM-1 holds eight, 22.05 kHz IMA ADPCM).
# The firmware reads it with LB_BIG (app.c lb_bank_ptr); the plugin puts it beside the core (VST3: Contents/Resources).
set -e
cd "$(dirname "$0")/.."
python3 firmware/tools/prep_breaks.py --list firmware/breaks/breaks.txt --all --pcm16 --rate 44100 --slots 128 \
    --capacity 200000000 --max-len 1048576 --break-slices 16 --transients -o "$1" | tail -2
