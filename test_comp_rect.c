// Unit tests for the occlusion-culling rectangle logic (comp_rect.c).
// These directly cover the x2/y2 intersection regression: two overlapping
// 100x100 rects must yield a 50x50 intersection, never negative sizes.
#include <assert.h>
#include <stdio.h>

#include "comp_rect.h"

#define R(a1, b1, a2, b2) \
  ((CompRect){.x1 = (a1), .y1 = (b1), .x2 = (a2), .y2 = (b2), \
              .w = (short)((a2) - (a1)), .h = (short)((b2) - (b1))})

static int rect_eq(CompRect *a, CompRect *b) {
  return a->x1 == b->x1 && a->y1 == b->y1 &&
         a->x2 == b->x2 && a->y2 == b->y2 &&
         a->w == b->w && a->h == b->h;
}

static void test_rect_intersect_basic(void) {
  // Regression: both min-branches must end in x2/y2. The old code used
  // x1/y1 there, producing w=-50, h=-50 for this pair.
  CompRect a = R(0, 0, 100, 100);
  CompRect b = R(50, 50, 150, 150);
  CompRect out;
  assert(rect_intersect(&a, &b, &out) == true);
  assert(rect_eq(&out, &R(50, 50, 100, 100)));
  assert(out.w == 50 && out.h == 50);
  assert(out.w > 0 && out.h > 0);
  printf("test_rect_intersect_basic passed.\n");
}

static void test_rect_intersect_symmetric(void) {
  CompRect a = R(0, 0, 100, 100);
  CompRect b = R(50, 50, 150, 150);
  CompRect out1, out2;
  rect_intersect(&a, &b, &out1);
  rect_intersect(&b, &a, &out2);
  assert(rect_eq(&out1, &out2));
  printf("test_rect_intersect_symmetric passed.\n");
}

static void test_rect_intersect_partial(void) {
  CompRect a = R(0, 0, 100, 100);
  CompRect b = R(-50, 50, 50, 200);
  CompRect out;
  assert(rect_intersect(&a, &b, &out));
  assert(rect_eq(&out, &R(0, 50, 50, 100)));
  printf("test_rect_intersect_partial passed.\n");
}

static void test_rect_intersect_no_overlap(void) {
  CompRect a = R(0, 0, 10, 10);
  CompRect b = R(20, 20, 30, 30);
  CompRect out = R(99, 99, 99, 99);
  assert(rect_intersect(&a, &b, &out) == false);
  assert(rect_eq(&out, &R(99, 99, 99, 99))); // unchanged
  printf("test_rect_intersect_no_overlap passed.\n");
}

static void test_rect_intersect_contained(void) {
  CompRect big = R(-10, -10, 200, 200);
  CompRect small = R(5, 5, 15, 15);
  CompRect out;
  assert(rect_intersect(&big, &small, &out) == true);
  assert(rect_eq(&out, &small));
  printf("test_rect_intersect_contained passed.\n");
}

static void test_rect_intersect_touching(void) {
  CompRect a = R(0, 0, 10, 10);
  CompRect b = R(10, 0, 20, 10);
  CompRect out;
  assert(rect_intersect(&a, &b, &out) == true);
  assert(out.w == 0 && out.h == 10); // edge contact -> zero width
  printf("test_rect_intersect_touching passed.\n");
}

static void test_paint_needed_contained(void) {
  CompRect ignore = R(0, 0, 100, 100);
  CompRect reg = R(10, 10, 90, 90);
  assert(rect_paint_needed(&ignore, &reg) == false);
  assert(rect_eq(&ignore, &R(0, 0, 100, 100))); // unchanged
  printf("test_paint_needed_contained passed.\n");
}

static void test_paint_needed_equal_rects(void) {
  CompRect ignore = R(0, 0, 100, 100);
  CompRect reg = R(0, 0, 100, 100);
  assert(rect_paint_needed(&ignore, &reg) == false);
  printf("test_paint_needed_equal_rects passed.\n");
}

static void test_paint_needed_zero_ignore_adopts_reg(void) {
  CompRect ignore = R(0, 0, 0, 0);
  CompRect reg = R(100, 100, 150, 150);
  assert(rect_paint_needed(&ignore, &reg) == true);
  assert(rect_eq(&ignore, &reg));
  printf("test_paint_needed_zero_ignore_adopts_reg passed.\n");
}

