#pragma once

#include "dbus/dbus.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include <dbus/dbus-protocol.h>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>

#include "ytk/misc/common.hpp"

namespace fcitxim {

namespace cdbus {

struct dbus_exception : public std::exception {
  dbus_exception() : str_("dbus exception") {}

  explicit dbus_exception(const std::string &msg) : str_(fmt::format("dbus exception: {}", msg)) {}

  const char *what() const noexcept override { return str_.c_str(); }

private:
  std::string str_;
};

void raise(const std::string &msg) { throw dbus_exception{ msg }; }

template <class FunctionType> struct cdbus_call_chkbool_impl {
  explicit cdbus_call_chkbool_impl(FunctionType *fn) : fn(fn) {}

  template <class... Args> void operator()(Args &&...args) {
    if(!std::invoke(fn, std::forward<Args>(args)...)) {
      raise("not enough memory");
    }
  }

  FunctionType *fn;
};

template <class FunctionType> struct cdbus_call_chkerr_impl {
  explicit cdbus_call_chkerr_impl(FunctionType *fn) : fn(fn) {}

  template <class... Args> auto operator()(Args &&...args) {
    DBusError err;
    dbus_error_init(&err);
    if constexpr(std::is_void_v<std::invoke_result_t<FunctionType *, Args..., DBusError *>>) {
      std::invoke(fn, std::forward<Args>(args)..., &err);
      if(dbus_error_is_set(&err)) {
        std::string err_name{ err.name };
        std::string err_msg{ err.message };
        dbus_error_free(&err);
        raise(fmt::format("{} -- {}", err_name, err_msg));
      }
    } else {
      auto ret = std::invoke(fn, std::forward<Args>(args)..., &err);
      if(dbus_error_is_set(&err)) {
        std::string err_name{ err.name };
        std::string err_msg{ err.message };
        dbus_error_free(&err);
        raise(fmt::format("{} -- {}", err_name, err_msg));
      }
      return ret;
    }
  }

  FunctionType *fn;
};

template <class FunctionType> auto cdbus_call_chkbool(FunctionType *fn) { return cdbus_call_chkbool_impl(fn); }

template <class FunctionType> auto cdbus_call_chkerr(FunctionType *fn) { return cdbus_call_chkerr_impl(fn); }

#define CDBUS_FNDEF_CHKBOOL(__name) static auto __name = cdbus_call_chkbool(dbus_##__name)

#define CDBUS_FNDEF_CHKERR(__name) static auto __name = cdbus_call_chkerr(dbus_##__name)

CDBUS_FNDEF_CHKBOOL(message_iter_append_basic);
CDBUS_FNDEF_CHKBOOL(connection_send);

CDBUS_FNDEF_CHKERR(bus_get);
CDBUS_FNDEF_CHKERR(bus_request_name);
CDBUS_FNDEF_CHKERR(bus_add_match);

namespace arg {

struct b {
  void parse_from(DBusMessageIter &iter) {
    if(dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_BOOLEAN) {
      raise("type error: not a boolean");
    }
    dbus_message_iter_get_basic(&iter, &result);
  }
  bool result;
};

struct s {
  void parse_from(DBusMessageIter &iter) {
    if(dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING) {
      raise("type error: not a string");
    }
    const char *cstr;
    dbus_message_iter_get_basic(&iter, &cstr);
    result = std::string{ cstr };
  }
  std::string result;
};

struct i {
  void parse_from(DBusMessageIter &iter) {
    if(dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_INT32) {
      raise("type error: not an int32");
    }
    dbus_message_iter_get_basic(&iter, &result);
  }
  int32_t result;
};

struct as {
  void parse_from(DBusMessageIter &iter) {
    if(dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
      raise("type error: not an array");
    }
    DBusMessageIter arr_iter;
    dbus_message_iter_recurse(&iter, &arr_iter);
    do {
      if(dbus_message_iter_get_arg_type(&arr_iter) == DBUS_TYPE_INVALID) {
        return;
      }
      if(dbus_message_iter_get_arg_type(&arr_iter) != DBUS_TYPE_STRING) {
        raise("type error: not an array of strings");
      }
      const char *cstr;
      dbus_message_iter_get_basic(&arr_iter, &cstr);
      result.emplace_back(cstr);
    } while(dbus_message_iter_next(&arr_iter));
  }
  std::vector<std::string> result;
};

}

template <class... Args> auto parse_args(DBusMessage *msg) {
  DBusMessageIter iter;

  std::tuple<Args...> stubs;
  bool first = true;
  auto functor = [msg, &iter, &first](auto &stub) -> int {
    bool few = false;
    if(first) {
      few = !dbus_message_iter_init(msg, &iter);
    } else {
      few = !dbus_message_iter_next(&iter);
    }
    if(few)
      raise("too few arguments");
    stub.parse_from(iter);
    first = false;
    return 1;
  };
  std::apply([&iter, &functor](auto &...stub) { (..., functor(stub)); }, stubs);
  return std::apply([&iter](auto &...stub) { return std::make_tuple(stub.result...); }, stubs);
}

}

struct fcitximc_t {
public:
  ~fcitximc_t() {
    running_ = false;
    if(thread_.joinable()) {
      thread_.join();
    }
  }

