#!/bin/env python3

import io
import os
import re
import cmd
import sys
import time
import select
import signal
import termios
import argparse
import traceback
import subprocess
from collections import deque
from multiprocessing import Process
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


def termios_disable_echo_icanon(file, t) -> None:
    t[3] &= ~(termios.ECHO | termios.ICANON)
    termios.tcsetattr(file.fileno(), termios.TCSANOW, t)


def termios_set(file, t) -> None:
    termios.tcsetattr(file.fileno(), termios.TCSANOW, t)


class RestoreStdin:
    def __init__(self, stdin, termios) -> None:
        self.stdin = stdin
        self.termios = termios.copy()

    def __enter__(self) -> None:
        termios_set(self.stdin, self.termios)
        sys.stdin = os.fdopen(os.dup(self.stdin.fileno()), 'r', buffering=1)

    def __exit__(self, exception_type, exception_value, traceback) -> None:
        sys.stdin.close()
        termios_disable_echo_icanon(self.stdin, self.termios)


class QemuMon(Process):
    buffer     : LineBuffer
    mon_input  : io.TextIOWrapper
    mon_output : io.TextIOWrapper


    class Cmd(cmd.Cmd):
        prompt = 'monitor> '

        def __init__(self, qemu_mon) -> None:
            super().__init__()
            self.qemu_mon = qemu_mon

        def do_exit(self, input) -> bool:
            print('Closing cmdline; enter ":" to show it again', end='', flush=True)
            return True

        do_EOF = do_exit

        def do_tlb(self, address) -> None:
            '''print TLB mapping; optionally address can be passed to show single TLB entry'''
            for line in self.qemu_mon.execute_and_read('info tlb'):
                if not ':' in line:
                    continue

                splitted = line.split()
                vaddr = int(splitted[0].strip(':'), 16)
                paddr = int(splitted[1], 16)

                if address:
                    addr = int(address, 16)
                    offset = addr & 0xfff
                    if addr - offset != vaddr:
                        continue
                print(f'{vaddr:#010x}: {paddr:#010x} {splitted[2]}')

        def do_help(self, command) -> None:
            self.qemu_mon.execute('help')

        def default(self, command) -> None:
            self.qemu_mon.execute(command)


    def __init__(self) -> None:
        self.termios = termios.tcgetattr(sys.stdin.fileno())
        super().__init__(target=self._reader_loop)
        self.start()

    def execute_and_read(self, cmd : str) -> List[str]:
        self.mon_input.write(cmd + '\n')
        return self._readlines()

    def execute(self, cmd : str) -> None:
        for line in self.execute_and_read(cmd):
            print(line)

    def _readlines(self) -> List[str]:
        lines : List[str] = []
        while True:
            output = self.buffer.readline()
            if '(qemu)' in output:
                break
            lines.append(output)
        return lines

    def _reader_loop(self) -> None:
        sys.stdin.close()
        signal.signal(signal.SIGINT, signal.SIG_IGN)

        self.mon_input = open('qemumon.in', 'w', buffering=1)
        self.mon_output = open('qemumon.out', 'r', buffering=1)
        self.buffer = LineBuffer(self.mon_output)

        stdin = open(os.ttyname(sys.stdout.fileno()), 'rb', buffering=0)
        termios_disable_echo_icanon(stdin, self.termios.copy())

        # Just to flush initial QEMU's prompt
        try:
            self._readlines()
        except:
            pass

        while True:
            line = stdin.read(1).decode()

            if line != ':': continue

            with RestoreStdin(stdin, self.termios):
                # FIXME: I can't force readline to handle escape
                # sequences properly even though original termios
                # is restored
                self.Cmd(self).cmdloop(' ')


class Qemu(subprocess.Popen):
    qemu_mon : QemuMon

    def __init__(self, qemu_args : List[str], args : argparse.Namespace) -> None:
        bufsize     : int
        qemu_stdout : int = None

        if args.raw:
            bufsize = -1
        else:
            bufsize = 1
            qemu_stdout = subprocess.PIPE

        super().__init__(
            qemu_args,
            errors='ignore',
            stdout=qemu_stdout,
            stdin=subprocess.DEVNULL,
            bufsize=bufsize,
            shell=False,
            universal_newlines=True)

        self.qemu_mon = QemuMon()

    def readline(self) -> str:
        return self.stdout.readline().rstrip('\n')

    def stop(self) -> None:
        self.communicate()
        self.qemu_mon.terminate()


