#!/usr/bin/env python
"""
gen-changelog   - generate changelog information
"""
from typing import NoReturn
from pathlib import Path
import sys
import re

#=== utils ====================================================================#

def _error(text: str) -> NoReturn:
    """ display on stderr then exit 1 """
    print(f"\033[31m{text}\033[0m", file=sys.stderr)
    sys.exit(1)

#=== core  ====================================================================

__ALPHA_VERSION_WARNING = (
    '> [!WARNING]\n' +
    '> This is an "alpha" release of the emulator, expect some bugs and ' +
    'broken feature.\n' +
    '> We highly recommend you to build the project from source until ' +
    'the first "beta" (version `0.1.*`) is released. You can check the ' +
    'instruction in directly in the [`README.md > Building`](/README.md)\n'
)

def changelog_generate(
    changelog_path: Path,
    output_path: Path,
    verinfo: re.Match[str]
) -> None:
    """ generate the final changelog
    """
    content = ''
    if verinfo[1] == '0' and verinfo[2] == '0':
        content += __ALPHA_VERSION_WARNING
        content += '\n---\n\n'
    with open(changelog_path, 'r', encoding='utf-8') as changelog_stream:
        content += changelog_stream.read()
    output_path.parent.mkdir(exist_ok=True)
    with open(output_path, 'x', encoding='utf-8') as output_stream:
        _ = output_stream.write(content)

#=== entry ====================================================================

def main(argv: list[str]) -> NoReturn:
    """ generate the changelog
    """
    if len(argv) != 3:
        _error(f"{argv[0]} <VERSION> <OUTPUT>")
    changelog_prefix = Path(f"{__file__}/../../docs/changelogs").resolve()
    output_path = Path(argv[2])
    version = argv[1]
    if not changelog_prefix.exists():
        _error('(internal) unable to find the changelog prefix')
    if output_path.exists():
        _error(f"output file `{output_path}` already exists")
    if not (verinfo := re.match(r'([0-9]+)\.([0-9]+)\.([0-9]+)', version)):
        _error(f"version `{version}` use an unvalid format")
    if not (changelog_path := changelog_prefix/f"{version}.md").exists():
        _error(f"unable to find the changelog for `{version}`")
    changelog_generate(changelog_path, output_path, verinfo)
    sys.exit(0)

if __name__ == '__main__':
    main(sys.argv)
