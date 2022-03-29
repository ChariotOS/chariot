#include <wasm.h>
#include "../../lib/wasm/src/m3_config.h"
#include "../../lib/wasm/src/m3_api_libc.h"
#include "../../lib/wasm/src/m3_api_wasi.h"
#include <stdio.h>
#include <sys/sysbind.h>
#include <ck/io.h>


#define FATAL(msg, ...)                        \
  {                                            \
    printf("Fatal: " msg "\n", ##__VA_ARGS__); \
    exit(EXIT_FAILURE);                        \
  }

int main(int argc, char **argv) {
  const char *target = getenv("CHARIOT_CHAINLOAD_TARGET");
  if (target == NULL) {
    if (argc != 2) {
      fprintf(stderr, "usage: wasm <program>\n");
      exit(EXIT_FAILURE);
    }
    target = argv[1];
  }

  M3Result result = m3Err_none;

  ck::file file;
  if (!file.open(target, "r")) {
    FATAL("Failed to open file %s\n", argv[1]);
  }

  auto mapping = file.mmap();

  uint8_t *wasm = (uint8_t *)mapping->data();
  uint32_t fsize = mapping->size();
  // printf("wasm: %p, sz: %zu\n", wasm, fsize);

  IM3Environment env = m3_NewEnvironment();
  if (!env) FATAL("m3_NewEnvironment failed");

  IM3Runtime runtime = m3_NewRuntime(env, 8192, NULL);
  if (!runtime) FATAL("m3_NewRuntime failed");

  IM3Module module;
  result = m3_ParseModule(env, &module, wasm, fsize);
  if (result) FATAL("m3_ParseModule: %s", result);

  result = m3_LoadModule(runtime, module);
  if (result) FATAL("m3_LoadModule: %s", result);

  result = m3_LinkWASI(module);
  if (result) FATAL("m3_LinkWASI: %s", result);

  result = m3_LinkLibC(module);
  if (result) FATAL("m3_LinkLibC: %s", result);

  auto call_func = [&](const char *name) -> bool {
    IM3Function f;
    result = m3_FindFunction(&f, runtime, name);
    if (result) FATAL("m3_FindFunction: %s", result);
    if (f == NULL) {
      // printf("WARN: symbol '%s' not defined\n", name);
      return false;
    }

    result = m3_CallV(f);
    if (result) {
      FATAL("m3_Call: %s", result);
      return false;
    }
    return true;
  };

  // initialize the `wasi-exec-model=reactor` stuff
  if (call_func("_initialize")) {
  	call_func("main");
		return 0;
	} else if (call_func("_start")) {
		return 0;
	} else {
		fprintf(stderr, "interface not understood.\n");
		return EXIT_FAILURE;
	}

  return 0;
}
