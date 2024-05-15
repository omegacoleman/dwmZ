#pragma once

#include "ytk/misc/common.hpp"
#include <cstddef>

namespace bar {

struct rgb_literal_t {
  uint8_t r = 0, g = 0, b = 0;
};

inline constexpr uint8_t parse_hex(char ch) {
  if(ch >= '0' && ch <= '9')
    return ch - '0';
  if(ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;
  if(ch >= 'A' && ch <= 'F')
    return ch - 'A' + 10;
  ytk::raise("invalid hex: {}", ch);
}

inline constexpr uint8_t parse_hex_pair(char a, char b) { return (parse_hex(a) << 4) + parse_hex(b); }

inline constexpr rgb_literal_t parse_rgb_literal(const char *str, std::size_t sz) {
  rgb_literal_t lit;
  if(sz == 7 && str[0] == '#') {
    str++;
    sz--;
  }
  if(sz != 6)
    ytk::raise("invalid rgb literal: {}", str);
  lit.r = parse_hex_pair(str[0], str[1]);
  lit.g = parse_hex_pair(str[2], str[3]);
  lit.b = parse_hex_pair(str[4], str[5]);
  return lit;
}

template <class Color> constexpr Color as_color(rgb_literal_t lit) { return Color(lit.r, lit.g, lit.b); }

template <class Color, class Alpha> constexpr Color as_color(rgb_literal_t lit, Alpha alpha) { return Color(lit.r, lit.g, lit.b, alpha); }

namespace literals {

inline constexpr rgb_literal_t operator""_rgb(const char *str, std::size_t sz) { return parse_rgb_literal(str, sz); }

}

}
