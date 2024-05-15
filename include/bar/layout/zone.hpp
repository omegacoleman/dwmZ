#pragma once

#include <cstdint>
#include <vector>

namespace bar {

struct zone_t {
  double x = 0;
  double y = 0;
  double w = 0;
  double h = 0;
};

struct zone_attr_t {
  double margin_ratio = 0;
  double bl_ratio = 0;
  double txth_ratio = 0;
  double volbar_ratio = 0;
};

inline std::vector<zone_t> xsplit(zone_t z, double n) {
  std::vector<zone_t> ret;
  zone_t sep = z;
  sep.w = z.w / n;
  for(double i = 0; i < n; i++) {
    ret.push_back(sep);
    sep.x += z.w / n;
  }
  return ret;
}

};
