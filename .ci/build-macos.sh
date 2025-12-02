#!/usr/bin/env bash
#
# Build script for MacOS. This script assumes being called by the CI
# workflow with the `macos-14` image with all external dependencies already
# installed.
# Also, this script require one argument which is the current project
# version (e.g 0.0.12). The version is assumed valid, no test will be
# performed to ensure its integrity
#

# setup a proper starting point
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." || exit 1

# rudimentary argument check
[ $# -ne 1 ] && echo 'missing version information' >&2 && exit 1

# manually and locally install Azur (temporary hack)
mkdir -p build/macos/azur
cd build/macos/azur || exit 1
mkdir -p sysroot
export AZUR_PATH_macos="$PWD/sysroot"
git clone \
    https://git.planet-casio.com/Lephenixnoir/Azur.git \
    --depth 1 \
    --recursive \
    source
cmake \
    -B build \
    -S source \
    -DAZUR_PLATFORM=linux \
    -DCMAKE_INSTALL_PREFIX="$AZUR_PATH_macos"
if ! cmake --build build --target install --parallel ; then
    echo 'Unable to build Azur, abord :(' >&2
    exit 1
fi
cd ../../../ || exit 1

# manually build MQ
git submodule update --init --recursive
cmake \
    -B build/macos/mq \
    -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DAZUR_PLATFORM=linux \
    -DMQ_DISABLE_OPTIMIZATIONS=0 \
    -DMQ_PROFILING_GPROF=0 \
    -DMQ_PROFILING_TRACY=0 \
    -DMQ_VIDEO_FFMPEG=1
if ! cmake --build build/macos/mq --target mq --parallel ; then
    echo 'Unable to build MQ, abord :(' >&2
    exit 1
fi

# generate final binary information
mv build/macos/mq/mq "./build/mq-macos-$1"
sha256sum "./build/mq-macos-$1" > "./build/mq-macos-$1.checksum"
