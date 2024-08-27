/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */

#include "bar/gc/colorscheme.hpp"
#include "bar/gc/gc.hpp"
#include "bar/layout/wmbar.hpp"
#include "bar/layout/zone.hpp"

#include "setbg/png_lanczos.hpp"
#include <X11/X.h>
#include <X11/extensions/Xrender.h>

#ifndef DWMZ_NO_WP
#include "wpvol/wpvol.hpp"
#endif

#include "agg_basics.h"
#include "agg_pixfmt_rgb.h"
#include "agg_pixfmt_rgba.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_renderer_scanline.h"
#include "agg_rendering_buffer.h"
#include "agg_scanline_p.h"
#include "agg_scanline_u.h"

#include "ev++.h"

#include "date/date.h"
#include "date/tz.h"

#include <chrono>
#include <cstdlib>
#include <map>

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

#include "drw.hpp"
#include "util.hpp"

/* macros */
#define BUTTONMASK (ButtonPressMask | ButtonReleaseMask)
#define CLEANMASK(mask) (mask & ~(numlockmask | LockMask) & (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask))
#define INTERSECT(x, y, w, h, m)                                                                                                                              \
  (DWM_MAX(0, DWM_MIN((x) + (w), (m)->wx + (m)->ww) - DWM_MAX((x), (m)->wx)) * DWM_MAX(0, DWM_MIN((y) + (h), (m)->wy + (m)->wh) - DWM_MAX((y), (m)->wy)))
#define ISVISIBLE(C) ((C->tags & C->mon->tagset[C->mon->seltags]))
#define HIDDEN(C) ((getstate(C->win) == IconicState))
#define LENGTH(X) (sizeof X / sizeof X[0])
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)
#define WIDTH(X) ((X)->w + 2 * (X)->bw + gappx)
#define HEIGHT(X) ((X)->h + 2 * (X)->bw + gappx)
#define TAGMASK ((1 << LENGTH(tags)) - 1)
#define OPAQUE 0xffU
#define ColFloat 3

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum {
  NetSupported,
  NetWMName,
  NetWMState,
  NetWMCheck,
  NetWMFullscreen,
  NetActiveWindow,
  NetWMWindowType,
  NetWMWindowTypeDialog,
  NetClientList,
  NetLast
};                                                                                              /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast };                                   /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

enum {
  DwmzAtomPanel0Text,
  DwmzAtomPanel1Text,
  DwmzAtomPanel2Text,
  DwmzAtomLast
};

typedef union {
  int i;
  unsigned int ui;
  float f;
  const void *v;
} Arg;

typedef struct {
  unsigned int click;
  unsigned int mask;
  unsigned int button;
  void (*func)(const Arg *arg);
  const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
  char name[256];
  float mina, maxa;
  int x, y, w, h;
  int oldx, oldy, oldw, oldh;
  int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
  int bw, oldbw;
  unsigned int tags;
  int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
  Client *next;
  Client *snext;
  Monitor *mon;
  Window win;
};

typedef struct {
  unsigned int mod;
  KeySym keysym;
  void (*func)(const Arg *);
  const Arg arg;
} Key;

typedef struct {
  bar::ltbutton_icon_t icon = bar::ltbutton_icon_t::tiled;
  void (*arrange)(Monitor *);
} Layout;

static bar::zone_attr_t barattr;

struct Monitor {
  bar::ltbutton_icon_t lticon;
  float mfact;
  int nmaster;
  int num;
  int by;             /* bar geometry */
  int mx, my, mw, mh; /* screen size */
  int wx, wy, ww, wh; /* window area  */
  unsigned int seltags;
  unsigned int sellt;
  unsigned int tagset[2];
  int showbar;
  int topbar;
  int hidsel;
  Client *clients;
  Client *sel;
  Client *stack;
  Monitor *next;

  Window barwin;
  XImage barimg;
  bar::wmbar_layout_t barlayo;
  std::vector<uint8_t> bardata;
  std::vector<std::pair<bar::zone_t, unsigned int>> barclick;
  std::vector<std::pair<bar::zone_t, Arg>> barclickarg;

  const Layout *lt[2];
};

typedef struct {
  const char *klass;
  const char *instance;
  const char *title;
  unsigned int tags;
  int isfloating;
  int monitor;
} Rule;

#ifndef DWMZ_NO_WP
static wpvol::wp_vol_t vol;
#endif

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstackvis(const Arg *arg);
static void focusstackhid(const Arg *arg);
static void focusstack(int inc, int vis);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void hide(const Arg *arg);
static void hidewin(Client *c);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
static void pop(Client *c);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static int sendevent(Client *c, Atom proto);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void seturgent(Client *c, int urg);
static void show(const Arg *arg);
static void showall(const Arg *arg);
static void showwin(Client *c);
static void showhide(Client *c);
static void spawn(const Arg *arg);
static void spawncmdptr(const Arg *arg);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *m);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void togglewin(const Arg *arg);
static void togglehide(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updatebg(Monitor *m);
static void updatebgs(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatetextprop(Atom atom, char* dest, size_t sz, const char* deftext);
static void updatealltextprop(void);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void xinitvisual();
static void zoom(const Arg *arg);

/* variables */
static const char broken[] = "broken";

#define DWMZ_TXT_SZ 256

static char stext[DWMZ_TXT_SZ];

#define DWMZ_TXTPANEL_COUNT 3

static char paneltext[DWMZ_TXT_SZ][DWMZ_TXTPANEL_COUNT];

static int screen;
static int sw, sh; /* X display screen geometry width, height */
static int bh;     /* bar height */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static std::map<int, void (*)(XEvent *)> handler{ { ButtonPress, buttonpress },
                                                  { ClientMessage, clientmessage },
                                                  { ConfigureRequest, configurerequest },
                                                  { ConfigureNotify, configurenotify },
                                                  { DestroyNotify, destroynotify },
                                                  { Expose, expose },
                                                  { FocusIn, focusin },
                                                  { KeyPress, keypress },
                                                  { MappingNotify, mappingnotify },
                                                  { MapRequest, maprequest },
                                                  { PropertyNotify, propertynotify },
                                                  { UnmapNotify, unmapnotify } };
static Atom wmatom[WMLast], netatom[NetLast], dwmzatom[DwmzAtomLast];
static Cur *cursor[CurLast];
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Window root, wmcheckwin;
static Pixmap bg_pm;
static GC root_gc;

static int useargb = 0;
static Visual *visual;
static int depth;
static Colormap cmap;

static ev::default_loop loop;

/* configuration, allows nested code to access above variables */

/* appearance */
static const char *bg_path = NULL;
static const unsigned int borderpx = 5; /* border pixel of windows */
static const unsigned int gappx = 9;    /* gaps between windows */
static const unsigned int snap = 32;    /* snap pixel */
static const int showbar = 1;           /* 0 means no bar */
static const int topbar = 1;            /* 0 means bottom bar */
static const int focusonwheel = 0;
static const double bh_ratio = 0.03;

bar::rgb_literal_t active_rgb = bar::colors::kanagawa::waveBlue2;
uint8_t active_alpha = 255;
bar::rgb_literal_t inactive_rgb = bar::colors::kanagawa::fujiGray;
uint8_t inactive_alpha = 204;

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
  /* xprop(1):
   *	WM_CLASS(STRING) = instance, class
   *	WM_NAME(STRING) = title
   */
  /* class      instance    title       tags mask     isfloating   monitor */
};

/* layout(s) */
static const float mfact = 0.55;     /* factor of master area size [0.05..0.95] */
static const int nmaster = 1;        /* number of clients in master area */
static const int resizehints = 0;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
  /* symbol     arrange function */
  { bar::ltbutton_icon_t::tiled, tile },    /* first entry is default */
  { bar::ltbutton_icon_t::floating, NULL }, /* no layout function means floating behavior */
  { bar::ltbutton_icon_t::monocle, monocle },
};

/* key definitions */
#define MODKEY Mod1Mask
// clang-format off
#define TAGKEYS(KEY, TAG) \
{ MODKEY, KEY, view, { .ui = 1 << TAG } }, \
{ MODKEY | ControlMask, KEY, toggleview, { .ui = 1 << TAG } }, \
{ MODKEY | ShiftMask, KEY, tag, { .ui = 1 << TAG } }, \
{ MODKEY | ControlMask | ShiftMask, KEY, toggletag, { .ui = 1 << TAG } },
// clang-format on

/* commands */
static const char *deflaunchercmd = "rofi -show drun";
static const char *launchercmdptr = deflaunchercmd;
static const char *deftermcmd = "xterm";
static const char *termcmdptr = deftermcmd;
static const char *deflockcmd = "xsecurelock";
static const char *lockcmdptr = deflockcmd;

#ifndef DWMZ_NO_WP
static const char *volupcmd[] = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", "10%+", NULL };
static const char *voldowncmd[] = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", "10%-", NULL };
static const char *togglemutecmd[] = { "wpctl", "set-mute", "@DEFAULT_AUDIO_SINK@", "toggle", NULL };
#endif

