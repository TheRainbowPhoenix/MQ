#! /usr/bin/env python

import itertools
import sys
import re
import os

def make_identifier(name):
    # Replace all invalid identifier characters with "_"
    name = re.sub(r"[^a-zA-Z0-9_]", "_", name)
    # Prepend namespace
    name = "mq_assets_" + name
    # Don't allow consecutive "_"
    return re.sub(r"_+", "_", name)

def embed_bin(data, fp_c, fp_h, name):
    size = len(data)

    fp_c.write(f"const unsigned char {name}[{size}] = {{\n")
    for line in itertools.batched(data, 12):
        fp_c.write("  ")
        fp_c.write(" ".join(f"0x{b:02x}," for b in line))
        fp_c.write("\n")

    fp_c.write("};\n")
    fp_c.write(f"const unsigned int {name}_len = {size};\n")

    fp_h.write(f"extern const unsigned char {name}[{size}];\n")
    fp_h.write(f"extern const unsigned int {name}_len;\n")

USAGE = """\
usage: gen-asset.py -c <OUTPUT.c> -h <OUTPUT.h> <FILES...>
Generates C code and headers for the given files, similar to xxd -i\
"""

def main(argv):
    if "--help" in argv:
        print(USAGE)
        return 0
    if len(argv) < 6 or argv[1] != "-c" or argv[3] != "-h":
        print(USAGE)
        return 1

    _, path_c, _, path_h, *binaries = sys.argv[1:]

    with open(path_c, "w") as fp_c:
        # Type check
        fp_c.write('#include "autogen/assets.h"\n')

        with open(path_h, "w") as fp_h:
            for path_bin in binaries:
                with open(path_bin, "rb") as fp_bin:
                    data = fp_bin.read()
                    name = os.path.splitext(os.path.basename(path_bin))[0]
                    embed_bin(data, fp_c, fp_h, make_identifier(name))

    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))

