#!/bin/env python3

import re
import argparse
import subprocess
from typing import List


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument('-a', '--app', action='store')
    parser.add_argument('-p', '--port', action='store')
    return parser.parse_args()


def build_gdb_args(app : str, text_start : str) -> List[str]:
    a = [
        'gdb',
        '-ex', 'set confirm off',
        '-ex', 'symbol-file system',
        '-ex', 'target remote localhost:9000',
    ]

    if app:
        a.extend(['-ex', f'add-symbol-file {app} {text_start}'])

    a.extend(['-x', '../scripts/kernel.gdb'])

    return a


def text_start_get(app : str) -> int:
    objdump = subprocess.Popen(
        ['/bin/objdump', '-d', '--prefix-addresses', '-j', '.text', app],
        errors='ignore',
        stdout=subprocess.PIPE,
        shell=False)

    asm_regex = re.compile(r'([a-f0-9]+) ')

    for line in objdump.stdout.readlines():
        match = re.match(asm_regex, line)
        if match:
            break

    objdump.communicate()

    return int(f'0x{match.group(1)}', 16)


def main() -> None:
    args = parse_args()
    text_start : str = None
    app : str = None

    if args.app:
        app = f'mnt/bin/{args.app}'
        text_start = text_start_get(app) + 0x1000
    elif args.port:
        app = f'mnt/bin/{args.port}'
        text_start = text_start_get(app) + 0x1000

    gdb = subprocess.Popen(
        build_gdb_args(app, text_start),
        errors='ignore',
        shell=False)

    while True:
        try:
            gdb.wait();
            if not gdb.returncode is None:
                return
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    main()
