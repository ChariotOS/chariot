/* This file is very simple... */

#include <ck/command.h>
#include <ck/dir.h>
#include <ck/eventloop.h>
#include <gfx/font.h>
#include <unistd.h>
#include "internal.h"

void scan_fonts(void) {
  FILE *f = fopen("/sys/fonts/fonts.dir", "r");
  printf("%p\n", f);

  ck::directory font_dir;

  if (!font_dir.open("/sys/fonts/")) {
    printf("Failed to open /sys/fonts/\n");
  }

  auto files = font_dir.all_files();
  for (auto &file : files) {
    ck::string path = "/sys/fonts/";
    path += file;
    auto font = gfx::font::open_absolute(path.get());

    if (font) {
      auto &face = font->ft_face();
      printf("%30s: %s - %s\n", file.get(), face->family_name, face->style_name);
    }
  }
}

/**
 * the main function for the window server
 */
int main(int argc, char **argv) {
  // make an eventloop
  ck::eventloop loop;

  // scan_fonts();

  // construct the context
  lumen::context ctx;



  ck::command term("term");
  term.start();
  term.leak();

  // run the loop
  loop.start();

  return 0;
}
