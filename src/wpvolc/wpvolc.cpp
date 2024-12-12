#include "wpvol/wpvol.hpp"
#include "ytk/misc/common.hpp"

#include <iostream>
#include <sstream>
#include <thread>

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

Display *dpy;
Window root;
Atom panel_text, utf8string;

void set_volume_s(std::string set_to) {
  XChangeProperty(
    dpy,
    root,
    panel_text,
    utf8string,
    8,
    PropModeReplace,
    reinterpret_cast<unsigned char*>(set_to.data()), set_to.size());
  XSync(dpy, False);
}

void init() {
  if(!(dpy = XOpenDisplay(NULL)))
    ytk::raise("dwm: cannot open display");
  root = DefaultRootWindow(dpy);
  panel_text = XInternAtom(dpy, "DWMZ_VOLUME", False);
  utf8string = XInternAtom(dpy, "UTF8_STRING", False);
}

int main(void) {
  init();

  wpvol::wp_vol_t vol;
  vol.on_update([&vol]() {
    auto res = vol.get_result();
    if(res) {
      std::cout << "vol changed to " << res->volume << " " << (res->mute ? "[M]" : "[ ]") << std::endl;
      if (res->mute) {
        set_volume_s("0");
      } else {
        std::ostringstream oss;
        oss << res->volume;
        set_volume_s(oss.str());
      }
    } else {
      std::cout << "vol becomes invalid" << std::endl;
      set_volume_s("");
    }
  });
  vol.run();
  return 0;
}
