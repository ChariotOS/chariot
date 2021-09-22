#include <stdio.h>
#include <wren/wren.hpp>
#if 0

static void writefn(WrenVM*, const char* text) { fputs(text, stdout); }


static void reportError(WrenVM* vm, WrenErrorType type, const char* module, int line, const char* message) {
  switch (type) {
    case WREN_ERROR_COMPILE:
      fprintf(stderr, "[%s line %d] %s\n", module, line, message);
      break;

    case WREN_ERROR_RUNTIME:
      fprintf(stderr, "%s\n", message);
      break;

    case WREN_ERROR_STACK_TRACE:
      fprintf(stderr, "[%s line %d] in %s\n", module, line, message);
      break;
  }
}

static void* wren_allocate(void* memory, size_t newSize) {
  // printf("wren_allocate %p %d\n", memory, newSize);
  if (newSize == 0) {
    free(memory);
    return NULL;
  }

  if (memory == NULL) return malloc(newSize);
  return realloc(memory, newSize);
}


wren::vm::vm(void) {
  WrenConfiguration config;
  wrenInitConfiguration(&config);
  config.reallocateFn = wren_allocate;
  config.writeFn = writefn;
  config.errorFn = reportError;

  config.initialHeapSize = 1024;
  config.minHeapSize = 1024;
  m_vm = wrenNewVM(&config);
}

wren::vm::~vm(void) { wrenFreeVM(m_vm); }


static char* read_file(const char* path) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) return NULL;

  // Find out how big the file is.
  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  fseek(file, 0L, SEEK_SET);

  // Allocate a buffer for it.
  char* buffer = (char*)malloc(fileSize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }

  // Read the entire file.
  size_t bytesRead = fread(buffer, 1, fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }

  // Terminate the string.
  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

WrenInterpretResult wren::vm::run_file(const char* path) {
  char* code = read_file(path);
  if (code == NULL) return WREN_RESULT_COMPILE_ERROR;

  auto res = wrenInterpret(m_vm, "main", code);
  free(code);
  return res;
}


WrenInterpretResult wren::vm::run_expr(const char* code) {
  auto res = wrenInterpret(m_vm, "main", code);
  return res;
}
#endif