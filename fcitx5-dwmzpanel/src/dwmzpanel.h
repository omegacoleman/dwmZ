#pragma once

#include <fcitx-utils/i18n.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/instance.h>
#include <thread>

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

namespace fcitx {

FCITX_CONFIGURATION(DwmzPanelConfig, );

class DwmzPanel : public AddonInstance {
public:
    DwmzPanel(Instance *instance);
    ~DwmzPanel();

    Instance *instance() { return instance_; }

    const Configuration *getConfig() const override { return &config_; }
    void setConfig(const RawConfig &config) override;
    // void reloadConfig() override;

private:
    void updatePanelText();
    void setPanelText(const std::string& str);

    static const inline std::string ConfPath = "conf/dwmzpanel.conf";
    Instance *instance_;
    DwmzPanelConfig config_;

    Display *dpy_;
    Window root_;
    Atom panel_text_;
    Atom utf8string_;

    std::vector<std::unique_ptr<HandlerTableEntry<EventHandler>>>
        eventWatchers_;
};

class DwmzPanelFactory : public AddonFactory {
public:
    AddonInstance *create(AddonManager *manager) override {
        return new DwmzPanel(manager->instance());
    }
};
} // namespace fcitx

