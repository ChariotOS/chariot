#pragma once

#include <ck/io.h>
#include <ck/map.h>
#include <ck/ptr.h>
#include <gfx/scribe.h>
#include <stdint.h>

#define GFX_FNT_MAGIC 0x464f4e54  // 'FONT'

namespace gfx {


  struct char_map_ent {
    uint32_t codepoint;
    uint32_t data_offset;

    int8_t bbX;
    int8_t bbY;
    int8_t bbW;
    int8_t bbH;
		int8_t adv;
  } __attribute__((packed));


  struct font_header {
    uint32_t magic;

		uint32_t height; // offset from the "top left" to the baseline
		uint32_t width; // offset from the "top left" to the baseline

    // how many chars in the cmap?
    uint32_t charsz;  // size of the char map (bytes)
    uint32_t datasz;  // size of the data map (bytes)
    struct char_map_ent cmap[0];

    // (implicit) uint32_t data[0];
  };



  class font : public ck::refcounted<font> {
		protected:
    font(ck::file &, int lh);


    struct font_header *hdr;
    size_t datasz;
		int m_line_height;

    ck::map<uint32_t, struct char_map_ent *> cmap;


		uint32_t *data(uint32_t off = 0);
   public:

    ~font(void);
    static ck::ref<font> open(const char *name, int lh);
    static ck::ref<font> get_default(void);

		inline int line_height(void) {
			return m_line_height;
		}


		// total width of a codepoint from the left edge
		uint32_t width(uint32_t cp);

    int draw(int &x, int &y, gfx::scribe &, uint32_t cp, uint32_t color);
  };

}  // namespace gfx
