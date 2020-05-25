#include <stdio.h>
#include <ck/vec.h>
#include <ck/single_list.h>
#include <ck/map.h>
#include <ck/string.h>
#include <stdlib.h>


int main() {

	ck::map<int, int> m;

	for (int i = 0; i < 1000; i++) {
		int c = rand() & 0xFF;
		m[c]++;
	}

	for (auto &kv : m) {
		printf("%3d: %5d\n", kv.key, kv.value);
	}


	ck::single_list<int> a;

	for (int i = 0; i < 10; i++) a.append(i);
	for (auto val : a) printf("%d\n", val);

	ck::string msg = "hello";
	printf("%s\n", msg.get());
	return 0;
}

