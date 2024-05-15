#pragma once

#include "bar/layout/zone.hpp"

namespace bar {

struct wmbar_layout_t {
  bool good_ = false;

  zone_t logo_;
  zone_t tags_;
  zone_t wins_;
  zone_t ltbutton_;

  zone_t im_;
  zone_t volume_;
  zone_t time_;
};

using layout_flag_t = uint32_t;

namespace layout_flags {
constexpr layout_flag_t has_im = 1 << 3;
constexpr layout_flag_t has_volume = 1 << 4;
}

inline wmbar_layout_t layout_wmbar(double w, double h, unsigned tags, layout_flag_t flags) {
  double curr_x = 0;
  double curr_x_bak = w;
  wmbar_layout_t layo;

  // left to right

  layo.logo_.x = curr_x;
  layo.logo_.y = 0;
  layo.logo_.w = h * 3;
  layo.logo_.h = h;
  curr_x += layo.logo_.w;

  layo.tags_.x = curr_x;
  layo.tags_.y = 0;
  layo.tags_.w = h * 1.2 * tags;
  layo.tags_.h = h;
  curr_x += layo.tags_.w;

  layo.ltbutton_.x = curr_x;
  layo.ltbutton_.y = 0;
  layo.ltbutton_.w = h * 1.2;
  layo.ltbutton_.h = h;
  curr_x += layo.ltbutton_.w;

  // right to left

  layo.time_.y = 0;
  layo.time_.w = h * 4;
  layo.time_.h = h;
  layo.time_.x = curr_x_bak - layo.time_.w;
  curr_x_bak -= layo.time_.w;

  if(flags & layout_flags::has_volume) {
    layo.volume_.y = 0;
    layo.volume_.w = h * 2.5;
    layo.volume_.h = h;
    layo.volume_.x = curr_x_bak - layo.volume_.w;
    curr_x_bak -= layo.volume_.w;
  }

  if(flags & layout_flags::has_im) {
    layo.im_.y = 0;
    layo.im_.w = h * 1.2;
    layo.im_.h = h;
    layo.im_.x = curr_x_bak - layo.im_.w;
    curr_x_bak -= layo.im_.w;
  }

  // remaining

  if(curr_x_bak <= curr_x) {
    layo.good_ = false;
    return layo;
  }

  layo.wins_.x = curr_x;
  layo.wins_.y = 0;
  layo.wins_.w = curr_x_bak - curr_x;
  layo.wins_.h = h;

  layo.good_ = true;
  return layo;
}

}
