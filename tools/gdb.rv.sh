#!/bin/bash

riscv64-gdb build/chariot.elf -iex "target remote localhost:1234"
