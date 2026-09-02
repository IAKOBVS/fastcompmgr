// Integration test helper: a plain X client meant to be run under Xvfb
// alongside a real `fastcompmgr` (see test_integration.sh). It verifies that
// the compositor took the _NET_WM_CM_S<n> selection, then hammers the X
// server with normal window traffic (create/map/move-resize/raise/destroy)
// while the shell script checks that the compositor stays alive.
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WIN_COUNT 100
#define TIMEOUT_S 5

static Display *dpy;

static int fail(const char *msg) {
  fprintf(stderr, "test_helper_win: FAIL: %s\n", msg);
  return 1;
}

static int wait_for_compositor(int screen) {
  char name[32];
  snprintf(name, sizeof(name), "_NET_WM_CM_S%d", screen);
  Atom cm_atom = XInternAtom(dpy, name, True);
  if (cm_atom == None)
    return fail("no _NET_WM_CM_Sn atom (no compositing manager mode)");

  for (int attempt = 0; attempt < TIMEOUT_S * 10; attempt++) {
    Window owner = XGetSelectionOwner (dpy, cm_atom);
    if (owner != None) {
      printf("test_helper_win: compositor owns %s (0x%lx)\n", name,
             (unsigned long)owner);
      return 0;
    }
    usleep(100 * 1000);
  }
  return fail("_compositor never took the _NET_WM_CM_Sn selection");
}

int main(void) {
  dpy = XOpenDisplay(NULL);
  if (!dpy)
    return fail("cannot open display");
  int screen = DefaultScreen(dpy);

  if (wait_for_compositor(screen) != 0) {
    XCloseDisplay(dpy);
    return 1;
  }

  Window root = RootWindow (dpy, screen);
  int root_w = DisplayWidth(dpy, screen);
  int root_h = DisplayHeight(dpy, screen);
  if (root_w != 1280 || root_h != 800) {
    XCloseDisplay(dpy);
    return fail("unexpected root size");
  }

  Window wins[WIN_COUNT];
  for (int i = 0; i < WIN_COUNT; i++) {
    wins[i] = XCreateSimpleWindow (dpy, root, 0, 0, 1, 1, 0, None, None);
  }
  XSync (dpy, False);

  // map a few at a time, jittered positions, then move/resize/raise
  for (int round = 0; round < 3; round++) {
    for (int i = 0; i < WIN_COUNT; i++) {
      if (i % 3 != round)
        continue;
      XMoveResizeWindow(dpy, wins[i], (i * 17) % 1000, (i * 29) % 500,
                        100 + (i % 50), 80 + (i % 40));
      XMapWindow(dpy, wins[i]);
    }
    XSync (dpy, False);
    for (int i = 0; i < WIN_COUNT; i++) {
      if (i % 3 != round)
        continue;
      XRaiseWindow(dpy, wins[i]);
      XMoveResizeWindow(dpy, wins[i], (i * 13) % 1000, (i * 7) % 500,
                        120 + (i % 60), 90 + (i % 30));
    }
    XSync (dpy, False);
  }

  // exercise reparenting traffic (override-redirect windows)
  for (int i = 0; i < 10; i++) {
    XSetWindowAttributes attr = {.override_redirect = True};
    Window w = XCreateWindow (dpy, root, 0, 0, 10, 10, 0, CopyFromParent,
                              InputOutput, CopyFromParent, CWOverrideRedirect,
                              &attr);
    XMapWindow(dpy, w);
    XSync (dpy, False);
    XDestroyWindow(dpy, w);
  }
  XSync (dpy, False);

  // destroy everything
  for (int i = 0; i < WIN_COUNT; i++) {
    XDestroyWindow(dpy, wins[i]);
  }
  XSync (dpy, False);

  // still alive after the hammering?
  if (XGetSelectionOwner (dpy,
                          XInternAtom (dpy, "_NET_WM_CM_S0", False)) == None) {
    XCloseDisplay(dpy);
    return fail("compositor lost the selection during the test");
  }

  XCloseDisplay(dpy);
  printf("test_helper_win: PASS\n");
  return 0;
}