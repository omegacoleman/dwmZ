#include <atomic>
#include <chrono>
#include <thread>

#include "ytk/event/event_types.hpp"
#include "ytk/event/registry.hpp"
#include "ytk/x/x.hpp"

#include "bar/gc/gc.hpp"
#include "bar/layout/wmbar.hpp"
#include "bar/layout/zone.hpp"

#include "agg_basics.h"
#include "agg_pixfmt_rgb.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_renderer_scanline.h"
#include "agg_rendering_buffer.h"
#include "agg_scanline_p.h"
#include "agg_scanline_u.h"

bar::wmbar_layout_t layo;
bar::zone_attr_t attr;
ytk::x::win_t win;
std::atomic<bool> exiting = ATOMIC_VAR_INIT(false);

void request_exit(const ytk::event::app_exit_t &) { exiting = true; }

void redraw() {
  win.redraw([](auto &pix) {
    auto gc = bar::agg_gc_t{ pix };

    gc.draw_text_panel("dwmZ", layo.logo_, attr, bar::panel_flavor_t::logo);
    gc.draw_text_panel("23:59", layo.time_, attr, bar::panel_flavor_t::datetime);
    gc.draw_text_panel("EN", layo.im_, attr, bar::panel_flavor_t::im);
    gc.draw_volbar_panel(0.7, layo.volume_, attr, bar::panel_flavor_t::volume);
    auto z_tags = bar::xsplit(layo.tags_, 9);
    for(int i = 0; i < 9; i++) {
      if(i == 3) {
        gc.draw_text_panel(fmt::format("{}", i), z_tags[i], attr, bar::panel_flavor_t::tagsel_active);
      } else {
        gc.draw_text_panel(fmt::format("{}", i), z_tags[i], attr, bar::panel_flavor_t::tagsel);
      }
    }

    auto z_wins = bar::xsplit(layo.wins_, 4);
    for(int i = 0; i < 4; i++) {
      if(i == 2) {
        gc.draw_text_panel("新窗口win", z_wins[i], attr, bar::panel_flavor_t::winsel_active);
      } else {
        gc.draw_text_panel("xterm", z_wins[i], attr, bar::panel_flavor_t::winsel);
      }
    }
  });
}

int main(void) {
  win.init(1024, 40);
  layo = bar::layout_wmbar(1024, 40, 9, bar::layout_flags::has_im | bar::layout_flags::has_volume);
  attr.bl_ratio = 0.7;
  attr.txth_ratio = 0.6;
  attr.margin_ratio = 0.15;
  attr.volbar_ratio = 0.2;

  ytk::event::set_handler<ytk::event::app_exit_t>([](const auto &) { exiting = true; });
  ytk::event::set_handler<ytk::event::expose_t>([](const auto &) { redraw(); });
  ytk::event::set_handler<ytk::event::win_resize_t>([](const ytk::event::win_resize_t &rsz) {
    layo = bar::layout_wmbar(rsz.width, std::min<double>(100, rsz.height), 9, bar::layout_flags::has_im | bar::layout_flags::has_volume);
    redraw();
  });

  while(!exiting) {
    win.process_event(true);
  }
  return 0;
}