static void test_paint_needed_disjoint_keep_larger(void) {
  CompRect ignore = R(0, 0, 100, 100);
  CompRect reg = R(200, 200, 250, 250);
  assert(rect_paint_needed(&ignore, &reg) == true);
  assert(rect_eq(&ignore, &R(0, 0, 100, 100))); // reg smaller, unchanged
  printf("test_paint_needed_disjoint_keep_larger passed.\n");
}

static void test_paint_needed_disjoint_adopt_bigger(void) {
  CompRect ignore = R(0, 0, 10, 10);
  CompRect reg = R(100, 100, 250, 250);
  assert(rect_paint_needed(&ignore, &reg) == true);
  assert(rect_eq(&ignore, &reg));
  printf("test_paint_needed_disjoint_adopt_bigger passed.\n");
}

static void test_paint_needed_overlap_pick_bigger(void) {
  // ignore 100x100 == reg area, intersection 50x50 smaller: ignore unchanged.
  CompRect ignore = R(0, 0, 100, 100);
  CompRect reg = R(50, 50, 150, 150);
  assert(rect_paint_needed(&ignore, &reg) == true);
  assert(rect_eq(&ignore, &R(0, 0, 100, 100)));
  printf("test_paint_needed_overlap_pick_bigger passed.\n");
}

static void test_paint_needed_overlap_reg_wins(void) {
  CompRect ignore = R(0, 0, 10, 10);
  CompRect reg = R(5, 5, 55, 55); // 50x50 > 10x10, so ignore becomes reg
  assert(rect_paint_needed(&ignore, &reg) == true);
  assert(rect_eq(&ignore, &reg));
  printf("test_paint_needed_overlap_reg_wins passed.\n");
}

static void test_paint_needed_touch_edge(void) {
  CompRect ignore = R(0, 0, 10, 10);
  CompRect reg = R(10, 0, 20, 10); // zero-area intersection, no crash/garbage
  assert(rect_paint_needed(&ignore, &reg) == true);
  assert(rect_eq(&ignore, &R(0, 0, 10, 10)));
  printf("test_paint_needed_touch_edge passed.\n");
}

static void test_paint_needed_accumulation(void) {
  // rect_paint_needed tracks a SINGLE rect: the largest single window/intersect
  // seen so far (strict > tie-break keeps the first on equal area). It is NOT a
  // union accumulator. Trace a bottom-to-top paint:
  CompRect ignore = R(0, 0, 0, 0);
  assert(rect_paint_needed(&ignore, &R(0, 0, 100, 100)) == true);
  assert(rect_eq(&ignore, &R(0, 0, 100, 100)));
  // same area (100x100) -> keep existing (strict >), ignore unchanged
  assert(rect_paint_needed(&ignore, &R(100, 0, 200, 100)) == true);
  assert(rect_eq(&ignore, &R(0, 0, 100, 100)));
  // bigger (200x100) -> adopted
  assert(rect_paint_needed(&ignore, &R(0, 100, 200, 200)) == true);
  assert(rect_eq(&ignore, &R(0, 100, 200, 200)));
  // equal area intersection, reg not contained -> still painted
  assert(rect_paint_needed(&ignore, &R(50, 50, 150, 150)) == true);
  assert(rect_eq(&ignore, &R(0, 100, 200, 200))); // keep larger
  // a smaller rect fully inside is skipped
  assert(rect_paint_needed(&ignore, &R(50, 120, 100, 160)) == false);
  printf("test_paint_needed_accumulation passed.\n");
}

int main(void) {
  printf("=== TEST COMP_RECT ===\n");
  test_rect_intersect_basic();
  test_rect_intersect_symmetric();
  test_rect_intersect_partial();
  test_rect_intersect_no_overlap();
  test_rect_intersect_contained();
  test_rect_intersect_touching();
  test_paint_needed_contained();
  test_paint_needed_equal_rects();
  test_paint_needed_zero_ignore_adopts_reg();
  test_paint_needed_disjoint_keep_larger();
  test_paint_needed_disjoint_adopt_bigger();
  test_paint_needed_overlap_pick_bigger();
  test_paint_needed_overlap_reg_wins();
  test_paint_needed_touch_edge();
  test_paint_needed_accumulation();
  printf("=== ALL COMP_RECT TESTS PASSED ===\n");
  return 0;
}