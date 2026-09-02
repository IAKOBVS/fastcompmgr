#!/bin/sh
# Integration test: run the real fastcompmgr binary under Xvfb and hammer it
# with a plain X client, verifying the compositor survives and keeps the
# _NET_WM_CM_S<n> selection. Skips (exit 0) if xvfb-run is unavailable.
set -u

command -v xvfb-run >/dev/null 2>&1 || {
  echo "test_integration: SKIP (xvfb-run not available)"
  exit 0
}
[ -x ./fastcompmgr ] && [ -x ./test_helper_win ] || {
  echo "test_integration: SKIP (missing ./fastcompmgr or ./test_helper_win; run 'make' and 'make test_helper_win' first)"
  exit 0
}

log=$(mktemp)
composer_pid=

cleanup() {
  [ -n "$composer_pid" ] && kill "$composer_pid" 2>/dev/null
  rm -f "$log"
}
trap cleanup EXIT

# Set up the isolated display with a fixed size, then:
#  1. start the compositor
#  2. wait for it to register
#  3. hammer it with window traffic via test_helper_win (checks selection)
#  4. verify the process is still alive
xvfb-run -a -s "-screen 0 1280x800x24" sh -c "
  set -e
  cd '$(pwd)'
  ./fastcompmgr 2>'$log' &
  composer_pid=\$!
  sleep 1
  kill -0 \$composer_pid || { echo 'test_integration: FAIL: fastcompmgr died during startup' >&2; exit 1; }
  ./test_helper_win || exit 1
  kill -0 \$composer_pid || { echo 'test_integration: FAIL: fastcompmgr crashed during test_helper_win' >&2; exit 1; }
  echo 'test_integration: PASS (compositor survived and kept the selection)'
  kill \$composer_pid 2>/dev/null
  wait \$composer_pid 2>/dev/null || true
" || {
  echo "--- fastcompmgr stderr (tail) ---"
  tail -n 20 "$log" 2>/dev/null || true
  echo "test_integration: FAIL"
  exit 1
}