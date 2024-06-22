#!/bin/env python3

import os
import re
import sys
import argparse
import subprocess
from collections import deque
from typing import List, Deque, Tuple


class Color:
    code = '\033[38;5;248m'
    line = '\033[38;5;244m'
    symbol = '\033[33m'
    address = '\033[34m'
    type = '\033[36m'
    clear = '\033[0m'


class Context:
    symbol : str
    address : str
    func : str
    code : Deque[str]
    path : str
    line : int

    def __init__(self) -> None:
        self.func = None
        self.path = None
        self.line = 0
        self.code = deque()
        self.symbol = None
        self.address = None
        self.stdout = None


def color_wrap(string : str, color : str) -> str:
    return f'{color}{string}{Color.clear}'


def line_print(line : str, context : Context):
    print(line, file=context.stdout)


class SymbolLineParser:
    regex = re.compile(r'^([0-9a-fA-F]+) <(.*)>:')

    def __init__(self, context):
        self.context = context

    def run(self, match) -> None:
        if self.context.symbol:
            line_print('}\n', self.context)
        self.context.address = match.group(1)
        self.context.symbol = match.group(2)
        line_print(color_wrap(f'SYMBOL {self.context.symbol} AT 0x{self.context.address}', Color.symbol), self.context)
        line_print('{', self.context)

    def eof(self) -> None:
        line_print('}', self.context)


class AsmLineParser:
    regex = re.compile(r'^\s*([0-9a-fA-F]+):(.*)')

    def __init__(self, context):
        self.context = context

    def run(self, match) -> None:
        if len(self.context.code):
            line_print(color_wrap(f'\n  Code: {self.context.func} at {self.context.path}:{self.context.line + 1}:', Color.type), self.context)
            for l in self.context.code:
                self.context.line += 1
                line_print(f'  {Color.line}| {self.context.line}{Color.clear} {l.rstrip()}', self.context)
            line_print(f'  {Color.type}Assembly:{Color.clear}', self.context)
            self.context.code.clear()

        addr = color_wrap(f'0x{match.group(1)}', Color.address)

        line_print(f'\t{addr}:{match.group(2)}', self.context)

    def eof(self) -> None:
        pass


class FuncLineParser:
    regex = re.compile(r'^([a-zA-Z_0-9]*)\(\):')

    def __init__(self, context):
        self.context = context

    def run(self, match) -> None:
        self.context.func = 0
        self.context.code.clear()
        self.context.func = match.group(1)

    def eof(self) -> None:
        pass


class PathLineParser:
    regex = re.compile(r'^(\/.*):([0-9]+)')

    def __init__(self, context):
        self.context = context

    def run(self, match) -> None:
        self.context.path = match.group(1)
        self.context.line = int(match.group(2))

    def eof(self) -> None:
        pass


class CodeLineParser:
    regex = re.compile(r'.*')

    def __init__(self, context):
        self.context = context

    def run(self, match) -> None:
        self.context.line -= 1
        self.context.code.append(color_wrap(match.group(0), Color.code))

    def eof(self) -> None:
        pass


def args_parse() -> Tuple[argparse.Namespace, List[str]]:
    parser = argparse.ArgumentParser()
    parser.add_argument('--jumps', action='store_true', help='Visualize jumps')
    parser.add_argument('--no-code', action='store_true', help='Don\'t inline source code')
    parser.add_argument('-b', '--binary', action='store', required=False, default='system', help='Binary to be read')
    parser.add_argument('-r', '--raw', action='store_true', help='Don\t process lines')
    return parser.parse_known_args()


def objdump_args_build(args : argparse.Namespace, other_args : List[str]) -> List[str]:
    argv = [
        '/bin/objdump',
        '--disassembler-color=color',
    ]

    if not args.no_code:
        argv.append('-Sl')
    else:
        argv.append('-dl')

    if args.jumps:
        argv.append('--visualize-jumps=extended-color')

    for function in other_args:
        argv.append(f'--disassemble={function}')

    argv.append(args.binary)

    return argv


def main() -> None:
    args, other_args = args_parse()
    context = Context()

    parsers = [
        SymbolLineParser(context),
        AsmLineParser(context),
        FuncLineParser(context),
        PathLineParser(context),
        CodeLineParser(context),
    ]

    objdump = subprocess.Popen(
        objdump_args_build(args, other_args),
        shell=False,
        stdout=subprocess.PIPE,
        universal_newlines=True)

    pager = subprocess.Popen(
        ['/bin/less', '-R'],
        shell=False,
        stdin=subprocess.PIPE,
        universal_newlines=True)

    context.stdout = pager.stdin

    try:
        for line in objdump.stdout.readlines():
            if args.raw:
                print(line, end='', file=context.stdout)
            else:
                for parser in parsers:
                    match = re.match(parser.regex, line)
                    if not match: continue
                    parser.run(match)
                    break
        for parser in parsers:
            parser.eof()
    except BrokenPipeError:
        pass

    objdump.communicate()
    pager.communicate()


if __name__ == '__main__':
    main()
