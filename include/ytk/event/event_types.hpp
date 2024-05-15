#pragma once

#include "fmt/format.h"

namespace ytk::event {

struct app_exit_t {
  std::string stringify() const { return "app exit"; }
};

struct key_press_t {
  key_press_t(uint32_t keycode) : keycode(keycode) {}

  std::string stringify() const { return fmt::format("keypress code={}", keycode); }

  uint32_t keycode = 0;
};

struct expose_t {
  std::string stringify() const { return "exposed"; }
};

struct win_resize_t {
  win_resize_t(unsigned width, unsigned height) : width(width), height(height) {}

  std::string stringify() const { return fmt::format("win resize to {}x{}", width, height); }

  unsigned width = 0;
  unsigned height = 0;
};

}
