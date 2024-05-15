#pragma once

#include <cerrno>

#include "fmt/format.h"
#include "fmt/ostream.h"

#include "spdlog/spdlog.h"

namespace ytk {

template <typename... Args> [[noreturn]] inline void raise(Args &&...args) {
  spdlog::critical(std::forward<Args>(args)...);
  std::abort();
}

inline void raise_ec(std::error_code ec) { throw std::system_error(ec); }

inline void raise_errno() { throw std::system_error(std::error_code(errno, std::system_category())); }

namespace log {

template <typename... Args> inline void info(Args &&...args) { spdlog::info(std::forward<Args>(args)...); }

template <typename... Args> inline void warn(Args &&...args) { spdlog::warn(std::forward<Args>(args)...); }

template <typename... Args> inline void error(Args &&...args) { spdlog::error(std::forward<Args>(args)...); }

}

}
