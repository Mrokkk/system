#!/bin/env python3

import os
import re
import jinja2
from typing import List, Tuple, TextIO, Set


syscall_h_template : str = """#ifndef __SYSCALL_H
#define __SYSCALL_H

{% for syscall in syscalls -%}
#define {{ "__NR_%-14s %u" | format(syscall.name, loop.index) }}
{% endfor %}
#define {{ "__NR_%-14s %u" | format("syscalls", syscalls.__len__() + 1) }}

#ifndef __ASSEMBLER__

{% for struct in structs -%}
{{ struct }};
{% endfor %}
#endif  // __ASSEMBLER__

#endif  // __SYSCALL_H

{% for syscall_variant in syscall_variants  -%}
#ifndef {{ syscall_variant }}
#define {{ syscall_variant }}(...)
#endif
{% endfor %}
{% for syscall in syscalls -%}
{{ syscall.line }}
{% endfor -%}"""


syscall_regex : str = r'(__syscall.*)\(\s*([a-z0-9_]*),\s(.*)\)'


class Syscall:
    name        : str
    variant     : str
    ret_type    : str
    arg_types   : List[str]
    line        : str

    def __init__(self, name=None, variant=None, ret_type=None, arg_types=None, line=None):
        self.name       = name
        self.variant    = variant
        self.ret_type   = ret_type
        self.arg_types  = arg_types
        self.line       = line


def syscall_create(match : re.Match, line : str) -> Syscall:
    signature = match.group(3).split(',')
    return Syscall(
        name        = match.group(2),
        variant     = match.group(1),
        ret_type    = signature[0].strip(),
        arg_types   = [x.strip() for x in signature[1:]],
        line        = line)


def read_data(lines : List[bytes]) -> Tuple[List[Syscall], List[str], List[str]]:
    syscalls    : List[Syscall] = []
    variants    : Set[str] = set()
    structs     : Set[str] = set()

    for line in [l.strip() for l in lines]:
        match   = re.search(syscall_regex, line)
        syscall = syscall_create(match, line)

        syscalls.append(syscall)
        variants.add(syscall.variant)

        for t in syscall.arg_types:
            if 'struct' in t:
                structs.add(t.replace('*', '').replace('const', '').strip())

    return (sorted(syscalls, key=lambda x: x.name), sorted(variants), sorted(structs))


def main():
    include_dir : str = f'{os.path.dirname(os.path.realpath(__file__))}/../include/kernel'
    input_file  : str = f'{include_dir}/syscall.h.in'
    output_file : str = f'{include_dir}/syscall.h'

    with open(input_file, mode='r') as f:
        f : TextIO
        syscalls, variants, structs = read_data(f.readlines())

    syscall_h = jinja2.Template(syscall_h_template).render(
        syscalls=syscalls,
        structs=structs,
        syscall_variants=variants)

    with open(output_file, mode='w') as f:
        f : TextIO
        f.write(syscall_h)


if __name__ == '__main__':
    main()
