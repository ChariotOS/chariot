#include <stdio.h>
#include <ck/vec.h>
#include <ck/string.h>

int main() {
	ck::vec<int> a;
	ck::string msg = "hello";
	printf("%s\n", msg.get());
	return 0;
}


