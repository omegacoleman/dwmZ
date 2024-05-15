/* See LICENSE file for copyright and license details. */

typedef struct {
  Cursor cursor;
} Cur;

typedef struct {
  Display *dpy;
  int screen;
  Window root;
  Visual *visual;
  unsigned int depth;
  Colormap cmap;
  GC gc;
} Drw;

/* Drawable abstraction */
Drw *drw_create(Display *dpy, int screen, Window win, Visual *visual, unsigned int depth, Colormap cmap);
void drw_free(Drw *drw);

/* Cursor abstraction */
Cur *drw_cur_create(Drw *drw, int shape);
void drw_cur_free(Drw *drw, Cur *cursor);
