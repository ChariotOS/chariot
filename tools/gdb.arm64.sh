#!/bin/bash

gdb build/chariot.elf -iex "target remote localhost:1234"
