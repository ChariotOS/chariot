#pragma once

// this was missing, so when clangformat had it's fun, the headers rearanged.
// Just declaring it here to save a headache
struct java_class_t;
struct java_class;

int hb_read_source_file(java_class_t* cls);
struct java_class* hb_load_class(const char* path);
