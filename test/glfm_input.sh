#!/bin/bash
# glfwm - host input regression for the GLFM backend (M2).
#
# Launches hello_glfwm with an extended frame budget, drives it with
# synthetic mouse/keyboard/scroll events (test/glfm_driver.m) and asserts
# the app logs the expected GLFW events, including the blocking clipboard
# read/write round-trip (system clipboard via pbcopy/pbpaste).
#
# Requires Accessibility permission (TCC) for the calling terminal:
# CGEventPost injects into the system event stream. Without the permission
# the events silently never arrive and the assertions fail.
#
# usage: glfm_input.sh [build-dir]   (default: build/glfm-host)
set -u

DIR="${1:-build/glfm-host}"
LOG="$DIR/hello_input.log"
DRIVER="$DIR/glfm_driver"
HELLO="$DIR/hello_glfwm"
OWNER=hello_glfwm

fail() {
  echo "input: FAIL - $1"
  [ -f "$LOG" ] && tail -30 "$LOG"
  exit 1
}

rm -f "$LOG"
GLFM_HELLO_FRAMES=6000 "$HELLO" > "$LOG" 2>&1 &
pid=$!
sleep 2
kill -0 $pid 2>/dev/null || fail "hello_glfwm exited immediately"

"$DRIVER" "$OWNER" input || { kill $pid 2>/dev/null; fail "driver input phase"; }
printf 'from-driver' | pbcopy
"$DRIVER" "$OWNER" key-v || { kill $pid 2>/dev/null; fail "driver key-v phase"; }
sleep 1
"$DRIVER" "$OWNER" key-c || { kill $pid 2>/dev/null; fail "driver key-c phase"; }
sleep 1
clip="$(pbpaste)"
[ "$clip" = "glfwm-set" ] || { kill $pid 2>/dev/null; fail "pbpaste expected 'glfwm-set', got '$clip'"; }
"$DRIVER" "$OWNER" esc || { kill $pid 2>/dev/null; fail "driver esc phase"; }

# Wait for hello_glfwm to exit on ESC (max ~5s), else kill and fail
dead=0
for i in $(seq 1 50); do
  kill -0 $pid 2>/dev/null || { dead=1; break; }
  sleep 0.1
done
if [ $dead -eq 0 ]; then
  kill $pid 2>/dev/null
  fail "hello_glfwm did not exit on ESC (events may not be arriving: check Accessibility permission)"
fi
wait $pid

# Assert the expected GLFW events in the app log
for pattern in \
  "mouse button: 0 press" \
  "mouse button: 0 release" \
  "mouse button: 1 press" \
  "cursor pos:" \
  "scroll:" \
  "key: key=86 " \
  "key: key=67 " \
  "char: 104" \
  "char: 105" \
  "clipboard: from-driver" \
  "clipboard set: glfwm-set"; do
  grep -q "$pattern" "$LOG" || fail "missing '$pattern' in log"
done
echo "input: PASS"
