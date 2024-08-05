#!/bin/env python3

import os
import json
import subprocess
from typing import List, Tuple, Dict, TypeAlias

JSON: TypeAlias = dict[str, 'JSON'] | list['JSON'] | str | int | float | bool | None

default_clang_flags = [
    '-Wno-error',
    '-Wno-format',
    '-Wno-address-of-packed-member',
    '-Wno-unused-command-line-argument',
    '-Wno-c2x-extensions',
    '-Wno-strict-prototypes',
    '-D_GCC_MAX_ALIGN_T',
]


def read_compile_commands() -> JSON:
    with open('compile_commands.json', mode='r') as f:
        return json.loads(f.read())


def save_compile_commands_to(compile_commands : JSON, directory : str):
    with open(f'{directory}/compile_commands.json', mode='w') as f:
        json.dump(compile_commands, f, indent = 4)


def read_compiler(compile_commands : JSON) -> str:
    return compile_commands[0]['command'].split()[0]


def get_compiler_include_paths(compiler : str) -> List[str]:
    include_paths : List[str] = []

    proc = subprocess.Popen(
        [compiler, '-xc', '-E', '-v', '-'],
        shell=False,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)

    stdout, stderr = proc.communicate()

    start = False
    for line in stderr.decode().splitlines():
        if '#include <...>' in line:
            start = True
        elif 'End of search list' in line:
            break
        elif start == True:
            include_paths.append(line.strip())

    return include_paths


def main():
    script_path : str = os.path.realpath(__file__)
    project_root : str = os.path.realpath(os.path.dirname(script_path) + '/..')
    compile_commands = read_compile_commands()

    compiler = read_compiler(compile_commands)
    include_paths = get_compiler_include_paths(compiler)

    for entry in compile_commands:
        flags = entry['command'].split()
        flags.extend(default_clang_flags)
        if 'bin' in entry['file']:
            flags.extend([f'-isystem {x}' for x in include_paths])
        else:
            flags.extend([f'-isystem {x}' for x in include_paths if 'native' in x])
        entry['command'] = ' '.join(flags)

    save_compile_commands_to(compile_commands, project_root)


if __name__ == '__main__':
    main()
