#!/usr/bin/env bash
#
# Internal script used to build Azur. Since Azur is a bit more complex than
# just clone and build, and (mainly) to avoid too many boiler plate in
# other build script (e.g `build-linux.sh`), this script will handle the
# whole process by itself.
#
# Since Azur require some information to be configured and build, this
# script will assume argument order like:
#     $1  ->  platform target ('linux' or emscripten)
#     $2  ->  build prefix (absolute path)
# No deep check will be performed here

# setup a proper starting point
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." || exit 1

# rudimentary argument check
[ $# -ne 2 ] && echo 'missing config information' >&2 && exit 1

# external tool check
if ! command -v cmake >/dev/null 2>&1; then
  echo 'missing "cmake" tool' >&2
  exit 1
fi
if ! command -v nasm >/dev/null 2>&1; then
  echo 'missing "nasm" tool' >&2
  exit 1
fi

# cloning Azur
mkdir -p "$2"
cd "$2" || exit 1
echo 'Cloning Azur...'
git clone \
    https://git.planet-casio.com/Lephenixnoir/Azur.git \
    --recurse-submodules \
    --depth 1 \
    source

# building submodules
echo 'Building Azur dependencies...'
declare "AZUR_PATH_$1"="$PWD/sysroot"
export "AZUR_PATH_$1"
[ "$1" = 'linux' ] && emenv='cmake' || emenv='emcmake cmake'
$emenv \
  -B build-3rdparty \
  -S source/3rdparty \
  -DAZUR_PLATFORM="$1" \
  -DCMAKE_INSTALL_PREFIX=sysroot
if ! cmake --build build-3rdparty --target install --parallel ; then
    echo 'Unable to build Azur dependencies, abort :(' >&2
    exit 1
fi

# building Azur
echo 'Building Azur...'
$emenv \
  -B build-azur \
  -S source \
  -DAZUR_PLATFORM="$1" \
  -DCMAKE_INSTALL_PREFIX=sysroot
if ! cmake --build build-azur --target install --parallel ; then
    echo 'Unable to build Azur, abort :(' >&2
    exit 1
fi
