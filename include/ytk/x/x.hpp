#pragma once

#include <cstdlib>

extern "C" {

#include "X11/Xatom.h"
#include "X11/Xlib.h"
#include "X11/Xutil.h"
}

#include <memory>

#include "ytk/misc/common.hpp"

#include "agg_basics.h"
#include "agg_pixfmt_rgb.h"
#include "agg_pixfmt_rgba.h"

namespace ytk::x {

struct win_t {
public:
  ~win_t() { close(); }

  void init(int default_width = 800, int default_height = 600, bool with_alpha = true) {
    width_ = default_width;
    height_ = default_height;

    disp_ = XOpenDisplay(nullptr);
    if(!disp_) {
      raise("cannot open display");
    }
    // init atoms
    WM_PROTOCOLS = XInternAtom(disp_, "WM_PROTOCOLS", False);
    WM_DELETE_WINDOW = XInternAtom(disp_, "WM_DELETE_WINDOW", False);

    int screen = DefaultScreen(disp_);
    vis_ = DefaultVisual(disp_, screen);
    Window root = DefaultRootWindow(disp_);

    XSetWindowAttributes swa;

    XVisualInfo vinfo;
    int cw = CWEventMask;
    if(with_alpha && XMatchVisualInfo(disp_, screen, 32, TrueColor, &vinfo)) {
      vis_ = vinfo.visual;
      depth_ = vinfo.depth;
    } else if(XMatchVisualInfo(disp_, screen, 24, TrueColor, &vinfo)) {
      vis_ = vinfo.visual;
      depth_ = vinfo.depth;
    } else {
      raise("oswin.xlib: unable to create a window with 24 or 32 bit true colors");
    }
    Colormap cm = XCreateColormap(disp_, root, vis_, AllocNone);
    swa.colormap = cm;
    swa.background_pixel = 0;
    swa.border_pixel = 0;
    cw |= CWBackPixel | CWColormap | CWBorderPixel;

    swa.event_mask = ExposureMask | KeyPressMask | ClientMessage | StructureNotifyMask;

    win_ = XCreateWindow(disp_, root, 0, 0, width_, height_, 0, depth_, InputOutput, vis_, cw, &swa);
    XSetWMProtocols(disp_, win_, &WM_DELETE_WINDOW, 1);

    gc_ = XCreateGC(disp_, win_, 0, nullptr);
    pix_data_.resize(width_ * height_ * 4);
    img_ = XCreateImage(disp_, vis_, depth_, ZPixmap, 0, reinterpret_cast<char *>(pix_data_.data()), width_, height_, 32, 0);

    XMapWindow(disp_, win_);
  }

  void close() {
    if(disp_) {
      XUnmapWindow(disp_, win_);
      img_->data = nullptr;
      XDestroyImage(img_);
      XFreeGC(disp_, gc_);
      XDestroyWindow(disp_, win_);
      XCloseDisplay(disp_);
      disp_ = nullptr;
    }
  }

  unsigned width() const noexcept { return width_; }
  unsigned height() const noexcept { return height_; }

  void set_size(unsigned w, unsigned h) { XResizeWindow(disp_, win_, w, h); }

  template <class Callable> void redraw_xwin(Callable func) { func(disp_, win_, gc_, width_, height_, depth_); }

  template <class Callable> void redraw(Callable func) {
    pix_data_.resize(width_ * height_ * 4);
    std::fill(pix_data_.begin(), pix_data_.end(), 0);
    img_->width = width_;
    img_->bytes_per_line = width_ * 4;
    img_->height = height_;
    img_->data = reinterpret_cast<char *>(pix_data_.data());

    agg::rendering_buffer rbuf{ pix_data_.data(), width_, height_, static_cast<int>(width_ * 4) };
    switch(depth_) {
    case 24: {
      agg::pixfmt_bgrx32 pix{ rbuf };
      func(pix);
    } break;
    case 32: {
      agg::pixfmt_bgra32 pix{ rbuf };
      func(pix);
    } break;
    default:
      raise("invalid depth");
    }
    XPutImage(disp_, win_, gc_, img_, 0, 0, 0, 0, width_, height_);
  }

  void process_event(bool block = false) {
    if(!block && !XPending(disp_))
      return;
    XEvent e;
    XNextEvent(disp_, &e);
    // handle event
    switch(e.type) {
    case Expose: {
      event::handle(event::expose_t{});
    } break;
    case ClientMessage: {
      if((e.xclient.message_type == WM_PROTOCOLS) && (e.xclient.data.s[0] == WM_DELETE_WINDOW)) {
        log::info("hit close button");
        event::handle(event::app_exit_t{});
      }
    } break;
    case KeyPress:
      event::handle(event::key_press_t{ e.xkey.keycode });
      break;
    case ConfigureNotify: {
      if(width_ != e.xconfigure.width || height_ != e.xconfigure.height) {
        width_ = e.xconfigure.width;
        height_ = e.xconfigure.height;
        log::info("oswin.xlib: size changed to: {}, {}", width_, height_);
        event::handle(event::win_resize_t{ width_, height_ });
      }
    }
    default:;
    }
  }

private:
  Display *disp_ = nullptr;
  Window win_ = 0;
  GC gc_ = nullptr;
  int depth_ = 0;

  Visual *vis_ = nullptr;
  XImage *img_ = nullptr;
  std::vector<uint8_t> pix_data_;

  Atom WM_PROTOCOLS;
  Atom WM_DELETE_WINDOW;

  unsigned width_ = 800;
  unsigned height_ = 600;
};

}
