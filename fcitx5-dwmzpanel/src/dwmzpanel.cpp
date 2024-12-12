#include "dwmzpanel.h"

#include <fcitx-config/iniparser.h>
#include <fcitx/event.h>
#include <fcitx/inputcontextmanager.h>
#include <queue>
#include <unistd.h>

namespace fcitx {

DwmzPanel::DwmzPanel(Instance *instance) : instance_(instance) {
  if(!(dpy_ = XOpenDisplay(NULL))) {
    FCITX_ERROR() << "cannot open display";
    return;
  }
  root_ = DefaultRootWindow(dpy_);
  panel_text_ = XInternAtom(dpy_, "DWMZ_INPUT_METHOD", False);
  utf8string_ = XInternAtom(dpy_, "UTF8_STRING", False);

  setPanelText("-");

  eventWatchers_.emplace_back(
      instance_->watchEvent(EventType::InputContextFocusIn,
        EventWatcherPhase::PostInputMethod, [this](Event &) {
          this->updatePanelText();
        }));
  eventWatchers_.emplace_back(
      instance_->watchEvent(EventType::InputContextFocusOut,
        EventWatcherPhase::PostInputMethod, [this](Event &) {
          this->updatePanelText();
        }));
  eventWatchers_.emplace_back(
      instance_->watchEvent(EventType::InputContextSwitchInputMethod,
        EventWatcherPhase::PostInputMethod, [this](Event &) {
          this->updatePanelText();
        }));
}

DwmzPanel::~DwmzPanel() {}

void DwmzPanel::updatePanelText() {
  std::string ind = "-";
  std::string curr_im = instance_->currentInputMethod();
  if (curr_im == "keyboard-us") {
    ind = "EN";
  }
  if (curr_im == "pinyin") {
    ind = "拼";
  }
  setPanelText(ind);
}

void DwmzPanel::setConfig(const RawConfig &config) {
    config_.load(config);
    safeSaveAsIni(config_, ConfPath);
}

void DwmzPanel::setPanelText(const std::string& str) {
  std::string copy = str;
  XChangeProperty(
    dpy_,
    root_,
    panel_text_,
    utf8string_,
    8,
    PropModeReplace,
    reinterpret_cast<unsigned char*>(copy.data()), copy.size());
  XSync(dpy_, False);
}

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::DwmzPanelFactory)

