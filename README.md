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
    [-DMQ_PROFILING_TRACY=1] \
    [-DMQ_VIDEO_FFMPEG=0]
% make -C build-linux
```

Optimizations are enabled in all modes by default, as the emulator is almost unusable without. Debug mode just turns off LTO. To fully disable optimizations, add `-DMQ_DISABLE_OPTIMIZATIONS`.

Debug modes can enable profiling.
- Select `-DMQ_PROFILING_GPROF` to profile with gprof; running the executable will produce `gmon.out` in the current folder.
- Select `-DMQ_PROFILING_TRACY` to enable [tracy](https://github.com/wolfpld/tracy), which must be cloned in `3rdparty/tracy`.

Linux builds enable video capture with [ffmpeg](https://ffmpeg.org/) libraries by default. This requires `libswscale`, `libavutil`, `libavformat` and `libavcodec`. If you don't have these libraries you can disable the feature with `-DMQ_VIDEO_FFMPEG=0`.

## Building for emscripten

Clone this repository recursively. Also install [Azur](https://git.planet-casio.com/Lephenixnoir/Azur) for the GUI.

```bash
# Path where you installed Azur for emscripten:
% export AZUR_PATH_emscripten="$HOME/.prefix-emscripten"

% emcmake cmake -B build-emscripten -D AZUR_PLATFORM=emscripten
% make -C build-emscripten
```

In order to test you'll need to serve the files somewehre, as the main `.html` file won't be able to load the others with a `file://` URL. You also need a server that provides appropriate COOP and COEP headers, see [the emscripten docs](https://emscripten.org/docs/porting/pthreads.html#pthreads-support) for details. A local server satisfying these requirement is given in `tools`.

