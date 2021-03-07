#!/bin/bash


# script_path=$(cd -P -- "$(dirname -- "$0")" && pwd -P)
# cd "${script_path}/.." || exit 1

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

grepper='\.c$\|\.cpp$\|\.h$'

files=$(git diff --name-only | grep $grepper)
files+=$(git ls-files | grep $grepper)


files=$(echo "${files[@]}" | tr ' ' '\n' | sort -u | tr '\n' ' ')

for FILE in $files
do
	echo $FILE
	clang-format -style=file -i $FILE
done
