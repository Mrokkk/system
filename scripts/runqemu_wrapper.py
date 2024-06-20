#!/bin/env python3

import io
import os
import re
import sys
import time
import select
import argparse
import traceback
import subprocess
from collections import deque
from typing import List, Dict, Tuple, TypeAlias


class Color:
    red = '\033[31m'
    green = '\033[32m'
    yellow = '\033[33m'
    blue = '\033[34m'
    magenta = '\033[35m'
    cyan = '\033[36m'
    bold_red = '\033[91m'
    bold_green = '\033[92m'
    bold_yellow = '\033[93m'
    bold_blue = '\033[94m'
    bold_magenta = '\033[95m'
    bold_cyan = '\033[96m'
    clear = '\033[0m'


class EOF(Exception):
    pass


# 2-stage buffering:
# 1. line-buffered stream which flushes data to next stage on new-line
# 2. buffer of unread lines from which readline pops line
class LineBuffer:
    input_stream : io.TextIOWrapper
    buffer       : deque[str]
    lines        : deque[str]

    def __init__(self, input_stream, timeout=0.02) -> None:
        self.input_stream = input_stream
        self.buffer = deque()
        self.lines = deque()
        self.timeout = timeout
        os.set_blocking(self.input_stream.fileno(), False)

    def readline(self) -> str:
        while not len(self.lines):
            reads, _, _ = select.select([self.input_stream], [], [], self.timeout)

            if not self.input_stream in reads:
                line = ''.join(self.buffer)
                self.buffer.clear()
                if line:
                    self.lines.append(line)
                continue

            output = self.input_stream.read(0x10000)

            if not output:
                raise EOF()

            if '\n' in output:
                splitted = output.split('\n')

                for line in splitted[:-1]:
                    if not line and not len(self.buffer):
                        continue

                    self.buffer.append(line)
                    self.lines.append(''.join(self.buffer))
                    self.buffer.clear()

                self.buffer.append(splitted[-1])
            else:
                self.buffer.append(output)

        return self.lines.popleft()


class Qemu(subprocess.Popen):
    buffer : LineBuffer

    def __init__(self, qemu_args : List[str], args : argparse.Namespace) -> None:
        bufsize     : int = 1
        qemu_stdout : int = None

        if args.raw:
            bufsize = -1
        else:
            bufsize = 1
            qemu_stdout = subprocess.PIPE

        super().__init__ (
            qemu_args,
            errors='ignore',
            stdout=qemu_stdout,
            bufsize=bufsize,
            shell=False,
            universal_newlines=True)

        if not args.raw:
            self.buffer = LineBuffer(self.stdout)

    def readline(self) -> str:
        return self.buffer.readline()


FuncFileLine : TypeAlias = Tuple[str, str, str]


class Addr2Line:
    binary : str
    cache  : Dict[str, List[FuncFileLine]]

    def __init__(self, binary : str) -> None:
        self.cache = {}
        self.binary = binary

    def resolve(self, addr : str) -> List[FuncFileLine]:

        if addr in self.cache:
            return self.cache[addr]

        # Spawning new addr2line is faster than keeping it running in interactive
        # mode and polling for all data (as I don't know how many lines will addr2line
        # print - there might be only 1, there might be more if there are some inlined
        # calls)
        addr2line = subprocess.Popen(
            ['/bin/addr2line', '-e', self.binary, '-f', '-C', '-i', '-p', addr],
            stdout=subprocess.PIPE,
            stdin=subprocess.PIPE,
            shell=False,
            universal_newlines=True)

        stdout, _ = addr2line.communicate()

        entries : List[FuncFileLine] = []

        for line in stdout.splitlines():
            offset = 0
            line = line.strip()
            if '(inlined' in line:
                offset = 2
            splitted = re.split(r'[ :]', line)
            entry = (splitted[offset], splitted[offset + 2], splitted[offset + 3])
            if len(entries):
                if entries[-1] == entry:
                    continue
            entries.append(entry)

        self.cache[sys.intern(addr)] = entries

        return entries


