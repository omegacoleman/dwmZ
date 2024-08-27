#include "ytk/misc/common.hpp"

#include <iostream>

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

int main(void) {
  Display *dpy;
  Window root;
  if(!(dpy = XOpenDisplay(NULL)))
    ytk::raise("dwm: cannot open display");
  root = DefaultRootWindow(dpy);

  Atom panel_text = XInternAtom(dpy, "DWMZ_PANEL0_TEXT", False);
  Atom utf8string = XInternAtom(dpy, "UTF8_STRING", False);
  std::string set_to = "hi";
  XChangeProperty(dpy, root, panel_text, utf8string, 8, PropModeReplace, reinterpret_cast<unsigned char*>(set_to.data()), set_to.size());

  XSync(dpy, False);

  return 0;
}
