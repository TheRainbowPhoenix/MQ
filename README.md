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

## Compatibility table

<!-- compat table start -->
<code>**🟢 Playable** (25.93%)</code> - Games that can be completed with playable performance and no game breaking glitches</br>
<code>**🟠 Ingame** (40.74%)</code> - Games that either can't be finished, have serious glitches or have insufficient performance</br>
<code>**🟤 Intro** (3.70%)</code> - Games that display image but don't make it past the menus</br>
<code>**🔴 Loadable** (7.41%)</code> - Games that display a black screen with a framerate on the window's title</br>
<code>**⚫ Nothing** (22.22%)</code> - Games that don't initialize properly, not loading at all and/or crashing the emulator</br>

| Addin | Status | Updated | Notes |
|-------|:------:|:-------:|-------|
| [`Mario Kart`](https://www.planet-casio.com/Fr/forums/topic17121-1-mario-kart-game-work-in-progress.html) by Circuit10 | <code>**🟢 Playable**</code> | `2025/08/21` | -Playable |
| [`After Burner`](https://www.planet-casio.com/Fr/programmes/programme4238-last-after-burner-lephenixnoir-jeux-add-ins.html) by Lephe | <code>**🟢 Playable**</code> | `2025/08/21` | -Perfect! |
| [`Super Mario 3D`](https://www.planet-casio.com/Fr/programmes/programme4343-last-super-mario-3d-farhi-jeux-add-ins.html) by Farhi | <code>**🟢 Playable**</code> | `2025/08/21` | -Perfect! |
| [`Maverick Bird`](https://www.planet-casio.com/Fr/programmes/programme4268-last-maverick-bird-lephenixnoir-jeux-add-ins.html) by Lephe | <code>**🟢 Playable**</code> | `2025/08/21` | -Perfect! |
| [`Dumb Clicker Goto Edition`](https://git.planet-casio.com/kdx/dcge) by Kikoodx | <code>**🟢 Playable**</code> | `2025/08/21` | -we can click efficiently, nice |
| [`Momento`](https://git.sr.ht/~kikoodx/momento) by Kikoodx | <code>**🟢 Playable**</code> | `2025/08/21` | -Perfect(?) |
| [`Teh Moon Show`](https://www.planet-casio.com/Fr/programmes/programme4240-last-teh-moon-show-massena-jeux-add-ins.html) by Massena | <code>**🟢 Playable**</code> | `2025/08/21` | -cannot restart a game, same on real hardware</br>- need to properly handle game exit |
| [`gintctl`](https://git.planet-casio.com/Lephenixnoir/gintctl) by Lephe | <code>**🟠 Ingame**</code> | `2025/08/21` | -Most things work</br>- This is used to test hardware features |
| [`Gravity Duck`](https://www.planet-casio.com/Fr/programmes/programme1856-1-gravityduck-pierrotll-jeux-add-ins.html) by PierrotLL | <code>**🟠 Ingame**</code> | `2025/08/21` | -No save</br>- Lots of NULL accesses; some are bugs from original program.</br>- IIRC crashes upon death in the later levels.</br>- Syscall-based. |
| [`Duet`](https://www.planet-casio.com/Fr/programmes/programme4173-1-duet-yatis-lephe-jeux-add-ins.html) by Yatis and Lephe | <code>**🟠 Ingame**</code> | `2025/08/21` | -No save</br>- Slow to unplayable depending on machines, but works decent on circuit10's emulator. |
| [`Cube Field`](https://www.planet-casio.com/Fr/programmes/programme1906-1-cubefield-pierrotll-jeux-add-ins.html) by PierrotLL | <code>**🟠 Ingame**</code> | `2025/08/21` | -Way too fast |
| [`OutRun`](https://www.planet-casio.com/Fr/programmes/programme4225-1-outrun-for-graph-90e-slyvtt-jeux-actionsport.html) by SlyvTT | <code>**🟠 Ingame**</code> | `2025/08/21` | -Upon first turn, suddenly slows down and background blinks. |
| [`Sonic`](https://www.planet-casio.com/Fr/programmes/programme1904-last-sonic-smashmaster-jeux-add-ins.html) by Smashmaster | <code>**🟠 Ingame**</code> | `2025/08/21` | -Too fast |
| [`Meta Ball`](https://www.planet-casio.com/Fr/programmes/programme2344-last-meta-ball-lancelot-jeux-add-ins.html) by Lancelot | <code>**🟠 Ingame**</code> | `2025/08/21` | -Too slow.</br>- Invalid write sometime |
| [`AST3 C`](https://www.planet-casio.com/Fr/programmes/programme4100-last-ast3-c-tituya-jeux-add-ins.html) by Tituya | <code>**🟠 Ingame**</code> | `2025/08/21` | -Broken first menu which not display frame entierly</br>- Rest of the game is playable |
| [`Mario 3D`](https://www.planet-casio.com/Fr/programmes/programme4411-last-mario-3d-games-jeux-add-ins.html) by Games | <code>**🟠 Ingame**</code> | `2025/08/21` | -Maybe graphical glitch in middle of the screen(?) |
| [`Frozen Frenzy`](https://www.planet-casio.com/Fr/programmes/programme4192-last-frozen-frenzy-massena-jeux-add-ins.html) by Massena | <code>**🟠 Ingame**</code> | `2025/08/21` | -too fast |
| [`Chaos Drop`](https://git.planet-casio.com/Lephenixnoir/chaos-drop) by Lephe | <code>**🟠 Ingame**</code> | `2025/08/21` | -too many NULL page write warning</br>- too slow |
| [`Demineur winXP`](https://www.planet-casio.com/Fr/programmes/programme2377-last-demineur-winxp-smashmaster-jeux-add-ins.html) by Smashmaster | <code>**🟤 Intro**</code> | `2025/08/21` | -Slow(?)</br>- First game work, but second crash |
| [`Cgsnake`](https://www.planet-casio.com/Fr/programmes/programme1902-last-cgsnake-eiyeron-jeux-add-ins.html) by Eiyeron | <code>**🔴 Loadable**</code> | `2025/08/21` | -Prizm</br>- Do not crash, just display nothing |
| [`Life game cg-20`](https://www.planet-casio.com/Fr/programmes/programme1853-last-life-game-cg-20-smashmaster-jeux-add-ins.html) by Smashmaster | <code>**🔴 Loadable**</code> | `2025/08/21` | -Prizm</br>- black screen</br>- Use PRIZM VRAM area |
| [`Sudoku solver`](https://www.planet-casio.com/Fr/programmes/programme2446-last-sudoku-solver-lancelot-jeux-add-ins.html) by Lancelot | <code>**⚫ Nothing**</code> | `2025/08/21` | -Prizm</br>- missing `%d39` syscall support |
| [`Flappy bird color`](https://www.planet-casio.com/Fr/programmes/programme2428-last-flappy-bird-color-lancelot-jeux-add-ins.html) by Lancelot | <code>**⚫ Nothing**</code> | `2025/08/21` | -Prizm</br>- missing `%d39` syscall support |
| [`Falldown colors`](https://www.planet-casio.com/Fr/programmes/programme2060-last-falldown-colors-dodormeur-jeux-add-ins.html) by Dodormeur | <code>**⚫ Nothing**</code> | `2025/08/21` | -Use PRIZM VRAM area |
| [`Obliterate`](https://www.planet-casio.com/Fr/programmes/programme2341-last-obliterate-kermmartian-jeux-add-ins.html) by planetcasio | <code>**⚫ Nothing**</code> | `2025/08/21` | -Prizm</br>- missing support of `%12b` syscall |
| [`Tetrizm`](https://www.cemetech.net/downloads/files/659/x659) by Planetcasio | <code>**⚫ Nothing**</code> | `2025/08/21` | -CG50 port</br>- missing support for `%12b` to boot |
| [`OpenJazz`](https://www.planet-casio.com/Fr/programmes/programme2390-last-openjazz-jackrabbit-programmern-jeux-add-ins.html) by Programmern | <code>**⚫ Nothing**</code> | `2025/08/21` | -missing syscall `%921 EnableColor()` syscall to boot</br>- missing syscall `%2a3 FrameColor()` syscall to boot</br>- missing syscall `%18f9 PrintXY()` syscall to boot |

<!-- compat table end -->
