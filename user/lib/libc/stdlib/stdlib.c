#include <chariot.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

extern char **environ;


static void insertion_sort(void* bot, size_t nmemb, size_t size, int (*compar)(const void*, const void*))
{
	int cnt;
	unsigned char ch;
	char *s1, *s2, *t1, *t2, *top;
	top = (char*)bot + nmemb * size;
	for (t1 = (char*)bot + size; t1 < top;) {
		for (t2 = t1; (t2 -= size) >= (char*)bot && compar(t1, t2) < 0;)
			;
		if (t1 != (t2 += size)) {
			for (cnt = size; cnt--; ++t1) {
				ch = *t1;
				for (s1 = s2 = t1; (s2 -= size) >= t2; s1 = s2)
					*s1 = *s2;
				*s1 = ch;
			}
		} else
			t1 += size;
	}
}

static void insertion_sort_r(void* bot, size_t nmemb, size_t size, int (*compar)(const void*, const void*, void*), void* arg)
{
	int cnt;
	unsigned char ch;
	char *s1, *s2, *t1, *t2, *top;
	top = (char*)bot + nmemb * size;
	for (t1 = (char*)bot + size; t1 < top;) {
		for (t2 = t1; (t2 -= size) >= (char*)bot && compar(t1, t2, arg) < 0;)
			;
		if (t1 != (t2 += size)) {
			for (cnt = size; cnt--; ++t1) {
				ch = *t1;
				for (s1 = s2 = t1; (s2 -= size) >= t2; s1 = s2)
					*s1 = *s2;
				*s1 = ch;
			}
		} else
			t1 += size;
	}
}

void qsort(void* bot, size_t nmemb, size_t size, int (*compar)(const void*, const void*))
{
	if (nmemb <= 1)
		return;

	insertion_sort(bot, nmemb, size, compar);
}

void qsort_r(void* bot, size_t nmemb, size_t size, int (*compar)(const void*, const void*, void*), void* arg)
{
	if (nmemb <= 1)
		return;

	insertion_sort_r(bot, nmemb, size, compar, arg);
}


int system(const char *command) {
	int err = 0;

	pid_t pid = spawn();
	if (pid <= -1) {
		err = -1;
		goto cleanup;
	}

	char *args[] = {"/bin/sh", "-c", (char *)command, NULL};

	int start_res = startpidvpe(pid, "/bin/sh", args, environ);
	if (start_res == 0) {
		int stat = 0;
		// TODO: block SIGCHILD and ignore SIGINT and SIGQUIT
		waitpid(pid, &stat, 0);
	} else {
		despawn(pid);
	}
cleanup:

	return err;
}

static void __env_rm_add(char *old, char *new) {
	static char **env_alloced;
	static size_t env_alloced_n;
	for (size_t i = 0; i < env_alloced_n; i++)
		if (env_alloced[i] == old) {
			env_alloced[i] = new;
			free(old);
			return;
		} else if (!env_alloced[i] && new) {
			env_alloced[i] = new;
			new = 0;
		}
	if (!new) return;
	char **t = realloc(env_alloced, sizeof *t * (env_alloced_n + 1));
	if (!t) return;
	(env_alloced = t)[env_alloced_n++] = new;
}

static int __putenv(char *s, size_t l, char *r) {
	size_t i = 0;
	if (environ) {
		for (char **e = environ; *e; e++, i++)
			if (!strncmp(s, *e, l + 1)) {
				char *tmp = *e;
				*e = s;
				__env_rm_add(tmp, r);
				return 0;
			}
	}
	static char **oldenv;
	char **newenv;
	if (environ == oldenv) {
		newenv = realloc(oldenv, sizeof *newenv * (i + 2));
		if (!newenv) goto oom;
	} else {
		newenv = malloc(sizeof *newenv * (i + 2));
		if (!newenv) goto oom;
		if (i) memcpy(newenv, environ, sizeof *newenv * i);
		free(oldenv);
	}
	newenv[i] = s;
	newenv[i + 1] = 0;
	environ = oldenv = newenv;
	if (r) __env_rm_add(0, r);
	return 0;
oom:
	free(r);
	return -1;
}

char *getenv(const char *name) {
	size_t l = strchrnul(name, '=') - name;
	if (l && !name[l] && environ)
		for (char **e = environ; *e; e++)
			if (!strncmp(name, *e, l) && l[*e] == '=') return *e + l + 1;
	return 0;
}

