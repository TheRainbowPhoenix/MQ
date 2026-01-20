#!/usr/bin/env python3
"""
compatibility   - handle compatibility file
"""
# pyright: reportAny=false
from typing import NoReturn
from dataclasses import dataclass
from pathlib import Path
from copy import copy
import sys
import re

import yaml

#---
# Utils
#---

def _error(text: str) -> NoReturn:
    """ display on stderr then exit 1 """
    print(f"\033[31m{text}\033[0m", file=sys.stderr)
    sys.exit(1)

#---
# Types
#---

@dataclass(frozen=True)
class CompatAddin:
    """ addin information """
    name:    str
    author:  str
    updated: str
    url:     str
    notes:   list[str]
    model:   str

@dataclass
class CompatInfo:
    """ high-level information """
    total: int
    stats: dict[str,int]
    addin: dict[str,list[CompatAddin]]

CompatModels: tuple[str,...] = (
    'fx9860g_sh3',
    'fx9860g_sh4',
    'fx9860g3',
    'fxcg20',
    'fxcg50',
    'fxcg100_2.00',
    'fxcp400_2.01.2',
)

CompatStatus: dict[str,tuple[str,str]] = {
    'complete': (
        '🟢',
        'Programs where most features work and games that can be completed ' +
        'with reasonable performance'
    ),
    'usable': (
        '🟠',
        'Programs that can be used and games that can be played but have ' +
        'missing features, glitches, or bad performance'
    ),
    'frame': (
        '🟤',
        'Programs that display at least one frame before failing'
    ),
    'nothing': (
        '⚫',
        'Programs that crash, loop, or get stuck before displaying their ' +
        'first frame'
    ),
}

#---
# Data loading
#---

def _compat_load(root: Path) -> CompatInfo:
    """ load and transform YAML compat description """
    with open(root/'docs/compatibility.yaml', 'r', encoding='utf8') as file:
        compatyaml = yaml.load(file.read(), Loader=yaml.CLoader)
    bad = ''
    compat = CompatInfo(0, {}, {})
    for i, addin in enumerate(compatyaml['addins']):
        try:
            if addin['status'] not in CompatStatus:
                bad += f"yaml: [{i}]: {addin['name']}: unknown status\n"
                continue
            if addin['model'] not in CompatModels:
                bad += f"yaml: [{i}]: {addin['name']}: unknown model\n"
                continue
            if not re.match(
                pattern = r'^202\d-[01]\d-[0-3]\d$',
                string  = str(addin['updated']),
            ):
                bad += f"yaml: [{i}]: {addin['name']}: broken date\n"
                continue
            if not re.match(
                pattern = r'^(https:\/\/)?[\w/\-?=%.]+\.[\w/\-&?=%.]+',
                string  = addin['urls'][0],
            ):
                bad += f"yaml: [{i}]: {addin['name']}: broken urls\n"
                continue
            if addin['status'] not in compat.addin:
                compat.addin[addin['status']] = []
            if addin['model'] not in compat.stats:
                compat.stats[addin['model']] = 0
            compat.addin[addin['status']].append(
                CompatAddin(
                    name    = addin['name'],
                    author  = addin['author'],
                    url     = addin['urls'][0],
                    updated = addin['updated'],
                    notes   = addin['remarks'],
                    model   = addin['model'],
                ),
            )
            compat.stats[addin['model']] += 1
            compat.total += 1
        except KeyError as err:
            bad += f"yaml: [{i}] `{addin.get('name')}`: missing key {err}\n"
    if bad:
        _error(bad)
    return compat

#---
# Content generation
#---

def _compat_gen_table(root: Path) -> str:
    """ generate the compatibility table """
    compatinfo = _compat_load(root)
    modelstats: dict[int,list[str]] = {}
    for name in CompatModels:
        total = 0 if name not in compatinfo.stats else compatinfo.stats[name]
        if total not in modelstats:
            modelstats[total] = []
        modelstats[total].append(name)
    content = '\n'
    content += f"Total programs: `all({compatinfo.total})`: "
    for total, names in sorted(modelstats.items(), reverse=True):
        for name in sorted(names, reverse=True):
            content += f" `{name}({total})`"
    content += '\n\n'
    for name, desc in CompatStatus.items():
        percent: float = 0
        if name in compatinfo.addin:
            percent = (len(compatinfo.addin[name])*100)/compatinfo.total
        content += f"<code>**{desc[0]} {name.capitalize()}** "
        content += f"({percent:.2f}%)</code> - {desc[1]}</br>\n"
    content += '\n\n'
    content += '| Addin | Status | Models | Updated | Notes |\n'
    content += '|-------|:------:|:------:|:-------:|-------|\n'
    for name, desc in CompatStatus.items():
        for addin in compatinfo.addin[name]:
            notes = '</br>- '.join(addin.notes)
            content += f"| [{addin.name}]({addin.url}) by {addin.author} "
            content += f"| <code>{desc[0]} {name.capitalize()}</code> "
            content += f"| `{addin.model}` "
            content += f"| {addin.updated} "
            content += f"| - {notes} |\n"
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
