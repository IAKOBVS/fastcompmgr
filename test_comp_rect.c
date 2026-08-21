#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "comp_rect.h"

void test_comp_rect_containment() {
  printf("Running test_comp_rect_containment...\n");
  CompRect r1 = {.x1 = 0, .y1 = 0, .x2 = 100, .y2 = 100, .w = 100, .h = 100};
  CompRect r2 = {.x1 = 10, .y1 = 10, .x2 = 50, .y2 = 50, .w = 40, .h = 40};

  // r1 contains r2, so paint is NOT needed for r2
  bool needed = rect_paint_needed(&r1, &r2);
  assert(needed == false);

  printf("test_comp_rect_containment passed.\n");
}

void test_comp_rect_no_intersection() {
  printf("Running test_comp_rect_no_intersection...\n");
  CompRect ignore = {.x1 = 0, .y1 = 0, .x2 = 50, .y2 = 50, .w = 50, .h = 50};
  CompRect reg = {.x1 = 100, .y1 = 100, .x2 = 200, .y2 = 200, .w = 100, .h = 100};

  bool needed = rect_paint_needed(&ignore, &reg);
  assert(needed == true);
  // Larger rect (reg: 100x100 = 10000 vs ignore: 50x50 = 2500) should become the new ignore rect
  assert(ignore.x1 == 100 && ignore.y1 == 100 && ignore.x2 == 200 && ignore.y2 == 200);

  printf("test_comp_rect_no_intersection passed.\n");
}

void test_comp_rect_intersection() {
  printf("Running test_comp_rect_intersection...\n");
  CompRect ignore = {.x1 = 0, .y1 = 0, .x2 = 50, .y2 = 50, .w = 50, .h = 50};
  CompRect reg = {.x1 = 25, .y1 = 25, .x2 = 125, .y2 = 125, .w = 100, .h = 100};

  bool needed = rect_paint_needed(&ignore, &reg);
  assert(needed == true);

  // Reg area = 10000 > ignore area (2500), so ignore rect should become reg.
  assert(ignore.x1 == 25 && ignore.y1 == 25 && ignore.x2 == 125 && ignore.y2 == 125);

  printf("test_comp_rect_intersection passed.\n");
}

void test_comp_rect_intersection_area() {
  printf("Running test_comp_rect_intersection_area...\n");
  // Test case where intersection rect has larger area than ignore rect
  // ignore: 10x10 at (0,0), area = 100
  // reg: 100x100 at (5,5), area = 10000
  CompRect ig = {.x1 = 0, .y1 = 0, .x2 = 10, .y2 = 10, .w = 10, .h = 10};
  CompRect rg = {.x1 = 5, .y1 = 5, .x2 = 105, .y2 = 105, .w = 100, .h = 100};
  bool needed = rect_paint_needed(&ig, &rg);
  assert(needed);
  assert(ig.x1 == 5 && ig.y1 == 5 && ig.x2 == 105 && ig.y2 == 105);

  printf("test_comp_rect_intersection_area passed.\n");
}

int main() {
  printf("=== STARTING COMP_RECT TESTS ===\n");
  test_comp_rect_containment();
  test_comp_rect_no_intersection();
  test_comp_rect_intersection();
  test_comp_rect_intersection_area();
  printf("=== COMP_RECT TESTS PASSED ===\n");
  return 0;
}