static const Key keys[] = {
  /* modifier                     key        function        argument */
  { MODKEY | ShiftMask, XK_l, spawncmdptr, { .v = &lockcmdptr } },
  { MODKEY, XK_p, spawncmdptr, { .v = &launchercmdptr } },
  { MODKEY | ShiftMask, XK_Return, spawncmdptr, { .v = &termcmdptr } },
#ifndef DWMZ_NO_WP
  { MODKEY, XK_Up, spawn, { .v = volupcmd } },
  { MODKEY, XK_Down, spawn, { .v = voldowncmd } },
  { MODKEY | ShiftMask, XK_m, spawn, { .v = togglemutecmd } },
#endif
  { MODKEY, XK_b, togglebar, { 0 } },
  { MODKEY, XK_j, focusstackvis, { .i = +1 } },
  { MODKEY, XK_k, focusstackvis, { .i = -1 } },
  { MODKEY | ShiftMask, XK_j, focusstackhid, { .i = +1 } },
  { MODKEY | ShiftMask, XK_k, focusstackhid, { .i = -1 } },
  { MODKEY, XK_i, incnmaster, { .i = +1 } },
  { MODKEY, XK_d, incnmaster, { .i = -1 } },
  { MODKEY, XK_h, setmfact, { .f = -0.05 } },
  { MODKEY, XK_l, setmfact, { .f = +0.05 } },
  { MODKEY, XK_Return, zoom, { 0 } },
  { MODKEY, XK_Tab, view, { 0 } },
  { MODKEY | ShiftMask, XK_c, killclient, { 0 } },
  { MODKEY, XK_t, setlayout, { .v = &layouts[0] } },
  { MODKEY, XK_f, setlayout, { .v = &layouts[1] } },
  { MODKEY, XK_m, setlayout, { .v = &layouts[2] } },
  { MODKEY, XK_space, setlayout, { 0 } },
  { MODKEY | ShiftMask, XK_space, togglefloating, { 0 } },
  { MODKEY, XK_0, view, { .ui = (unsigned)-1 } },
  { MODKEY | ShiftMask, XK_0, tag, { .ui = (unsigned)-1 } },
  { MODKEY, XK_comma, focusmon, { .i = -1 } },
  { MODKEY, XK_period, focusmon, { .i = +1 } },
  { MODKEY | ShiftMask, XK_comma, tagmon, { .i = -1 } },
  { MODKEY | ShiftMask, XK_period, tagmon, { .i = +1 } },
  { MODKEY | ShiftMask, XK_s, showall, { 0 } },
  { MODKEY, XK_o, togglehide, { 0 } },
  // clang-format off
  TAGKEYS(XK_1, 0)
  TAGKEYS(XK_2, 1)
  TAGKEYS(XK_3, 2)
  TAGKEYS(XK_4, 3)
  TAGKEYS(XK_5, 4)
  TAGKEYS(XK_6, 5)
  TAGKEYS(XK_7, 6)
  TAGKEYS(XK_8, 7)
  TAGKEYS(XK_9, 8)
  // clang-format on
  { MODKEY | ShiftMask, XK_q, quit, { 0 } },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
  /* click                event mask      button          function        argument */
  { ClkLtSymbol, 0, Button1, setlayout, { 0 } },
  { ClkLtSymbol, 0, Button3, setlayout, { .v = &layouts[2] } },
  { ClkWinTitle, 0, Button1, togglewin, { 0 } },
  { ClkWinTitle, 0, Button2, zoom, { 0 } },
  { ClkStatusText, 0, Button2, spawncmdptr, { .v = &termcmdptr } },
  { ClkClientWin, MODKEY, Button1, movemouse, { 0 } },
  { ClkClientWin, MODKEY, Button2, togglefloating, { 0 } },
  { ClkClientWin, MODKEY, Button3, resizemouse, { 0 } },
  { ClkTagBar, 0, Button1, view, { 0 } },
  { ClkTagBar, 0, Button3, toggleview, { 0 } },
  { ClkTagBar, MODKEY, Button1, tag, { 0 } },
  { ClkTagBar, MODKEY, Button3, toggletag, { 0 } },
};
/* end of configuration */

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags {
  char limitexceeded[LENGTH(tags) > 31 ? -1 : 1];
};

/* function implementations */
unsigned long rgbatopixel(bar::rgb_literal_t lit, uint8_t alpha) {
  XColor col;
  if(depth != 32) {
    col.red = ((unsigned short)lit.r) << 8;
    col.green = ((unsigned short)lit.g) << 8;
    col.blue = ((unsigned short)lit.b) << 8;
  } else {
    // x11 expects premultiplied color value
    col.red = (((unsigned short)lit.r) * alpha / 255) << 8;
    col.green = (((unsigned short)lit.g) * alpha / 255) << 8;
    col.blue = (((unsigned short)lit.b) * alpha / 255) << 8;
  }
  col.flags = DoRed | DoGreen | DoBlue;
  if(!XAllocColor(dpy, DefaultColormap(dpy, screen), &col))
    return 0;
  if(depth == 32) {
    return (col.pixel & 0x00ffffff) | ((unsigned long)alpha << 24);
  }
  return col.pixel;
}

unsigned long activepixel(void) { return rgbatopixel(active_rgb, active_alpha); }

unsigned long inactivepixel(void) { return rgbatopixel(inactive_rgb, inactive_alpha); }

