// Shared definitions for test binaries that `#include "cm-window.c"`
// (and transitively "cm-event.c") to reach static internals.
//
// Only include this from ONE translation unit per test binary.
#pragma once

#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>

#include "cm-global.h"
#include "cm-window.h"

// Definitions for globals that would normally live in cm-root.c /
// cm-global.c / fastcompmgr.c. The test never talks to an X server, so
// g_dpy stays NULL and the atoms only need to exist for the linker.
Display *g_dpy;
int g_screen;

Atom atom_opacity, atom_win_type, atom_pixmap, atom_wm_state;
Atom atom_net_frame_extents, atom_gtk_frame_extents, atom_net_wm_state;
Atom atom_net_wm_state_hidden, atom_net_wm_state_focused;
Atom atom_net_active_window;