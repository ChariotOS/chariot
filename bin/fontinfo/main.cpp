#include <ck/dir.h>
#include <gfx/font.h>

void scan_fonts(void) {
  ck::directory font_dir;

  if (!font_dir.open("/sys/fonts/")) {
    printf("Failed to open /sys/fonts/\n");
  }

  auto files = font_dir.all_files();
  for (auto &file : files) {
    ck::string path = "/sys/fonts/";
    path += file;
    auto font = gfx::font::open_absolute(path.get());

    auto &face = font->ft_face();
    printf("%30s: %s - %s\n", file.get(), face->family_name, face->style_name);
  }
}

int main() {
  scan_fonts();
  return 0;
}
