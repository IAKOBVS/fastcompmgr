// Unit tests for the window lookup hash map (cm-window.c win_hash_* and
// find_win). Includes cm-window.c and cm-event.c directly to reach the
// otherwise static hash internals, no X server required.
#include <assert.h>
#include <stdio.h>

#include "test_support.h"
#include "cm-event.c"
#include "cm-window.c"

static win *make_win(Window id) {
  win *w = calloc(1, sizeof(win));
  assert(w != NULL);
  w->id = id;
  return w;
}

static const Window B = 0x400001; // base id (X server ids start there)

static void test_empty(void) {
  assert(find_win(B) == NULL);
  win_hash_remove(B); // must be a no-op, no crash
  printf("test_empty passed.\n");
}

static void test_insert_find_remove(void) {
  win *w = make_win(B + 1);
  win_hash_insert(B + 1, w);
  assert(find_win(B + 1) == w);

  win_hash_remove(B + 1);
  assert(find_win(B + 1) == NULL);
  free(w);
  printf("test_insert_find_remove passed.\n");
}

static void test_destroyed_is_invisible(void) {
  win *w = make_win(B + 2);
  win_hash_insert(B + 2, w);
  w->destroyed = True; // destroy_win marks destroyed before free
  assert(find_win(B + 2) == NULL);
  win_hash_remove(B + 2);
  free(w);
  printf("test_destroyed_is_invisible passed.\n");
}

static void test_reinsert_same_id(void) {
  win *w1 = make_win(B + 3);
  win *w2 = make_win(B + 3);
  win_hash_insert(B + 3, w1);
  win_hash_insert(B + 3, w2); // duplicate -> update, not double-insert
  assert(find_win(B + 3) == w2);
  // removing once must fully clean up
  win_hash_remove(B + 3);
  assert(find_win(B + 3) == NULL);
  free(w1);
  free(w2);
  printf("test_reinsert_same_id passed.\n");
}

static void test_remove_then_reuse_slot(void) {
  // Deleting leaves tombstones; reinsert must reuse them so a heavily
  // churned table keeps finding its entries.
  win *w1 = make_win(B + 4);
  win *w2 = make_win(B + 5);
  win_hash_insert(B + 4, w1);
  win_hash_insert(B + 5, w2);
  win_hash_remove(B + 4);
  // force a probe chain through the deleted slot
  for (Window id = B + 6; id < B + 6 + 32; id++) {
    win_hash_insert(id, make_win(id));
  }
  assert(find_win(B + 5) == w2);
  for (Window id = B + 6; id < B + 6 + 32; id++) {
    assert(find_win(id) != NULL);
  }
  free(w1);
  free(w2);
  printf("test_remove_then_reuse_slot passed.\n");
}

static void test_many_windows_and_growth(void) {
  // Far beyond the old fixed 256-slot table: exercises rehashing/growth.
  // Use adversarial ids that all collide in low bits.
  const unsigned int n = 1000;
  win **wins = calloc(n, sizeof(win *));
  assert(wins != NULL);
  Window *ids = calloc(n, sizeof(Window));
  assert(ids != NULL);

  for (unsigned int i = 0; i < n; i++) {
    ids[i] = B + (Window)(i * 64); // every id shares low 6 bits
    wins[i] = make_win(ids[i]);
    win_hash_insert(ids[i], wins[i]);
  }
  for (unsigned int i = 0; i < n; i++) {
    assert(find_win(ids[i]) == wins[i]);
  }
  // remove every other one, then verify the rest remain reachable
  for (unsigned int i = 0; i < n; i += 2) {
    win_hash_remove(ids[i]);
  }
  for (unsigned int i = 1; i < n; i += 2) {
    assert(find_win(ids[i]) == wins[i]);
  }
  for (unsigned int i = 0; i < n; i += 2) {
    assert(find_win(ids[i]) == NULL);
  }
  for (unsigned int i = 0; i < n; i++) {
    free(wins[i]);
  }
  free(wins);
  free(ids);
  printf("test_many_windows_and_growth passed.\n");
}

int main(void) {
  printf("=== TEST WIN_HASH ===\n");
  test_empty();
  test_insert_find_remove();
  test_destroyed_is_invisible();
  test_reinsert_same_id();
  test_remove_then_reuse_slot();
  test_many_windows_and_growth();
  printf("=== ALL WIN_HASH TESTS PASSED ===\n");
  return 0;
}