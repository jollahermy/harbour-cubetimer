#!/bin/bash
# Build bin/harbour-cubetimer on the phone with the user-space toolchain (no moc needed).
set -e
S=/home/defaultuser/devtools/sysroot
cd "$(dirname "$0")/src"
export PATH=$S/usr/bin:$PATH LD_LIBRARY_PATH=$S/usr/lib64:/usr/lib64
INC="-isystem $S/usr/include/sailfishapp -isystem $S/usr/include/qt5 -isystem $S/usr/include/qt5/QtCore -isystem $S/usr/include/qt5/QtGui -isystem $S/usr/include/qt5/QtQml -isystem $S/usr/include/qt5/QtQuick"   # -isystem: no warnings from Qt headers
OBJ=""
# Only modules that passed test/native_diff.sh go into the app; s333 is a stub until verified.
MODULES="${MODULES:-scramble/rng.cpp scramble/scramble.cpp scramble/s222.cpp scramble/spyr.cpp scramble/sskb.cpp scramble/ssq1.cpp scramble/s333_stub.cpp}"
for f in main.cpp nativescrambler.cpp $MODULES; do
  o="../build/$(echo $f | tr / _ | sed 's/\.cpp$/.o/')"
  mkdir -p ../build
  g++ --sysroot=$S -B$S/usr/bin/ -std=gnu++11 -O2 -Wall -Wextra -fPIC -pthread $INC -c $f -o $o
  OBJ="$OBJ $o"
done
g++ --sysroot=$S -B$S/usr/bin/ -pthread -o ../bin/harbour-cubetimer $OBJ -L/usr/lib64 -lsailfishapp -lQt5Quick -lQt5Qml -lQt5Gui -lQt5Core -Wl,-rpath,/usr/lib64
chown $(stat -c %u:%g main.cpp) ../bin/harbour-cubetimer
ls -la ../bin/harbour-cubetimer
