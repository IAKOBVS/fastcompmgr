#!/usr/bin/env bash
set -e

echo "=== STARTING INTEGRATION TESTS ==="

# 1. Test Usage / Help Output
echo "Testing CLI --help..."
HELP_OUTPUT=$(./fastcompmgr --help 2>&1 || true)
if echo "$HELP_OUTPUT" | grep -q "usage: ./fastcompmgr"; then
    echo "CLI --help test passed."
else
    echo "CLI --help test failed."
    exit 1
fi

echo "Testing CLI -h..."
HELP_OUTPUT_SHORT=$(./fastcompmgr -h 2>&1 || true)
if echo "$HELP_OUTPUT_SHORT" | grep -q "usage: ./fastcompmgr"; then
    echo "CLI -h test passed."
else
    echo "CLI -h test failed."
    exit 1
fi

# 2. Test invalid options
echo "Testing CLI invalid option..."
INVALID_OUTPUT=$(./fastcompmgr --invalid-option 2>&1 || true)
if echo "$INVALID_OUTPUT" | grep -q "usage: ./fastcompmgr"; then
    echo "CLI invalid option test passed."
else
    echo "CLI invalid option test failed."
    exit 1
fi

# 3. Test X11 Virtual Server Integration with Xvfb
if ! command -v Xvfb >/dev/null 2>&1; then
    echo "Xvfb is not installed. Skipping live X11 integration test."
    echo "=== INTEGRATION TESTS COMPLETED (SKIPPED XVFB) ==="
    exit 0
fi

echo "Testing live X11 integration with Xvfb..."
XVFB_DISPLAY=":99"
Xvfb $XVFB_DISPLAY -screen 0 1024x768x24 > /dev/null 2>&1 &
XVFB_PID=$!

cleanup() {
    echo "Cleaning up Xvfb (PID: $XVFB_PID)..."
    kill $XVFB_PID 2>/dev/null || true
}
trap cleanup EXIT

# Wait briefly for Xvfb to start
sleep 1

# Start fastcompmgr on virtual display
./fastcompmgr -d $XVFB_DISPLAY -c -C &
COMP_PID=$!

sleep 1

if kill -0 $COMP_PID 2>/dev/null; then
    echo "fastcompmgr started successfully on $XVFB_DISPLAY."
else
    echo "fastcompmgr failed to start on $XVFB_DISPLAY."
    exit 1
fi

# Test duplicate compositor detection
echo "Testing duplicate compositor manager detection..."
DUP_OUTPUT=$(./fastcompmgr -d $XVFB_DISPLAY -c -C 2>&1 || true)
if echo "$DUP_OUTPUT" | grep -q "Another composite manager is already running"; then
    echo "Duplicate compositor detection test passed."
else
    echo "Duplicate compositor detection test failed. Output: $DUP_OUTPUT"
    kill $COMP_PID 2>/dev/null || true
    exit 1
fi

# Stop fastcompmgr
kill $COMP_PID 2>/dev/null || true
wait $COMP_PID 2>/dev/null || true

echo "=== ALL INTEGRATION TESTS PASSED ==="
