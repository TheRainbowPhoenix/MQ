# MQ: Calculator emulator

Project description is TODO.

```bash
git clone --recursive https://git.planet-casio.com/Lephenixnoir/mq
```

![](misc/screenshot.png)

## Building for Linux

Clone this repository recursively. Also install [Azur](https://git.planet-casio.com/Lephenixnoir/Azur) for the GUI.

```bash
# Path where you installed Azur for Linux:
% export AZUR_PATH_linux="$HOME/.local"

% cmake -B build-linux -DAZUR_PLATFORM=linux \
    -DCMAKE_BUILD_TYPE=[Debug|Release] \
    [-DMQ_DISABLE_OPTIMIZATIONS=1] \
    [-DMQ_PROFILING_GPROF=1] \
    [-DMQ_PROFILING_TRACY=1]
% make -C build-linux
```

Optimizations are enabled in all modes by default, as the emulator is almost unusable without. Debug mode just turns off LTO. To fully disable optimizatoins, add `-DMQ_DISABLE_OPTIMIZATIONS`.

Debug modes can enable profiling.
- Select `-DMQ_PROFILING_GPROF` to profile with gprof; running the executable will produce `gmon.out` in the current folder.
- Select `-DMQ_PROFILING_TRACY` to enable [tracy](https://github.com/wolfpld/tracy), which must be cloned in `3rdparty/tracy`.

## Building for emscripten

Clone this repository recursively. Also install [Azur](https://git.planet-casio.com/Lephenixnoir/Azur) for the GUI.

```bash
# Path where you installed Azur for emscripten:
% export AZUR_PATH_emscripten="$HOME/.prefix-emscripten"

% emcmake cmake -B build-emscripten -D AZUR_PLATFORM=emscripten
% make -C build-emscripten
```

In order to test you'll need to serve the files somewehre, as the main `.html` file won't be able to load the others with a `file://` URL.

```bash
% python -m http.server -d build-emscripten 8000
% firefox http://0.0.0.0:8000
```

## Profiling on Linux

MQ supports profiling with [Tracy](https://github.com/wolfpld/tracy).

After cloning, build the Tracy server. The GUI uses [nativefiledialogs-extended](https://github.com/btzy/nativefiledialog-extended) for file dialogs, which tries to use the XDG portal by default on Linux. If this doesn't work for you (i.e. you get an error message when using open/save file features in the GUI), try to configure with `-DGTK_FILESELECTOR=1`.

```bash
git submodule update --init --recursive
cd 3rdparty/tracy
# Set a cache for both profiler and capture so the libraries don't get rebuilt
export CPM_SOURCE_CACHE=$(realpath .)/.cpm-cache
# Build the live profiler
cmake -B profiler/build -S profiler -DCMAKE_BUILD_TYPE=Release
cmake --build profiler/build --config Release
# Build the capture program (analysis after-the-fact)
cmake -B capture/build -S capture -DCMAKE_BUILD_TYPE=Release
cmake --build capture/build --config Release
```

You can run Tracy in one of two ways:

1. **Live**: run `profiler/build/tracy-profiler`. Start MQ then click "Connect". You can then use the profiler live, pause, etc.
2. **Offline**: run `capture/build/tracy-capture -o [FILE]`. Start MQ and run some code. Quit MQ then interrupt the capture; this leaves you with a trace in `FILE`. Run `profiler/build/tracy-profiler` and choose "Open saved trace" to open the file.

Profiling TODOs:
- Assign fixed cores with high priority for the process.
- Set these cores to a fixed frequency.
- Once threads, name threads (see doc @2.4)

## List of tested programs

This is a list of _some_ (not all) programs that were tested.

fx-series programs (fx-9860G, G-II, G-III, Graph 35/75/85 +/+E/+E II):

| **Program** | **Functionality** | **Performance** | Remarks |
|-------------|-------------------|-----------------|---------|
| [Jetpack Joyride](https://www.planet-casio.com/Fr/programmes/programme2749-1-jetpack-joyride-drakalex007-jeux-add-ins.html) by Drakalex007 | Playable | Way too fast! | No save. Selects SH4 API. |

fx-CG programs (fx-CG 10/20/50, Prizm, Graph 90+E, fx-CG 100, Graph Math+):

| **Program** | **Functionality** | **Performance** | Remarks |
|-------------|-------------------|-----------------|---------|
| [gintctl](https://git.planet-casio.com/Lephenixnoir/gintctl) by Lephe | Most things work | OK | This is used to test hardware features. |
| [Gravity Duck](https://www.planet-casio.com/Fr/programmes/programme1856-1-gravityduck-pierrotll-jeux-add-ins.html) | Playable | OK | No save. Lots of NULL accesses; some are bugs from original program. IIRC crashes upon death in the later levels. Syscall-based. |
| [Duet](https://www.planet-casio.com/Fr/programmes/programme4173-1-duet-yatis-lephe-jeux-add-ins.html) by Yatis and Lephe | Playable | Slow | No save. Slow to unplayable depending on machines, but works decent on circuit10's emulator. |
| [Mario Kart](https://www.planet-casio.com/Fr/forums/topic17121-1-mario-kart-game-work-in-progress.html) by circuit10 | Playable | Slow | N/A |
| [Cube Field](https://www.planet-casio.com/Fr/programmes/programme1906-1-cubefield-pierrotll-jeux-add-ins.html) by PierrotLL | Playable | Way too fast | N/A |
| [OutRun](https://www.planet-casio.com/Fr/programmes/programme4225-1-outrun-for-graph-90e-slyvtt-jeux-actionsport.html) by SlyVTT | Playable | Variable | Upon first turn, suddenly slows down and background blinks. |
| [Demineur winXP](https://www.planet-casio.com/Fr/programmes/programme2377-last-demineur-winxp-smashmaster-jeux-add-ins.html) by Smashmaster | Playable* | Slow(?) | First game work, but second crash |
| [Sudoku solver](https://www.planet-casio.com/Fr/programmes/programme2446-last-sudoku-solver-lancelot-jeux-add-ins.html) by Lancelot | Not Boot | KO | missing `%d39` syscall support |
| [Flappy bird color](https://www.planet-casio.com/Fr/programmes/programme2428-last-flappy-bird-color-lancelot-jeux-add-ins.html) by Lancelot | Not Boot | KO | missing `%d39` syscall support |
| [Cgsnake](https://www.planet-casio.com/Fr/programmes/programme1902-last-cgsnake-eiyeron-jeux-add-ins.html) by Eiyeron | Not Boot | KO | Do not crash, just display nothing |
| [Sonic](https://www.planet-casio.com/Fr/programmes/programme1904-last-sonic-smashmaster-jeux-add-ins.html) by Smashmaster | Playable | OK | Too fast |
| [Falldown colors](https://www.planet-casio.com/Fr/programmes/programme2060-last-falldown-colors-dodormeur-jeux-add-ins.html) by Dodormeur | Not Boot | KO | Use PRIZM VRAM area |
| [Obliterate](https://www.planet-casio.com/Fr/programmes/programme2341-last-obliterate-kermmartian-jeux-add-ins.html) by planetcasio | Not Boot | KO | missing support of `%12b` syscall |
| [Meta Ball](https://www.planet-casio.com/Fr/programmes/programme2344-last-meta-ball-lancelot-jeux-add-ins.html) by Lancelot | Playable | Slow | Too slow. Invalid write sometime |
| [Life game cg-20](https://www.planet-casio.com/Fr/programmes/programme1853-last-life-game-cg-20-smashmaster-jeux-add-ins.html) by Smashmaster | No display | KO | Use PRIZM VRAM area |
| [After Burner](https://www.planet-casio.com/Fr/programmes/programme4238-last-after-burner-lephenixnoir-jeux-add-ins.html) by Lephe | Perfect | OK | Perfect! |
| [AST3 C](https://www.planet-casio.com/Fr/programmes/programme4100-last-ast3-c-tituya-jeux-add-ins.html) by Tituya | Playable | OK | Broken first menu which not display frame entierly |
| [Super Mario 3D](https://www.planet-casio.com/Fr/programmes/programme4343-last-super-mario-3d-farhi-jeux-add-ins.html) by Farhi | Perfect | OK | Perfect! |
| [Maverick Bird](https://www.planet-casio.com/Fr/programmes/programme4268-last-maverick-bird-lephenixnoir-jeux-add-ins.html) by Lephe | Perfect | OK | Perfect! |
| [Mario 3D](https://www.planet-casio.com/Fr/programmes/programme4411-last-mario-3d-games-jeux-add-ins.html) by Games | Playable | OK | Maybe graphical glitch in middle of the screen(?) |

fx-CP programs:

None so far.
