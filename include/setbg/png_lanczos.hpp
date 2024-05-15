#pragma once

#include <X11/Xlib.h>

#include "agg_image_accessors.h"
#include "agg_image_filters.h"
#include "agg_pixfmt_rgba.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_renderer_scanline.h"
#include "agg_rendering_buffer.h"
#include "agg_scanline_p.h"
#include "agg_span_allocator.h"
#include "agg_span_image_filter_rgba.h"
#include "agg_span_interpolator_linear.h"
#include "agg_trans_affine.h"

#include "png++/png.hpp"
#include "png++/rgba_pixel.hpp"
#include "png++/solid_pixel_buffer.hpp"

namespace setbg::png_lanczos {

template <class SrcPixelFormat, class DstPixelFormat> void fit(const SrcPixelFormat &src, DstPixelFormat &dst) {
  unsigned sw = src.width();
  unsigned sh = src.height();
  unsigned dw = dst.width();
  unsigned dh = dst.height();

  double s_aspect = static_cast<double>(sw) / static_cast<double>(sh);
  double d_aspect = static_cast<double>(dw) / static_cast<double>(dh);

  agg::trans_affine mat;
  if(s_aspect < d_aspect) {
    mat.scale(static_cast<double>(dw + 4) / static_cast<double>(sw));
    mat.translate(-2, (static_cast<double>(dh) - static_cast<double>(dw) / s_aspect) / 2);
  } else {
    mat.scale(static_cast<double>(dh + 4) / static_cast<double>(sh));
    mat.translate((static_cast<double>(dw) - static_cast<double>(dh) * s_aspect) / 2, -2);
  }
  mat.invert();

  typedef agg::span_interpolator_linear<agg::trans_affine> InterpolatorType;
  typedef agg::image_accessor_clip<SrcPixelFormat> ImageAccessorType;
  typedef agg::span_image_filter_rgba<ImageAccessorType, InterpolatorType> SpanGeneratorType;
  typedef agg::span_allocator<typename DstPixelFormat::color_type> SpanAllocatorType;
  agg::rasterizer_scanline_aa<> ras;
  agg::scanline_p8 scanline;
  InterpolatorType interpolator(mat);
  ImageAccessorType ia(src, agg::rgba8(255, 255, 255, 0));
  agg::image_filter_lut filter;
  filter.calculate(agg::image_filter_lanczos(1.0));
  SpanGeneratorType sg(ia, interpolator, filter);
  SpanAllocatorType sa;
  ras.reset();
  ras.move_to_d(0, 0);
  ras.line_to_d(dw, 0);
  ras.line_to_d(dw, dh);
  ras.line_to_d(0, dh);
  agg::render_scanlines_aa(ras, scanline, dst, sa, sg);
}

void draw_png(const std::string &path, Display *dpy, Drawable draw, GC gc, unsigned x, unsigned y, unsigned w, unsigned h, unsigned depth) {
  std::vector<uint8_t> dst_data;
  dst_data.resize(w * h * 4);

  XImage img;
  img.width = w;
  img.height = h;
  img.xoffset = 0;
  img.format = ZPixmap;
  img.data = reinterpret_cast<char *>(dst_data.data());
  img.byte_order = ImageByteOrder(dpy);
  img.bitmap_unit = 32;
  img.bitmap_bit_order = ImageByteOrder(dpy);
  img.bitmap_pad = 32;
  img.depth = depth;
  img.bits_per_pixel = 32;
  img.bytes_per_line = w * 4;
  img.red_mask = 0x00ff0000;
  img.green_mask = 0x0000ff00;
  img.blue_mask = 0x000000ff;
  img.obdata = 0;
  XInitImage(&img);

  png::image<png::rgba_pixel, png::solid_pixel_buffer<png::rgba_pixel>> png{ path };
  agg::rendering_buffer rbuf{ png.get_pixbuf().get_bytes().data(), static_cast<unsigned>(png.get_width()), static_cast<unsigned>(png.get_height()),
                              static_cast<int>(png.get_pixbuf().get_stride()) };
  agg::pixfmt_rgba32 pix{ rbuf };
  agg::rendering_buffer rbuf_dst{ dst_data.data(), w, h, static_cast<int>(w * 4) };
  agg::pixfmt_bgra32 pix_dst{ rbuf_dst };
  fit(pix, pix_dst);

  XPutImage(dpy, draw, gc, &img, 0, 0, x, y, w, h);
}

}