class Context:
    addr2lines : Dict[str, Addr2Line]
    bt_number  : int
    last_app   : str
    exceptions : List[str]
    args       : argparse.Namespace

    def __init__(self, args : argparse.Namespace) -> None:
        self.addr2lines = {}
        self.bt_number = 0
        self.last_app = None
        self.exceptions = []
        self.args = args

    def addr2line(self, binary : str) -> Addr2Line:
        if not binary in self.addr2lines:
            self.addr2lines[binary] = Addr2Line(binary)
        return self.addr2lines[binary]


def color_wrap(string : str, color : str) -> str:
    return f'{color}{string}{Color.clear}'


def gdb_stacktrace_line(
    timestamp : str,
    number    : int,
    addr      : str,
    func      : str,
    file      : str,
    line      : str
) -> str:
    formatted_line = [
        f'{timestamp}{Color.clear}',
        f'#{number} ',
        color_wrap(addr, Color.blue),
        ' in ',
        color_wrap(func, Color.yellow),
        ' at ',
        color_wrap(file, Color.green),
        f':{line}'
    ]
    return ''.join(formatted_line)


def exception_save(context : Context) -> int:
    id = len(context.exceptions)
    context.exceptions.append(traceback.format_exc())
    return id


def line_print(line : str) -> None:
    line = f'\n{line}'.encode()
    length = len(line)
    index = 0

    while True:
        _, writes, _ = select.select([], [sys.stdout], [])

        if sys.stdout in writes:
            try:
                res = os.write(sys.stdout.fileno(), line[index:])
                length -= res
                if length:
                    index += res
                    continue
                break
            except BlockingIOError:
                pass


def line_process(line : str, context : Context) -> None:

    if 'backtrace:' in line:
        context.bt_number = 0
        line_print(line)

    elif '/bin' in line:
        match = re.search(r'\/bin\/(.*)\[[0-9]*\]:', line)

        if match:
            app = match.group(1)
            context.last_app = f'mnt/bin/{app}'

        line_print(line)

    # Format of backtrace printed by kernel:
    # [       2.767291] [<0x0000baf2>] __libc_strcspn+0x52/0x94
    elif '[<' in line:
        splitted = re.split('[<>]', line)
        addr = splitted[1]
        binary : str = None

        try:
            if int(addr, 16) > 0xc0100000:
                binary = 'system'
            elif context.last_app:
                binary = context.last_app

            addr2line = context.addr2line(binary)

            timestamp = splitted[0][:-1]
            entries = addr2line.resolve(addr)

            if len(entries) == 0:
                raise Exception(f'No entries given for {binary} at {addr}')

            for func, file, linenr in entries:
                context.bt_number += 1

                stacktrace_line = gdb_stacktrace_line(timestamp, context.bt_number, addr, func, file, linenr)
                line_print(stacktrace_line)

        except Exception as e:
            id = exception_save(context)
            line_print(f'>> Internal exception #{id} encountered for {binary} at {addr}')

    elif '(qemu)' in line:
        line_print(line)

    elif line.startswith('\x1b'):
        line_print(line)


def exceptions_print(context : Context) -> None:

    if not len(context.exceptions):
        return

    print(color_wrap('Internal exceptions:', Color.red))
    for i, exception in enumerate(context.exceptions):
        print(color_wrap(f'Exception #{i}:', Color.red))
        for line in exception.splitlines():
            print(f'>>>> {line}')


def args_parse() -> Tuple[argparse.Namespace, List[str]]:

    parser = argparse.ArgumentParser()
    parser.add_argument('--raw', action='store_true', help='Don\'t process lines')
    return parser.parse_known_args()


def main() -> None:

    args, qemu_args = args_parse()

    qemu = Qemu(qemu_args, args)
    context = Context(args)

    if not args.raw:
        while True:
            try:
                line = qemu.readline()
            except EOF:
                break
            except Exception as e:
                id = exception_save(context)
                line_print(f'>> Internal exception #{id} encountered when reading line')

            line_process(line, context)

    qemu.communicate()
    print()

    exceptions_print(context)


if __name__ == '__main__':
    main()