```bash
% python3 tools/local-http-server.py build-emscripten 8000
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

This section is generated automatically based on [this YAML summary](docs/compatibility.yaml).

<!-- compat table start -->
Total programs: `all(59)`:  `fxcg50(27)` `fx9860g_sh4(23)` `fxcg20(6)` `fx9860g_sh3(3)` `fxcp400_2.01.2(0)` `fxcg100_2.00(0)` `fx9860g3(0)`

<code>**🟢 Complete** (47.46%)</code> - Programs where most features work and games that can be completed with reasonable performance</br>
<code>**🟠 Usable** (20.34%)</code> - Programs that can be used and games that can be played but have missing features, glitches, or bad performance</br>
<code>**🟤 Frame** (10.17%)</code> - Programs that display at least one frame before failing</br>
<code>**⚫ Nothing** (22.03%)</code> - Programs that crash, loop, or get stuck before displaying their first frame</br>


| Addin | Status | Models | Updated | Notes |
|-------|:------:|:------:|:-------:|-------|
| [Gravity Duck](https://www.planet-casio.com/Fr/programmes/programme1856-1-gravityduck-pierrotll-jeux-add-ins.html) by PierrotLL | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - Graphical animation bug</br>- Lots of NULL accesses; some are bugs from original program.</br>- Syscall-based |
| [Duet](https://www.planet-casio.com/Fr/programmes/programme4173-1-duet-yatis-lephe-jeux-add-ins.html) by Yatis and Lephe | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - the emulator doesn't support screen rotation</br>- miss automatic arrows key bind with `KEY_0` and `KEY_7` |
| [Mario Kart](https://www.planet-casio.com/Fr/forums/topic17121-1-mario-kart-game-work-in-progress.html) by Circuit10 | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - Pefect?! |
| [Cube Field](https://www.planet-casio.com/Fr/programmes/programme1906-1-cubefield-pierrotll-jeux-add-ins.html) by PierrotLL | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - Perfect?! |
| [OutRun](https://www.planet-casio.com/Fr/programmes/programme4225-1-outrun-for-graph-90e-slyvtt-jeux-actionsport.html) by SlyvTT | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - Perfect?! |
| [After Burner](https://www.planet-casio.com/Fr/programmes/programme4238-last-after-burner-lephenixnoir-jeux-add-ins.html) by Lephe | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - Perfect?!</br>- Throttle doesn't work? |
| [Super Mario 3D](https://www.planet-casio.com/Fr/programmes/programme4343-last-super-mario-3d-farhi-jeux-add-ins.html) by Farhi | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - Perfect?! |
| [Maverick Bird](https://www.planet-casio.com/Fr/programmes/programme4268-last-maverick-bird-lephenixnoir-jeux-add-ins.html) by Lephe | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - Perfect?! |
| [Mario 3D](https://www.planet-casio.com/Fr/programmes/programme4411-last-mario-3d-games-jeux-add-ins.html) by Games | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - Perfect?! |
| [Dumb Clicker Goto Edition](https://git.planet-casio.com/kdx/dcge) by Kikoodx | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - we can click efficiently, nice |
| [Momento](https://git.sr.ht/~kikoodx/momento) by Kikoodx | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - Perfect?! |
| [Teh Moon Show](https://www.planet-casio.com/Fr/programmes/programme4240-last-teh-moon-show-massena-jeux-add-ins.html) by Massena | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - cannot restart a game, same on real hardware</br>- need to properly handle game exit |
| [Chaos Drop](https://git.planet-casio.com/Lephenixnoir/chaos-drop) by Lephe | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - too many NULL page write warning |
| [Geometry Dash](https://www.planet-casio.com/Fr/programmes/programme3115-last-geometry-dash-fife86-jeux-add-ins.html) by Fife86 | <code>🟢 Complete</code> | `fx9860g_sh4` | 2026-02-21 | - Perfect?! |
| [Gravity Duck](https://www.planet-casio.com/Fr/programmes/programme1795-last-gravity-duck-pierrotll-jeux-add-ins.html) by PierrotLL | <code>🟢 Complete</code> | `fx9860g_sh4` | 2026-02-21 | - Perfect?! |
| [2048 delux](https://www.planet-casio.com/Fr/programmes/programme2597-last-2048-delux-kirafi-jeux-add-ins.html) by Kirafi | <code>🟢 Complete</code> | `fx9860g_sh4` | 2025-09-03 | - save file loading is broken |
| [Mipjabok](https://www.planet-casio.com/Fr/programmes/programme2303-last-mipjabok-louloux-jeux-add-ins.html) by Louloux | <code>🟢 Complete</code> | `fx9860g_sh4` | 2026-02-21 | - Perfect?! |
| [Bomberman](https://www.planet-casio.com/Fr/programmes/programme2242-1-bomberman-dodormeur-jeux-add-ins.html) by Dodormeur | <code>🟢 Complete</code> | `fx9860g_sh4` | 2025-09-03 | - Perfect?! |
| [FxLibcTest](https://git.planet-casio.com/Lephenixnoir/FxLibcTest) by Lephenixnoir | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - Perfect?! |
| [Arena](https://www.planet-casio.com/Fr/programmes/programme3152-1-arena-lephenixnoir-jeux-add-ins.html) by Lephenixnoir | <code>🟢 Complete</code> | `fx9860g_sh4` | 2026-02-21 | - Perfect?! |
| [Azuray](https://git.planet-casio.com/Lephenixnoir/azuray) by Lephenixnoir | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - Perfect?!</br>- too many unhandled read/write `e500deb8` |
| [BosonX](https://www.planet-casio.com/Fr/programmes/programme4566-1-boson-x-yatis-lephe-jeux-add-ins.html) by Lephenixnoir | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - Perfect?! |
| [CG Demo](https://www.planet-casio.com/Fr/programmes/programme4234-1-ccjdemo-slyvtt-jeux-concours-casio.html) by Slyvtt | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - Perfect?! |
| [IpodSpike](https://www.planet-casio.com/Fr/programmes/programme2634-1-ipod-spikebird-kirafi-jeux-add-ins.html) by Kirafi | <code>🟢 Complete</code> | `fx9860g_sh4` | 2026-02-21 | - Perfect?! |
| [IpodGraviton](https://www.planet-casio.com/Fr/programmes/programme2594-1-ipod-graviton-kirafi-jeux-add-ins.html) by Kirafi | <code>🟢 Complete</code> | `fx9860g_sh4` | 2026-02-21 | - Perfect?! |
| [SeaRush](https://www.planet-casio.com/Fr/programmes/programme3026-1-searush-kirafi-jeux-add-ins.html) by Kirafi | <code>🟢 Complete</code> | `fx9860g_sh4` | 2026-02-21 | - Perfect?! |
| [IpodSpin](https://www.planet-casio.com/Fr/programmes/programme3053-1-ipod-spin-kirafi-jeux-add-ins.html) by Kirafi | <code>🟢 Complete</code> | `fx9860g_sh4` | 2026-02-21 | - Perfect?! |
| [PythonExtra](https://git.planet-casio.com/Lephenixnoir/PythonExtra/releases) by Lephenixnoix | <code>🟢 Complete</code> | `fxcg50` | 2026-02-21 | - Perfect?! |
| [gintctl](https://git.planet-casio.com/Lephenixnoir/gintctl) by Lephe | <code>🟠 Usable</code> | `fxcg50` | 2026-02-21 | - Most things work</br>- This is used to test hardware features |
| [HorlogeSH4](https://www.planet-casio.com/Fr/programmes/programme2606-1-horloge-sh4-lephenixnoir-utilitaires-add-ins.html) by Lephe | <code>🟠 Usable</code> | `fx9860g_sh4` | 2026-02-21 | - displays and works in real-time</br>- blocked by `%14d Bdisp_AreaReverseVRAM` in date/time edition menu |
| [Sonic](https://www.planet-casio.com/Fr/programmes/programme1904-last-sonic-smashmaster-jeux-add-ins.html) by Smashmaster | <code>🟠 Usable</code> | `fxcg50` | 2026-02-21 | - Lephenixnoir's fxcg50 port</br>- require the throtle to be playable, too fast othewise |
| [Meta Ball](https://www.planet-casio.com/Fr/programmes/programme2344-last-meta-ball-lancelot-jeux-add-ins.html) by Lancelot | <code>🟠 Usable</code> | `fxcg50` | 2026-02-21 | - Lephenixnoir's fxcg50 port</br>- Too slow.</br>- Invalid write sometime (`0xaeffffc2`) |
| [AST3 C](https://www.planet-casio.com/Fr/programmes/programme4100-last-ast3-c-tituya-jeux-add-ins.html) by Tituya | <code>🟠 Usable</code> | `fxcg50` | 2026-02-21 | - Broken first menu which not display frame entierly</br>- Rest of the game is playable</br>- a lot of broken frame in the level selection |
| [Frozen Frenzy](https://www.planet-casio.com/Fr/programmes/programme4192-last-frozen-frenzy-massena-jeux-add-ins.html) by Massena | <code>🟠 Usable</code> | `fxcg50` | 2025-08-21 | - require throttle to be playable, too fast otherwise |
| [Angry Bird](https://www.planet-casio.com/Fr/programmes/programme1943-last-angry-birds-louloux-jeux-add-ins.html) by LouLoux | <code>🟠 Usable</code> | `fx9860g_sh4` | 2025-08-27 | - too fast |
| [Evasion](https://www.planet-casio.com/Fr/programmes/programme2082-last-evasion-surv-dodormeur-jeux-add-ins.html) by Dodormeur | <code>🟠 Usable</code> | `fx9860g_sh4` | 2026-02-21 | - missing syscall `%462 GetAppName()` |
| [gintctl](https://git.planet-casio.com/Lephenixnoir/gintctl) by Lephe | <code>🟠 Usable</code> | `fx9860g_sh4` | 2026-02-21 | - libprof tests freeze (no crash)</br>- missing instruction `macw` in `CPU parallelism` |
| [Destiny](https://www.planet-casio.com/Fr/programmes/programme2684-last-destiny-dodormeur-jeux-add-ins.html) by Dodormeur | <code>🟠 Usable</code> | `fx9860g_sh4` | 2025-09-03 | - missing `%118 Timer_Install` support |
| [EditeurHexa](https://www.planet-casio.com/Fr/programmes/programme4267-1-editeur-hexadecimal-lephenixnoir-utilitaires-add-ins.html) by Lephenixnoir | <code>🟠 Usable</code> | `fxcg50` | 2026-02-21 | - missing support to `Bfile_GetPos()`</br>- weird behaviour with empty folder |
| [IpodDextris](https://www.planet-casio.com/Fr/programmes/programme2574-1-ipod-dextris-kirafi-jeux-add-ins.html) by Kirafi | <code>🟠 Usable</code> | `fx9860g_sh4` | 2026-02-21 | - too fast, use the throttle with 120FPS |
| [Demineur winXP](https://www.planet-casio.com/Fr/programmes/programme2377-last-demineur-winxp-smashmaster-jeux-add-ins.html) by Smashmaster | <code>🟤 Frame</code> | `fxcg50` | 2026-02-21 | - fxcg50 port</br>- First game work, but second game (first click) crash (double fault) |
| [Mario land ce](https://www.planet-casio.com/Fr/programmes/programme1064-last-mario-land-ce-bebe-vador-jeux-add-ins.html) by Bebe-vador | <code>🟤 Frame</code> | `fx9860g_sh3` | 2026-02-21 | - first frame of the game</br>- missing support for `%3ed` `Interrupt_SetOrClrStatusFlags()` |
| [OrtonSH4](https://www.planet-casio.com/Fr/programmes/programme1455-1-orton-pierrotll-jeux-add-ins.html) by PierrotLL | <code>🟤 Frame</code> | `fx9860g_sh4` | 2026-02-21 | - stuck with the splash-screen |
| [Hard game](https://www.planet-casio.com/Fr/programmes/programme1456-1-hard-game-pierrotll-jeux-add-ins.html) by PierrotLL | <code>🟤 Frame</code> | `fx9860g_sh4` | 2025-08-28 | - stuck with the splash-screen |
| [DarkLaby](https://www.planet-casio.com/Fr/programmes/programme2158-last-dark-laby-louloux-jeux-add-ins.html) by LouLoux | <code>🟤 Frame</code> | `fx9860g_sh4` | 2026-02-21 | - missing support for `%118 Timer_Install()` |
| [FruitNinja](https://www.planet-casio.com/Fr/programmes/programme2146-last-fruit-ninja-dark-storm-jeux-add-ins.html) by Dark Storm | <code>🟤 Frame</code> | `fx9860g_sh4` | 2026-02-21 | - freeze after save loading |
| [Sudoku solver](https://www.planet-casio.com/Fr/programmes/programme2446-last-sudoku-solver-lancelot-jeux-add-ins.html) by Lancelot | <code>⚫ Nothing</code> | `fxcg20` | 2025-08-21 | - Prizm</br>- missing `%d39` syscall support |
| [Flappy bird color](https://www.planet-casio.com/Fr/programmes/programme2428-last-flappy-bird-color-lancelot-jeux-add-ins.html) by Lancelot | <code>⚫ Nothing</code> | `fxcg20` | 2026-02-21 | - version fxcg50 (Lephenixoir)</br>- missing `%d39 KeyBoard_PRGM_GetKey()` syscall support |
| [Cgsnake](https://www.planet-casio.com/Fr/programmes/programme1902-last-cgsnake-eiyeron-jeux-add-ins.html) by Eiyeron | <code>⚫ Nothing</code> | `fxcg20` | 2026-02-21 | - Prizm</br>- Do not crash, just display nothing |
| [Falldown colors](https://www.planet-casio.com/Fr/programmes/programme2060-last-falldown-colors-dodormeur-jeux-add-ins.html) by Dodormeur | <code>⚫ Nothing</code> | `fxcg20` | 2025-08-21 | - Use PRIZM VRAM area |
| [Obliterate](https://www.planet-casio.com/Fr/programmes/programme2341-last-obliterate-kermmartian-jeux-add-ins.html) by planetcasio | <code>⚫ Nothing</code> | `fxcg20` | 2026-02-21 | - Prizm</br>- missing support of `%12b FKey_map()` syscall |
| [Life game cg-20](https://www.planet-casio.com/Fr/programmes/programme1853-last-life-game-cg-20-smashmaster-jeux-add-ins.html) by Smashmaster | <code>⚫ Nothing</code> | `fxcg20` | 2025-08-21 | - Prizm</br>- black screen</br>- Use PRIZM VRAM area |
| [Tetrizm](https://www.cemetech.net/downloads/files/659/x659) by Planetcasio | <code>⚫ Nothing</code> | `fxcg50` | 2026-02-21 | - CG50 port</br>- missing support for `%12b` to boot |
| [OpenJazz](https://www.planet-casio.com/Fr/programmes/programme2390-last-openjazz-jackrabbit-programmern-jeux-add-ins.html) by Programmern | <code>⚫ Nothing</code> | `fxcg50` | 2026-02-21 | - fxcg50 official port(?)</br>- missing syscall `%1b0b GetVRAMWorkBuffer()` to boot</br>- missing syscall `%921 EnableColor()` to boot</br>- missing syscall `%2a3 FrameColor()` to boot</br>- missing syscall `%18f9 PrintXY()` to boot |
| [FlappyBird](https://www.planet-casio.com/Fr/programmes/programme2424-last-flappy-bird-dark-storm-jeux-add-ins.html) by Dark storm | <code>⚫ Nothing</code> | `fx9860g_sh4` | 2026-02-21 | - missing `%d39 GetKey()` syscall support |
| [Doodle jump](https://www.planet-casio.com/Fr/programmes/programme1746-last-doodle-jump-kevkevvtt-jeux-add-ins.html) by Kevkevvtt | <code>⚫ Nothing</code> | `fx9860g_sh3` | 2026-02-21 | - SH3 with RevolutionFX</br>- execution crash |
| [Ball game](https://www.planet-casio.com/Fr/programmes/programme1397-1-ball-game-pierrotll-jeux-add-ins.html) by PierrotLL | <code>⚫ Nothing</code> | `fx9860g_sh3` | 2026-02-21 | - missing syscall `%24c Keyboard_IsSpecialKeyDown()`</br>- need SH3 timer support |
| [Atlantis](https://www.planet-casio.com/Fr/programmes/programme3024-1-atlantis-lephenixnoir-jeux-add-ins.html) by Lephenixnoir | <code>⚫ Nothing</code> | `fx9860g_sh4` | 2026-02-21 | - missing `%840 MCSGetDlen2()` syscall support |
| [IpodFall](https://www.planet-casio.com/Fr/programmes/programme2819-1-ipod-fallblocs-kirafi-jeux-add-ins.html) by Kirafi | <code>⚫ Nothing</code> | `fx9860g_sh4` | 2026-02-21 | - Instant crash after many invalid read</br>- any version of the addin crash |

<!-- compat table end -->
