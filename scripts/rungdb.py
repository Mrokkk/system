#!/bin/env python3

import argparse
import subprocess

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('-a', '--app', action='store')
    parser.add_argument('-p', '--port', action='store')
    return parser.parse_args()

def build_gdb_args(args):
    a = [
        'gdb',
        '-ex', 'set confirm off',
        '-ex', 'symbol-file system',
        '-ex', 'target remote localhost:9000',
    ]
    if args.app:
        a.extend(['-ex', f'add-symbol-file bin/{args.app}/{args.app}'])
    if args.port:
        a.extend(['-ex', f'add-symbol-file sysroot/bin/{args.port}'])
    return a

def main():
    args = parse_args()
    gdb_args = build_gdb_args(args)
    print(gdb_args)

    gdb = subprocess.Popen(
        build_gdb_args(args),
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