int putenv(char *s) {
	size_t l = strchrnul(s, '=') - s;
	if (!l || !s[l]) return unsetenv(s);
	return __putenv(s, l, 0);
}

int setenv(const char *var, const char *value, int overwrite) {
	char *s;
	size_t l1, l2;

	if (!var || !(l1 = strchrnul(var, '=') - var) || var[l1]) {
		// TODO: errno
		// errno = EINVAL;
		return -1;
	}
	if (!overwrite && getenv(var)) return 0;

	l2 = strlen(value);
	s = malloc(l1 + l2 + 2);
	if (!s) return -1;
	memcpy(s, var, l1);
	s[l1] = '=';
	memcpy(s + l1 + 1, value, l2 + 1);
	return __putenv(s, l1, s);
}

int unsetenv(const char *name) {
	size_t l = strchrnul(name, '=') - name;
	if (!l || name[l]) {
		// TODO: errno
		// errno = EINVAL;
		return -1;
	}
	if (environ) {
		char **e = environ, **eo = e;
		for (; *e; e++)
			if (!strncmp(name, *e, l) && l[*e] == '=')
				__env_rm_add(*e, 0);
			else if (eo != e)
				*eo++ = *e;
			else
				eo++;
		if (eo != e) *eo = 0;
	}
	return 0;
}


void abort(void) {

	fprintf(stderr, "abort() called!\n");
	exit(EXIT_FAILURE);
}


// nonstandard
char *path_join(char *a, char *b) {
	int l = strlen(a) + 1;
	int k = strlen(b);

	char *dst = malloc(l + k + 1);
	char *z = a + l - 1;

	memcpy(dst, a, z - a);
	dst[z - a] = '/';
	memcpy(dst + (z - a) + (z > a), b, k + 1);

	return dst;
}


static uint64_t seed;

void srand(unsigned s)
{
	seed = s-1;
}

int rand(void)
{
	seed = 6364136223846793005ULL*seed + 1;
	return seed>>33;
}




#define C_RED 91
#define C_GREEN 92
#define C_YELLOW 93
#define C_BLUE 94
#define C_MAGENTA 95
#define C_CYAN 96

#define C_RESET 0
#define C_GRAY 90

static int current_color = 0;
static void set_color(int code) {
	if (code != current_color) {
		printf("\x1b[%dm", code);
		current_color = code;
	}
}

static void set_color_for(char c) {
	if (c >= 'A' && c <= 'z') {
		set_color(C_YELLOW);
	} else if (c >= '!' && c <= '~') {
		set_color(C_CYAN);
	} else if (c == '\n' || c == '\r') {
		set_color(C_GREEN);
	} else if (c == '\a' || c == '\b' || c == 0x1b || c == '\f' || c == '\n' ||
			c == '\r') {
		set_color(C_RED);
	} else if ((unsigned char)c == 0xFF) {
		set_color(C_MAGENTA);
	} else {
		set_color(C_GRAY);
	}
}


void debug_hexdump(void *vbuf, size_t len) {

	unsigned awidth = 4;

	if (len > 0xFFFFL) awidth = 8;

	unsigned char *buf = (unsigned char *)vbuf;
	int w = 16;

	for (unsigned long long i = 0; i < len; i += w) {
		unsigned char *line = buf + i;

		set_color(C_RESET);
		printf("|");
		set_color(C_GRAY);

		printf("%.*llx", awidth, i);

		set_color(C_RESET);
		printf("|");
		for (int c = 0; c < w; c++) {
			if (c % 8 == 0) {
				printf(" ");
			}
			if (i + c >= len) {
				printf("   ");
			} else {
				set_color_for(line[c]);
				printf("%02X ", line[c]);
			}
		}

		set_color(C_RESET);
		printf("|");
		for (int c = 0; c < w; c++) {
			if (c != 0 && (c % 8 == 0)) {
				set_color(C_RESET);
				printf(" ");
			}

			if (i + c >= len) {
				printf(" ");
			} else {
				set_color_for(line[c]);

				printf("%c", (line[c] < 0x20) || (line[c] > 0x7e) ? '.' : line[c]);
			}
		}
		set_color(C_RESET);
		printf("|\n");

	}
}
