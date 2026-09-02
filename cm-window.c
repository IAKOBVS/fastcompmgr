
#include <stdio.h>
#include <stdlib.h>

#include <X11/Xatom.h>

#include "cm-event.h"
#include "cm-root.h"
#include "cm-window.h"
#include "cm-global.h"
#include "cm-util.h"


win *list;

#define WIN_HASH_INITIAL_SIZE 64
#define WIN_HASH_GROW_FACTOR 2
#define WIN_HASH_TOMBSTONE ((Window)-1)

typedef struct {
  Window key;
  win *value;
} WinHashEntry;

// Open-addressed hash, grown to keep the load factor <= 0.5 so probe
// chains stay short. Window ids are never 0, so 0 marks an empty slot.
static WinHashEntry *win_hash;
static unsigned int win_hash_size;   // number of slots (0 until first use)
static unsigned int win_hash_mask;   // size - 1 (size is always a power of two)
static unsigned int win_hash_count;  // live (non-empty, non-tombstone) entries

static inline unsigned int win_hash_fn(Window id) {
  return (unsigned int)(id * 2654435761UL) & win_hash_mask;
}

static void win_hash_grow(void) {
  unsigned int new_size = win_hash_size ? win_hash_size * WIN_HASH_GROW_FACTOR
                                        : WIN_HASH_INITIAL_SIZE;
  unsigned int new_mask = new_size - 1;
  WinHashEntry *new_table = calloc(new_size, sizeof(WinHashEntry));

  for (unsigned int i = 0; i < win_hash_size; i++) {
    if (win_hash[i].key != 0 && win_hash[i].key != WIN_HASH_TOMBSTONE) {
      unsigned int idx = (unsigned int)(win_hash[i].key * 2654435761UL) & new_mask;
      while (new_table[idx].key != 0)
        idx = (idx + 1) & new_mask;
      new_table[idx] = win_hash[i];
    }
  }
  free(win_hash);
  win_hash = new_table;
  win_hash_size = new_size;
  win_hash_mask = new_mask;
}

void win_hash_insert(Window id, win *w) {
  if (win_hash_size == 0 || win_hash_count * 2 >= win_hash_size) {
    win_hash_grow();
  }
  unsigned int idx = win_hash_fn(id);
  for (unsigned int i = 0; i < win_hash_size; i++) {
    if (win_hash[idx].key == 0 || win_hash[idx].key == WIN_HASH_TOMBSTONE) {
      win_hash[idx].key = id;
      win_hash[idx].value = w;
      win_hash_count++;
      return;
    }
    if (win_hash[idx].key == id) {
      win_hash[idx].value = w;
      return;
    }
    idx = (idx + 1) & win_hash_mask;
  }
}

void win_hash_remove(Window id) {
  if (win_hash_size == 0) return;
  unsigned int idx = win_hash_fn(id);
  for (unsigned int i = 0; i < win_hash_size; i++) {
    if (win_hash[idx].key == 0) return;
    if (win_hash[idx].key == id) {
      win_hash[idx].key = WIN_HASH_TOMBSTONE;
      win_hash[idx].value = NULL;
      win_hash_count--;
      return;
    }
    idx = (idx + 1) & win_hash_mask;
  }
}

typedef struct _AtomArr {
  Atom *atoms;
  unsigned long n_items;
} AtomArr;


_Static_assert (sizeof(Atom) == sizeof(long),
                "_query_atom_values depends on long-sized atom. See XGetWindowProperty");

static AtomArr _query_atom_values(Window window, Atom property) {
  Atom actual_type;
  int actual_format;
  unsigned long n_items, bytes_after;
  Atom *atoms = NULL;
  AtomArr ret;

  set_ignore(g_dpy, NextRequest(g_dpy));
  int result = XGetWindowProperty(g_dpy, window, property, 0, (~0L), False,
                                  XA_ATOM, &actual_type, &actual_format,
                                  &n_items, &bytes_after, (unsigned char**)&atoms);
  if(result != Success || atoms == NULL){
    memset(&ret, 0, sizeof(AtomArr));
    return ret;
  }
  if(unlikely(actual_format != 32)){
    fprintf(stderr, "fastcompmgr error: expected actual_format==32, got %d\n",
            actual_format);
    XFree(atoms);
    memset(&ret, 0, sizeof(AtomArr));
    return ret;
  }
  ret.atoms = atoms;
  ret.n_items = n_items;
  return ret;
}


static bool win_has_atom(Window window, Atom atom){
  Atom type = None;
  int format;
  unsigned long nitems, after;
  unsigned char *data = NULL;
  int res;

  set_ignore(g_dpy, NextRequest(g_dpy));
  res = XGetWindowProperty(
    g_dpy, window, atom, 0, 0, False,
    AnyPropertyType, &type, &format, &nitems,
    &after, &data);
  if (likely(res == Success && data != NULL )) {
      XFree(data);
      if (likely(type)) return true;
  }
  return false;
}


win* find_win(Window id) {
  if (win_hash_size == 0) return NULL;
  unsigned int idx = win_hash_fn(id);
  for (unsigned int i = 0; i < win_hash_size; i++) {
    if (win_hash[idx].key == 0) return NULL;
    if (win_hash[idx].key == id && win_hash[idx].value
        && !win_hash[idx].value->destroyed)
      return win_hash[idx].value;
    idx = (idx + 1) & win_hash_mask;
  }
  return NULL;
}


win* find_win_any_parent(Window w) {
  Window root, parent;
  Window *children;
  win* res = NULL;
  unsigned int nchildren;
  if((res=find_win(w)) != NULL){
      return res;
  }
  set_ignore(g_dpy, NextRequest(g_dpy));
  if (!XQueryTree(g_dpy, w, &root,
      &parent, &children, &nchildren)) {
    return NULL;
  }
  if (children) XFree((char *)children);
  if(parent == root){
      return NULL;
  }
  return find_win_any_parent(parent);
}


bool win_state_is_hidden(Window window) {
  AtomArr atom_arr;
  bool hidden;
  atom_arr = _query_atom_values(window, atom_net_wm_state);
  if(atom_arr.atoms == NULL) {
    return false;
  }
  hidden = false;
  for(unsigned long i=0; i<atom_arr.n_items; i++) {
    // After i3 restart in tabbed mode, a window may be _NET_WM_STATE_HIDDEN AND
    // _NET_WM_STATE_FOCUSED (a bug?), rendering it blank. Let's dissalow hidden
    // focused windows.
    if (atom_arr.atoms[i] == atom_net_wm_state_hidden) {
      hidden = true;
    } else if (atom_arr.atoms[i] == atom_net_wm_state_focused) {
      hidden = false;
      break;
    }
  }
  XFree(atom_arr.atoms);
  return hidden;
}


bool win_is_client(Window window){
  return win_has_atom(window, atom_wm_state);
}


void win_register_client_events(Window window)
{
  set_ignore(g_dpy, NextRequest(g_dpy));
  XSelectInput(g_dpy, window, PropertyChangeMask);
}
