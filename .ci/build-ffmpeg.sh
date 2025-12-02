#!/usr/bin/env bash
#
# Build script for ffmpeg (linux). This script assumes being called by the CI
# workflow with the `ubuntu` image with all external dependencies already
# installed.
#

# setup a proper starting point
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." || exit 1

# prepare build prefix
mkdir -p build/linux/ffmpeg
mkdir -p build/linux/ffmpeg/sysroot
cd build/linux/ffmpeg || exit 1

# configure the project
../../../3rdparty/ffmpeg/configure  \
    --disable-all                   \
    --prefix="$(pwd)/sysroot"       \
    --enable-encoder=h264_nvenc     \
    --enable-encoder=h264_vaapi     \
    --enable-encoder=h264_vulkan    \
    --enable-encoder=libvpx_vp9     \
    --enable-encoder=libx264        \
    --enable-encoder=hevc_nvenc     \
    --enable-encoder=hevc_vaapi     \
    --enable-encoder=hevc_vulkan    \
    --enable-avcodec                \
    --enable-avformat               \
    --enable-swscale                \
    --enable-gpl                    \
    --enable-libx264                \
    --enable-libx265                \
    --enable-libvpx                 \
    --enable-cuda-llvm              \
    --enable-vaapi                  \
    --enable-ffnvcodec              \
    --enable-nvenc                  \
    --enable-opengl                 \
    --enable-vulkan                 \
    --enable-lto

# build the project
make -j "$(nproc)" all install
