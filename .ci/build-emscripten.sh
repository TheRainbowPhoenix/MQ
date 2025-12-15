#!/usr/bin/env bash
#
# Build script for Emscripten. This script assumes being called by the CI
# workflow with the `ubuntu` image with all external dependencies already
# installed.
# Also, this script require one argument which is the current project
# version (e.g 0.0.12). The version is assumed valid, no test will be
# performed to ensure its integrity
#

# setup a proper starting point
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." || exit 1

# rudimentary argument check
[ $# -ne 1 ] && echo 'missing version information' >&2 && exit 1

# manually install emsdk
echo 'Install emsdk...'
mkdir -p build/web/
cd build/web/ || exit 1
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk/ || exit 1
./emsdk install latest
./emsdk activate latest
# shellcheck disable=SC1091
# do not statically check if this file exists. It will exists during the
# execution
source ./emsdk_env.sh
cd ../../../ || exit 1

# manually and locally install Azur (temporary hack)
echo 'Building Azur...'
mkdir -p build/web/azur
cd build/web/azur || exit 1
git clone \
    https://git.planet-casio.com/Lephenixnoir/Azur.git \
    --recurse-submodules \
    --depth 1 \
    source
mkdir -p sysroot
export AZUR_PATH_emscripten="$PWD/sysroot"
emcmake cmake \
    -B build \
    -S source \
    -DAZUR_PLATFORM=emscripten \
    -DCMAKE_INSTALL_PREFIX="$AZUR_PATH_emscripten"
if ! cmake --build build --target install --parallel ; then
    echo 'Unable to build Azur, abort :(' >&2
    exit 1
fi
cd ../../../ || exit 1

# manually build MQ
echo 'Building MQ...'
emcmake cmake \
    -B build/web/mq \
    -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DAZUR_PLATFORM=emscripten \
    -DMQ_DISABLE_OPTIMIZATIONS=0 \
    -DMQ_PROFILING_GPROF=0 \
    -DMQ_PROFILING_TRACY=0 \
    -DMQ_VIDEO_FFMPEG=0
if ! cmake --build build/web/mq --target mq --parallel ; then
    echo 'Unable to build MQ, abord :(' >&2
    exit 1
fi

# generate final binary information
cd build/web/mq || exit 1
tar -cjf "mq-web-$1.tar.bz2" mq.data mq.js mq.wasm index.html
mv "mq-web-$1.tar.bz2" ../../