class Box86(subprocess.Popen):
    def __init__(self, box86_args : List[str], args : argparse.Namespace) -> None:
        bufsize      : int
        box86_stderr : int = None

        if args.raw:
            bufsize = -1
        else:
            bufsize = 1
            box86_stderr = subprocess.PIPE

        super().__init__(
            box86_args,
            errors='ignore',
            stderr=box86_stderr,
            stdin=subprocess.DEVNULL,
            bufsize=bufsize,
            shell=False,
            universal_newlines=True)

    def readline(self) -> str:
        return self.stderr.readline().rstrip('\n')

    def stop(self) -> None:
        self.communicate()


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
            try:
                entry = (splitted[offset], splitted[offset + 2], splitted[offset + 3])
                if len(entries):
                    if entries[-1] == entry:
                        continue
            except:
                entry = ('??', '??', '??')
            entries.append(entry)

        self.cache[sys.intern(addr)] = entries

        return entries


class Context:
    addr2lines : Dict[str, Addr2Line]
    bt_number  : int
    exceptions : List[str]
    args       : argparse.Namespace

    def __init__(self, args : argparse.Namespace) -> None:
        self.addr2lines = {}
        self.bt_number = 0
        self.exceptions = []
        self.args = args

    def addr2line(self, binary : str) -> Addr2Line:
        if not binary in self.addr2lines:
            self.addr2lines[binary] = Addr2Line(binary)
        return self.addr2lines[binary]


def color_wrap(string : str, color : str) -> str:
    return f'{color}{string}{Color.clear}'


def gdb_stacktrace_line(
    number    : int,
    addr      : str,
    func      : str,
    file      : str,
    line      : str
) -> str:
    formatted_line = [
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


def line_print(line : str, continuation=False) -> None:
    if continuation:
        line = line.encode()
    else:
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


def line_continuation_print(line : str) -> None:
    line = f'{line}'.encode()
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


def backtrace_get(addr: str, binary : str, context : Context) -> List[str]:
    try:
        lines : List[str] = []

        addr2line = context.addr2line(binary)

        entries = addr2line.resolve(addr)

        if len(entries) == 0:
            raise Exception(f'No entries given for {binary} at {addr}')

        for func, file, linenr in entries:
            context.bt_number += 1

            stacktrace_line = gdb_stacktrace_line(context.bt_number, addr, func, file, linenr)
            lines.append(stacktrace_line)

    except Exception as e:
        id = exception_save(context)
        line_print(f'>> Internal exception #{id} encountered for {binary} at {addr}')

    return lines


line_regex = re.compile(r'^([0-9]),([0-9]*),([0-9]*\.[0-9]*);(.*)$')

loglevel_to_color = [
    '',
    '\033[38;5;245m',
    Color.blue,
    Color.blue,
    Color.yellow,
    Color.red,
    Color.red,
    Color.magenta,
]

def line_process(line : str, context : Context) -> None:

    if line[0] == ' ':
        line_print(line[1:], continuation=True)
        return

    match = re.match(line_regex, line)

    if not match:
        line_print(line)
        return

    loglevel = int(match.group(1))
    sequence = int(match.group(2))
    timestamp = match.group(3)
    content = match.group(4)

    if 'backtrace:' in content:
        context.bt_number = 0

    if match := re.search(r'USER:([x0-9a-f]*) ([x0-9a-f]*) (.*):', content):
        # FIXME: how to properly detect non-dynamic executable
        # and print both addresses as the same in kernel?
        addr = match.group(2)

        binary = f'mnt{match.group(3)}'

        if '[' in binary:
            content = gdb_stacktrace_line(context.bt_number, addr, '??', '??', '??')
            line = f'{Color.green}[{timestamp:>14}] {Color.clear}{content}'
            line_print(line)
            return

        for content in backtrace_get(addr, binary, context):
            line = f'{Color.green}[{timestamp:>14}] {Color.clear}{content}'
            line_print(line)

    # Format of backtrace printed by kernel:
    # [       2.767291] [<0x0000baf2>] __libc_strcspn+0x52/0x94
    elif '[<' in line:
        splitted = re.split('[<>]', line)
        addr = splitted[1]
        binary = 'system'

        for content in backtrace_get(addr, binary, context):
            line = f'{Color.green}[{timestamp:>14}] {Color.clear}{content}'
            line_print(line)

    else:
        line = f'{Color.green}[{timestamp:>14}] {loglevel_to_color[loglevel]}{content}'
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

    args, emu_args = args_parse()

    emu = None

    if 'qemu' in emu_args[0]:
        emu = Qemu(emu_args, args)
    elif '86Box' in emu_args[0]:
        emu = Box86(emu_args, args)

    context = Context(args)

    sys.stdin.close()
    signal.signal(signal.SIGINT, signal.SIG_IGN)

    if not args.raw:
        while True:
            line = emu.readline()

            if not line:
                break

            line_process(line, context)

    emu.stop()
    print()

    exceptions_print(context)


if __name__ == '__main__':
    main()
