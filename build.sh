#!/bin/bash
# Build bin/harbour-cubetimer on the phone with the user-space toolchain (no moc needed).
set -e
S=/home/defaultuser/devtools/sysroot
cd "$(dirname "$0")/src"
export PATH=$S/usr/bin:$PATH LD_LIBRARY_PATH=$S/usr/lib64:/usr/lib64
INC="-I$S/usr/include/sailfishapp -I$S/usr/include/qt5 -I$S/usr/include/qt5/QtCore -I$S/usr/include/qt5/QtGui -I$S/usr/include/qt5/QtQml -I$S/usr/include/qt5/QtQuick"
OBJ=""
for f in main.cpp nativescrambler.cpp $(ls scramble/*.cpp | grep -v cli.cpp); do
  o="../build/$(echo $f | tr / _ | sed 's/\.cpp$/.o/')"
  mkdir -p ../build
  g++ --sysroot=$S -B$S/usr/bin/ -std=gnu++11 -O2 -fPIC -pthread $INC -c $f -o $o
  OBJ="$OBJ $o"
done
g++ --sysroot=$S -B$S/usr/bin/ -pthread -o ../bin/harbour-cubetimer $OBJ -L/usr/lib64 -lsailfishapp -lQt5Quick -lQt5Qml -lQt5Gui -lQt5Core -Wl,-rpath,/usr/lib64
chown $(stat -c %u:%g main.cpp) ../bin/harbour-cubetimer
ls -la ../bin/harbour-cubetimer
