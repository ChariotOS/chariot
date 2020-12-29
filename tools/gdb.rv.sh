#!/bin/bash

riscv64-unknown-elf-gdb build/root/bin/chariot.elf -iex "target remote localhost:1234"
