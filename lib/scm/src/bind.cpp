#include <scm/scm.h>
#include <stdio.h>
#include "scheme.h"
#include "scheme-private.h"

#define init_file "/usr/src/scm/init.scm"


pointer my_func(struct scheme *sc, pointer args) {

	printf("hello from my func\n");
	return sc->NIL;
}

scm::Scheme::Scheme(void) {
  // allocate and initialize a tinyscheme env
  sc = scheme_init_new();

  scheme_set_input_port_file(sc, stdin);
  scheme_set_output_port_file(sc, stdout);

	scheme_define(sc, sc->global_env, mk_symbol(sc, "my-func"), mk_foreign_func(sc, my_func));
  load(init_file, "*init*");
}


scm::Scheme::~Scheme(void) {
  // let scheme clean itself up
  scheme_deinit(sc);
}


void scm::Scheme::eval(const char *expr) { scheme_load_string(sc, expr); }


void scm::Scheme::load(const char *path, const char *name) {
	if (name == nullptr) name = path;
  printf("load %s from '%s'\n", name, path);
  FILE *fin = fopen(path, "r");
  if (fin == NULL) {
		fprintf(stderr, "scheme: could not open '%s' for loading\n", path);
		return;
	}

  scheme_load_named_file(sc, fin, path);

  fclose(fin);
}
