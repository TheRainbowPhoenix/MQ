#!/usr/bin/env bash
#
# Build script for Linux. This script assumes being called by the CI
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

# ensure dependencies are pulled
git submodule update --init --recursive

# manually and locally install Azur (temporary hack)
mkdir -p build/linux/azur
cd build/linux/azur || exit 1
mkdir -p sysroot
export AZUR_PATH_linux="$PWD/sysroot"
cmake \
    -B build \
    -S ../../../3rdparty/azur \
    -DAZUR_PLATFORM=linux \
    -DCMAKE_INSTALL_PREFIX="$AZUR_PATH_linux"
if ! cmake --build build --target install --parallel ; then
    echo 'Unable to build Azur, abort :(' >&2
    exit 1
fi
cd ../../../ || exit 1

# manually build MQ
cmake \
    -B build/linux/mq \
    -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DAZUR_PLATFORM=linux \
    -DMQ_DISABLE_OPTIMIZATIONS=0 \
    -DMQ_PROFILING_GPROF=0 \
    -DMQ_PROFILING_TRACY=0 \
    -DMQ_VIDEO_FFMPEG=1
if ! cmake --build build/linux/mq --target mq --parallel ; then
    echo 'Unable to build MQ, abord :(' >&2
    exit 1
fi

# generate final binary information
mv build/linux/mq/mq "./build/mq-linux-$1"
sha256sum "./build/mq-linux-$1" > "./build/mq-linux-$1.checksum"
