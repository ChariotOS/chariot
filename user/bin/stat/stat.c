#include <chariot.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

void do_stat(char *path) {
	struct stat st;

	if (lstat(path, &st) != 0) {
		fprintf(stderr, "stat: failed to open file '%s'\n", path);
		exit(EXIT_FAILURE);
	}

#define F(name, thing, fmt) printf(name ": %-10" fmt, thing)
	F("- File", path, "s");
	printf("\n");

	F("  Size", st.st_size, "d");
	F("Blocks", st.st_blocks, "d");
	F(" Links", st.st_nlink, "ld");
	printf("\n");

	char mode[12];

	int m = st.st_mode;
	snprintf(mode, 12, "%c%c%c%c%c%c%c%c%c", m & S_IRUSR ? 'r' : '-',
			m & S_IWUSR ? 'w' : '-', m & S_IXUSR ? 'x' : '-',

			m & S_IRGRP ? 'r' : '-', m & S_IWGRP ? 'w' : '-',
			m & S_IXGRP ? 'x' : '-',

			m & S_IROTH ? 'r' : '-', m & S_IWOTH ? 'w' : '-',
			m & S_IXOTH ? 'x' : '-');
	F("  Mode", mode, "s");
	F("   UID", st.st_uid, "ld");
	F("   GID", st.st_gid, "ld");
	printf("\n");
}


extern char **environ;

int main(int argc, char **argv) {
	char buf[50];

	char *args[] = {
		"echo",
		buf,
		NULL
	};

	for (int i = 0; i < 100; i++) {

		sprintf(buf, "%d", i);

		pid_t pid = spawn();
		if (pid <= -1) {
			perror("spawn");
			exit(0);
		}

		int start_res = startpidvpe(pid, args[0], args, environ);
		if (start_res == 0) {
			int stat = 0;
			waitpid(pid, &stat, 0);

			int exit_code = WEXITSTATUS(stat);
			if (exit_code != 0) {
				fprintf(stderr, "%s: exited with code %d\n", args[0], exit_code);
			}
		} else {
			printf("failed to execute: '%s'\n", args[0]);
			despawn(pid);
		}
		sprintf(buf, "echo %d", i);
		system(buf);
	}
	return 0;

	argc -= 1;
	argv += 1;

	if (argc == 0) {
		fprintf(stderr, "stat: provided no paths\n");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < argc; i++) {
		do_stat(argv[i]);
	}

	return 0;
}
