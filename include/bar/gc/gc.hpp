#pragma once

#include "agg_basics.h"
#include "agg_conv_contour.h"
#include "agg_conv_curve.h"
#include "agg_font_cache_manager.h"
#include "agg_font_freetype.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_renderer_scanline.h"
#include "agg_rendering_buffer.h"
#include "agg_scanline_u.h"

#include "agg_gradient_lut.h"
#include "agg_span_allocator.h"
#include "agg_span_gradient.h"
#include "agg_span_interpolator_linear.h"
#include "agg_span_interpolator_trans.h"
#include "agg_trans_perspective.h"

#include "bar/gc/colorscheme.hpp"
#include "bar/layout/zone.hpp"
#include "utf8proc.h"
#include "ytk/misc/common.hpp"

#include "fontconfig/fontconfig.h"

#include "bar/gc/colors/kanagawa.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <sstream>

namespace bar {

enum class panel_flavor_t { none, datetime, logo, tagsel, tagsel_active, winsel, winsel_active, volume, im, tagsel_occ, tagsel_urg, winsel_hidden, ltbutton };

enum class ltbutton_icon_t { floating, monocle, tiled };

using font_token_t = uint32_t;

struct font_matcher_t {
  font_matcher_t() { conf_ = FcInitLoadConfigAndFonts(); }

  font_token_t get_or_create_token(const std::string &path) {
    auto it = file_to_token_.find(path);
    if(it == file_to_token_.end()) {
      font_token_t tok = next_token_++;
      file_to_token_.emplace(path, tok);
      token_to_file_.emplace(tok, path);
      return tok;
    }
    return it->second;
  }

  const std::string &get_path(font_token_t tok) const { return token_to_file_.at(tok); }

  std::vector<font_token_t> match_fonts(const std::string &sel) {
    std::vector<font_token_t> tokens;

    FcResult result;
    FcPattern *pat = FcNameParse(reinterpret_cast<const FcChar8 *>(sel.c_str()));
    if(!pat)
      ytk::raise("FcNameParse(\"{}\") failed", sel);

    FcConfigSubstitute(conf_, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcFontSet *fs = FcFontSort(conf_, pat, FcTrue, 0, &result);
    if(fs) {
      for(unsigned i = 0; i < fs->nfont; i++) {
        FcValue v;
        FcPatternGet(fs->fonts[i], FC_FILE, 0, &v);
        std::string path{ reinterpret_cast<const char *>(v.u.f) };
        tokens.push_back(get_or_create_token(path));
      }
      FcFontSetSortDestroy(fs);
    }
    FcPatternDestroy(pat);
    return tokens;
  }

  FcConfig *conf_ = nullptr;
  font_token_t next_token_ = 1;
  std::map<std::string, font_token_t> file_to_token_;
  std::map<font_token_t, std::string> token_to_file_;
};

inline font_matcher_t &fmatcher() {
  static font_matcher_t m;
  return m;
}

struct agg_font_entry_t {
  typedef agg::font_engine_freetype_int32 font_engine_type;
  typedef agg::font_cache_manager<font_engine_type> font_manager_type;

  explicit agg_font_entry_t(const std::string &path) {
    if(!feng_.load_font(path.c_str(), 0, agg::glyph_ren_outline)) {
      ytk::raise("font file not found : {}", path);
    }
    feng_.height(50);
    curves_.approximation_scale(2.0);
    contour_.auto_detect_orientation(false);
    contour_.width(0.5);
    feng_.flip_y(true);
  }

  void height(unsigned px) { feng_.height(px); }

  const auto *glyph(uint_least32_t code) { return curr_glyph_ = fman_.glyph(code); }

  auto &vertex_source(double x, double y) {
    fman_.init_embedded_adaptors(curr_glyph_, x, y);
    return contour_;
  }

  font_engine_type feng_;
  font_manager_type fman_{ feng_ };
  const agg::glyph_cache *curr_glyph_ = nullptr;
  agg::conv_curve<font_manager_type::path_adaptor_type> curves_{ fman_.path_adaptor() };
  agg::conv_contour<agg::conv_curve<font_manager_type::path_adaptor_type>> contour_{ curves_ };
};

struct agg_font_context_t {
  void select(const std::string &sel) { curr_sel_ = fmatcher().match_fonts(sel); }

  void height(unsigned px) { curr_height_ = px; }

