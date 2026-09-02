// Integration test (no X server): simulates the compositor's per-frame
// decision pipeline as it is wired together in paint_all / main():
//   window registry (hash) <-> stacking list order <-> occlusion culling.
// Uses the REAL rect_paint_needed, win_hash_*, find_win and win structs;
// only the X requests are omitted.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "test_support.h"
#include "comp_rect.h"
#include "cm-event.c"
#include "cm-window.c"

#ifndef OPAQUE
#define OPAQUE 0xffffffff // defined in fastcompmgr.c in the real build
#endif

static const int ROOT_W = 1280;
static const int ROOT_H = 800;

// Mirrors add_win() list-insert + hash-insert bookkeeping (minus X calls).
static win *sim_add_win(Window id, int x, int y, int width, int height,
                        Window prev) {
  win *new = calloc(1, sizeof(win));
  assert(new != NULL);

  win **p;
  if (prev) {
    for (p = &list; *p; p = &(*p)->next) {
      if ((*p)->id == prev && !(*p)->destroyed)
        break;
    }
  } else {
    p = &list;
  }

  new->id = id;
  new->a.x = x;
  new->a.y = y;
  new->a.width = width;
  new->a.height = height;
  new->a.map_state = IsViewable;
  new->opacity = OPAQUE;
  new->damaged = 1;

  new->next = *p;
  *p = new;
  win_hash_insert(id, new); // order must match add_win: list first, then hash
  return new;
}

// Mirrors destroy_win() + finish_destroy_win() bookkeeping (minus X calls).
static void sim_destroy_win(Window id) {
  win *w = find_win(id);
  assert(w != NULL);
  w->destroyed = True;

  win **prev;
  for (prev = &list; (w = *prev); prev = &w->next) {
    if (w->id == id && w->destroyed) {
      *prev = w->next;
      win_hash_remove(w->id); // order must match finish_destroy_win
      free(w);
      return;
    }
  }
  assert(0);
}

static void assert_list_len(unsigned int expected) {
  unsigned int n = 0;
  for (win *w = list; w; w = w->next)
    n++;
  assert(n == expected);
}

// Mirrors win_paint_needed()'s geometry gating + rect_paint_needed().
static bool win_geometry_cull(win *w, CompRect *ignore_reg) {
  if (unlikely(w->a.x + w->a.width < 1 || w->a.y + w->a.height < 1
               || w->a.x >= ROOT_W || w->a.y >= ROOT_H)) {
    return false;
  }
  // unmapped/destroyed/translucent windows never contribute to ignore region
  if (w->a.map_state != IsViewable || w->destroyed || w->opacity != OPAQUE ||
      w->a.override_redirect) {
    return true;
  }
  CompRect w_rect = {.x1 = w->a.x, .y1 = w->a.y,
                     .x2 = w->a.x + w->a.width, .y2 = w->a.y + w->a.height,
                     .w = w->a.width, .h = w->a.height};
  return rect_paint_needed(ignore_reg, &w_rect);
}

// Mirrors paint_all()'s culling loop (damaged + occlusion-culling only).
static unsigned int run_frame(void) {
  CompRect ignore_reg = {0};
  unsigned int painted = 0;
  for (win *w = list; w; w = w->next) {
    if (w->destroyed || !w->damaged)
      continue;
    w->paint_needed = win_geometry_cull(w, &ignore_reg);
    if (w->paint_needed)
      painted++;
  }
  return painted;
}

static void damage_all(void) {
  for (win *w = list; w; w = w->next)
    w->damaged = 1;
}

static void test_lifecycle_and_lookup(void) {
  // add 5 windows in stacking order (later = on top)
  for (Window id = 0x800001; id <= 0x800005; id++)
    sim_add_win(id, 0, 0, 100, 100, id > 0x800001 ? id - 1 : None);
  assert_list_len(5);
  // hash lookup matches list for every id, and each find points to the
  // correct window (id field).
  for (win *w = list; w; w = w->next)
    assert(find_win(w->id) == w);
  assert_list_len(5);

  // destroy the middle window: gone from hash and list, others intact.
  sim_destroy_win(0x800003);
  assert(find_win(0x800003) == NULL);
  assert_list_len(4);
  for (win *w = list; w; w = w->next)
    assert(find_win(w->id) == w);
  // X reuses window ids: re-create with the same id.
  sim_add_win(0x800003, 0, 0, 100, 100, None);
  assert(find_win(0x800003) != NULL);
  assert_list_len(5);

  // teardown
  while (list)
    sim_destroy_win(list->id);
  assert_list_len(0);
  printf("test_lifecycle_and_lookup passed.\n");
}

static void test_occluded_window_skipped(void) {
  // The list head is the TOPMOST window (add_win's insert-before semantics).
  // A window fully covered by an opaque window ABOVE it must not be painted.
  sim_add_win(0x820001, 0, 0, 400, 300, None);      // bottom
  sim_add_win(0x820002, 0, 0, 400, 300, 0x820001);  // top, identical rect
  damage_all();
  assert(run_frame() == 1); // only the top window reaches the screen

  // Same with a small window fully inside a bigger one above it.
  sim_destroy_win(0x820001);
  sim_add_win(0x820003, 50, 50, 100, 100, None);      // bottom, small
  sim_add_win(0x820004, 0, 0, 400, 300, 0x820003);    // top, covers it fully
  damage_all();
  assert(run_frame() == 1);
  while (list)
    sim_destroy_win(list->id);
  printf("test_occluded_window_skipped passed.\n");
}

static void test_partial_overlap_painted(void) {
  // Two intersecting windows neither fully covering the other: both painted.
  sim_add_win(0x830001, 0, 0, 400, 300, None);
  sim_add_win(0x830002, 200, 150, 400, 300, 0x830001);
  damage_all();
  assert(run_frame() == 2);
  while (list)
    sim_destroy_win(list->id);
  printf("test_partial_overlap_painted passed.\n");
}

static void test_destroy_reveals_previously_skipped(void) {
  // 0x840001 (bottom) is hidden behind full-screen 0x840002 (top). After the
  // cover window dies, the bottom window must be painted again.
  sim_add_win(0x840001, 0, 0, 400, 300, None);
  sim_add_win(0x840002, 0, 0, ROOT_W, ROOT_H, 0x840001);
  damage_all();
  assert(run_frame() == 1);
  assert(find_win(0x840002)->paint_needed == 1);
  assert(find_win(0x840001)->paint_needed == 0); // culled

  sim_destroy_win(0x840002);
  damage_all();
  assert(run_frame() == 1);
  assert(find_win(0x840001)->paint_needed == 1); // revealed
  while (list)
    sim_destroy_win(list->id);
  printf("test_destroy_reveals_previously_skipped passed.\n");
}

int main(void) {
  printf("=== TEST PIPELINE ===\n");
  test_lifecycle_and_lookup();
  test_occluded_window_skipped();
  test_partial_overlap_painted();
  test_destroy_reveals_previously_skipped();
  printf("=== ALL PIPELINE TESTS PASSED ===\n");
  return 0;
}