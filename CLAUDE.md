# Huemote

Pebble (Time 2 / color) app to control Philips Hue lights over the **local** Hue
Bridge v2 API. Shared Pebble conventions auto-load from `../CLAUDE.md` — this file
covers only what's specific to Huemote. User-facing overview is in `README.md`.

## Layout

- `src/c/huemote.c` — entry point + the watch half of AppMessage (send commands,
  receive status/list items). Implements the `comm_*()` API.
- `src/c/comm.h` — protocol **values**: `HueCommand`, `BridgeStatus`, `ItemKind`
  enums + the `comm_*()` prototypes. Mirror any change here in `index.js`.
- `src/c/model.{h,c}` — in-RAM rooms/scenes/status, fixed arrays.
- `src/c/windows/` — `rooms_window` (home: Quick/All-Off + room list),
  `room_window` (toggle + brightness ramp + hold-Select→scenes),
  `scenes_window` (apply a scene). Each exposes `_push()` and `_reload()`.
- `src/pkjs/index.js` — phone side: discovery, link-button pairing, all HTTP to the
  bridge (local API v1), and the AppMessage relay.

## AppMessage protocol (keep both sides in sync)

Keys are declared in `package.json` → `messageKeys`. Values:

- **Watch → JS**, key `cmd` (`HueCommand`): `REQUEST_ROOMS=1`, `TOGGLE_ROOM=2`
  (+`room_id`), `SET_BRI=3` (+`room_id`,`bri` 0-254), `APPLY_SCENE=4` (+`scene_id`),
  `ALL_OFF=5`, `START_PAIRING=6`, `REQUEST_SCENES=7` (+`room_id`).
- **JS → Watch**, key `bridge_status` (`BridgeStatus`): `NOT_PAIRED=0`, `PAIRING=1`,
  `READY=2`, `ERROR=3`. On `READY` the watch auto-sends `REQUEST_ROOMS`.
- **JS → Watch list streaming**: one message per item with `item_kind` (0=room,
  1=scene), `item_index`, `item_count`, `item_name`, `item_id`, plus `item_on`/
  `item_bri` (rooms) or `item_group` (scenes). Watch clears the list at index 0 and
  refreshes UI at the last index. JS serializes sends through its outbox queue.

The watch updates **optimistically**; JS re-fetches rooms after each mutation to resync.

## Hue specifics

- Local API **v1** over plain HTTP: `http://<bridge-ip>/api/<user>/...`. Groups of
  type `Room`/`Zone` are the rooms; `groups/0/action` is "all". Scenes recalled via
  `groups/0/action {scene: id}`.
- Pairing: POST `/api {devicetype}` → error type 101 until the link button is pressed
  (JS polls ~90s), then stores `{ip,user}` in `localStorage`. IP via
  `discovery.meethue.com` or manual entry in the settings page.

## Build / run / demo

- `dev/emulate.sh run` (headless) or `gui` (visible window); `shot`/`btn`/`hold` to
  drive it. See the `pebble-emulate` skill.
- `index.js` has a `DEMO` flag (ships **false**) that serves canned rooms/scenes so
  the full UI is navigable in the emulator without a bridge. Set it back to false
  before committing.

## Not yet (MVP boundary)

Color/temperature picker, a flat favorites screen, Quick-Launch "All Off" binding, and
the Hue API v2 event stream for live state push.
