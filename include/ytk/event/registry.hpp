#pragma once

#include "ytk/misc/common.hpp"

#include <functional>

namespace ytk::event {

template <class EventType> struct registry_of {
  static std::function<void(const EventType &)> handler_;
};

template <class EventType> inline std::function<void(const EventType &)> registry_of<EventType>::handler_;

template <class EventType> void set_handler(std::function<void(const EventType &)> handler) { registry_of<EventType>::handler_ = handler; }

template <class EventType> void handle(const EventType &ev) {
  auto &fn = registry_of<EventType>::handler_;
  if(!fn) {
    log::warn("unhandled event: {}", ev.stringify());
    return;
  }
  fn(ev);
}

}
