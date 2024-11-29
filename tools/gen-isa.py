#! /usr/bin/env python

import dataclasses
from typing import Any, Dict
import copy
import sys
import re

from ll1 import *

def error(*args, **kwargs):
    print("\x1b[31;1merror:\x1b[0m ", end="", file=sys.stderr)
    print(*args, **kwargs, file=sys.stderr)

def warning(*args, **kwargs):
    print("\x1b[33;1mwarning:\x1b[0m ", end="", file=sys.stderr)
    print(*args, **kwargs, file=sys.stderr)

#=== Input parser =============================================================#

T = enum.Enum("T", ["WS", "COMMENT", "SWITCH", "DECIDE", "INT", "IDENT"])
class SwitchTreeLexer(NaiveRegexLexer):
    TOKEN_REGEX = [
        (r"[ \t\n]+", T.WS, None),
        (r"\#[^\n]*", T.COMMENT, None),
        (r"0|[1-9][0-9]*|0b[0-1]+|0[xX][0-9a-fA-F]+", T.INT,
            lambda m: int(m[0], 0)),
        (r"\bswitch\b", T.SWITCH, None),
        (r"\bdecide\b", T.DECIDE, None),
        (r"[a-zA-Z][a-zA-Z0-9_]*", T.IDENT, lambda m: m[0]),
        (r"\.\.|[.,:;={}_$()\[\]]", lambda m: m[0], None),
    ]
    TOKEN_DISCARD = lambda t: t.type == T.WS or t.type == T.COMMENT

@dataclasses.dataclass
class Slice:
    start: int
    size: int

@dataclasses.dataclass
class Decide:
    name: str | None
    args: list[Slice]

@dataclasses.dataclass
class Switch:
    discr: Slice
    cases: Dict[int, "Switch | Decide"]
    generator: "Switch | Decide | None"

    def addCase(self, label, stmt):
        match label:
            case "_":
                if self.generator is not None:
                    raise Exception("duplicate default case in switch")
                self.generator = stmt
            case (start, end):
                for i in range(start, end+1):
                    self.addCase(i, copy.deepcopy(stmt))
            case int():
                if label in self.cases:
                    raise Exception("duplicate case {} in switch".format(label))
                self.cases[label] = stmt

    def getOrCreateCase(self, label):
        if label in self.cases:
            return self.cases[label]
        if self.generator is not None:
            self.cases[label] = copy.deepcopy(self.generator)
            return self.cases[label]
        return None

class SwitchTreeParser(LL1Parser):
    def caseLabel(self):
        t = self.expect([T.INT, "_"])
        l = "_" if t.type == "_" else t.value
        if self.expect("..", optional=True):
            return (l, self.expect(T.INT).value)
        return l

    def caseStmt(self):
        labels = self.separatedList(self.caseLabel, sep=",", term=":")
        self.expect(":")
        return (labels, self.stmt())

    def stmt(self):
        t = self.expect([T.SWITCH, T.DECIDE])
        self.expect("(")
        if t.type == T.SWITCH:
            s = Switch(self.slice(), {}, None)
            self.expect(")")
            if self.la.type == "{":
                self.expect("{")
                for labels, stmt in self.directList(self.caseStmt, term="}"):
                    for l in labels:
                        s.addCase(l, copy.deepcopy(stmt))
                self.expect("}")
            else:
                s.addCase("_", self.stmt())
            return s
        elif t.type == T.DECIDE:
            slices = self.separatedList(self.slice, sep=",", term=")")
            self.expect(")")
            return Decide(None, slices)

    def slice(self):
        self.expect("$")
        self.expect("[")
        start = self.expect(T.INT).value
        self.expect(":")
        size = self.expect(T.INT).value
        self.expect("]")
        return Slice(start, size)

@dataclasses.dataclass
class Instruction:
    encoding: str
    size: int
    name: str
    tags: list[str]

    def slice(self, s: Slice) -> str:
        start, end = s.start, s.start + s.size
        assert s.size > 0
        assert start in range(self.size) and end - 1 in range(self.size)
        return self.encoding[self.size-end : self.size-start]

    def constantSlice(self, s: Slice) -> int:
        bits = self.slice(s)
        if not all(b == "0" or b == "1" for b in bits):
            raise Exception(f"non-constant slice {s} for {self.encoding}")
        return int(bits, 2)

    def isZeroSlice(self, s: Slice) -> bool:
        return all(b == "0" for b in self.slice(s))

    def isFieldSlice(self, s: Slice) -> bool:
        bits = self.slice(s)
        if not all(b == bits[0] for b in bits):
            return False
        if not bits[0].islower():
            return False
        if len(bits) != self.encoding.count(bits[0]):
            return False
        return True

    def mask(self, s: Slice | None = None) -> int:
        s = s or Slice(0, self.size)
        return ((1 << s.size) - 1) << s.start

    def maskedString(self, mask: int) -> str:
        str = ""
        for i, c in enumerate(self.encoding):
            bit = 1 << (len(self.encoding) - 1 - i)
            before, after = ("\x1b[4m", "\x1b[0m") if mask & bit else ("", "")
            str += before + c + after
        return str + " ({})".format(self.name)

