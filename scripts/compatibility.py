"""
compatibility   - handle compatibility file
"""
# pyright: reportAny=false
from typing import NoReturn
from dataclasses import dataclass
from pathlib import Path
from copy import copy
import sys

import yaml

#---
# Utils
#---

def _error(text: str) -> NoReturn:
    """ display on stderr then exit 1 """
    print(f"\033[31m{text}\033[0m", file=sys.stderr)
    sys.exit(1)

#---
# Content generation
#---

@dataclass(frozen=True)
class CompatAddin:
    """ addin information """
    name: str
    author: str
    updated: str
    url: str
    notes: list[str]

@dataclass
class CompatInfo:
    """ high-level information """
    total: int
    addin: dict[str,list[CompatAddin]]

CompatStatus: dict[str,tuple[str,str]] = {
    'playable': (
        '🟢',
        'Games that can be completed with playable performance and no ' +
        'game breaking glitches'
    ),
    'ingame': (
        '🟠',
        'Games that either can\'t be finished, have serious glitches or ' +
        'have insufficient performance'
    ),
    'intro': (
        '🟤',
        'Games that display image but don\'t make it past the menus'
    ),
    'loadable': (
        '🔴',
        'Games that display a black screen with a framerate on the ' +
        'window\'s title'
    ),
    'nothing': (
        '⚫',
        'Games that don\'t initialize properly, not loading at all ' +
        'and/or crashing the emulator'
    ),
}

def _compat_load(root: Path) -> CompatInfo:
    """ load and transform YAML compat description """
    with open(root/'docs/compatibility.yaml', 'r', encoding='utf8') as file:
        compatyaml = yaml.load(file.read(), Loader=yaml.CLoader)
    bad = ''
    compat = CompatInfo(0, {})
    for i, addin in enumerate(compatyaml['addins']):
        if addin['status'] not in CompatStatus:
            bad += f"yaml: [{i}]: {addin.get('name')}: unknown status"
            continue
        if addin['status'] not in compat.addin:
            compat.addin[addin['status']] = []
        try:
            compat.addin[addin['status']].append(
                CompatAddin(
                    name    = addin['name'],
                    author  = addin['author'],
                    url     = addin['urls'][0],
                    updated = addin['updated'],
                    notes   = addin['remarks'],
                ),
            )
            compat.total += 1
        except KeyError as err:
            bad += f"yaml: [{i}] `{addin.get('name')}` -> missing key {err}\n"
    if bad:
        _error(bad)
    return compat

def _compat_gen_table(root: Path) -> str:
    """ generate the compatibility table """
    content = '\n'
    compatinfo = _compat_load(root)
    for name, desc in CompatStatus.items():
        percent: float = 0
        if name in compatinfo.addin:
            percent = (len(compatinfo.addin[name])*100)/compatinfo.total
        content += f"<code>**{desc[0]} {name.capitalize()}** "
        content += f"({percent:.2f}%)</code> - {desc[1]}</br>\n"
    content += '\n'
    content += '| Addin | Status | Updated | Notes |\n'
    content += '|-------|:------:|:-------:|-------|\n'
    for name, desc in CompatStatus.items():
        for addin in compatinfo.addin[name]:
            notes = '</br>- '.join(addin.notes)
            content += f"| [`{addin.name}`]({addin.url}) by {addin.author} "
            content += f"| <code>**{desc[0]} {name.capitalize()}**</code> "
            content += f"| `{addin.updated}` "
            content += f"| -{notes} |\n"
    content += '\n'
    return content

#---
# README handling
#---

def _compat_update_readme(root: Path, only_check: bool, content: str) -> None:
    """ update (or check) the README file """
    readme_path = root/'README.md'
    with open(readme_path, 'r', encoding='utf-8') as readme_stream:
        readme_origin = readme_stream.read()
    readme_data = copy(readme_origin)
    assert (start := readme_data.find('<!-- compat table start -->')) > 0
    assert (end := readme_data.find('<!-- compat table end -->')) > 0
    readme_data = readme_data[:start+27] + content + readme_data[end:]
    if only_check:
        if readme_data != readme_origin:
            _error('README not up-to-date!!')
        return
    readme_path.unlink()
    with open(readme_path, 'x', encoding='utf-8') as readme_stream:
        _ = readme_stream.write(readme_data)

#---
# Entry
#---

def main(argv: list[str]) -> NoReturn:
    """ update or check the readme """
    root = Path(f"{__file__}/../../").resolve()
    _compat_update_readme(
        root       = root,
        only_check = '--check' in argv,
        content    = _compat_gen_table(root),
    )
    sys.exit(0)

if __name__ == '__main__':
    main(sys.argv)
