#!/bin/bash

riscv64-elf-gdb build/chariot.elf -iex "target remote localhost:1234"
