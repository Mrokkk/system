#!/bin/env python3

import os
import re
import sys
import jinja2
from typing import List, Tuple, TextIO, Set, Dict


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


trace_syscall_template : str = """#include <kernel/trace.h>
#include <kernel/kernel.h>
#include <kernel/api/types.h>

syscall_t trace_syscalls[] = {
    { },
{% for syscall in syscalls %}
    {
        .name   = "{{ syscall.name }}",
        .ret    = {{ syscall.ret_enum }},
        .nargs  = {{ syscall.arg_types.__len__() }},
        .args   = { {{ syscall.arg_enums|join(', ')  }} },
    },
{% endfor -%}
};
"""


syscall_regex : str = r'(.*)\: (.*)'


class Syscall:
    name        : str
    ret_type    : str
    ret_enum    : str
    arg_types   : List[str]
    arg_enums   : List[str]

    def __init__(self, name=None, variant=None, ret_type=None, arg_types=None):
        self.name       = name
        self.ret_type   = ret_type
        self.ret_enum   = type_convert(ret_type)
        self.arg_types  = arg_types
        self.headers    = []
        self.arg_enums  = [type_convert(x) for x in arg_types]

    @property
    def variant(self) -> str:
        variant : str = f'__syscall{len(self.arg_types)}'
        if self.ret_type == 'void':
            return f'{variant}_noret'
        else:
            return variant

    @property
    def line(self) -> str:
        if len(self.arg_types) == 0:
            return f'{self.variant}({self.name}, {self.ret_type})'
        else:
            return f'{self.variant}({self.name}, {self.ret_type}, {', '.join(self.arg_types)})'


def syscall_create(match : re.Match, line : str) -> Syscall:
    signature = match.group(2).split(',')
    return Syscall(
        name        = match.group(1),
        ret_type    = signature[0].strip(),
        arg_types   = [x.strip() for x in signature[1:]])


def read_data(lines : List[bytes]) -> Tuple[List[Syscall], List[str], List[str]]:
    syscalls    : List[Syscall] = []
    variants    : Set[str] = set()
    structs     : Set[str] = set()

    for line in [l.strip() for l in lines]:
        if line[0] == '#':
            continue

        match   = re.search(syscall_regex, line)

        if not match:
            continue

        syscall = syscall_create(match, line)

        syscalls.append(syscall)
        variants.add(syscall.variant)

        for t in syscall.arg_types:
            if 'struct' in t:
                structs.add(t.replace('*', '').replace('const', '').strip())

    return (syscalls, sorted(variants), sorted(structs))


def type_convert(t : str):
    if t == 'char*': return 'TYPE_CHAR_PTR'
    elif t == 'const char*': return 'TYPE_CONST_CHAR_PTR'
    elif '*' in t: return 'TYPE_VOID_PTR'
    elif t in ('int', 'long', 'gid_t'): return 'TYPE_LONG'
    elif t == 'short': return 'TYPE_SHORT'
    elif t == 'char': return 'TYPE_CHAR'
    elif t in ('unsigned long', 'unsigned int', 'size_t', 'off_t', 'uint32_t', 'time_t', 'clockid_t', 'timer_t'): return 'TYPE_UNSIGNED_LONG'
    elif t in ('unsigned short', 'mode_t', 'uid_t', 'dev_t'): return 'TYPE_UNSIGNED_SHORT'
    elif t == 'void': return 'TYPE_VOID'
    else:
        print(f'Unknown type: {t}')
        sys.exit(-1)


def main():
    include_dir : str = f'{os.path.dirname(os.path.realpath(__file__))}/../include/kernel'
    trace_dir : str = f'{os.path.dirname(os.path.realpath(__file__))}/../kernel/trace'
    input_file  : str = f'{include_dir}/syscall.in'
    syscall_h_path : str = f'{include_dir}/syscall.h'
    trace_gen_c_path : str = f'{trace_dir}/trace_gen.c'

    with open(input_file, mode='r') as f:
        f : TextIO
        syscalls, variants, structs = read_data(f.readlines())

    syscall_h = jinja2.Template(syscall_h_template).render(
        syscalls=syscalls,
        structs=structs,
        syscall_variants=variants)

    trace_gen_c = jinja2.Template(trace_syscall_template).render(
        syscalls=syscalls)

    with open(syscall_h_path, mode='w') as f:
        f : TextIO
        f.write(syscall_h)

    with open(trace_gen_c_path, mode='w') as f:
        f : TextIO
        f.write(trace_gen_c)


if __name__ == '__main__':
    main()
