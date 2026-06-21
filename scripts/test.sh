#!/usr/bin/env bash
# Boot the kernel headless in QEMU, capture COM1 serial output, and pass
# iff the expected marker appears within the timeout.
#
# Usage: scripts/test.sh <EXPECTED_MARKER> [kernel_path] [timeout_seconds]
set -u

EXPECT="${1:?usage: test.sh <EXPECTED_MARKER> [kernel_path] [timeout_seconds]}"
KERNEL="${2:-build/kernel.bin}"
TIMEOUT="${3:-20}"

if [[ ! -f "$KERNEL" ]]; then
  echo "FAIL: $KERNEL not built (run 'make' first)"; exit 2
fi

LOG="$(mktemp)"
qemu-system-i386 -kernel "$KERNEL" \
  -serial "file:$LOG" \
  -display none -no-reboot \
  -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
  >/dev/null 2>&1 &
QPID=$!

# Poll up to TIMEOUT seconds for QEMU to exit on its own (isa-debug-exit).
for ((i = 0; i < TIMEOUT * 10; i++)); do
  kill -0 "$QPID" 2>/dev/null || break
  sleep 0.1
done
# If still running (e.g. kernel hung), kill it.
if kill -0 "$QPID" 2>/dev/null; then
  kill "$QPID" 2>/dev/null
fi
wait "$QPID" 2>/dev/null

echo "----- serial output -----"
cat "$LOG"
echo "-------------------------"

if grep -q "$EXPECT" "$LOG"; then
  echo "PASS: found '$EXPECT'"
  rm -f "$LOG"; exit 0
else
  echo "FAIL: '$EXPECT' not found in serial output"
  rm -f "$LOG"; exit 1
fi
