#!/bin/env python3

import os
import re
import argparse
import subprocess
from typing import List, Dict, Any


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument('-a', '--app', action='store')
    return parser.parse_args()


def build_gdb_args(app : str) -> List[str]:
    a = ['gdb', '-q']

    if app:
        a.extend(['-ex', f'py app = "{app}"'])

    a.extend(['-x', os.path.realpath(__file__)])

    return a


def main() -> None:
    args = parse_args()
    app : str = None

    if args.app:
        app = f'mnt/bin/{args.app}'

    # This script is injected into gdb,
    # where it starts from gdb_main()
    gdb = subprocess.Popen(
        build_gdb_args(app),
        errors='ignore',
        shell=False)

    while True:
        try:
            gdb.wait();
            if not gdb.returncode is None:
                return
        except KeyboardInterrupt:
            pass


def singleton(orig_class):
    class Wrapper(orig_class):
        _instance = None

        def __new__(orig_class, *args, **kwargs):
            if Wrapper._instance is None:
                Wrapper._instance = super(Wrapper, orig_class).__new__(orig_class, *args, **kwargs)
                Wrapper._instance._sealed = False
            return Wrapper._instance

        def __init__(self, *args, **kwargs):
            if self._sealed:
                return
            super(Wrapper, self).__init__(*args, **kwargs)
            self._sealed = True

    Wrapper.__name__ = orig_class.__name__
    return Wrapper


class Executable:
    path      : str
    libraries : Dict[str, int]
    address   : int

    def __init__(self, path : str, address : int) -> None:
        self.path = path
        self.address = address
        self.libraries = {}


@singleton
class GdbContext:
    loaded_executable    : Executable
    breakpoint_handlers  : Dict[str, Any]
    requested_executable : str

    def __init__(self) -> None:
        self.requested_executable = None
        self.loaded_executable = None
        self.breakpoint_handlers = {}


def sections_read(binary : str) -> Dict[str, int]:
    readelf = subprocess.Popen(
        ['/bin/readelf', '-S', '-W', binary],
        errors='ignore',
        stdout=subprocess.PIPE,
        shell=False)

    line_regex = re.compile(r'^\s*\[\s*[0-9]*\]')

    sections : Dict[str, int] = {}

    for line in readelf.stdout.readlines():
        match = re.search(line_regex, line)

        if not match or ' 0]' in match.group(0):
            continue

        line = line.removeprefix(match.group(0))
        splitted = line.strip().split()
        name = splitted[0]
        flags = splitted[6]
        address = int('0x' + splitted[2], 16)
        offset = int('0x' + splitted[3], 16)

        if not 'A' in flags:
            continue

        if '.ko' in binary:
            # In case of kernel module (which is a plain object file),
            # actual address for all sections is 0, so I need to use
            # offset
            address = offset

        sections[name] = address

    readelf.communicate()

    return sections


def symbol_file_load(binary : str, base_address : int) -> int:
    sections = sections_read(binary)

    cmd = f'add-symbol-file {binary}'

    for section, address in sections.items():
        cmd += f' -s {section} {hex(address + base_address)}'

    print(f'loading {binary} @ {hex(base_address)}')
    gdb.execute(cmd)

    return sections['.text']


def symbol_file_unload(address : int) -> None:
    gdb.execute(f'remove-symbol-file -a {hex(address)}')


class Breakpoint:

    def __init__(self, name : str) -> None:
        bp = gdb.Breakpoint(name, type=gdb.BP_HARDWARE_BREAKPOINT)
        bp.silent = True
        GdbContext().breakpoint_handlers[name] = self


class KernelBreakpoint(Breakpoint):

    def __init__(self) -> None:
        super().__init__('breakpoint')

    def __call__(self) -> None:
        gdb.execute('finish')


class ModuleBreakpoint(Breakpoint):

    def __init__(self) -> None:
        super().__init__('module_breakpoint')

    def __call__(self) -> None:
        binary = 'mnt' + gdb.parse_and_eval('name').string()
        base_address = gdb.parse_and_eval('base_address')
        symbol_file_load(binary, base_address)


class LoaderBreakpoint(Breakpoint):

    def __init__(self) -> None:
        super().__init__('loader_breakpoint')

    def __call__(self) -> None:
        context = GdbContext()

        if not context.requested_executable:
            return

        binary = 'mnt' + gdb.parse_and_eval('name').string()
        base_address = gdb.parse_and_eval('base_address')

        if not context.loaded_executable:
            if context.requested_executable == binary:
                address = symbol_file_load(binary, base_address)
                context.loaded_executable = Executable(binary, address)
            else:
                gdb.execute('continue')
        elif '.so' in binary:
            if not binary in context.loaded_executable.libraries:
                address = symbol_file_load(binary, base_address)
                context.loaded_executable.libraries[binary] = address
            gdb.execute('continue')


def stop_handler(event) -> None:
    if not hasattr(event, 'breakpoints'):
        return

    for b in event.breakpoints:
        func = b.location

        if func in GdbContext().breakpoint_handlers:
            GdbContext().breakpoint_handlers[func]()


def gdb_main() -> None:

    if 'app' in globals():
        GdbContext().requested_executable = globals()['app']

    gdb.execute('set pagination off')
    gdb.execute('set confirm off')
    gdb.execute('file system')
    gdb.execute('target remote localhost:9000')
    symbol_file_load('mnt/lib/ld.so', 0xbff00000)

    KernelBreakpoint()
    ModuleBreakpoint()
    LoaderBreakpoint()

    gdb.events.stop.connect(stop_handler)


if __name__ == '__main__':
    if 'gdb' in globals():
        gdb_main()
    else:
        main()
