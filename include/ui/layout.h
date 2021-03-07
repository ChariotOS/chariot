#pragma once

#include <ck/io.h>
#include <ck/map.h>
#include <ck/ptr.h>
#include <ck/vec.h>
#include <ui/attr.h>

/**
 * The UI system can use HTML files to define view layouts. The style of HTML
 * that it uses is a pretty barebones version, but it works :)
 */

#define LAYOUT_INT_ERR 0xFADEFADE

namespace ui {

  namespace layout {

    class parser;

    struct node {
      ck::string name;
      ck::vec<ck::unique_ptr<ui::layout::node>> children;


      long attr_num(ui::attr);
      ck::string attr(ui::attr);

      void set_attr(ui::attr a, ck::string val);

     private:
      friend class ui::layout::parser;
      ck::map<ui::attr, ck::string> m_attrs;
    };

    class parser {
      const char *buf;
      size_t len;

     public:
      parser(const char *buf, size_t len);
    };

  }  // namespace layout
}  // namespace ui
