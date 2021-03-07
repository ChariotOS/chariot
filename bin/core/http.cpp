#include <ck/string.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysbind.h>
#include <unistd.h>

enum http_method { GET };



int main(int argc, char **argv) {
  http_method method = GET;
  int ch;
  const char *flags = "";
  while ((ch = getopt(argc, argv, flags)) != -1) {
    switch (ch) {
      case '?':
        puts("http: invalid option\n");
        printf("    usage: %s [%s] [URL]\n", argv[0], flags);
        return -1;
    }
  }


  argc -= optind;
  argv += optind;


  if (argc != 1) {
    puts("http: invalid option\n");
    printf("    usage: %s [%s] [URL]\n", argv[0], flags);
  }


  char *save;

  char *domain = strtok_r(argv[0], "/", &save);
  domain = strdup(domain);


  printf("domain: %s\n", domain);

  printf("argv: %s\n", argv[0]);
  char *end = argv[0] + strlen(domain);
  end[0] = '/';
  printf("end: %s\n", end);

  free(domain);

  return EXIT_SUCCESS;
}
