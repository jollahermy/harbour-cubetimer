# Cube Timer

**English** | [繁體中文](README.zh-TW.md)

A speedcubing timer for [Sailfish OS](https://sailfishos.org/): hold to start, release to run, tap
to stop. Solves are stored on the device, and every event gets a random-state scramble generated
natively on the phone.

<p>
  <img src="docs/screenshots/timer.png" alt="Timer screen with scramble and live statistics" width="270">
  <img src="docs/screenshots/history.png" alt="History table with session bests" width="270">
</p>

## Features

- Hold-to-start timer: red while you hold, green when ready, release to start, tap to stop
- 13 events: 2x2, 3x3, 4x4, 5x5, 6x6, 7x7, 3x3 OH, 3x3 BLD, Pyraminx, Skewb, Megaminx, Square-1, Clock
- Random-state scrambles for every event, generated natively in C++ (see [Scrambles](#scrambles))
- Swipe the scramble sideways to skip it or step back; long-press to copy it
- Sessions per event: create, rename, switch, delete
- Penalties on the last solve from the pull-down menu: +2, DNF, Delete, OK
- Live time, ao5, ao12 and ao100 (the best and worst 5 %, rounded up, are dropped), and a history
  table with session bests highlighted
- Fully offline: no account, no network use, no special permissions

## Requirements

Sailfish OS 5.2 or newer on aarch64. Developed and tested on the Jolla Phone (2026).

## Build

On a device with the Sailfish user-space toolchain (Qt 5.6, libsailfishapp):

```sh
./build.sh                  # compiles bin/harbour-hermy-cubetimer (SYSROOT=<path> overrides the toolchain)
rpm/harbour-hermy-cubetimer.spec  # packages the binary and share/ into an RPM
```

`build.sh` looks for a Sailfish sysroot at `../sysroot` unless `SYSROOT` is set. The QML and
JavaScript under `share/harbour-hermy-cubetimer/qml/` are loaded at start, so UI-only changes need no
rebuild.

## Scrambles

`src/scramble/` is a C++ port of the scramble generators of
[csTimer](https://github.com/cs0x7f/cstimer) (commit `2547d82`), including the min2phase 3x3x3 solver
and the 4x4x4 reduction solver by Shuang Chen (cs0x7f) and contributors. For the same random seed
the port reproduces csTimer's output exactly. These are csTimer-equivalent scramblers, not the WCA's
own TNoodle. The Pyraminx random-state filter follows WCA Regulation 4b3f.

## AI-assisted development

Cube Timer was written by an AI assistant (Hermy, the account `jollahermy`) together with her user,
who directs the project and tests it on a real phone. The source is published under the GPL so you
can read exactly what the app does.

## License

GPL-3.0, see [LICENSE](LICENSE). Third-party code:

- csTimer scramble generators and min2phase (GPL-3.0), Shuang Chen (cs0x7f) and contributors
- ISAAC PRNG, MIT License, Copyright (c) 2012 Yves-Marie K. Rinquin
- Nunito font, SIL Open Font License 1.1 (`share/harbour-hermy-cubetimer/qml/fonts/OFL.txt`)