  agg_font_entry_t &get_or_create_entry(font_token_t tok) {
    auto it = entries_.find(tok);
    if(it == entries_.end()) {
      it = entries_.emplace(tok, std::make_unique<agg_font_entry_t>(fmatcher().get_path(tok))).first;
    }
    return *it->second;
  }

  const agg::glyph_cache *glyph(uint_least32_t code) {
    for(auto it : curr_sel_) {
      auto &ent = get_or_create_entry(it);
      auto *glyph_ptr = ent.glyph(code);
      if(glyph_ptr) {
        ent.height(curr_height_);
        glyph_ptr = ent.glyph(code);
        curr_ent_ = &ent;
        return glyph_ptr;
      }
    }
    if(code == 0)
      return nullptr;
    return glyph(0);
  }

  auto &vertex_source(double x, double y) { return curr_ent_->vertex_source(x, y); }

  std::vector<font_token_t> curr_sel_;
  agg_font_entry_t *curr_ent_ = nullptr;
  unsigned curr_height_ = 50;
  std::map<font_token_t, std::unique_ptr<agg_font_entry_t>> entries_;
};

inline agg_font_context_t &fctx() {
  static agg_font_context_t ctx;
  return ctx;
}

template <class PixFmt> struct agg_gc_t {
  explicit agg_gc_t(PixFmt &pixfmt) : pixfmt_(&pixfmt), ren_(pixfmt), ren_solid_(ren_) {}

  static auto conv(rgb_literal_t lit) { return as_color<agg::rgba8>(lit); }

  static auto conv(rgb_literal_t lit, uint8_t a) { return as_color<agg::rgba8>(lit, a); }

  static auto panel_bg_color(panel_flavor_t flavor) {
    switch(flavor) {
    case panel_flavor_t::datetime:
      return conv(colors::kanagawa::waveBlue1);
    case panel_flavor_t::logo:
      return conv(colors::kanagawa::sumiInk1);
    case panel_flavor_t::tagsel:
      return conv(colors::kanagawa::waveBlue1, 204);
    case panel_flavor_t::tagsel_active:
      return conv(colors::kanagawa::waveBlue2);
    case panel_flavor_t::ltbutton:
      return conv(colors::kanagawa::winterBlue);
    case panel_flavor_t::winsel_active:
      return conv(colors::kanagawa::fujiWhite);
    case panel_flavor_t::im:
      return conv(colors::kanagawa::winterBlue);
    case panel_flavor_t::volume:
      return conv(colors::kanagawa::sumiInk3);
    case panel_flavor_t::none:   // fallthrough
    case panel_flavor_t::winsel: // fallthrough
    default:
      return conv(colors::kanagawa::sumiInk3, 204);
    }
  }

  static auto panel_pin_color(panel_flavor_t flavor) {
    switch(flavor) {
    case panel_flavor_t::tagsel_urg:
      return conv(colors::kanagawa::peachRed);
    case panel_flavor_t::tagsel_occ:
      return conv(colors::kanagawa::fujiWhite);
    case panel_flavor_t::winsel_hidden:
      return conv(colors::kanagawa::katanaGray);
    default:
      return conv(colors::kanagawa::winterBlue);
    }
  }

  static auto volbar_plate_color(panel_flavor_t) { return conv(colors::kanagawa::fujiGray); }

  static auto panel_fg_color(panel_flavor_t flavor) {
    switch(flavor) {
    case panel_flavor_t::datetime:
      return conv(colors::kanagawa::springBlue);
    case panel_flavor_t::logo:
      return conv(colors::kanagawa::springViolet1);
    case panel_flavor_t::winsel_active:
      return conv(colors::kanagawa::waveBlue2);
    case panel_flavor_t::tagsel:
      return conv(colors::kanagawa::surimiOrange);
    case panel_flavor_t::tagsel_active:
      return conv(colors::kanagawa::surimiOrange);
    case panel_flavor_t::ltbutton: // fallthrough
    case panel_flavor_t::none:     // fallthrough
    default:
      return conv(colors::kanagawa::fujiWhite);
    }
  }

  static std::string panel_font_selector(panel_flavor_t flavor) {
    // return "deja vu sans mono";
    return "monospace";
  }

  void draw_volbar(double rate, double x, double y, double w, double h, panel_flavor_t flavor = panel_flavor_t::none) {
    ras_.reset();
    ras_.move_to_d(x, y);
    ras_.line_to_d(x + w, y);
    ras_.line_to_d(x + w, y + h);
    ras_.line_to_d(x, y + h);
    agg::render_scanlines_aa_solid(ras_, sl_, ren_, volbar_plate_color(flavor));

    ras_.reset();
    ras_.move_to_d(x, y);
    ras_.line_to_d(x + (w * rate), y);
    ras_.line_to_d(x + (w * rate), y + h);
    ras_.line_to_d(x, y + h);
    agg::render_scanlines_aa_solid(ras_, sl_, ren_, panel_fg_color(flavor));
  }

  void draw_ltbutton_icon(ltbutton_icon_t icon, double x, double y, double w, double h, panel_flavor_t flavor = panel_flavor_t::none) {
    ras_.reset();
    switch(icon) {
    case ltbutton_icon_t::monocle:
      ras_.move_to_d(x, y);
      ras_.line_to_d(x + w, y);
      ras_.line_to_d(x + w, y + h);
      ras_.line_to_d(x, y + h);
      ras_.close_polygon();
      break;
    case ltbutton_icon_t::floating: {
      double x1 = x + (w / 3);
      double x2 = x + (w / 3) * 2;
      double x3 = x + w;
      double y1 = y + (h / 3);
      double y2 = y + (h / 3) * 2;
      double y3 = y + h;
      ras_.move_to_d(x, y1);
      ras_.line_to_d(x, y3);
      ras_.line_to_d(x2, y3);
      ras_.line_to_d(x2, y2);
      ras_.line_to_d(x1, y2);
      ras_.line_to_d(x1, y1);
      ras_.close_polygon();
      ras_.move_to_d(x1, y1);
      ras_.line_to_d(x2, y1);
      ras_.line_to_d(x2, y2);
      ras_.line_to_d(x3, y2);
      ras_.line_to_d(x3, y);
      ras_.line_to_d(x1, y);
      ras_.close_polygon();
    } break;
    default: {
      double x1 = x + w * 0.45;
      double x2 = x + w * 0.55;
      double x3 = x + w;
      double y1 = y + h * 0.45;
      double y2 = y + h * 0.55;
      double y3 = y + h;
      ras_.move_to_d(x, y);
      ras_.line_to_d(x, y3);
      ras_.line_to_d(x1, y3);
      ras_.line_to_d(x1, y);
      ras_.close_polygon();
      ras_.move_to_d(x2, y);
      ras_.line_to_d(x2, y1);
      ras_.line_to_d(x3, y1);
      ras_.line_to_d(x3, y);
      ras_.close_polygon();
      ras_.move_to_d(x2, y2);
      ras_.line_to_d(x2, y3);
      ras_.line_to_d(x3, y3);
      ras_.line_to_d(x3, y2);
      ras_.close_polygon();
    } break;
    }
    agg::render_scanlines_aa_solid(ras_, sl_, ren_, panel_fg_color(flavor));
  }

  void draw_panel_bg(double x, double y, double w, double h, panel_flavor_t flavor = panel_flavor_t::none) {
    ras_.reset();
    ras_.move_to_d(x, y);
    ras_.line_to_d(x + w, y);
    ras_.line_to_d(x + w, y + h);
    ras_.line_to_d(x, y + h);
    agg::render_scanlines_aa_solid(ras_, sl_, ren_, panel_bg_color(flavor));
  }

  void draw_panel_pin(double x, double y, double a, panel_flavor_t flavor = panel_flavor_t::none) {
    ras_.reset();
    ras_.move_to_d(x, y);
    ras_.line_to_d(x + a, y);
    ras_.line_to_d(x, y + a);
    agg::render_scanlines_aa_solid(ras_, sl_, ren_, panel_pin_color(flavor));
  }

  void draw_text(std::string_view txt, double x, double bl_y, double w, double h, panel_flavor_t flavor = panel_flavor_t::none) {
    std::vector<uint32_t> codepoints;
    while(txt.size()) {
      int32_t codepoint = 0;
      ssize_t sz = utf8proc_iterate(reinterpret_cast<const uint8_t *>(txt.data()), txt.size(), &codepoint);
      if(sz < 0) {
        // attempt to skip over garbage and resume
        txt.remove_prefix(1);
        codepoint = 0; // draw a tofu
      }
      txt.remove_prefix(sz);
      codepoints.emplace_back(codepoint);
    }

    fctx().select(panel_font_selector(flavor));
    fctx().height(h);
    double x_max = x + w;

    double txt_w = 0;
    for(const uint32_t &codepoint : codepoints) {
      const agg::glyph_cache *glyph = fctx().glyph(codepoint);
      if(!glyph)
        ytk::raise("fonts installed too broken to even draw a tofu");
      txt_w += glyph->advance_x;
    }

    bool fade_out = false;
    if(txt_w <= w) {
      x += (w - txt_w) / 2;
    } else {
      fade_out = true;
    }

    using color_type = typename PixFmt::color_type;
    using span_interpolator_type = agg::span_interpolator_linear<>;
    using color_func_type = agg::gradient_lut<agg::color_interpolator<color_type>, 256>;
    using gradient_type = agg::gradient_x;

    agg::trans_affine trans;
    trans.translate(x, 0);
    trans.invert();

    gradient_type gr;
    agg::span_allocator<color_type> span_alloc;
    span_interpolator_type inter{ trans };
    color_func_type colors;

    auto color = panel_fg_color(flavor);
    auto color_fade = color;
    color_fade.a = 0;
    colors.add_color(0.0, color);
    colors.add_color(0.8, color);
    colors.add_color(1.0, color_fade);
    colors.build_lut();
    agg::span_gradient<color_type, span_interpolator_type, gradient_type, color_func_type> span_gen{ inter, gr, colors, 0, w };

    if(!fade_out) {
      ren_solid_.color(color);
    }

    for(auto codepoint : codepoints) {
    __reenter:
      const agg::glyph_cache *glyph = fctx().glyph(codepoint);

      switch(glyph->data_type) {
      case agg::glyph_data_outline:
        ras_.reset();
        ras_.add_path(fctx().vertex_source(x, bl_y));
        if(fade_out) {
          if(x > x_max)
            return;
          agg::render_scanlines_aa(ras_, sl_, ren_, span_alloc, span_gen);
        } else {
          agg::render_scanlines(ras_, sl_, ren_solid_);
        }
        break;
      default:
        if(codepoint != 0) {
          codepoint = 0;
          goto __reenter;
        }
        ytk::raise("fonts installed too broken to even draw a tofu");
      }

    __move_on:
      x += glyph->advance_x;
    }
  }

  void draw_text(const std::string &txt, const zone_t &z, const zone_attr_t &attr, panel_flavor_t flavor = panel_flavor_t::none) {
    double margin = attr.margin_ratio * z.h;
    draw_text(txt, z.x + margin, z.y + z.h * attr.bl_ratio, z.w - margin * 2, z.h * attr.txth_ratio, flavor);
  }

  void draw_volbar(double rate, const zone_t &z, const zone_attr_t &attr, panel_flavor_t flavor = panel_flavor_t::none) {
    double margin = attr.margin_ratio * z.h;
    draw_volbar(rate, z.x + margin, z.y + z.h * (1.0 - attr.volbar_ratio) / 2, z.w - margin * 2, z.h * attr.volbar_ratio, flavor);
  }

  void draw_ltbutton_icon(ltbutton_icon_t icon, const zone_t &z, const zone_attr_t &attr, panel_flavor_t flavor = panel_flavor_t::none) {
    double margin = attr.margin_ratio * z.h;
    draw_ltbutton_icon(icon, z.x + margin, z.y + margin, z.w - margin * 2, z.h - (2 * margin));
  }

  void draw_panel_bg(const zone_t &z, panel_flavor_t flavor = panel_flavor_t::none) { draw_panel_bg(z.x, z.y, z.w, z.h, flavor); }

  void draw_text_panel(const std::string &txt, const zone_t &z, const zone_attr_t &attr, panel_flavor_t flavor = panel_flavor_t::none) {
    draw_panel_bg(z, flavor);
    draw_text(txt, z, attr, flavor);
  }

  void draw_volbar_panel(double rate, const zone_t &z, const zone_attr_t &attr, panel_flavor_t flavor = panel_flavor_t::none) {
    draw_panel_bg(z, flavor);
    draw_volbar(rate, z, attr, flavor);
  }

  void draw_panel_pin(const zone_t &z, panel_flavor_t flavor = panel_flavor_t::none) { draw_panel_pin(z.x, z.y, z.h * 0.35, flavor); }

  PixFmt *pixfmt_;
  agg::renderer_base<PixFmt> ren_;
  agg::renderer_scanline_aa_solid<agg::renderer_base<PixFmt>> ren_solid_;
  agg::rasterizer_scanline_aa<> ras_;
  agg::scanline_u8 sl_;
};

}
