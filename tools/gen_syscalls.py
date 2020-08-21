#!/bin/env python3

# this file is intended to be the source of systemcall definitions in
# chariot. Calling the following syscall() function will define the syscall
# and plop it's defn in all the locations it needs to be

import re
import sys
import toml


src = toml.load('meta/syscalls.toml')


next_syscall_num = 0

register_order = ['rdi', 'rsi', 'rdx', 'rcx', 'r8', 'r9']

class Systemcall:
    def __init__(self, name, entry):
        global next_syscall_num
        self.name = name
        self.data = entry
        self.num = next_syscall_num
        next_syscall_num += 1


        if 'ret' not in self.data:
            self.data['ret'] = 'void'

        if 'args' not in self.data:
            self.data['args'] = []


        self.args = []
        for arg in self.data['args']:
            parts = arg.split(': ')
            self.args.append((parts[0].strip(), parts[1].strip()))


    def format_args(self):
        return ", ".join(f'{b} {a}' for (a, b) in self.args)

    def cdecl(self, prefix = ''):
        return '{} {}{}({})'.format(self.data['ret'], prefix, self.name, self.format_args())

    def cmacro(self):
        args = ""
        if len(self.args) > 0:
            args = ', ' + self.format_args()
        return f'__SYSCALL(0x{self.num:02x}, {self.name}{args})'

syscalls = []
for sc in src['sc']:
    syscalls.append(Systemcall(sc, src['sc'][sc]))



with open('include/syscalls.inc', 'w+') as f:
    for s in syscalls:
        f.write(s.cmacro() + '\n')



with open('usr.include/sys/syscall_defs.h', 'w+') as f:
    for s in syscalls:
        f.write(f'#define SYS_{s.name:24s} (0x{s.num:02x})\n')






with open('include/syscall.def.h', 'w+') as f:
    for inc in src['kernel']['includes']:
        f.write(f'#include {inc}\n')

    f.write('namespace sys {\n')
    for s in syscalls:
        f.write(s.cdecl() + ';\n')
    f.write('}\n')







with open('usr.include/sys/sysbind.h', 'w+') as f:
    f.write('// Generated by tools/gen_syscalls.py. Do not change\n')

    f.write("#pragma once\n")


    f.write('#ifdef USERLAND\n')
    for inc in src['userspace']['includes']:
        f.write(f'#include {inc}\n')
    f.write('#else\n')
    for inc in src['kernel']['includes']:
        f.write(f'#include {inc}\n')
    f.write('#endif\n\n')

    f.write('#ifdef __cplusplus\n')
    f.write('extern "C" {\n')
    f.write('#endif\n')

    f.write('\n')
    for s in syscalls:
        f.write(s.cdecl('sysbind_') + ';\n')


    for s in syscalls:
       if s.data['ret'] == 'int' and False:
           f.write(s.cdecl('errbind_') + ';\n')

    f.write('#ifdef __cplusplus\n')
    f.write('}\n')

    f.write('#endif\n')


with open('libc/src/sysbind.c', 'w+') as f:
    f.write("#include <sys/sysbind.h>\n");
    f.write("#include <sys/syscall.h>\n");
    f.write("""
extern long __syscall_eintr(int num, unsigned long long a, unsigned long long b,
                      unsigned long long c, unsigned long long d,
                      unsigned long long e, unsigned long long f);""")

    for sc in syscalls:
        prefixes = [
            ('sysbind_', lambda call: call)
        ]

        if sc.data['ret'] == 'int' and False:
            prefixes.append(('errbind_', lambda call: f'errno_wrap({call})'))

        for pfx in prefixes:
            decl = sc.cdecl(pfx[0])
            args = ['0'] * 6
            ret = sc.data['ret']
            for i, arg in enumerate(sc.args):
                args[i] = '(unsigned long long)' + arg[0]
            f.write(f'{decl} {{\n')

            the_call = f'__syscall_eintr({sc.num}, {", ".join(args)})'
            f.write(f'    return ({ret}){pfx[1](the_call)};\n')
            f.write(f'}}\n')
            f.write(f'\n')
