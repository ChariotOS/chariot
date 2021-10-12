#!/bin/bash

riscv64-linux-gnu-gdb build/chariot.elf -iex "target remote localhost:1234"