  void run() {
    thread_ = std::thread{ [this]() { this->routine(); } };
  }

  std::string get_im() {
    std::unique_lock lg{ mut_ };
    return current_im_;
  }

  void on_update(std::function<void()> fn) { on_update_ = fn; }

private:
  void routine() {
    std::unordered_map<std::string, std::function<void(DBusMessage *)>> msg_handlers = {
      { "s$org.fcitx.Fcitx.Controller1$InputContextFocusIn", [this](DBusMessage *msg) { this->handle_ic_focus_in(msg); } },
      { "s$org.fcitx.Fcitx.Controller1$InputContextFocusOut", [this](DBusMessage *msg) { this->handle_ic_focus_out(msg); } },
      { "s$org.fcitx.Fcitx.Controller1$InputContextSwitchInputMethod", [this](DBusMessage *msg) { this->handle_ic_switch_im(msg); } },
      { "mr$$", [this](DBusMessage *msg) { this->handle_curr_im_result(msg); } },
    };

    try {
      conn_ = cdbus::bus_get(DBUS_BUS_SESSION);

      (void)cdbus::bus_request_name(conn_, "org.dwmz.fcitximc", DBUS_NAME_FLAG_REPLACE_EXISTING);

      cdbus::bus_add_match(conn_, "type='signal',interface='org.fcitx.Fcitx.Controller1'");

      // initially retrieve the value after program start
      trigger_update();

      while(running_) {
        DBusMessage *msg = dbus_connection_pop_message(conn_);
        if(!msg) {
          if(!dbus_connection_read_write(conn_, 1000))
            break;
          continue;
        }

        int mtype = dbus_message_get_type(msg);
        std::string type_s = "u";
        switch(mtype) {
        case DBUS_MESSAGE_TYPE_SIGNAL:
          type_s = "s";
          break;
        case DBUS_MESSAGE_TYPE_METHOD_CALL:
          type_s = "mc";
          break;
        case DBUS_MESSAGE_TYPE_METHOD_RETURN:
          type_s = "mr";
          break;
        case DBUS_MESSAGE_TYPE_ERROR:
          type_s = "e";
          break;
        default:;
        }
        const char *sintf = dbus_message_get_interface(msg);
        const char *smember = dbus_message_get_member(msg);
        std::string selector = fmt::format("{}${}${}", type_s, sintf ? sintf : "", smember ? smember : "");

        auto it = msg_handlers.find(selector);
        if(it != msg_handlers.end()) {
          it->second(msg);
        }

        dbus_message_unref(msg);
      }
      dbus_connection_unref(conn_);
    } catch(const cdbus::dbus_exception &e) {
      ytk::log::error(e.what());
    }
    {
      std::unique_lock lg{ mut_ };
      current_im_ = "";
    }
  }

  void handle_ic_focus_in(DBusMessage *) { trigger_update(); }

  void handle_ic_focus_out(DBusMessage *) { trigger_update(); }

  void handle_ic_switch_im(DBusMessage *) { trigger_update(); }

  void handle_curr_im_result(DBusMessage *msg) {
    auto [im_str] = cdbus::parse_args<cdbus::arg::s>(msg);
    bool updated = false;
    {
      std::unique_lock lg{ mut_ };
      if(current_im_ != im_str)
        updated = true;
      current_im_ = im_str;
    }
    if(on_update_ && updated)
      on_update_();
  }

  void trigger_update() {
    DBusMessage *msg = dbus_message_new_method_call("org.fcitx.Fcitx5", "/controller", "org.fcitx.Fcitx.Controller1", "CurrentInputMethod");
    uint32_t serial = 0;
    cdbus::connection_send(conn_, msg, &serial);
    dbus_message_unref(msg);
  }

  DBusConnection *conn_ = nullptr;
  std::string current_im_ = "";
  std::thread thread_;
  std::mutex mut_;
  std::function<void()> on_update_;
  std::atomic<bool> running_ = ATOMIC_VAR_INIT(true);
};

}
