# Huemote

Control Philips Hue lights from a Pebble (Time 2 / color platforms) over the
**local** Hue Bridge v2 API — no cloud round-trip.

## How it works

A Pebble watch has no network of its own, so Huemote is split in two:

```
Watch (C app)  ──AppMessage/BT──►  Phone (PebbleKit JS)  ──HTTP──►  Hue Bridge v2  ──Zigbee──►  lights
```

- **Watch (`src/c/`)** — the UI. Renders rooms/scenes, sends commands, stays
  optimistic so the screen reacts instantly.
- **Phone (`src/pkjs/index.js`)** — does all networking against the Hue local
  REST API v1. Must be on the same WiFi as the bridge.

The two halves share a small AppMessage protocol; the command/status values
live in `src/c/comm.h` and are mirrored at the top of `index.js`. The key names
themselves are declared in `package.json` (`messageKeys`).

## Screens

- **Rooms** (`rooms_window`) — home list. Sections: *Quick* (All Lights Off)
  and *Rooms*. Long-press a room to toggle it without drilling in. When unpaired
  it shows a single "tap to pair" row.
- **Room control** (`room_window`) — Up/Down ramp brightness, Select toggles,
  **hold Select** opens that room's scenes.
- **Scenes** (`scenes_window`) — pick a scene; it applies and pops back.

## Pairing

1. Open the app → tap the pairing row (or use Settings in the Pebble app).
2. Press the **link button** on the Hue Bridge within ~30s.
3. The app stores the bridge IP + API key on the phone (`localStorage`) and
   loads your rooms. The IP can also be set manually in Settings if
   auto-discovery (via `discovery.meethue.com`) doesn't find it.

## MVP scope

Implemented: pairing, room list, on/off, brightness, scenes, All Off.
Not yet: color/temperature picker, favorites screen, Quick Launch binding,
Hue API v2 event stream for live state push.

## Build & run

Requires the Pebble SDK (`pebble` tool + ARM toolchain) — not bundled here.

```sh
pebble build
pebble install --emulator emery     # or: --phone <ip> for a real watch
pebble logs                          # watch + JS console output
```

`emery` is the Pebble Time 2 platform; `basalt`/`chalk` are also targeted.
