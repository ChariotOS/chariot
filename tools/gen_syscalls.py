#!/bin/env python3

# this file is intended to be the source of systemcall definitions in
# chariot. Calling the following syscall() function will define the syscall
# and plop it's defn in all the locations it needs to be

import re
import sys

defs_path = 'include/syscall.def.h'

# gross.
parser = re.compile('((unsigned )?[a-zA-Z_0-9*]+ \*?)([a-zA-Z_0-9]+)\((.*)\);')
defs = []


print('Generating systemcall tables')


class Syscall:
    def __init__(self, name, ret, args, attr):
        self.name = name
        self.ret = ret
        self.args = args
        self.attr = attr
    def k_decl(self):
        s = ''
        for k, v in self.attr.items():
            s += f'/// {k}={v}\n'
        s += f'{self.ret} {self.name}({self.args});'
        return s
    def k_macro(self):
        return f'__SYSCALL({self.attr["num"]}, {self.name})'

    def u_macro(self):
        return f'#define SYS_{self.name:24s} ({self.attr["num"]})'

with open(defs_path, 'r') as f:
    attrs = []
    for line in f:
        if line.startswith('#') or line == '\n':
            continue
        if line.startswith('///'):
            attr = line.replace('///', '').replace('\n', '')
            attrs.append(attr)
        m = parser.match(line)
        if not m:
            continue
        attrs = [s.strip() for s in ', '.join(attrs).split(', ')]
        attr_d = {}
        for a in attrs:
            parts = a.split('=')
            if len(parts) == 2:
                attr_d[parts[0]] = parts[1]
        if not 'num' in attr_d:
            print('syscall defined as "{}" does not have a syscall number.'.format(line.strip()))
            exit()


        ret = m[1]
        name = m[3]
        args = m[4]

        print(m)

        scdef = Syscall(name, ret, args, attr_d)
        defs.append(scdef)
        attrs = []



with open('include/syscalls.inc', 'w+') as f:
    for s in defs:
        f.write(s.k_macro() + '\n')


with open('usr.include/sys/syscall_defs.h', 'w+') as f:
    for s in defs:
        f.write(s.u_macro() + '\n')
