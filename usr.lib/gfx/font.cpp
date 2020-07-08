#include <errno.h>
#include <gfx/font.h>
#include <sys/mman.h>

namespace gfx {

  font::font(ck::file &file, int lh) : m_line_height(lh) {
    file.seek(0, SEEK_SET);


    datasz = file.size();

    hdr = (struct font_header *)mmap(NULL, datasz, PROT_READ, MAP_PRIVATE,
                                     file.fileno(), 0);


    auto count = hdr->charsz / sizeof(char_map_ent);

    // parse!
    for (int i = 0; i < (int)count; i++) {
      auto *cp = &hdr->cmap[i];
      cmap[cp->codepoint] = cp;
    }
  }

  font::~font(void) { munmap(hdr, datasz); }



  // return the data at an offset
  uint32_t *font::data(uint32_t off) {
    auto *base = ((char *)hdr->cmap + hdr->charsz);
    return (uint32_t *)(base + off);
  }


  ck::ref<gfx::font> font::get_default(void) {
    static ck::ref<gfx::font> font;
    if (!font) font = gfx::font::open("terv-normal", 12);
    return font;
  }


  ck::ref<font> font::open(const char *name, int lh) {
    char path[100];
    snprintf(path, 100, "/usr/res/fonts/%s-%d.fnt", name, lh);

    ck::file f;
    if (!f.open(path, "r")) {
      return nullptr;
    }


    uint32_t m = 0;
    f.read(&m, sizeof(m));
    if (m != GFX_FNT_MAGIC) {
      return nullptr;
    }

    f.seek(0, SEEK_SET);


    return new font(f, lh);
  }

  uint32_t font::width(uint32_t cp) {
    auto *ent = cmap[cp];
    if (ent == NULL) return 0;
    // total width
    return ent->bbH + ent->bbX;
  }



  int font::draw(int &dx, int &dy, gfx::scribe &s, uint32_t cp,
                 uint32_t color) {
    auto *ent = cmap[cp];
    if (ent == NULL) return -ENOENT;


    uint32_t *ch = data(ent->data_offset);
    auto w = ent->bbW;
    auto h = ent->bbH;

    int x = dx + ent->bbX;
    int y = dy - ent->bbY;

    // s.draw_hline(dx, dy, ent->bbW, 0xFF00FF);
    // s.draw_hline(x, y, ent->bbW, 0x338888);

    for (int r = 0; r < h; r++) {
      for (int c = 0; c <= w; c++) {
        int b = 0;
        b = (ch[r] & (1 << (w - c)));
        if (b) s.draw_pixel(x + c, y - r, color);
      }
    }


    dx += ent->adv;

    return 0;
  }


};  // namespace gfx