void applyrules(Client *c) {
  const char *klass, *instance;
  unsigned int i;
  const Rule *r;
  Monitor *m;
  XClassHint ch = { NULL, NULL };

  /* rule matching */
  c->isfloating = 0;
  c->tags = 0;
  XGetClassHint(dpy, c->win, &ch);
  klass = ch.res_class ? ch.res_class : broken;
  instance = ch.res_name ? ch.res_name : broken;

  for(i = 0; i < LENGTH(rules); i++) {
    r = &rules[i];
    if((!r->title || strstr(c->name, r->title)) && (!r->klass || strstr(klass, r->klass)) && (!r->instance || strstr(instance, r->instance))) {
      c->isfloating = r->isfloating;
      c->tags |= r->tags;
      for(m = mons; m && m->num != r->monitor; m = m->next)
        ;
      if(m)
        c->mon = m;
    }
  }
  if(ch.res_class)
    XFree(ch.res_class);
  if(ch.res_name)
    XFree(ch.res_name);
  c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact) {
  int baseismin;
  Monitor *m = c->mon;

  /* set minimum possible */
  *w = DWM_MAX(1, *w);
  *h = DWM_MAX(1, *h);
  if(interact) {
    if(*x > sw)
      *x = sw - WIDTH(c);
    if(*y > sh)
      *y = sh - HEIGHT(c);
    if(*x + *w + 2 * c->bw < 0)
      *x = 0;
    if(*y + *h + 2 * c->bw < 0)
      *y = 0;
  } else {
    if(*x >= m->wx + m->ww)
      *x = m->wx + m->ww - WIDTH(c);
    if(*y >= m->wy + m->wh)
      *y = m->wy + m->wh - HEIGHT(c);
    if(*x + *w + 2 * c->bw <= m->wx)
      *x = m->wx;
    if(*y + *h + 2 * c->bw <= m->wy)
      *y = m->wy;
  }
  if(*h < bh)
    *h = bh;
  if(*w < bh)
    *w = bh;
  if(resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
    if(!c->hintsvalid)
      updatesizehints(c);
    /* see last two sentences in ICCCM 4.1.2.3 */
    baseismin = c->basew == c->minw && c->baseh == c->minh;
    if(!baseismin) { /* temporarily remove base dimensions */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for aspect limits */
    if(c->mina > 0 && c->maxa > 0) {
      if(c->maxa < (float)*w / *h)
        *w = *h * c->maxa + 0.5;
      else if(c->mina < (float)*h / *w)
        *h = *w * c->mina + 0.5;
    }
    if(baseismin) { /* increment calculation requires this */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for increment value */
    if(c->incw)
      *w -= *w % c->incw;
    if(c->inch)
      *h -= *h % c->inch;
    /* restore base dimensions */
    *w = DWM_MAX(*w + c->basew, c->minw);
    *h = DWM_MAX(*h + c->baseh, c->minh);
    if(c->maxw)
      *w = DWM_MIN(*w, c->maxw);
    if(c->maxh)
      *h = DWM_MIN(*h, c->maxh);
  }
  return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void arrange(Monitor *m) {
  if(m)
    showhide(m->stack);
  else
    for(m = mons; m; m = m->next)
      showhide(m->stack);
  if(m) {
    arrangemon(m);
    restack(m);
  } else
    for(m = mons; m; m = m->next)
      arrangemon(m);
}

void arrangemon(Monitor *m) {
  m->lticon = m->lt[m->sellt]->icon;
  if(m->lt[m->sellt]->arrange)
    m->lt[m->sellt]->arrange(m);
}

void attach(Client *c) {
  c->next = c->mon->clients;
  c->mon->clients = c;
}

void attachstack(Client *c) {
  c->snext = c->mon->stack;
  c->mon->stack = c;
}

bool hittest(bar::zone_t z, int x, int y) { return (z.x < x && (z.x + z.w) > x) && (z.y < y && (z.y + z.h) > y); }

void buttonpress(XEvent *e) {
  unsigned int i, x, click;
  Arg arg = { 0 };
  Client *c;
  Monitor *m;
  XButtonPressedEvent *ev = &e->xbutton;

  click = ClkRootWin;
  /* focus monitor if necessary */
  if((m = wintomon(ev->window)) && m != selmon && (focusonwheel || (ev->button != Button4 && ev->button != Button5))) {
    unfocus(selmon->sel, 1);
    selmon = m;
    focus(NULL);
  }
  if(ev->window == selmon->barwin) {
    for(const auto &[z, zclick] : m->barclick) {
      if(hittest(z, ev->x, ev->y)) {
        click = zclick;
        break;
      }
    }

    for(const auto &[z, zarg] : m->barclickarg) {
      if(hittest(z, ev->x, ev->y)) {
        arg = zarg;
        break;
      }
    }
  } else if((c = wintoclient(ev->window))) {
    if(focusonwheel || (ev->button != Button4 && ev->button != Button5))
      focus(c);
    XAllowEvents(dpy, ReplayPointer, CurrentTime);
    click = ClkClientWin;
  }
  for(i = 0; i < LENGTH(buttons); i++)
    if(click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
      buttons[i].func((click == ClkTagBar || click == ClkWinTitle) && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void checkotherwm(void) {
  xerrorxlib = XSetErrorHandler(xerrorstart);
  /* this causes an error if some other window manager is running */
  XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
  XSync(dpy, False);
  XSetErrorHandler(xerror);
  XSync(dpy, False);
}

void cleanup(void) {
  Arg a = { .ui = (unsigned)-1 };
  Layout foo = { bar::ltbutton_icon_t::floating, NULL };
  Monitor *m;
  size_t i;

  view(&a);
  selmon->lt[selmon->sellt] = &foo;
  for(m = mons; m; m = m->next)
    while(m->stack)
      unmanage(m->stack, 0);
  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  while(mons)
    cleanupmon(mons);
  for(i = 0; i < CurLast; i++)
    drw_cur_free(drw, cursor[i]);
  XDestroyWindow(dpy, wmcheckwin);
  drw_free(drw);
  XFreePixmap(dpy, bg_pm);
  XFreeGC(dpy, root_gc);
  XSync(dpy, False);
  XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
  XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void cleanupmon(Monitor *mon) {
  Monitor *m;

  if(mon == mons)
    mons = mons->next;
  else {
    for(m = mons; m && m->next != mon; m = m->next)
      ;
    m->next = mon->next;
  }
  XUnmapWindow(dpy, mon->barwin);
  XDestroyWindow(dpy, mon->barwin);
  delete mon;
}

void clientmessage(XEvent *e) {
  XClientMessageEvent *cme = &e->xclient;
  Client *c = wintoclient(cme->window);

  if(!c)
    return;
  if(cme->message_type == netatom[NetWMState]) {
    if(cme->data.l[1] == netatom[NetWMFullscreen] || cme->data.l[2] == netatom[NetWMFullscreen])
      setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
                        || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
  } else if(cme->message_type == netatom[NetActiveWindow]) {
    if(c != selmon->sel && !c->isurgent)
      seturgent(c, 1);
  }
}

void configure(Client *c) {
  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.display = dpy;
  ce.event = c->win;
  ce.window = c->win;
  ce.x = c->x;
  ce.y = c->y;
  ce.width = c->w;
  ce.height = c->h;
  ce.border_width = c->bw;
  ce.above = None;
  ce.override_redirect = False;
  XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void configurenotify(XEvent *e) {
  Monitor *m;
  Client *c;
  XConfigureEvent *ev = &e->xconfigure;
  int dirty;

  /* TODO: updategeom handling sucks, needs to be simplified */
  if(ev->window == root) {
    dirty = (sw != ev->width || sh != ev->height);
    sw = ev->width;
    sh = ev->height;
    if(updategeom() || dirty) {
      updatebars();
      updatebgs();
      for(m = mons; m; m = m->next) {
        for(c = m->clients; c; c = c->next)
          if(c->isfullscreen)
            resizeclient(c, m->mx, m->my, m->mw, m->mh);
      }
      focus(NULL);
      arrange(NULL);
    }
  }
}

void configurerequest(XEvent *e) {
  Client *c;
  Monitor *m;
  XConfigureRequestEvent *ev = &e->xconfigurerequest;
  XWindowChanges wc;

  if((c = wintoclient(ev->window))) {
    if(ev->value_mask & CWBorderWidth)
      c->bw = ev->border_width;
    else if(c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
      m = c->mon;
      if(ev->value_mask & CWX) {
        c->oldx = c->x;
        c->x = m->mx + ev->x;
      }
      if(ev->value_mask & CWY) {
        c->oldy = c->y;
        c->y = m->my + ev->y;
      }
      if(ev->value_mask & CWWidth) {
        c->oldw = c->w;
        c->w = ev->width;
      }
      if(ev->value_mask & CWHeight) {
        c->oldh = c->h;
        c->h = ev->height;
      }
      if((c->x + c->w) > m->mx + m->mw && c->isfloating)
        c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
      if((c->y + c->h) > m->my + m->mh && c->isfloating)
        c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
      if((ev->value_mask & (CWX | CWY)) && !(ev->value_mask & (CWWidth | CWHeight)))
        configure(c);
      if(ISVISIBLE(c))
        XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
    } else
      configure(c);
  } else {
    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
  }
  XSync(dpy, False);
}

Monitor *createmon(void) {
  Monitor *m;

  m = new Monitor;
  memset(m, 0, sizeof(Monitor));
  m->tagset[0] = m->tagset[1] = 1;
  m->mfact = mfact;
  m->nmaster = nmaster;
  m->showbar = showbar;
  m->topbar = topbar;
  m->lt[0] = &layouts[0];
  m->lt[1] = &layouts[1 % LENGTH(layouts)];
  m->lticon = layouts[0].icon;
  return m;
}

void destroynotify(XEvent *e) {
  Client *c;
  XDestroyWindowEvent *ev = &e->xdestroywindow;

  if((c = wintoclient(ev->window)))
    unmanage(c, 1);
}

void detach(Client *c) {
  Client **tc;

  for(tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next)
    ;
  *tc = c->next;
}

void detachstack(Client *c) {
  Client **tc, *t;

  for(tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext)
    ;
  *tc = c->snext;

  if(c == c->mon->sel) {
    for(t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext)
      ;
    c->mon->sel = t;
  }
}

Monitor *dirtomon(int dir) {
  Monitor *m = NULL;

  if(dir > 0) {
    if(!(m = selmon->next))
      m = mons;
  } else if(selmon == mons)
    for(m = mons; m->next; m = m->next)
      ;
  else
    for(m = mons; m->next != selmon; m = m->next)
      ;
  return m;
}

void drawbar(Monitor *m) {
  int x, w, tw = 0, n = 0, scm;
  unsigned int i, occ = 0, urg = 0;
  Client *c;

  if(!m->showbar)
    return;

  std::fill(m->bardata.begin(), m->bardata.end(), 0);
  m->barclick.clear();
  m->barclickarg.clear();

  agg::rendering_buffer rbuf{ m->bardata.data(), (unsigned)m->ww, (unsigned)bh, m->ww * 4 };

  agg::pixfmt_bgra32 pix{ rbuf };
  bar::agg_gc_t gc{ pix };

  const auto &layo = m->barlayo;
  const auto &attr = barattr;

  gc.draw_text_panel(stext, layo.logo_, attr, bar::panel_flavor_t::logo);
  m->barclick.emplace_back(layo.logo_, ClkStatusText);
  static int nn = 0;
  auto zoned = date::make_zoned(date::current_zone(), std::chrono::system_clock::now());
  auto local = zoned.get_local_time();
  auto tod = date::make_time(local - date::floor<date::days>(local));
  gc.draw_text_panel(fmt::format("{:02}:{:02}:{:02}", tod.hours().count(), tod.minutes().count(), tod.seconds().count()), layo.time_, attr,
                     bar::panel_flavor_t::datetime);

  for(c = m->clients; c; c = c->next) {
    if(ISVISIBLE(c))
      n++;
    occ |= c->tags;
    if(c->isurgent)
      urg |= c->tags;
  }

  gc.draw_panel_bg(layo.tags_, bar::panel_flavor_t::tagsel);
  auto z_tags = bar::xsplit(layo.tags_, LENGTH(tags));
  for(int i = 0; i < LENGTH(tags); i++) {
    auto flavor = bar::panel_flavor_t::tagsel;
    if(m->tagset[m->seltags] & (1 << i)) {
      flavor = bar::panel_flavor_t::tagsel_active;
      gc.draw_panel_bg(z_tags[i], flavor);
    }
    gc.draw_text(tags[i], z_tags[i], attr, flavor);
    m->barclick.emplace_back(z_tags[i], ClkTagBar);
    Arg arg;
    arg.ui = 1 << i;
    m->barclickarg.emplace_back(z_tags[i], arg);
    if(urg & (1 << i)) {
      gc.draw_panel_pin(z_tags[i], bar::panel_flavor_t::tagsel_urg);
    } else if(occ & (1 << i)) {
      gc.draw_panel_pin(z_tags[i], bar::panel_flavor_t::tagsel_occ);
    }
  }

  gc.draw_panel_bg(layo.ltbutton_, bar::panel_flavor_t::ltbutton);
  gc.draw_ltbutton_icon(m->lticon, layo.ltbutton_, attr, bar::panel_flavor_t::ltbutton);
  m->barclick.emplace_back(layo.ltbutton_, ClkLtSymbol);

  gc.draw_panel_bg(layo.wins_, bar::panel_flavor_t::winsel);
  auto z_wins = bar::xsplit(layo.wins_, n);
  i = 0;
  for(c = m->clients; c; c = c->next) {
    auto flavor = bar::panel_flavor_t::winsel;
    if(!ISVISIBLE(c))
      continue;
    if(m->sel == c) {
      flavor = bar::panel_flavor_t::winsel_active;
      gc.draw_panel_bg(z_wins[i], flavor);
    }
    gc.draw_text(c->name, z_wins[i], attr, flavor);
    if(HIDDEN(c) || (m->hidsel && m->sel == c)) {
      gc.draw_panel_pin(z_wins[i], bar::panel_flavor_t::winsel_hidden);
    }
    m->barclick.emplace_back(z_wins[i], ClkWinTitle);
    Arg arg;
    arg.v = c;
    m->barclickarg.emplace_back(z_wins[i], arg);
    i++;
  }

#ifndef DWMZ_NO_WP
  double vshow = 1.0;
  auto vres = vol.get_result();
  if(vres) {
    vshow = vres->volume;
    if(vres->mute)
      vshow = 0.0;
  }
  gc.draw_volbar_panel(vshow, layo.volume_, attr, bar::panel_flavor_t::volume);
#endif

  // TODO draw to custom panel space
  gc.draw_text_panel(std::string{paneltext[0]}, layo.im_, attr, bar::panel_flavor_t::im);

  XPutImage(dpy, m->barwin, drw->gc, &m->barimg, 0, 0, 0, 0, m->ww, bh);
  XSync(drw->dpy, False);
}

void drawbars(void) {
  Monitor *m;

  for(m = mons; m; m = m->next)
    drawbar(m);
}

void expose(XEvent *e) {
  Monitor *m;
  XExposeEvent *ev = &e->xexpose;

  if(ev->count == 0 && (m = wintomon(ev->window))) {
    drawbar(m);
  }
}

void focus(Client *c) {
  if(!c || !ISVISIBLE(c))
    for(c = selmon->stack; c && (!ISVISIBLE(c) || HIDDEN(c)); c = c->snext)
      ;
  if(selmon->sel && selmon->sel != c) {
    unfocus(selmon->sel, 0);

    if(selmon->hidsel) {
      hidewin(selmon->sel);
      if(c)
        arrange(c->mon);
      selmon->hidsel = 0;
    }
  }
  if(c) {
    if(c->mon != selmon)
      selmon = c->mon;
    if(c->isurgent)
      seturgent(c, 0);
    detachstack(c);
    attachstack(c);
    grabbuttons(c, 1);
    XSetWindowBorder(dpy, c->win, activepixel());
    setfocus(c);
  } else {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
  selmon->sel = c;
  drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void focusin(XEvent *e) {
  XFocusChangeEvent *ev = &e->xfocus;

  if(selmon->sel && ev->window != selmon->sel->win)
    setfocus(selmon->sel);
}

void focusmon(const Arg *arg) {
  Monitor *m;

  if(!mons->next)
    return;
  if((m = dirtomon(arg->i)) == selmon)
    return;
  unfocus(selmon->sel, 0);
  selmon = m;
  focus(NULL);
}

void focusstackvis(const Arg *arg) { focusstack(arg->i, 0); }

void focusstackhid(const Arg *arg) { focusstack(arg->i, 1); }

void focusstack(int inc, int hid) {
  Client *c = NULL, *i;
  // if no client selected AND exclude hidden client; if client selected but fullscreened
  if((!selmon->sel && !hid) || (selmon->sel && selmon->sel->isfullscreen && lockfullscreen))
    return;
  if(!selmon->clients)
    return;
  if(inc > 0) {
    if(selmon->sel)
      for(c = selmon->sel->next; c && (!ISVISIBLE(c) || (!hid && HIDDEN(c))); c = c->next)
        ;
    if(!c)
      for(c = selmon->clients; c && (!ISVISIBLE(c) || (!hid && HIDDEN(c))); c = c->next)
        ;
  } else {
    if(selmon->sel) {
      for(i = selmon->clients; i != selmon->sel; i = i->next)
        if(ISVISIBLE(i) && !(!hid && HIDDEN(i)))
          c = i;
    } else
      c = selmon->clients;
    if(!c)
      for(; i; i = i->next)
        if(ISVISIBLE(i) && !(!hid && HIDDEN(i)))
          c = i;
  }
  if(c) {
    focus(c);
    restack(selmon);
    if(HIDDEN(c)) {
      showwin(c);
      c->mon->hidsel = 1;
    }
  }
}

Atom getatomprop(Client *c, Atom prop) {
  int di;
  unsigned long dl;
  unsigned char *p = NULL;
  Atom da, atom = None;

  if(XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM, &da, &di, &dl, &dl, &p) == Success && p) {
    atom = *(Atom *)p;
    XFree(p);
  }
  return atom;
}

int getrootptr(int *x, int *y) {
  int di;
  unsigned int dui;
  Window dummy;

  return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long getstate(Window w) {
  int format;
  long result = -1;
  unsigned char *p = NULL;
  unsigned long n, extra;
  Atom real;

  if(XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState], &real, &format, &n, &extra, (unsigned char **)&p) != Success)
    return -1;
  if(n != 0)
    result = *p;
  XFree(p);
  return result;
}

int gettextprop(Window w, Atom atom, char *text, unsigned int size) {
  char **list = NULL;
  int n;
  XTextProperty name;

  if(!text || size == 0)
    return 0;
  text[0] = '\0';
  if(!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
    return 0;
  if(name.encoding == XA_STRING) {
    strncpy(text, (char *)name.value, size - 1);
  } else if(XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
    strncpy(text, *list, size - 1);
    XFreeStringList(list);
  }
  text[size - 1] = '\0';
  XFree(name.value);
  return 1;
}

void grabbuttons(Client *c, int focused) {
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask | LockMask };
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    if(!focused)
      XGrabButton(dpy, AnyButton, AnyModifier, c->win, False, BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
    for(i = 0; i < LENGTH(buttons); i++)
      if(buttons[i].click == ClkClientWin)
        for(j = 0; j < LENGTH(modifiers); j++)
          XGrabButton(dpy, buttons[i].button, buttons[i].mask | modifiers[j], c->win, False, BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
  }
}

void grabkeys(void) {
  updatenumlockmask();
  {
    unsigned int i, j, k;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask | LockMask };
    int start, end, skip;
    KeySym *syms;

    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    XDisplayKeycodes(dpy, &start, &end);
    syms = XGetKeyboardMapping(dpy, start, end - start + 1, &skip);
    if(!syms)
      return;
    for(k = start; k <= end; k++)
      for(i = 0; i < LENGTH(keys); i++)
        /* skip modifier codes, we do that ourselves */
        if(keys[i].keysym == syms[(k - start) * skip])
          for(j = 0; j < LENGTH(modifiers); j++)
            XGrabKey(dpy, k, keys[i].mod | modifiers[j], root, True, GrabModeAsync, GrabModeAsync);
    XFree(syms);
  }
}

void hide(const Arg *arg) {
  hidewin(selmon->sel);
  focus(NULL);
  arrange(selmon);
}

void togglehide(const Arg *arg) {
  if(selmon->hidsel) {
    show(arg);
  } else {
    hide(arg);
  }
}

void hidewin(Client *c) {
  if(!c || HIDDEN(c))
    return;

  Window w = c->win;
  static XWindowAttributes ra, ca;

  // more or less taken directly from blackbox's hide() function
  XGrabServer(dpy);
  XGetWindowAttributes(dpy, root, &ra);
  XGetWindowAttributes(dpy, w, &ca);
  // prevent UnmapNotify events
  XSelectInput(dpy, root, ra.your_event_mask & ~SubstructureNotifyMask);
  XSelectInput(dpy, w, ca.your_event_mask & ~StructureNotifyMask);
  XUnmapWindow(dpy, w);
  setclientstate(c, IconicState);
  XSelectInput(dpy, root, ra.your_event_mask);
  XSelectInput(dpy, w, ca.your_event_mask);
  XUngrabServer(dpy);
}

void incnmaster(const Arg *arg) {
  selmon->nmaster = DWM_MAX(selmon->nmaster + arg->i, 0);
  arrange(selmon);
}

#ifdef XINERAMA
static int isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info) {
  while(n--)
    if(unique[n].x_org == info->x_org && unique[n].y_org == info->y_org && unique[n].width == info->width && unique[n].height == info->height)
      return 0;
  return 1;
}
#endif /* XINERAMA */

void keypress(XEvent *e) {
  unsigned int i;
  KeySym keysym;
  XKeyEvent *ev;

  ev = &e->xkey;
  keysym = XkbKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0, 0);
  for(i = 0; i < LENGTH(keys); i++)
    if(keysym == keys[i].keysym && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state) && keys[i].func)
      keys[i].func(&(keys[i].arg));
}

void killclient(const Arg *arg) {
  if(!selmon->sel)
    return;
  if(!sendevent(selmon->sel, wmatom[WMDelete])) {
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(dpy, DestroyAll);
    XKillClient(dpy, selmon->sel->win);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
}

void manage(Window w, XWindowAttributes *wa) {
  Client *c, *t = NULL;
  Window trans = None;
  XWindowChanges wc;

  c = (Client *)ecalloc(1, sizeof(Client));
  c->win = w;
  /* geometry */
  c->x = c->oldx = wa->x;
  c->y = c->oldy = wa->y;
  c->w = c->oldw = wa->width;
  c->h = c->oldh = wa->height;
  c->oldbw = wa->border_width;

  updatetitle(c);
  if(XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
    c->mon = t->mon;
    c->tags = t->tags;
  } else {
    c->mon = selmon;
    applyrules(c);
  }

  if(c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
    c->x = c->mon->wx + c->mon->ww - WIDTH(c);
  if(c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
    c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
  c->x = DWM_MAX(c->x, c->mon->wx);
  c->y = DWM_MAX(c->y, c->mon->wy);
  c->bw = borderpx;

  wc.border_width = c->bw;
  XConfigureWindow(dpy, w, CWBorderWidth, &wc);
  XSetWindowBorder(dpy, w, inactivepixel());
  configure(c); /* propagates border_width, if size doesn't change */
  updatewindowtype(c);
  updatesizehints(c);
  updatewmhints(c);
  c->x = c->mon->mx + (c->mon->mw - WIDTH(c)) / 2;
  c->y = c->mon->my + (c->mon->mh - HEIGHT(c)) / 2;
  XSelectInput(dpy, w, EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
  grabbuttons(c, 0);
  if(!c->isfloating)
    c->isfloating = c->oldstate = trans != None || c->isfixed;
  if(c->isfloating)
    XRaiseWindow(dpy, c->win);
  if(c->isfloating)
    XSetWindowBorder(dpy, w, inactivepixel());
  attach(c);
  attachstack(c);
  XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *)&(c->win), 1);
  XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
  if(!HIDDEN(c))
    setclientstate(c, NormalState);
  if(c->mon == selmon)
    unfocus(selmon->sel, 0);
  c->mon->sel = c;
  arrange(c->mon);
  if(!HIDDEN(c))
    XMapWindow(dpy, c->win);
  focus(NULL);
}

void mappingnotify(XEvent *e) {
  XMappingEvent *ev = &e->xmapping;

  XRefreshKeyboardMapping(ev);
  if(ev->request == MappingKeyboard)
    grabkeys();
}

void maprequest(XEvent *e) {
  static XWindowAttributes wa;
  XMapRequestEvent *ev = &e->xmaprequest;

  if(!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect)
    return;
  if(!wintoclient(ev->window))
    manage(ev->window, &wa);
}

void monocle(Monitor *m) {
  unsigned int n = 0;
  Client *c;

  for(c = m->clients; c; c = c->next)
    if(ISVISIBLE(c))
      n++;
  for(c = nexttiled(m->clients); c; c = nexttiled(c->next))
    resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void movemouse(const Arg *arg) {
  int x, y, ocx, ocy, nx, ny;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;

  if(!(c = selmon->sel))
    return;
  if(c->isfullscreen) /* no support moving fullscreen windows by mouse */
    return;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if(XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
    return;
  if(!getrootptr(&x, &y))
    return;
  do {
    XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
    switch(ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nx = ocx + (ev.xmotion.x - x);
      ny = ocy + (ev.xmotion.y - y);
      if(abs(selmon->wx - nx) < snap)
        nx = selmon->wx;
      else if(abs((int)((selmon->wx + selmon->ww) - (nx + WIDTH(c)))) < snap)
        nx = selmon->wx + selmon->ww - WIDTH(c);
      if(abs(selmon->wy - ny) < snap)
        ny = selmon->wy;
      else if(abs((int)((selmon->wy + selmon->wh) - (ny + HEIGHT(c)))) < snap)
        ny = selmon->wy + selmon->wh - HEIGHT(c);
      if(!c->isfloating && selmon->lt[selmon->sellt]->arrange && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
        togglefloating(NULL);
      if(!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, nx, ny, c->w, c->h, 1);
      break;
    }
  } while(ev.type != ButtonRelease);
  XUngrabPointer(dpy, CurrentTime);
  if((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  }
}

Client *nexttiled(Client *c) {
  for(; c && (c->isfloating || !ISVISIBLE(c) || HIDDEN(c)); c = c->next)
    ;
  return c;
}

void pop(Client *c) {
  detach(c);
  attach(c);
  focus(c);
  arrange(c->mon);
}

void propertynotify(XEvent *e) {
  Client *c;
  Window trans;
  XPropertyEvent *ev = &e->xproperty;

  if (ev->window == root) {
    if (ev->atom == XA_WM_NAME) {
      updatetextprop(ev->atom, stext, DWMZ_TXT_SZ, "dwmZ");
      return;
    } else if (ev->atom == dwmzatom[DwmzAtomPanel0Text]) {
      updatetextprop(ev->atom, paneltext[0], DWMZ_TXT_SZ, "");
      return;
    } else if (ev->atom == dwmzatom[DwmzAtomPanel1Text]) {
      updatetextprop(ev->atom, paneltext[1], DWMZ_TXT_SZ, "");
      return;
    } else if (ev->atom == dwmzatom[DwmzAtomPanel2Text]) {
      updatetextprop(ev->atom, paneltext[0], DWMZ_TXT_SZ, "");
      return;
    }
  }
  if(ev->state == PropertyDelete)
    return; /* ignore */
  else if((c = wintoclient(ev->window))) {
    switch(ev->atom) {
    default:
      break;
    case XA_WM_TRANSIENT_FOR:
      if(!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) && (c->isfloating = (wintoclient(trans)) != NULL))
        arrange(c->mon);
      break;
    case XA_WM_NORMAL_HINTS:
      c->hintsvalid = 0;
      break;
    case XA_WM_HINTS:
      updatewmhints(c);
      drawbars();
      break;
    }
    if(ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
      updatetitle(c);
      if(c == c->mon->sel)
        drawbar(c->mon);
    }
    if(ev->atom == netatom[NetWMWindowType])
      updatewindowtype(c);
  }
}

void quit(const Arg *arg) {
  // fix: reloading dwm keeps all the hidden clients hidden
  Monitor *m;
  Client *c;
  for(m = mons; m; m = m->next) {
    if(m) {
      for(c = m->stack; c; c = c->next)
        if(c && HIDDEN(c))
          showwin(c);
    }
  }

  loop.break_loop();
}

Monitor *recttomon(int x, int y, int w, int h) {
  Monitor *m, *r = selmon;
  int a, area = 0;

  for(m = mons; m; m = m->next)
    if((a = INTERSECT(x, y, w, h, m)) > area) {
      area = a;
      r = m;
    }
  return r;
}

void resize(Client *c, int x, int y, int w, int h, int interact) {
  if(applysizehints(c, &x, &y, &w, &h, interact))
    resizeclient(c, x, y, w, h);
}

void resizeclient(Client *c, int x, int y, int w, int h) {
  XWindowChanges wc;
  unsigned int n;
  unsigned int gapoffset;
  unsigned int gapincr;
  Client *nbc;

  wc.border_width = c->bw;

  /* Get number of clients for the client's monitor */
  for(n = 0, nbc = nexttiled(c->mon->clients); nbc; nbc = nexttiled(nbc->next), n++)
    ;

  /* Do nothing if layout is floating */
  if(c->isfloating || c->mon->lt[c->mon->sellt]->arrange == NULL) {
    gapincr = gapoffset = 0;
  } else {
    /* Remove border and gap if layout is monocle or only one client */
    if(c->mon->lt[c->mon->sellt]->arrange == monocle || n == 1) {
      gapoffset = -borderpx;
      gapincr = -2 * borderpx;
    } else {
      gapoffset = gappx;
      gapincr = 2 * gappx;
    }
  }

  c->oldx = c->x;
  c->x = wc.x = x + gapoffset;
  c->oldy = c->y;
  c->y = wc.y = y + gapoffset;
  c->oldw = c->w;
  c->w = wc.width = w - gapincr;
  c->oldh = c->h;
  c->h = wc.height = h - gapincr;

  XConfigureWindow(dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
  configure(c);
  XSync(dpy, False);
}

void resizemouse(const Arg *arg) {
  int ocx, ocy, nw, nh;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;

  if(!(c = selmon->sel))
    return;
  if(c->isfullscreen) /* no support resizing fullscreen windows by mouse */
    return;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if(XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
    return;
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
  do {
    XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
    switch(ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nw = DWM_MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
      nh = DWM_MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
      if(c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww && c->mon->wy + nh >= selmon->wy
         && c->mon->wy + nh <= selmon->wy + selmon->wh) {
        if(!c->isfloating && selmon->lt[selmon->sellt]->arrange && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
          togglefloating(NULL);
      }
      if(!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, c->x, c->y, nw, nh, 1);
      break;
    }
  } while(ev.type != ButtonRelease);
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
  XUngrabPointer(dpy, CurrentTime);
  while(XCheckMaskEvent(dpy, EnterWindowMask, &ev))
    ;
  if((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  }
}

void restack(Monitor *m) {
  Client *c;
  XEvent ev;
  XWindowChanges wc;

  drawbar(m);
  if(!m->sel)
    return;
  if(m->sel->isfloating || !m->lt[m->sellt]->arrange)
    XRaiseWindow(dpy, m->sel->win);
  if(m->lt[m->sellt]->arrange) {
    wc.stack_mode = Below;
    wc.sibling = m->barwin;
    for(c = m->stack; c; c = c->snext)
      if(!c->isfloating && ISVISIBLE(c)) {
        XConfigureWindow(dpy, c->win, CWSibling | CWStackMode, &wc);
        wc.sibling = c->win;
      }
  }
  XSync(dpy, False);
  while(XCheckMaskEvent(dpy, EnterWindowMask, &ev))
    ;
}

void drainxevent() {
  XEvent ev;
  while(XPending(dpy)) {
    XNextEvent(dpy, &ev);
    if(handler[ev.type])
      handler[ev.type](&ev); /* call handler */
  }
}

void drainxevent_io(ev::io &, int) { drainxevent(); }

void timerdrawbars(ev::timer &, int) {
  drawbars();
  drainxevent();
}

static ev::async a_drawbars;

void asyncdrawbars(ev::async &, int) {
  drawbars();
  drainxevent();
}

void run(void) {
  ev::io x_io;
  x_io.set(ConnectionNumber(dpy), ev::READ);
  x_io.set<&drainxevent_io>();
  x_io.start();

  ev::timer tim;
  tim.set<&timerdrawbars>();
  tim.start(1., 1.);

  a_drawbars.set<&asyncdrawbars>();
  a_drawbars.start();
#ifndef DWMZ_NO_WP
  vol.on_update([]() { a_drawbars.send(); });
  vol.run();
#endif

  XEvent ev;
  /* main event loop */
  XSync(dpy, False);
  loop.run();
}

void scan(void) {
  unsigned int i, num;
  Window d1, d2, *wins = NULL;
  XWindowAttributes wa;

  if(XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
    for(i = 0; i < num; i++) {
      if(!XGetWindowAttributes(dpy, wins[i], &wa) || wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
        continue;
      if(wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
        manage(wins[i], &wa);
    }
    for(i = 0; i < num; i++) { /* now the transients */
      if(!XGetWindowAttributes(dpy, wins[i], &wa))
        continue;
      if(XGetTransientForHint(dpy, wins[i], &d1) && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
        manage(wins[i], &wa);
    }
    if(wins)
      XFree(wins);
  }
}

void sendmon(Client *c, Monitor *m) {
  if(c->mon == m)
    return;
  unfocus(c, 1);
  detach(c);
  detachstack(c);
  c->mon = m;
  c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
  attach(c);
  attachstack(c);
  focus(NULL);
  arrange(NULL);
}

void setclientstate(Client *c, long state) {
  long data[] = { state, None };

  XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32, PropModeReplace, (unsigned char *)data, 2);
}

int sendevent(Client *c, Atom proto) {
  int n;
  Atom *protocols;
  int exists = 0;
  XEvent ev;

  if(XGetWMProtocols(dpy, c->win, &protocols, &n)) {
    while(!exists && n--)
      exists = protocols[n] == proto;
    XFree(protocols);
  }
  if(exists) {
    ev.type = ClientMessage;
    ev.xclient.window = c->win;
    ev.xclient.message_type = wmatom[WMProtocols];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = proto;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, c->win, False, NoEventMask, &ev);
  }
  return exists;
}

void setfocus(Client *c) {
  if(!c->neverfocus) {
    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&(c->win), 1);
  }
  sendevent(c, wmatom[WMTakeFocus]);
}

void setfullscreen(Client *c, int fullscreen) {
  if(fullscreen && !c->isfullscreen) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char *)&netatom[NetWMFullscreen], 1);
    c->isfullscreen = 1;
    c->oldstate = c->isfloating;
    c->oldbw = c->bw;
    c->bw = 0;
    c->isfloating = 1;
    resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
    XRaiseWindow(dpy, c->win);
  } else if(!fullscreen && c->isfullscreen) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char *)0, 0);
    c->isfullscreen = 0;
    c->isfloating = c->oldstate;
    c->bw = c->oldbw;
    c->x = c->oldx;
    c->y = c->oldy;
    c->w = c->oldw;
    c->h = c->oldh;
    resizeclient(c, c->x, c->y, c->w, c->h);
    arrange(c->mon);
  }
}

void setlayout(const Arg *arg) {
  if(!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
    selmon->sellt ^= 1;
  if(arg && arg->v)
    selmon->lt[selmon->sellt] = (Layout *)arg->v;
  selmon->lticon = selmon->lt[selmon->sellt]->icon;
  if(selmon->sel)
    arrange(selmon);
  else
    drawbar(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void setmfact(const Arg *arg) {
  float f;

  if(!arg || !selmon->lt[selmon->sellt]->arrange)
    return;
  f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
  if(f < 0.05 || f > 0.95)
    return;
  selmon->mfact = f;
  arrange(selmon);
}

void setup(void) {
  int i;
  XSetWindowAttributes wa;
  Atom utf8string;
  struct sigaction sa;

  /* do not transform children into zombies when they terminate */
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
  sa.sa_handler = SIG_IGN;
  sigaction(SIGCHLD, &sa, NULL);

  /* clean up any zombies (inherited from .xinitrc etc) immediately */
  while(waitpid(-1, NULL, WNOHANG) > 0)
    ;

  /* init screen */
  screen = DefaultScreen(dpy);
  sw = DisplayWidth(dpy, screen);
  sh = DisplayHeight(dpy, screen);
  bh = sh * bh_ratio;
  root = RootWindow(dpy, screen);
  xinitvisual();
  drw = drw_create(dpy, screen, root, visual, depth, cmap);
  updategeom();
  /* init atoms */
  utf8string = XInternAtom(dpy, "UTF8_STRING", False);
  wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
  wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
  wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
  netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
  netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
  netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
  netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
  netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
  netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
  dwmzatom[DwmzAtomPanel0Text] = XInternAtom(dpy, "DWMZ_PANEL0_TEXT", False);
  dwmzatom[DwmzAtomPanel1Text] = XInternAtom(dpy, "DWMZ_PANEL1_TEXT", False);
  dwmzatom[DwmzAtomPanel2Text] = XInternAtom(dpy, "DWMZ_PANEL2_TEXT", False);
  /* init cursors */
  cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
  cursor[CurResize] = drw_cur_create(drw, XC_sizing);
  cursor[CurMove] = drw_cur_create(drw, XC_fleur);
  /* init bars */
  updatebars();
  updatebgs();
  updatealltextprop();

  /* supporting window for NetWMCheck */
  wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wmcheckwin, 1);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8, PropModeReplace, (unsigned char *)"dwm", 3);
  XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wmcheckwin, 1);
  /* EWMH support per view */
  XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32, PropModeReplace, (unsigned char *)netatom, NetLast);
  XDeleteProperty(dpy, root, netatom[NetClientList]);
  /* select events */
  wa.cursor = cursor[CurNormal]->cursor;
  wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask | PointerMotionMask | EnterWindowMask | LeaveWindowMask
                  | StructureNotifyMask | PropertyChangeMask;
  XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &wa);
  XSelectInput(dpy, root, wa.event_mask);
  grabkeys();
  focus(NULL);
}

void seturgent(Client *c, int urg) {
  XWMHints *wmh;

  c->isurgent = urg;
  if(!(wmh = XGetWMHints(dpy, c->win)))
    return;
  wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
  XSetWMHints(dpy, c->win, wmh);
  XFree(wmh);
}

void show(const Arg *arg) {
  if(selmon->hidsel)
    selmon->hidsel = 0;
  showwin(selmon->sel);
}

void showall(const Arg *arg) {
  Client *c = NULL;
  selmon->hidsel = 0;
  for(c = selmon->clients; c; c = c->next) {
    if(ISVISIBLE(c))
      showwin(c);
  }
  if(!selmon->sel) {
    for(c = selmon->clients; c && !ISVISIBLE(c); c = c->next)
      ;
    if(c)
      focus(c);
  }
  restack(selmon);
}

void showwin(Client *c) {
  if(!c || !HIDDEN(c))
    return;

  XMapWindow(dpy, c->win);
  setclientstate(c, NormalState);
  arrange(c->mon);
}

void showhide(Client *c) {
  if(!c)
    return;
  if(ISVISIBLE(c)) {
    /* show clients top down */
    XMoveWindow(dpy, c->win, c->x, c->y);
    if((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
      resize(c, c->x, c->y, c->w, c->h, 0);
    showhide(c->snext);
  } else {
    /* hide clients bottom up */
    showhide(c->snext);
    XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
  }
}

void dospawn(const char *file, char *const argv[]) {
  struct sigaction sa;

  if(fork() == 0) {
    if(dpy)
      close(ConnectionNumber(dpy));
    setsid();

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &sa, NULL);

    execvp(file, argv);
    ytk::log::error("dwm: execvp '{}' failed", file);
    ytk::raise_errno();
  }
}

void spawn(const Arg *arg) { dospawn(((char **)arg->v)[0], (char **)arg->v); }

void spawncmdptr(const Arg *arg) {
  char sh[] = "/bin/sh";
  char dashc[] = "-c";

  char *argv[4];
  argv[0] = sh;
  argv[1] = dashc;
  argv[2] = *((char **)arg->v);
  argv[3] = NULL;
  dospawn(sh, argv);
}

void tag(const Arg *arg) {
  if(selmon->sel && arg->ui & TAGMASK) {
    selmon->sel->tags = arg->ui & TAGMASK;
    focus(NULL);
    arrange(selmon);
  }
}

void tagmon(const Arg *arg) {
  if(!selmon->sel || !mons->next)
    return;
  sendmon(selmon->sel, dirtomon(arg->i));
}

void tile(Monitor *m) {
  unsigned int i, n, h, mw, my, ty;
  Client *c;

  for(n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
    ;
  if(n == 0)
    return;

  if(n > m->nmaster)
    mw = m->nmaster ? m->ww * m->mfact : 0;
  else
    mw = m->ww;
  for(i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
    if(i < m->nmaster) {
      h = (m->wh - my) / (DWM_MIN(n, m->nmaster) - i);
      resize(c, m->wx, m->wy + my, mw - (2 * c->bw) + (n > 1 ? gappx : 0), h - (2 * c->bw), 0);
      if(my + HEIGHT(c) < m->wh)
        my += HEIGHT(c);
    } else {
      h = (m->wh - ty) / (n - i);
      resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2 * c->bw), h - (2 * c->bw), 0);
      if(ty + HEIGHT(c) < m->wh)
        ty += HEIGHT(c);
    }
}

void togglebar(const Arg *arg) {
  selmon->showbar = !selmon->showbar;
  updatebarpos(selmon);
  XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, bh);
  arrange(selmon);
}

void togglefloating(const Arg *arg) {
  if(!selmon->sel)
    return;
  if(selmon->sel->isfullscreen) /* no support for fullscreen windows */
    return;
  selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
  XSetWindowBorder(dpy, selmon->sel->win, activepixel());
  if(selmon->sel->isfloating)
    resize(selmon->sel, selmon->sel->x, selmon->sel->y, selmon->sel->w, selmon->sel->h, 0);
  arrange(selmon);
}

void toggletag(const Arg *arg) {
  unsigned int newtags;

  if(!selmon->sel)
    return;
  newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
  if(newtags) {
    selmon->sel->tags = newtags;
    focus(NULL);
    arrange(selmon);
  }
}

void toggleview(const Arg *arg) {
  unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);

  if(newtagset) {
    selmon->tagset[selmon->seltags] = newtagset;
    focus(NULL);
    arrange(selmon);
  }
}

void togglewin(const Arg *arg) {
  Client *c = (Client *)arg->v;

  if(c == selmon->sel) {
    hidewin(c);
    focus(NULL);
    arrange(c->mon);
  } else {
    if(HIDDEN(c))
      showwin(c);
    focus(c);
    restack(selmon);
  }
}

void unfocus(Client *c, int setfocus) {
  if(!c)
    return;
  grabbuttons(c, 0);
  XSetWindowBorder(dpy, c->win, inactivepixel());
  if(setfocus) {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
}

void unmanage(Client *c, int destroyed) {
  Monitor *m = c->mon;
  XWindowChanges wc;

  detach(c);
  detachstack(c);
  if(!destroyed) {
    wc.border_width = c->oldbw;
    XGrabServer(dpy); /* avoid race conditions */
    XSetErrorHandler(xerrordummy);
    XSelectInput(dpy, c->win, NoEventMask);
    XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    setclientstate(c, WithdrawnState);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
  free(c);
  focus(NULL);
  updateclientlist();
  arrange(m);
}

void unmapnotify(XEvent *e) {
  Client *c;
  XUnmapEvent *ev = &e->xunmap;

  if((c = wintoclient(ev->window))) {
    if(ev->send_event)
      setclientstate(c, WithdrawnState);
    else
      unmanage(c, 0);
  }
}

void updatebars(void) {
  Monitor *m;
  XSetWindowAttributes wa;
  wa.override_redirect = True;
  wa.background_pixel = 0;
  wa.border_pixel = 0;
  wa.colormap = cmap;
  wa.event_mask = ButtonPressMask | ExposureMask;
  static char chs[] = "dwm";
  XClassHint ch = { chs, chs };
  for(m = mons; m; m = m->next) {
    if(m->barwin) {
      XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
    } else {
      m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0, depth, InputOutput, visual,
                                CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWColormap | CWEventMask, &wa);
    }
    bar::layout_flag_t flags = 0;
#ifndef DWMZ_NO_WP
    flags |= bar::layout_flags::has_volume;
#endif

    // TODO layout custom panels properly
    flags |= bar::layout_flags::has_im;

    m->barlayo = bar::layout_wmbar(m->ww, bh, LENGTH(tags), flags);
    m->bardata.resize(m->ww * bh * 4);
    m->barimg.width = m->ww;
    m->barimg.height = bh;
    m->barimg.xoffset = 0;
    m->barimg.format = ZPixmap;
    m->barimg.data = reinterpret_cast<char *>(m->bardata.data());
    m->barimg.byte_order = ImageByteOrder(dpy);
    m->barimg.bitmap_unit = 32;
    m->barimg.bitmap_bit_order = ImageByteOrder(dpy);
    m->barimg.bitmap_pad = 32;
    m->barimg.depth = depth == 32 ? 32 : 24;
    m->barimg.bits_per_pixel = 32;
    m->barimg.bytes_per_line = m->ww * 4;
    m->barimg.red_mask = 0x00ff0000;
    m->barimg.green_mask = 0x0000ff00;
    m->barimg.blue_mask = 0x000000ff;
    m->barimg.obdata = 0;
    XInitImage(&m->barimg);

    XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
    XMapRaised(dpy, m->barwin);
    XSetClassHint(dpy, m->barwin, &ch);
  }

  barattr.bl_ratio = 0.7;
  barattr.txth_ratio = 0.6;
  barattr.margin_ratio = 0.15;
  barattr.volbar_ratio = 0.2;
}

void updatebarpos(Monitor *m) {
  bh = sh * bh_ratio;

  m->wy = m->my;
  m->wh = m->mh;
  if(m->showbar) {
    m->wh -= bh;
    m->by = m->topbar ? m->wy : m->wy + m->wh;
    m->wy = m->topbar ? m->wy + bh : m->wy;
  } else
    m->by = -bh;
}

void updateclientlist() {
  Client *c;
  Monitor *m;

  XDeleteProperty(dpy, root, netatom[NetClientList]);
  for(m = mons; m; m = m->next)
    for(c = m->clients; c; c = c->next)
      XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend, (unsigned char *)&(c->win), 1);
}

int updategeom(void) {
  int dirty = 0;

#ifdef XINERAMA
  if(XineramaIsActive(dpy)) {
    int i, j, n, nn;
    Client *c;
    Monitor *m;
    XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
    XineramaScreenInfo *unique = NULL;

    for(n = 0, m = mons; m; m = m->next, n++)
      ;
    /* only consider unique geometries as separate screens */
    unique = ecalloc(nn, sizeof(XineramaScreenInfo));
    for(i = 0, j = 0; i < nn; i++)
      if(isuniquegeom(unique, j, &info[i]))
        memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
    XFree(info);
    nn = j;

    /* new monitors if nn > n */
    for(i = n; i < nn; i++) {
      for(m = mons; m && m->next; m = m->next)
        ;
      if(m)
        m->next = createmon();
      else
        mons = createmon();
    }
    for(i = 0, m = mons; i < nn && m; m = m->next, i++)
      if(i >= n || unique[i].x_org != m->mx || unique[i].y_org != m->my || unique[i].width != m->mw || unique[i].height != m->mh) {
        dirty = 1;
        m->num = i;
        m->mx = m->wx = unique[i].x_org;
        m->my = m->wy = unique[i].y_org;
        m->mw = m->ww = unique[i].width;
        m->mh = m->wh = unique[i].height;
        updatebarpos(m);
      }
    /* removed monitors if n > nn */
    for(i = nn; i < n; i++) {
      for(m = mons; m && m->next; m = m->next)
        ;
      while((c = m->clients)) {
        dirty = 1;
        m->clients = c->next;
        detachstack(c);
        c->mon = mons;
        attach(c);
        attachstack(c);
      }
      if(m == selmon)
        selmon = mons;
      cleanupmon(m);
    }
    free(unique);
  } else
#endif /* XINERAMA */
  {    /* default monitor setup */
    if(!mons)
      mons = createmon();
    if(mons->mw != sw || mons->mh != sh) {
      dirty = 1;
      mons->mw = mons->ww = sw;
      mons->mh = mons->wh = sh;
      updatebarpos(mons);
    }
  }
  if(dirty) {
    selmon = mons;
    selmon = wintomon(root);
  }
  return dirty;
}

void updatebg(Monitor *m) {
  if(bg_path) {
    setbg::png_lanczos::draw_png(bg_path, dpy, bg_pm, root_gc, m->mx, m->my, m->mw, m->mh, DefaultDepth(dpy, screen));
  }
}

void updatebgs(void) {
  if(bg_pm) {
    XFreePixmap(dpy, bg_pm);
  }
  if(!root_gc) {
    root_gc = XCreateGC(dpy, root, 0, NULL);
  }
  bg_pm = XCreatePixmap(dpy, root, sw, sh, DefaultDepth(dpy, screen));
  for(Monitor *m = mons; m; m = m->next) {
    updatebg(m);
  }
  XSetWindowBackgroundPixmap(dpy, root, bg_pm);
  XClearWindow(dpy, root);

  // for working with compositors
  XChangeProperty(dpy, root, XInternAtom(dpy, "_XROOTPMAP_ID", False), XA_PIXMAP, 32, PropModeReplace, (unsigned char *)&bg_pm, 1);
  XChangeProperty(dpy, root, XInternAtom(dpy, "ESETROOT_PMAP_ID", False), XA_PIXMAP, 32, PropModeReplace, (unsigned char *)&bg_pm, 1);
}

void updatenumlockmask(void) {
  unsigned int i, j;
  XModifierKeymap *modmap;

  numlockmask = 0;
  modmap = XGetModifierMapping(dpy);
  for(i = 0; i < 8; i++)
    for(j = 0; j < modmap->max_keypermod; j++)
      if(modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(dpy, XK_Num_Lock))
        numlockmask = (1 << i);
  XFreeModifiermap(modmap);
}

void updatesizehints(Client *c) {
  long msize;
  XSizeHints size;

  if(!XGetWMNormalHints(dpy, c->win, &size, &msize))
    /* size is uninitialized, ensure that size.flags aren't used */
    size.flags = PSize;
  if(size.flags & PBaseSize) {
    c->basew = size.base_width;
    c->baseh = size.base_height;
  } else if(size.flags & PMinSize) {
    c->basew = size.min_width;
    c->baseh = size.min_height;
  } else
    c->basew = c->baseh = 0;
  if(size.flags & PResizeInc) {
    c->incw = size.width_inc;
    c->inch = size.height_inc;
  } else
    c->incw = c->inch = 0;
  if(size.flags & PMaxSize) {
    c->maxw = size.max_width;
    c->maxh = size.max_height;
  } else
    c->maxw = c->maxh = 0;
  if(size.flags & PMinSize) {
    c->minw = size.min_width;
    c->minh = size.min_height;
  } else if(size.flags & PBaseSize) {
    c->minw = size.base_width;
    c->minh = size.base_height;
  } else
    c->minw = c->minh = 0;
  if(size.flags & PAspect) {
    c->mina = (float)size.min_aspect.y / size.min_aspect.x;
    c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
  } else
    c->maxa = c->mina = 0.0;
  c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
  c->hintsvalid = 1;
}

void updatetextpropvalue(Atom atom, char* dest, size_t sz, const char* deftext) {
  if(!gettextprop(root, atom, dest, sz))
    strcpy(dest, deftext);
}

void updatetextprop(Atom atom, char* dest, size_t sz, const char* deftext) {
  updatetextpropvalue(atom, dest, sz, deftext);
  drawbar(selmon);
}

void updatealltextprop() {
  updatetextpropvalue(XA_WM_NAME, stext, DWMZ_TXT_SZ, "dwmZ");
  updatetextpropvalue(dwmzatom[DwmzAtomPanel0Text], paneltext[0], DWMZ_TXT_SZ, "");
  updatetextpropvalue(dwmzatom[DwmzAtomPanel1Text], paneltext[1], DWMZ_TXT_SZ, "");
  updatetextpropvalue(dwmzatom[DwmzAtomPanel2Text], paneltext[2], DWMZ_TXT_SZ, "");
  drawbar(selmon);
}

void updatetitle(Client *c) {
  if(!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
    gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
  if(c->name[0] == '\0') /* hack to mark broken clients */
    strcpy(c->name, broken);
}

void updatewindowtype(Client *c) {
  Atom state = getatomprop(c, netatom[NetWMState]);
  Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

  if(state == netatom[NetWMFullscreen])
    setfullscreen(c, 1);
  if(wtype == netatom[NetWMWindowTypeDialog])
    c->isfloating = 1;
}

void updatewmhints(Client *c) {
  XWMHints *wmh;

  if((wmh = XGetWMHints(dpy, c->win))) {
    if(c == selmon->sel && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(dpy, c->win, wmh);
    } else
      c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
    if(wmh->flags & InputHint)
      c->neverfocus = !wmh->input;
    else
      c->neverfocus = 0;
    XFree(wmh);
  }
}

void view(const Arg *arg) {
  if((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
    return;
  selmon->seltags ^= 1; /* toggle sel tagset */
  if(arg->ui & TAGMASK)
    selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
  focus(NULL);
  arrange(selmon);
}

Client *wintoclient(Window w) {
  Client *c;
  Monitor *m;

  for(m = mons; m; m = m->next)
    for(c = m->clients; c; c = c->next)
      if(c->win == w)
        return c;
  return NULL;
}

Monitor *wintomon(Window w) {
  int x, y;
  Client *c;
  Monitor *m;

  if(w == root && getrootptr(&x, &y))
    return recttomon(x, y, 1, 1);
  for(m = mons; m; m = m->next)
    if(w == m->barwin)
      return m;
  if((c = wintoclient(w)))
    return c->mon;
  return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int xerror(Display *dpy, XErrorEvent *ee) {
  if(ee->error_code == BadWindow || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
     || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable) || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
     || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable) || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
     || (ee->request_code == X_GrabButton && ee->error_code == BadAccess) || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
     || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
    return 0;
  ytk::log::error("dwm: fatal error: request code={}, error code={}", ee->request_code, ee->error_code);
  return xerrorxlib(dpy, ee); /* may call exit */
}

int xerrordummy(Display *dpy, XErrorEvent *ee) { return 0; }

/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display *dpy, XErrorEvent *ee) {
  ytk::raise("dwm: another window manager is already running");
  return -1;
}

void xinitvisual() {
  XVisualInfo *infos;
  XRenderPictFormat *fmt;
  int nitems;
  int i;

  XVisualInfo tpl = { .screen = screen, .depth = 32, .c_class = TrueColor };
  long masks = VisualScreenMask | VisualDepthMask | VisualClassMask;

  infos = XGetVisualInfo(dpy, masks, &tpl, &nitems);
  visual = NULL;
  for(i = 0; i < nitems; i++) {
    fmt = XRenderFindVisualFormat(dpy, infos[i].visual);
    if(fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
      visual = infos[i].visual;
      depth = infos[i].depth;
      cmap = XCreateColormap(dpy, root, visual, AllocNone);
      useargb = 1;
      break;
    }
  }

  XFree(infos);

  if(!visual) {
    visual = DefaultVisual(dpy, screen);
    depth = DefaultDepth(dpy, screen);
    cmap = DefaultColormap(dpy, screen);
  }
}

void zoom(const Arg *arg) {
  Client *c = selmon->sel;

  if(!selmon->lt[selmon->sellt]->arrange || !c || c->isfloating)
    return;
  if(c == nexttiled(selmon->clients) && !(c = nexttiled(c->next)))
    return;
  pop(c);
}

void loadcfg(void) {
  char *str;
  if(str = getenv("DWMZ_BG")) {
    bg_path = str;
  }
  if(str = getenv("DWMZ_LAUNCHER")) {
    launchercmdptr = str;
  }
  if(str = getenv("DWMZ_TERM")) {
    termcmdptr = str;
  }
  if(str = getenv("DWMZ_LOCK")) {
    lockcmdptr = str;
  }
}

int main(int argc, char *argv[]) {
  if(argc == 2 && !strcmp("-v", argv[1])) {
    ytk::log::info("dwm");
    return EXIT_SUCCESS;
  } else if(argc != 1) {
    ytk::log::info("usage: dwm [-v]");
    return EXIT_FAILURE;
  }
  loadcfg();

  if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    ytk::log::warn("dwm: no locale support");
  if(!(dpy = XOpenDisplay(NULL)))
    ytk::raise("dwm: cannot open display");
  checkotherwm();
  setup();
#ifdef __OpenBSD__
  if(pledge("stdio rpath proc exec", NULL) == -1)
    ytk::raise("pledge");
#endif /* __OpenBSD__ */
  scan();
  run();
  cleanup();
  XCloseDisplay(dpy);
  return EXIT_SUCCESS;
}
