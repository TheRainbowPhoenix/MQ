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

# manually and locally install Azur and manually export installation
# information
./.ci/build-azur.sh emscripten "$PWD/build/web/azur" false
AZUR_PATH_emscripten="$PWD/build/web/azur/sysroot"
export AZUR_PATH_emscripten

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
    echo 'Unable to build MQ, abort :(' >&2
    exit 1
fi

# generate final binary information
cd build/web/mq || exit 1
zip "mq-web-$1.zip" mq.data mq.js mq.wasm index.html
mv "mq-web-$1.zip" ../../