def parseSpec(spec, filename):
    RE_INS = re.compile(
        r"([0-1a-z.]+)\s+"                    # Encoding with field letters
        r"([a-zA-Z_][a-zA-Z0-9_]*)\s*"        # Instruction identifier
        r"((?:![a-zA-Z_][a-zA-Z0-9_]*\s*)*)"  # Optional tags
    )

    # Split tree definition and instruction list
    assert "%\n" in spec
    tree, isa = spec.split("%\n", 1) # TODO: Brutal

    instructions = []
    for l in isa.splitlines():
        l = l.strip()
        if not l or l.startswith("#"):
            continue
        if not (m := RE_INS.fullmatch(l)):
            print("invalid instruction line:", l)
        else:
            pattern = m[1].replace(".", "")
            tags = [t.removeprefix("!") for t in m[3].split()]
            instructions.append(Instruction(pattern, len(pattern), m[2], tags))

    try:
        l = SwitchTreeLexer(tree, filename)
        p = SwitchTreeParser(l)
        tree = p.fullParse(p.stmt)
    except SyntaxError as e:
        error(str(e.loc) + ":", e.message)
        tree = None

    return tree, instructions

#=== Tree processing and analysis =============================================#

def traverseTree(tree, ins, mask=None):
    # Build a mask to check if all bits are matched at some point
    if mask is None:
        mask = ins.mask()

    match tree:
        case Switch() as sw:
            b = ins.constantSlice(sw.discr)
            mask &= ~ins.mask(sw.discr)
            return traverseTree(sw.getOrCreateCase(b), ins, mask)
        case Decide() as dc:
            newArgs = []
            for slice in dc.args:
                mask &= ~ins.mask(slice)
                if ins.isZeroSlice(slice):
                    pass
                elif not ins.isFieldSlice(slice):
                    warning("bad field:", ins.maskedString(ins.mask(slice)))
                else:
                    newArgs.append(slice)
            if mask:
                warning("unused bits after decision:", ins.maskedString(mask))
            return dc, newArgs
        case _:
            error("invalid tree traversal for {}, ends at {}".format(
                ins.encoding, tree))
            return None, None

def resolveDecisions(tree, instructions):
    for ins in instructions:
        dc, args = traverseTree(tree, ins)
        if dc is None or args is None:
            error(f"switch tree does not cover {ins.encoding} ({ins.name})")
            continue
        if dc.name is not None:
            error(f"decision conflict between {dc.name} and {ins.name}")
            continue
        dc.name = ins.name
        dc.args = args

#=== Decoder generation =======================================================#

def codegen_caseLabels(spec):
    if spec == "_":
        return ["default:"]
    elif isinstance(spec, tuple):
        return ["case {}:".format(i) for i in range(spec[0], spec[1]+1)]
    else:
        return ["case {}:".format(spec)]

def codegen(node, depth=0) -> str:
    indent = "  " * depth
    match node:
        case Switch():
            s = indent + "switch(" + codegen(node.discr) + ") {\n"
            for labels, block in node.cases.items():
                labels = codegen_caseLabels(labels)
                s += "".join(indent + x + "\n" for x in labels)
                s += codegen(block, depth+1)
                s += indent + "  break;\n"
            return s + indent + "}\n"
        case Decide():
            args = ", ".join(codegen(s) for s in node.args)
            return indent + str(node.name) + "(" + args + ");\n"
        case Slice():
            mask = (1 << node.size) - 1
            return "(_OPCODE >> {}) & 0x{:x}".format(node.start, mask)
    return ""

def generateDecoder(spec, filename="<inline>"):
    tree, ins = parseSpec(spec, filename)
    if tree is not None:
        resolveDecisions(tree, ins)
        return codegen(tree)

#=== Main function ============================================================#

USAGE = """\
usage: gen-isa.py <INPUT.def> <OUTPUT.c>
Generates the decoder for MQ based on an ISA description\
"""

def main(argv):
    if "--help" in argv:
        print(USAGE)
        return 0
    if len(argv) != 3:
        print(USAGE)
        return 1

    with open(sys.argv[1], "r") as fp_in:
        spec = fp_in.read()

    c_code = generateDecoder(spec, sys.argv[1])

    if c_code is not None:
        with open(sys.argv[2], "w") as fp_out:
            fp_out.write(c_code)

if __name__ == "__main__":
    sys.exit(main(sys.argv))
