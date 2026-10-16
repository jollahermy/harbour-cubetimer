# Cube Timer

**English** | [繁體中文](README.zh-TW.md)

A speedcubing timer for [Sailfish OS](https://sailfishos.org/), in the spirit of csTimer: hold to
start, release to run, tap to stop. Every solve is stored on the device, and every event gets a
random-state scramble generated natively on the phone.

<p>
  <img src="docs/screenshots/timer.png" alt="Timer screen with scramble and live statistics" width="270">
  <img src="docs/screenshots/history.png" alt="History table with session bests" width="270">
</p>

## Features

- Hold-to-start timer: red while you hold, green when ready, release to start, tap to stop
- 13 events: 2x2, 3x3, 4x4, 5x5, 6x6, 7x7, 3x3 OH, 3x3 BLD, Pyraminx, Skewb, Megaminx, Square-1, Clock
- Random-state scrambles per event, generated natively in C++ (see [Scrambles](#scrambles))
- Swipe the scramble sideways to skip it or step back; long-press to copy it
- Sessions per event: create, rename, switch, delete
- Penalties on the last solve from the pull-down menu: +2, DNF, Delete, OK
- Live time, ao5, ao12 and ao100; a history table with session bests highlighted
- Fully offline: no account, no network use, no special permissions

## Install

Cube Timer is built for Sailfish OS (aarch64). The Harbour package is `harbour-cubetimer`; the
RPM is validated with the Harbour RPM validator before each release (current version 1.8-1).

To build from source on the device you need the Sailfish user-space toolchain (Qt 5.6, libsailfishapp):

```sh
./build.sh                        # compiles bin/harbour-cubetimer (SYSROOT=<path> to override the toolchain)
rpm/harbour-cubetimer.spec        # packages the prebuilt binary plus share/ into an RPM
```

`build.sh` expects a Sailfish sysroot next to the repository (`../sysroot`) unless `SYSROOT` is set.
The QML and JavaScript under `share/harbour-cubetimer/qml/` are loaded at start, so UI-only changes
need no rebuild.

## Scrambles

`src/scramble/` is a C++ port of the scramble generators of
[csTimer](https://github.com/cs0x7f/cstimer) (commit `2547d82`), including the min2phase 3x3x3 solver
and the 4x4x4 reduction solver by Shuang Chen (cs0x7f) and contributors. For the same random seed
the port reproduces csTimer's output exactly; this was verified event by event against csTimer's
JavaScript. These are csTimer-equivalent scramblers. They are **not** the WCA's own TNoodle, so do
not describe the output as official WCA scrambles. The Pyraminx random-state filter follows WCA
Regulation 4b3f.

Square-1 and Clock scrambles have not been play-tested by hand.

## Statistics

ao5, ao12 and ao100 drop the best and worst 5 % (rounded up) of the solves in the window, and
follow the usual DNF rules. WCA itself defines no official ao100, so call these "WCA-style
averages".

## AI-assisted development

Cube Timer was written by an AI assistant (Hermy, the account `jollahermy`) working with her user,
who directs the project and tests it on a real phone. Large parts of the code and the documentation
were produced this way. The source is published under the GPL so you can read exactly what the app
does.

## License

GPL-3.0, see [LICENSE](LICENSE). Third-party code:

- csTimer scramble generators and min2phase (GPL-3.0), Shuang Chen (cs0x7f) and contributors
- ISAAC PRNG, MIT License, Copyright (c) 2012 Yves-Marie K. Rinquin
- Nunito font, SIL Open Font License 1.1 (`share/harbour-cubetimer/qml/fonts/OFL.txt`)
