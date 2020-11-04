#!/bin/bash

FILE="$1"
shift


OUTPUT="$1"
shift

NAME="$1"
shift

ASM_FILE="$OUTPUT.res.asm"

touch $ASM_FILE
echo "[section .resources]" >> $ASM_FILE
echo "  dq the_name" >> $ASM_FILE
echo "  dq the_data" >> $ASM_FILE
echo "  dq the_end - the_data" >> $ASM_FILE
echo "[section .data]" >> $ASM_FILE
echo "the_name: db \"$NAME\", 0" >> $ASM_FILE
echo "the_data: incbin \"$FILE\"" >> $ASM_FILE
echo "the_end:" >> $ASM_FILE

# cat $ASM_FILE

nasm -felf64 $ASM_FILE -o "$OUTPUT.res.o"
