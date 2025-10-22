#! /usr/bin/env python

import dataclasses
from typing import Dict
from collections.abc import Sequence, Generator
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

    def allOpcodes(self, _i=0, _base=0) -> Generator[int]:
        if _i >= len(self.encoding):
            yield _base
            return
        if self.encoding[_i] != "1":
            yield from self.allOpcodes(_i+1, _base)
        if self.encoding[_i] != "0":
            bitPosition = len(self.encoding) - _i - 1
            yield from self.allOpcodes(_i+1, _base + (1 << bitPosition))

    def allFieldSlices(self) -> Generator[tuple[Slice, str, int]]:
        for m in re.finditer("n+|m+|d+|i+|c+|s+", self.encoding):
            s = Slice(16 - m.end(), m.end() - m.start())
            yield s, m[0][0], self.mask(s)

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
            if "delayslot" in tags:
                tags.append("illslot")
            i = Instruction(pattern, len(pattern), m[2], tags)
            if "int" in tags and "delayslot" in tags:
                raise Exception(f"{i} is a delay slot and generates interrupts")
            instructions.append(i)

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
        if dc.name is not None and "overload" not in ins.tags:
            error(f"decision conflict between {dc.name} and {ins.name}")
            continue
        dc.name = ins.name
        dc.args = args

#=== Decoder generation =======================================================#

DECODER_HEADER = ""
DECODER_FOOTER = """\
#undef _OPCODE
#undef _DECIDE
"""

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
            args = [str(node.name)] + [codegen(s) for s in node.args]
            return "{}_DECIDE({});\n".format(indent, ", ".join(args))
        case Slice():
            mask = (1 << node.size) - 1
            return "(_OPCODE >> {}) & 0x{:x}".format(node.start, mask)
    return ""

def generateDecoder(spec, filename="<inline>"):
    tree, ins = parseSpec(spec, filename)
    print(ins)
    if tree is not None:
        resolveDecisions(tree, ins)
        return DECODER_HEADER + codegen(tree) + DECODER_FOOTER

#=== Decoder (table version) generation =======================================#

WRAPPERS_TEMPLATE = """
//---
// Generated instruction wrapper functions
//----

static void invalid_wrapper(mqMachine *mach, mqCpu *cpu, u16 inst) {
   mq_log(MQ_LOG_ERROR, "unable to decode instruction %08x", inst);
   mq_machine_setStuck(mach);
   (void)cpu;
}
"""

ILLSLOT_TEMPLATE = """\
    if(MQ_UNLIKELY(mq_cpu_inDelaySlot(cpu)))
        return mq_cpu_raiseException2(mach, cpu, SH_EXC_ILLEGAL_SLOT, 0);
"""
INT_TEMPLATE = """\
    if(MQ_UNLIKELY(cpu->excMask & (1 << SH_EXC_INTERRUPT)))
        mq_cpu_handleException(mach, cpu);
"""

TABLE_TEMPLATE = """
//---
// Generated translation and wrapper table
//----

static void (*mq_inst_wrapper_table[65536])(mqMachine*,mqCpu*,u16) = {{
{}
}};
"""

def generateDecoderTableWrapperFunc(ins: list[Instruction]) -> str:
    c_content = WRAPPERS_TEMPLATE

    for i in ins:
        arg_list = ['mach', 'cpu']
        c_content += f"static void {i.name}_wrapper("
        c_content += 'mqMachine *mach, mqCpu *cpu, u16 inst) {\n'
        if "illslot" in i.tags:
            c_content += ILLSLOT_TEMPLATE
        for slice, name, mask in i.allFieldSlices():
            arg_list.append(name)
            shift = slice.start
            c_content += f"    int {name} = (inst & {mask:#06x}) >> {shift};\n"
        if len(arg_list) == 2:
            c_content += '    (void)inst;\n'
        c_content += f"    {i.name}({', '.join(arg_list)});\n"
        if "int" in i.tags:
            c_content += INT_TEMPLATE
        c_content += '}\n\n'
    return c_content


def generateDecoderTableInfo(
        ins: list[Instruction], opcodeMap: Sequence[int]) -> str:
    entries = ""
    for instId in opcodeMap:
        name = ins[instId].name if instId >= 0 else "invalid"
        entries += f"    &{name}_wrapper,\n"
    return TABLE_TEMPLATE.format(entries)


def generateDecoderTable(spec, filename="<inline>"):
    tree, ins = parseSpec(spec, filename)
    opcodeMap = [-1] * 65536

    for instId, inst in enumerate(ins):
        for opcode in inst.allOpcodes():
            if opcodeMap[opcode] >= 0 and "overload" not in inst.tags:
                raise Exception("instruction collision @ {:04x}: {} vs. {}" \
                    .format(opcode, ins[opcodeMap[opcode]], inst))
            opcodeMap[opcode] = instId

    c_content = generateDecoderTableWrapperFunc(ins)
    c_content += generateDecoderTableInfo(ins, opcodeMap)
    return c_content

#=== Main function ============================================================#

USAGE = """\
usage: gen-isa.py [--table|--switch] <INPUT.def> <OUTPUT.c>
Generates the decoder for MQ based on an ISA description\
"""

def main(argv):
    if "--help" in argv:
        print(USAGE)
        return 0
    if len(argv) != 4 or sys.argv[1] not in ["--table", "--switch"]:
        print(USAGE)
        return 1

    mode, in_path, out_path = sys.argv[1:]

    with open(in_path, "r") as fp_in:
        spec = fp_in.read()

    if mode == "--table":
        c_code = generateDecoderTable(spec, in_path)
    elif mode == "--switch":
        c_code = generateDecoder(spec, in_path)

    if c_code is not None:
        with open(out_path, "w") as fp_out:
            fp_out.write(c_code)

if __name__ == "__main__":
    sys.exit(main(sys.argv))
