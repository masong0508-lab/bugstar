#!/bin/bash
# pc/make_dist.sh: rebuild the two download packages from the current source.
#   pc/dist/BUGSTAR-PC-linux64.tar.gz     (Linux x86-64: BUGSTAR, lib/libSDL2, README.txt, portable.txt, data/)
#   pc/dist/BUGSTAR-PC-windows32.zip      (Windows 32 bit: BUGSTAR.exe, SDL2.dll, README.txt, portable.txt, data/)
#
#   ./pc/make_dist.sh                 build both, repack both
#   ./pc/make_dist.sh linux           only the Linux package
#   ./pc/make_dist.sh windows         only the Windows package
#
# What it does (the same steps as the hand-made mkpkg script it replaces):
#   1. builds the Linux game with pc/build.sh (pc/bugstar_pc)
#   2. cross-builds the Windows game with pc/build.sh and the MinGW settings below (pc/BUGSTAR.exe, objects in pc/objwin)
#   3. unpacks the packages that are already in pc/dist and keeps everything that is not the game itself (README.txt, portable.txt,
#      SDL2.dll, lib/libSDL2-2.0.so.0), swaps in the fresh BUGSTAR / BUGSTAR.exe, empties data/ (no one's saves ship) and puts
#      data/readme.txt back, then packs them again into pc/dist
# It does NOT touch git: afterwards `git add -f pc/dist pc/BUGSTAR.exe pc/objwin/game.stamp`, commit, and push as usual.
# (The exe and objwin/game.stamp are tracked; if you only want to look around, `git checkout -- pc/BUGSTAR.exe pc/objwin/game.stamp` undoes the build.)
#
# What you need (Linux, x86-64):
#   - gcc, python3 with numpy/pillow if build.sh asks for them, SDL2 development files (libsdl2-dev) for the Linux build
#   - for the Windows package, two downloads, unpacked anywhere:
#       llvm-mingw   (https://github.com/mstorsjo/llvm-mingw/releases, e.g. llvm-mingw-20240619-ucrt-ubuntu-20.04-x86_64.tar.xz)
#       SDL2 mingw   (https://github.com/libsdl-org/SDL/releases, SDL2-devel-2.30.0-mingw.tar.gz, the i686-w64-mingw32 folder)
#     and two environment variables that point at them:
#       MINGW_BIN=/path/to/llvm-mingw-.../bin          (has i686-w64-mingw32-gcc; skip it if the compiler is already on PATH)
#       SDL_WIN=/path/to/SDL2-2.30.0/i686-w64-mingw32  (has include/ and lib/)
#   - the existing packages in pc/dist (they hold the files that are not built: README.txt, portable.txt, SDL2.dll, libSDL2)
#   Optional: COPY_TO=/some/folder copies the two finished packages there as well.
# Without the Windows pieces, `make_dist.sh linux` still works and `make_dist.sh` stops with a message before it changes anything.
set -e
cd "$(dirname "$0")"; PCD=$(pwd)
WHAT=${1:-both}
case "$WHAT" in both|linux|windows) ;; *) echo "usage: pc/make_dist.sh [linux|windows]"; exit 2;; esac
DIST=$PCD/dist; LIN=BUGSTAR-PC-linux64; WIN=BUGSTAR-PC-windows32
[ -f $DIST/$LIN.tar.gz ] || [ $WHAT = windows ] || { echo "make_dist: $DIST/$LIN.tar.gz is missing (it is the template for README.txt, portable.txt and lib/)"; exit 1; }
[ -f $DIST/$WIN.zip ] || [ $WHAT = linux ] || { echo "make_dist: $DIST/$WIN.zip is missing (it is the template for README.txt, portable.txt and SDL2.dll)"; exit 1; }
if [ $WHAT != linux ]; then
  [ -n "$MINGW_BIN" ] && export PATH="$MINGW_BIN:$PATH"
  command -v i686-w64-mingw32-gcc >/dev/null || { echo "make_dist: i686-w64-mingw32-gcc not found. Set MINGW_BIN to the bin folder of llvm-mingw (see the top of this script), or run: pc/make_dist.sh linux"; exit 1; }
  [ -d "$SDL_WIN/include" ] || { echo "make_dist: SDL_WIN must be the i686-w64-mingw32 folder of the SDL2 mingw development package (see the top of this script)"; exit 1; }
fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT

if [ $WHAT != windows ]; then
  echo "== Linux build"; ./build.sh | tail -1
fi
if [ $WHAT != linux ]; then
  echo "== Windows build"
  # the generated C is GNU C with a few implicit declarations that MinGW's stricter headers complain about: same flags as the CI notes
  PC_CFLAGS="-include stdio.h -include string.h -include stdlib.h -Wno-error -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types" \
  PC_WIN=$SDL_WIN PC_CC=i686-w64-mingw32-gcc PC_ARCH=-m32 PC_OUT=BUGSTAR.exe ./build.sh | tail -1
fi

putdata(){ rm -rf "$1"/data/*; mkdir -p "$1/data"; echo "saves go here" > "$1/data/readme.txt"; }   # (no saves, settings or screenshots in a package)
if [ $WHAT != windows ]; then
  echo "== $LIN.tar.gz"; tar xzf $DIST/$LIN.tar.gz -C $T
  cp bugstar_pc $T/$LIN/BUGSTAR; putdata $T/$LIN
  ( cd $T && tar czf $T/$LIN.tar.gz $LIN ) && cp $T/$LIN.tar.gz $DIST/$LIN.tar.gz
fi
if [ $WHAT != linux ]; then
  echo "== $WIN.zip"; ( cd $T && unzip -q $DIST/$WIN.zip )
  cp BUGSTAR.exe $T/$WIN/BUGSTAR.exe; putdata $T/$WIN
  ( cd $T && rm -f $T/$WIN.zip && zip -qr $T/$WIN.zip $WIN ) && cp $T/$WIN.zip $DIST/$WIN.zip
fi
[ -z "$COPY_TO" ] || { mkdir -p "$COPY_TO"; [ $WHAT = windows ] || cp $DIST/$LIN.tar.gz "$COPY_TO"/; [ $WHAT = linux ] || cp $DIST/$WIN.zip "$COPY_TO"/; }
ls -l $DIST/*.tar.gz $DIST/*.zip | awk '{print $5, $9}'
echo "done. Now: git add -f pc/dist pc/BUGSTAR.exe pc/objwin/game.stamp && git commit && push"
