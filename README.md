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
