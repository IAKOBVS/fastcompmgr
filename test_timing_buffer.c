#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>

#include "cm-util.h"
#include "ringbuffer.h"

#define False 0
#define True 1

// We can include cm-event.c directly to test its static variables or functions,
// or declare them. Including cm-event.c is a great way to test its static structures.
#include "cm-event.c"

// Tests for get_time_in_milliseconds
void test_timing_basic() {
  printf("Running test_timing_basic...\n");

  // Save original _program_start_secs
  time_t orig_start = _program_start_secs;

  // Case 1: normal initialization
  _program_start_secs = time(NULL) - 10; // 10 seconds ago
  int64_t t1 = get_time_in_milliseconds();
  assert(t1 >= 10000 && t1 < 12000);

  // Restore
  _program_start_secs = orig_start;
  printf("test_timing_basic passed.\n");
}

void test_timing_overflow() {
  printf("Running test_timing_overflow...\n");

  // Save original _program_start_secs
  time_t orig_start = _program_start_secs;

  // Case 2: Simulate no program start initialization (starts at 0)
  // This causes get_time_in_milliseconds() to return values larger than INT_MAX
  _program_start_secs = 0;
  int64_t t_large = get_time_in_milliseconds();
  printf("Simulated current time in ms (no init): %ld\n", t_large);
  assert(t_large > 2147483647LL); // Must be > INT_MAX

  // Let's ensure that if we do time arithmetic with these large values, it is overflow-safe.
  int64_t fade_delta = 10;
  int64_t fade_time = t_large + fade_delta;
  int64_t now = t_large;

  // Safety test for fade_timeout equivalent logic
  int64_t delta = fade_time - now;
  assert(delta == 10);
  assert(delta >= 0);
  if (delta > INT_MAX) delta = INT_MAX;
  int timeout = (int)delta;
  assert(timeout == 10);

  // Safety test for run_fades equivalent logic
  assert(fade_time - now > 0); // shouldn't run fade yet
  now = t_large + 15;
  assert(fade_time - now <= 0); // should run fade now
  int64_t steps = 1 + (now - fade_time) / fade_delta;
  assert(steps == 1);

  // Safety test for check_paint equivalent logic
  int64_t EVERY_MILISEC = 2;
  int64_t configure_time = t_large + EVERY_MILISEC;
  now = t_large;
  int64_t paint_delta = now - configure_time;
  assert(paint_delta < EVERY_MILISEC); // should return early

  now = t_large + 3;
  paint_delta = now - configure_time;
  assert(paint_delta >= 1); // should paint

  // Restore
  _program_start_secs = orig_start;
  printf("test_timing_overflow passed.\n");
}

void test_normalize_d() {
  printf("Running test_normalize_d...\n");
  assert(normalize_d(1.5) == 1.0);
  assert(normalize_d(-0.5) == 0.0);
  assert(normalize_d(0.75) == 0.75);
  printf("test_normalize_d passed.\n");
}

// Tests for ring buffer
ringBuffer_typedef(int, TestRingBuf);

void test_ringbuffer_basic() {
  printf("Running test_ringbuffer_basic...\n");

  TestRingBuf buf;
  bufferInit(buf, 4, int);

  assert(isBufferEmpty(&buf));
  assert(!isBufferFull(&buf));

  // Write elements
  bufferWrite(&buf, 10);
  bufferWrite(&buf, 20);
  bufferWrite(&buf, 30);

  assert(!isBufferEmpty(&buf));
  assert(!isBufferFull(&buf));

  int val;
  bufferRead(&buf, val);
  assert(val == 10);
  bufferRead(&buf, val);
  assert(val == 20);

  bufferWrite(&buf, 40);
  bufferWrite(&buf, 50);
  // Buffer has size 4, so it can hold up to 4 elements.
  // We read 10 and 20, remaining: 30, 40, 50 (3 elements).
  assert(!isBufferFull(&buf));

  bufferWrite(&buf, 60);
  // Now contains: 30, 40, 50, 60 (4 elements). It should be full.
  assert(isBufferFull(&buf));

  bufferDestroy(&buf);
  printf("test_ringbuffer_basic passed.\n");
}

void test_ringbuffer_expansion_and_wrap_around() {
  printf("Running test_ringbuffer_expansion_and_wrap_around...\n");

  TestRingBuf buf;
  bufferInit(buf, 4, int);

  // Test bufferIncrease when buffer is empty
  bufferIncrease(&buf, 8);
  assert(buf.size == 8);
  assert(isBufferEmpty(&buf));

  // 1. Fill part of it
  bufferWrite(&buf, 1);
  bufferWrite(&buf, 2);
  bufferWrite(&buf, 3);

  // Test bufferIncrease when non-empty and start < end
  bufferIncrease(&buf, 16);
  assert(buf.size == 16);
  assert(buf.start == 0 && buf.end == 3);

  // 2. Read some to move start
  int val;
  bufferRead(&buf, val);
  assert(val == 1);
  bufferRead(&buf, val);
  assert(val == 2);

  // 3. Write more to cause wrap around (start > end)
  for (int i = 4; i <= 17; i++) {
    bufferWrite(&buf, i);
  }

  assert(buf.start > buf.end || isBufferFull(&buf));

  // 4. Trigger bufferIncrease to test the circular wrap-around copy logic
  bufferIncrease(&buf, buf.size * 2);

  // Read elements to verify order is preserved
  int expected = 3;
  while (!isBufferEmpty(&buf)) {
    bufferRead(&buf, val);
    assert(val == expected);
    expected++;
  }
  assert(expected == 18);

  bufferDestroy(&buf);
  printf("test_ringbuffer_expansion_and_wrap_around passed.\n");
}

void test_event_ignore() {
  printf("Running test_event_ignore...\n");

  // Initialize the event ignore ringbuffer
  bool init_ok = event_init();
  assert(init_ok);

  assert(isBufferEmpty(p_ignore_ringbuf));

  // Ignore some sequences
  set_ignore(NULL, 100);
  set_ignore(NULL, 101);
  set_ignore(NULL, 105);

  assert(!isBufferEmpty(p_ignore_ringbuf));

  // should_ignore returns true for the first pending ignore match after discarding older ones.
  // When should_ignore is called, it calls discard_ignore.
  // sequence 100: should be ignored.
  assert(should_ignore(NULL, 100));

  // Since sequence 100 was at the peek of the buffer, should_ignore returned True,
  // but let's check what happens when we ask about sequence 102.
  // sequence 102: should NOT be ignored because we only have 101 and 105 left.
  // Calling should_ignore with 102 will discard 101 (since 102 > 101) and stop at 105.
  assert(!should_ignore(NULL, 102));

  // Now, 101 has been discarded. Peek of buffer should be 105.
  assert(should_ignore(NULL, 105));

  // Clean up
  bufferDestroy(p_ignore_ringbuf);

  printf("test_event_ignore passed.\n");
}

int main() {
  printf("=== STARTING TEST SUITE ===\n");
  test_timing_basic();
  test_timing_overflow();
  test_normalize_d();
  test_ringbuffer_basic();
  test_ringbuffer_expansion_and_wrap_around();
  test_event_ignore();
  printf("=== ALL TESTS PASSED SUCCESSFULLY ===\n");
  return 0;
}
