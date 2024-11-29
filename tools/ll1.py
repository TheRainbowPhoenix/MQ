from dataclasses import dataclass
from typing import Any, Tuple
import enum
import sys
import re

@dataclass
class Loc:
    path: str
    line: int
    column: int

    def __str__(self):
        path = self.path or "(anonymous)"
        return f"{path}:{self.line}:{self.column}"

@dataclass
class SyntaxError(Exception):
    loc: Loc
    message: str

@dataclass
class Token:
    type: Any
    value: Any
    loc: Loc

    def __str__(self):
        if self.value is None:
            return f"{self.type}"
        else:
            return f"{self.type}({self.value})"

class NaiveRegexLexer:
    """
    Base class for a very naive regex-based lexer. This class provides the
    naive matching algorithm that applies all regexes at the current point and
    constructs a token with the longest, earliest match in the list. Regular
    expressions for tokens as specified in a class-wide TOKEN_REGEX list which
    consist of triples (regex, token type, token value).
    """

    # Override with list of (regex, token type, token value). Both the token
    # type and value can be functions, in which case they'll be called with the
    # match object as parameter.
    Rule = Tuple[str, Any, Any] | Tuple[str, Any, Any, int]
    TOKEN_REGEX: list[Rule] = []
    # Override with token predicate that matches token to be discarded and not
    # sent to the parser (typically, whitespace and comments).
    TOKEN_DISCARD = lambda _: False

    def __init__(self, input, inputFilename):
        self.input = input
        self.inputFilename = inputFilename
        self.line = 1
        self.column = 1

        # TODO: Precompile the regular expressions

    def loc(self):
        return Loc(self.inputFilename, self.line, self.column)

    def raiseError(self, message):
        raise SyntaxError(self.loc(), message)

    def advancePosition(self, lexeme):
        for c in lexeme:
            if c == "\n":
                self.line += 1
                self.column = 0
            self.column += 1

    def nextToken(self) -> Token:
        """Return the next token in the input stream, None at EOF."""
        if not len(self.input):
            return None

        highestPriority = 0
        longestMatch = None
        longestMatchIndex = -1

        for i, (regex, _, _, *rest) in enumerate(self.TOKEN_REGEX):
            priority = rest[0] if len(rest) else 0

            if (m := re.match(regex, self.input)):
                score = (priority, len(m[0]))
                if longestMatch is None or \
                    score > (highestPriority, len(longestMatch[0])):
                    highestPriority = priority
                    longestMatch = m
                    longestMatchIndex = i

        if longestMatch is None:
            nextWord = self.input.split(None, 1)[0]
            self.raiseError(f"unknown lexical notation '{nextWord}'")

        # Build the token
        _, type_info, value_info, *rest = self.TOKEN_REGEX[longestMatchIndex]
        m = longestMatch

        typ = type_info(m) if callable(type_info) else type_info
        value = value_info(m) if callable(value_info) else value_info
        t = Token(typ, value, self.loc())

        self.advancePosition(m[0])
        # Urgh. I need to find how to match a regex at a specific offset.
        self.input = self.input[len(m[0]):]
        return t

    def lex(self):
        """Return the next token that's visible to the parser, None at EOF."""
        t = self.nextToken()
        discard = type(self).TOKEN_DISCARD
        while t is not None and discard(t):
            t = self.nextToken()
        return t

    def dump(self, showDiscarded=False, fp=sys.stdout):
        """Dump all remaining tokens on a stream, for debugging."""
        t = 0
        discard = type(self).TOKEN_DISCARD
        while t is not None:
            t = self.nextToken()
            if t is not None and discard(t):
                if showDiscarded:
                    print(t, "(discarded)")
            else:
                print(t)

class LL1Parser:
    """
    Base class for an LL(1) recursive descent parser. This class provides the
    base mechanisms for hooking up a lexer, consuming tokens, checking the
    lookahead, and combinators for writing common types of rules such as
    expressions with operator precedence.
    """
    la: Token

    def __init__(self, lexer):
        self.lexer = lexer
        self.la = None
        self.advance()

    def advance(self):
        """Return the next token and update the lookahead."""
        t, self.la = self.la, self.lexer.lex()
        return t

    def atEnd(self):
        return self.la is None

    def raiseErrorAt(self, token, message):
        raise SyntaxError(token.loc, message)

    def fullParse(self, method):
            ret = method()
            if not self.atEnd():
                self.raiseErrorAt(self.la, "expected end of input")
            return ret

    def expect(self, types, pred=None, optional=False) -> Token:
        """
        Read the next token, ensuring it is one of the specified types; if
        `pred` is specified, also tests the predicate. If `optional` is set,
        returns None in case of mismatch rather than raising an error.
        """

        if not isinstance(types, list):
            types = [types]
        if self.la is not None and self.la.type in types and \
            (pred is None or pred(self.la)):
            return self.advance()
        if optional:
            return None

        expected = ", ".join(str(t) for t in types)
        err = f"expected one of {expected}, got {self.la}"
        if pred is not None:
            err += " (with predicate)"
        self.raiseErrorAt(self.la, err)

    # A list of elementFunction terminated by a clear termination marker.
    def directList(self, elementFunction, *, term):
        return self.separatedList(elementFunction, sep=None, term=term)

    # A list of elementFunction separated by sep, with an optional final sep.
    # There must be a distinguishable termination marker "term" in order to
    # determine whether there are more elements incoming. "term" can either be
    # a token type or a callable applied to self.la.
    def separatedList(self, elementFunction, *, sep, term):
        elements = []
        termFunction = term if callable(term) else lambda la: la.type == term
        while not termFunction(self.la):
            elements.append(elementFunction())
            if termFunction(self.la):
                break
            if sep is not None:
                self.expect(sep)
        return elements

    # A non-empty list of elementFunction separated by sep that keeps on going
    # as long as there's a separator.
    def openList(self, elementFunction, *, sep):
        elements = [elementFunction()]
        while self.expect(sep, optional=True) is not None:
            elements.append(elementFunction())
        return elements

    # Rule combinators implementing unary and binary operators with precedence

    @staticmethod
    def binaryOpsLeft(ctor, ops):
        def decorate(f):
            def symbol(self):
                e = f(self)
                while (op := self.expect(ops, optional=True)) is not None:
                    e = ctor(op, [e, f(self)])
                return e
            return symbol
        return decorate

    @staticmethod
    def binaryOps(ctor, ops, *, rassoc=False):
        def decorate(f):
            def symbol(self):
                lhs = f(self)
                if (op := self.expect(ops, optional=True)) is not None:
                    rhs = symbol(self) if rassoc else f(self)
                    return ctor(op, [lhs, rhs])
                else:
                    return lhs
            return symbol
        return decorate

    @staticmethod
    def binaryOpsRight(ctor, ops):
        return LL1Parser.binaryOps(ctor, ops, rassoc=True)

    @staticmethod
    def unaryOps(ctor, ops, assoc=True):
        def decorate(f):
            def symbol(self):
                if (op := self.expect(ops, optional=True)) is not None:
                    arg = symbol(self) if assoc else f(self)
                    return ctor(op, [arg])
                else:
                    return f(self)
            return symbol
        return decorate
