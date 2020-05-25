#include <ck/defer.h>
#include <ck/map.h>
#include <ck/ptr.h>
#include <ck/single_list.h>
#include <ck/string.h>
#include <ck/vec.h>
#include <stdio.h>
#include <stdlib.h>
#include <ck/command.h>

void print(const char *msg) {
	ck::command("echo", msg).exec();
}

int main() {
	print("hello, world");
	print("this is a test.");
  return 0;
}

