#pragma once

#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

extern "C" {

#include <pipewire/extensions/session-manager/keys.h>
#include <pipewire/keys.h>
#include <pipewire/pipewire.h>
#include <wp/wp.h>
}

namespace wpvol {

struct wp_vol_t;

void on_core_disconnected(wp_vol_t *self);
void on_om_installed(wp_vol_t *self);
void on_mixer_changed(wp_vol_t *self);
void on_def_nodes_changed(wp_vol_t *self);
void on_plugin_activated(WpObject *p, GAsyncResult *res, wp_vol_t *self);
void on_plugin_loaded(WpObject *p, GAsyncResult *res, wp_vol_t *self);

struct wp_vol_t {
public:
  friend void on_core_disconnected(wp_vol_t *self);
  friend void on_om_installed(wp_vol_t *self);
  friend void on_mixer_changed(wp_vol_t *self);
  friend void on_def_nodes_changed(wp_vol_t *self);
  friend void on_plugin_activated(WpObject *p, GAsyncResult *res, wp_vol_t *self);
  friend void on_plugin_loaded(WpObject *p, GAsyncResult *res, wp_vol_t *self);

  wp_vol_t() {}

  wp_vol_t(const wp_vol_t &) = delete;
  wp_vol_t &operator=(const wp_vol_t &) = delete;
  wp_vol_t(wp_vol_t &&) = delete;
  wp_vol_t &operator=(wp_vol_t &&) = delete;

  struct result_t {
    double volume;
    bool mute;
  };

  std::optional<result_t> get_result() {
    std::unique_lock lg{ mut_ };
    if(!cached_valid_ || status_ != status_t::running)
      return std::nullopt;
    result_t res;
    res.mute = cached_mute_;
    res.volume = cached_vol_;
    return res;
  }

  void on_update(std::function<void()> fn) { on_update_ = fn; }

  void run() {
    thread_ = std::thread{ [this]() { this->routine(); } };
  }

  ~wp_vol_t() {
    if(thread_.joinable()) {
      g_main_loop_quit(loop_);
      thread_.join();
    }
  }

private:
  void clear() {
    g_clear_object(&def_nodes_api_);
    g_clear_object(&mixer_api_);
    g_clear_object(&om_);
    g_clear_object(&core_);
    g_clear_pointer(&error_, g_error_free);
    g_clear_pointer(&mc_, g_main_context_unref);
    g_clear_pointer(&loop_, g_main_loop_unref);
  }

  void handle_error(std::string s) {
    err_s_ = std::move(s);
    status_ = status_t::error;
    if(loop_)
      g_main_loop_quit(loop_);
  }

  void handle_gerror(GError *error) { handle_error(error->message); }

  void routine() {
    wp_init(WP_INIT_ALL);

    mc_ = g_main_context_new();
    loop_ = g_main_loop_new(g_main_context_get_thread_default(), false);
    core_ = wp_core_new(g_main_context_get_thread_default(), nullptr, nullptr);

    om_ = wp_object_manager_new();
    wp_object_manager_add_interest(om_, WP_TYPE_NODE, nullptr);
    wp_object_manager_add_interest(om_, WP_TYPE_CLIENT, nullptr);
    wp_object_manager_request_object_features(om_, WP_TYPE_GLOBAL_PROXY, WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);

    loading_plugins_ = 2;
    wp_core_load_component(core_, "libwireplumber-module-default-nodes-api", "module", nullptr, nullptr, nullptr, (GAsyncReadyCallback)on_plugin_loaded, this);
    wp_core_load_component(core_, "libwireplumber-module-mixer-api", "module", nullptr, nullptr, nullptr, (GAsyncReadyCallback)on_plugin_loaded, this);

    if(!wp_core_connect(core_)) {
      handle_error("Could not connect to PipeWire");
      return;
    }

    g_signal_connect_swapped(core_, "disconnected", (GCallback)on_core_disconnected, this);
    g_signal_connect_swapped(om_, "installed", (GCallback)on_om_installed, this);

    g_main_loop_run(loop_);

    clear();
  }

