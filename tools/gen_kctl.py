#!/usr/bin/env python3

names = [
    'hw',
    'ncpus',
    'proc',

    # proc.PID.*
    'name',
]

def name_to_int(name: str) -> str:
    return hex(int.from_bytes(bytes(name, encoding="ascii"), "little"))

with open('include/chariot/kctl.inc', 'w+') as f:
    f.write('#define KCTL_NAME_MASK 0x80000000000000\n')
    for i, name in enumerate(names):
        f.write(f'#define KCTL_{name.upper()} (KCTL_NAME_MASK | {name_to_int(name)}LLU) // "{name}"\n')

    f.write('\n\n')
    f.write('#define ENUMERATE_KCTL_NAMES \\\n')
    for i, name in enumerate(names):
        f.write(f'   __KCTL(KCTL_{name.upper()}, {name}, {name.upper()}) \\\n')
    f.write('\n')
