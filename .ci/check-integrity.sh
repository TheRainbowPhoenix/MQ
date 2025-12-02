#!/usr/bin/env bash
#
# Script used to check the integrity of the current project version by
# checking the project version in the CMake, checking changelog entries,
# checking compatibility list, ... and more.
# Also, this script require one argument which is the project version
#

# setup a proper starting point
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." || exit 1

# rudimentary argument check
[ $# -ne 1 ] && echo 'missing version information' >&2 && exit 1

# checking bash scripts
if ! shellcheck ./.ci/*.sh ; then
    echo 'broken shellcheck, fix and retry' >&2
    exit 1
fi

# checking compatibility list
if ! ./scripts/gen-compatibility.py --check ; then
    echo 'compatibility list not up-to-date, fix and retry' >&2
    exit 1
fi

# checking CMake project version
version=$(cat ./CMakeLists.txt)
version=$(echo "$version" | grep -Eo 'project\(MQ VERSION ([0-9]+.){2}[0-9]+')
version=$(echo "$version" | grep -Eo '([0-9]+.?){3}')
if [ "$version" != "$1" ] ; then
    echo 'CMakeLists.txt version mismatch, fix and retry' >&2
    echo "$version != $1" >&2
    exit 1
fi
