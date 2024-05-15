/* See LICENSE file for copyright and license details. */
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drw.hpp"
#include "util.hpp"

#include "ytk/misc/common.hpp"

Drw *drw_create(Display *dpy, int screen, Window root, Visual *visual, unsigned int depth, Colormap cmap) {
  Drw *drw = (Drw *)ecalloc(1, sizeof(Drw));

  drw->dpy = dpy;
  drw->screen = screen;
  drw->root = root;
  drw->visual = visual;
  drw->depth = depth;
  drw->cmap = cmap;
  Pixmap pm = XCreatePixmap(dpy, root, 1, 1, depth);
  drw->gc = XCreateGC(dpy, pm, 0, NULL);
  XFreePixmap(dpy, pm);
  return drw;
}

void drw_free(Drw *drw) {
  XFreeGC(drw->dpy, drw->gc);
  free(drw);
}

Cur *drw_cur_create(Drw *drw, int shape) {
  Cur *cur;

  if(!drw || !(cur = (Cur *)ecalloc(1, sizeof(Cur))))
    return NULL;

  cur->cursor = XCreateFontCursor(drw->dpy, shape);

  return cur;
}

void drw_cur_free(Drw *drw, Cur *cursor) {
  if(!cursor)
    return;

  XFreeCursor(drw->dpy, cursor->cursor);
  free(cursor);
}
