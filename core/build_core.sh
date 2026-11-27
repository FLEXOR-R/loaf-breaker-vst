#!/bin/sh
# The firmware's core as a shared library: core/build_core.sh OUT [CC]   (liblbcore.so on Linux, lbcore.dll with
# MinGW-w64's gcc on Windows, liblbcore.dylib with clang on a Mac: arm64 + x86_64). Needs the firmware's generated headers first: (cd firmware && ./build.sh --host), or
# the CI's "gen" artifact (firmware/app/bank_data.h, firmware/app/build/gen/).
set -e
cd "$(dirname "$0")/.."
OUT="$1"; CC="${2:-cc}"
F=firmware; B=$F/base
INC="-Icore -I$B/host -I$B/device -I$B/include -I$B/lib -I$F/app -I$F/app/build/gen -I$F/app/dsp"
FLAGS="-O2 -ffp-contract=off -std=gnu99 -w -DPLAT_HOST -DPLAT_WEB -DPLAT_RFM1 -DFM1B_MIRROR=1 -DLB_BIG=1 -DAPP_VERSION=\"vst\" -fvisibility=hidden"
SHARED="-shared"
case "$OUT" in
    *.dll) PIC=""; LIBS="-static -static-libgcc -Wl,-Bstatic -lpthread -lwinmm" ;;
    *.dylib) # macOS: one library for Apple Silicon and Intel, macOS 11 on
        MAC="-arch arm64 -arch x86_64 -mmacosx-version-min=11.0"
        FLAGS="$FLAGS $MAC"; PIC="-fPIC"; LIBS="$MAC"; SHARED="-dynamiclib -install_name @rpath/liblbcore.dylib" ;;
    *) PIC="-fPIC"; LIBS="-lm" ;;
esac
TMP="$(mktemp -d)"
for f in $F/app/dsp/*.c; do
    $CC $FLAGS $PIC $INC -c "$f" -o "$TMP/$(basename "$f" .c).o" &
done
$CC $FLAGS $PIC $INC -c core/lbcore.c -o "$TMP/lbcore.o"
wait
$CC $SHARED -o "$OUT" "$TMP"/*.o $LIBS
rm -rf "$TMP"
echo "core: $OUT"
