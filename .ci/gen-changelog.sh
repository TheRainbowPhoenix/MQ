#!/usr/bin/env bash
#
# Script used to "generate" the release changelog that will be pasted to
# the forge's release body. Note that this script require an argument which
# is the current project version (no check will be performed to ensure that
# the version provided is valid)
#

# setup a proper starting point
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." || exit 1

# rudimentary argument check
[ $# -ne 1 ] && echo 'missing version information' >&2 && exit 1

# generate the changelog
if ! ./scripts/gen-changelog.py "$1" './build/changelog.md' ; then
    echo 'Unable to generate the changelog, abord :(' >&2
    exit 1
fi