  void handle_plugin_loaded(WpObject *p, GAsyncResult *res) {
    if(!wp_core_load_component_finish(core_, res, &error_)) {
      handle_gerror(error_);
      return;
    }

    if(--loading_plugins_ == 0) {
      def_nodes_api_ = wp_plugin_find(core_, "default-nodes-api");
      mixer_api_ = wp_plugin_find(core_, "mixer-api");
      g_object_set(G_OBJECT(mixer_api_), "scale", 1 /* cubic */, nullptr);

      pending_plugins_ = 2;
      wp_object_activate(WP_OBJECT(def_nodes_api_), WP_PLUGIN_FEATURE_ENABLED, nullptr, (GAsyncReadyCallback)on_plugin_activated, this);
      wp_object_activate(WP_OBJECT(mixer_api_), WP_PLUGIN_FEATURE_ENABLED, nullptr, (GAsyncReadyCallback)on_plugin_activated, this);
    }
  }

  void handle_plugin_activated(WpObject *p, GAsyncResult *res) {
    if(!wp_object_activate_finish(p, res, &error_)) {
      handle_gerror(error_);
      return;
    }

    if(--pending_plugins_ == 0)
      wp_core_install_object_manager(core_, om_);
  }

  void handle_om_installed() {
    status_ = status_t::running;

    update_cache();

    g_signal_connect_swapped(mixer_api_, "changed", (GCallback)on_mixer_changed, this);
    g_signal_connect_swapped(def_nodes_api_, "changed", (GCallback)on_def_nodes_changed, this);
  }

  void handle_core_disconnected() { handle_error("PipeWire core disconnected"); }

  bool get_default_audio_sink_id(guint32 *res) {
    g_signal_emit_by_name(def_nodes_api_, "get-default-node", "Audio/Sink", res);
    if(*res <= 0 || *res >= G_MAXUINT32) {
      return false;
    }
    return true;
  }

  void update_cache() {
    std::unique_lock lg{ mut_ };

    cached_valid_ = false;

    if(status_ != status_t::running)
      return;

    guint32 id;
    if(!get_default_audio_sink_id(&id))
      return;

    GVariant *variant = nullptr;
    g_signal_emit_by_name(mixer_api_, "get-volume", id, &variant);
    if(!variant) {
      return;
    }
    gboolean mute = false;
    gdouble volume = 1.0;
    g_variant_lookup(variant, "volume", "d", &volume);
    g_variant_lookup(variant, "mute", "b", &mute);
    g_clear_pointer(&variant, g_variant_unref);

    cached_vol_ = volume;
    cached_mute_ = mute;
    cached_valid_ = true;

    lg.unlock();
    if(on_update_)
      on_update_();
  }

  void handle_mixer_changed() { update_cache(); }

  void handle_def_nodes_changed() { update_cache(); }

  GMainContext *mc_ = nullptr;
  GMainLoop *loop_ = nullptr;
  GError *error_ = nullptr;

  WpCore *core_ = nullptr;
  WpObjectManager *om_ = nullptr;

  WpPlugin *def_nodes_api_ = nullptr;
  WpPlugin *mixer_api_ = nullptr;

  guint loading_plugins_ = 0;
  guint pending_plugins_ = 0;

  double cached_vol_ = 0.0;
  bool cached_mute_ = true;
  bool cached_valid_ = false;

  enum class status_t { error, init, running };
  status_t status_ = status_t::init;
  std::string err_s_ = "";

  std::thread thread_;
  std::mutex mut_;

  std::function<void()> on_update_;
};

inline void on_core_disconnected(wp_vol_t *self) { self->handle_core_disconnected(); }
inline void on_om_installed(wp_vol_t *self) { self->handle_om_installed(); }
inline void on_mixer_changed(wp_vol_t *self) { self->handle_mixer_changed(); }
inline void on_def_nodes_changed(wp_vol_t *self) { self->handle_def_nodes_changed(); }
inline void on_plugin_activated(WpObject *p, GAsyncResult *res, wp_vol_t *self) { self->handle_plugin_activated(p, res); }
inline void on_plugin_loaded(WpObject *p, GAsyncResult *res, wp_vol_t *self) { self->handle_plugin_loaded(p, res); }

}
