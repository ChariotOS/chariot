#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ck/string.h>
#include <ck/vec.h>
#include <ck/io.h>
#include <ck/defer.h>
#include <sys/mman.h>


int main(int argc, const char **argv) {
	if (argc != 2) {
		fprintf(stderr, "ded: the dumb editor.\n");
		fprintf(stderr, "     usage: ded <file>\n");
		exit(EXIT_FAILURE);
	}

	// open the file!
	const char *file = argv[1];
	auto stream = ck::file(file, "rw");

	if (!stream) {
		fprintf(stderr, "Failed to open '%s'\n", file);
		exit(EXIT_FAILURE);
	}


	// map the file into memory and parse it for files
	size_t size = stream.size();
	auto buf = (char*)mmap(NULL, size, PROT_READ, MAP_PRIVATE, stream.fileno(), 0);

	ck::vec<ck::string> lines;
	{
		ck::string current;
		for (size_t i = 0; i < size; i++) {
			if (buf[i] == '\n') {
				lines.push(current);
				current.clear();
			} else {
				current.push(buf[i]);
			}
		}
		if (current.len() != 0) lines.push(current);
	}

	// unmap the buffer we used to read the file
	munmap(buf, size);

	int selected_line = 0;

	bool done = false;

	bool dirty = false;
	while (!done) {
		// read a single char from the user
		if (dirty) printf("dirty ");
		printf("%% ");
		fflush(stdout);
		char c = getchar();
		printf("%c\n", c);

		switch (c) {
			// replace line
			case 'r':
				{
					ck::string new_line;

					printf("old: %s\n", lines[selected_line].get());
					printf("new: ");
					while (1) {
						char input = getchar();
						printf("%c", input);
						fflush(stdout);
						if (input == '\n') break;
						if (input == 0x7f) {
							new_line.pop();
							printf(" \x7f");
							fflush(stdout);
						} else {
							new_line.push(input);
						}
					}
					printf("line replaced. %d (%s)\n", selected_line, new_line.get());
					lines[selected_line] = new_line.get();
					dirty = true;
				}
				break;


			case 'l':
				for (int i = 0; i < lines.size(); i++) {
					printf("%c%2d: %s\n", selected_line == i ? '>' : ' ', i + 1, lines[i].get());
				}
				break;

			// next line
			case 'j':
				if (selected_line == lines.size() - 1) {
					printf("ERR\n");
				} else {
					selected_line += 1;
				}
				printf(">%2d: %s\n", selected_line + 1, lines[selected_line].get());
				break;

			// previous line
			case 'k':
				if (selected_line == 0) {
					printf("ERR\n");
				} else {
					selected_line -= 1;
				}
				printf(">%2d: %s\n", selected_line + 1, lines[selected_line].get());
				break;

			// append a line below
			case 'a':
				selected_line += 1;
				lines.insert(selected_line, "");
				break;

			// delete the current line
			case 'd':
				if (lines.size() > 1) {
					lines.remove(selected_line);
					selected_line -= 1;
					if (selected_line < 0) selected_line = 0;
				}
				break;




			// write the file
			case 'w':
				{
					int written = 0;
					stream.seek(0, SEEK_SET);
					for (auto &line : lines) {
						written += stream.writef("%s\n", line.get());
					}
					printf("%d written\n", written);
					dirty = false;
				}
				break;


			case '?':
				printf("binds:\n");
				printf(" r: replace line\n");
				printf(" l: display all lines\n");
				printf(" j: next line\n");
				printf(" k: previous line\n");
				printf(" a: append line below\n");
				printf(" d: delete current line\n");
				printf(" w: write the file\n");
				printf(" q: quit\n");
				break;

			// exit!
			case 'q':
				if (dirty) {
					while (1) {
						printf("Exit without saving? (y/n): ");
						fflush(stdout);

						char answer = getchar();
						printf("\n");
						if (answer == 'y') {
							done = true;
							break;
						} else if (answer == 'n') {
							done = false;
							break;
						}
						printf("Unknown answer '%c'\n", answer);

					}

				} else {
					done = true;
				}
				break;

			default:
				printf("?\n");
				break;
		}
	}


	return 0;
}
