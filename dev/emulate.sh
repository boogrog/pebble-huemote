#!/usr/bin/env bash
# Build, run, and drive a Pebble app on the emulator headlessly via Docker.
# Works for any Pebble project — run it from the project root (the dir with
# package.json/appinfo.json), or set PEBBLE_PROJECT to point at one.
#
#   emulate.sh start          # boot a persistent headless emulator container
#   emulate.sh install        # build + install the app, keep JS (pypkjs) attached
#   emulate.sh run            # start + install in one go
#   emulate.sh gui            # boot WITH a visible watch window (needs X11) + install
#   emulate.sh shot <file>    # screenshot -> screenshots/<file>
#   emulate.sh btn <name> [n] # press back|up|select|down (n times)
#   emulate.sh hold <name>    # long-press (~0.75s) back|up|select|down
#   emulate.sh logs           # tail the JS (pkjs) log
#   emulate.sh stop           # remove this project's emulator container
#
# Env overrides:
#   PEBBLE_PROJECT=/path/to/app   (default: $PWD)
#   PEBBLE_PLATFORM=emery         (default: emery — Pebble Time 2; or basalt/chalk)
#   PEBBLE_IMAGE=rebble/pebble-sdk
#
# Why each non-obvious thing is here:
#  - SDL_VIDEODRIVER=dummy: headless qemu, else `install` hangs on "Waiting for
#    the firmware to boot" forever waiting for a display that doesn't exist.
#  - `nohup pebble install ... --logs &`: pypkjs (the JS phone sim) only answers
#    AppMessages while a --logs session is attached, so we leave one running.
#  - docker exec -i: required so heredoc scripts reach `python -`'s stdin;
#    without it stdin is empty and the button helper silently no-ops.
#  - button websocket: opcode 0x0b, protocol 8 (QemuButton); Back=1 Up=2
#    Select=4 Down=8. pypkjs's port is RANDOM per run — read it from
#    /proc/*/cmdline (--port), never hardcode.

set -euo pipefail

PROJECT="${PEBBLE_PROJECT:-$PWD}"
cd "$PROJECT"

IMAGE="${PEBBLE_IMAGE:-rebble/pebble-sdk}"
PLATFORM="${PEBBLE_PLATFORM:-emery}"
# Per-project container name so multiple apps can emulate at once.
SLUG=$(basename "$PROJECT" | tr '[:upper:]' '[:lower:]' | tr -c 'a-z0-9_-' '-')
NAME="pebble-emu-${SLUG}"
PEBBLE=/opt/pebble-sdk-4.5-linux64/bin/pebble
PY=/opt/pebble-sdk-4.5-linux64/.env/bin/python
DEX="docker exec -i -e SDL_VIDEODRIVER=dummy -e SDL_AUDIODRIVER=dummy $NAME"

build_in_container() {
  docker run --rm -v "$PROJECT:/app" -w /app "$IMAGE" pebble build
}

boot_app() {
  $DEX bash -c "pkill -f pypkjs 2>/dev/null; pkill -f 'emulator $PLATFORM' 2>/dev/null; true"
  $DEX bash -c "cd /app && nohup $PEBBLE install --emulator $PLATFORM --logs > /tmp/sess.log 2>&1 & \
                sleep 18; grep -aiE 'ready|installed|JS failed|error' /tmp/sess.log | head -5 || true"
}

pypkjs_port() {
  $DEX bash -c "for d in /proc/[0-9]*; do tr '\0' ' ' < \$d/cmdline 2>/dev/null; echo; done \
                | sed -n 's/.*pypkjs.*--port \([0-9]\+\).*/\1/p' | head -1"
}

press() { # $1=name $2=count $3=hold_seconds
  local port; port=$(pypkjs_port)
  [ -n "$port" ] || { echo "could not find pypkjs port (is the emulator running?)"; exit 1; }
  $DEX "$PY" - "$1" "$2" "$3" "$port" <<'PYEOF'
import sys, time, websocket
BTN = {"back":1, "up":2, "select":4, "down":8}
name, count, hold, port = sys.argv[1], int(sys.argv[2]), float(sys.argv[3]), sys.argv[4]
state = BTN[name]
ws = websocket.create_connection("ws://127.0.0.1:%s/" % port, timeout=5)
for _ in range(count):
    ws.send_binary(bytearray([0x0b, 0x08, state])); time.sleep(hold)
    ws.send_binary(bytearray([0x0b, 0x08, 0x00]));  time.sleep(0.25)
ws.close(); print("%s %s x%d" % ("held" if hold > 0.4 else "pressed", name, count))
PYEOF
}

case "${1:-}" in
  start)
    docker rm -f "$NAME" >/dev/null 2>&1 || true
    docker run -d --name "$NAME" -v "$PROJECT:/app" -w /app "$IMAGE" sleep infinity >/dev/null
    echo "emulator container '$NAME' up"
    ;;

  install)
    build_in_container >/dev/null
    boot_app
    ;;

  run)
    docker rm -f "$NAME" >/dev/null 2>&1 || true
    docker run -d --name "$NAME" -v "$PROJECT:/app" -w /app "$IMAGE" sleep infinity >/dev/null
    build_in_container >/dev/null
    boot_app
    ;;

  gui)
    [ -n "${DISPLAY:-}" ] || { echo "No \$DISPLAY — GUI mode needs an X server (use 'run' for headless)."; exit 1; }
    xhost +local:docker >/dev/null 2>&1 || echo "warning: 'xhost' missing; window may be blocked by X access control"
    docker rm -f "$NAME" >/dev/null 2>&1 || true
    docker run -d --name "$NAME" \
      -e DISPLAY="$DISPLAY" -v /tmp/.X11-unix:/tmp/.X11-unix \
      -v "$PROJECT:/app" -w /app "$IMAGE" sleep infinity >/dev/null
    build_in_container >/dev/null
    echo "launching emulator window (DISPLAY=$DISPLAY)…"
    # NOTE: no dummy SDL driver here so qemu opens a real window.
    docker exec "$NAME" bash -c "cd /app && nohup $PEBBLE install --emulator $PLATFORM --logs > /tmp/sess.log 2>&1 & \
                                 sleep 18; grep -aiE 'ready|installed|JS failed|error' /tmp/sess.log | head -5 || true"
    echo "window open. Drive it: emulate.sh btn down  (or click the watch)"
    ;;

  shot)
    mkdir -p screenshots
    $DEX bash -c "cd /app && $PEBBLE screenshot --emulator $PLATFORM screenshots/${2:?usage: shot <file>} --no-open" 2>&1 | grep -i saved
    ;;

  btn)  press "${2:?usage: btn <back|up|select|down> [count]}" "${3:-1}" 0.10 ;;
  hold) press "${2:?usage: hold <back|up|select|down>}" 1 0.75 ;;

  logs) $DEX bash -c "tail -f /tmp/sess.log" ;;
  stop) docker rm -f "$NAME" >/dev/null 2>&1 && echo "stopped $NAME" ;;
  *)    sed -n '2,33p' "$0" ;;
esac
