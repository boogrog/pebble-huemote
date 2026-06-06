#pragma once
#include <pebble.h>

// ---------------------------------------------------------------------------
// AppMessage protocol shared between the watch (C) and the phone (PebbleKit JS).
//
// The named message keys themselves are declared in package.json and surface
// as MESSAGE_KEY_<name>. This header defines the *values* that travel inside
// those keys (the command and status enums) plus the watch-side command API.
// ---------------------------------------------------------------------------

// Watch -> JS: value carried in MESSAGE_KEY_cmd.
typedef enum {
  CMD_REQUEST_ROOMS  = 1,  // ask JS to (re)send the room list
  CMD_TOGGLE_ROOM    = 2,  // + room_id
  CMD_SET_BRI        = 3,  // + room_id, bri (0-254)
  CMD_APPLY_SCENE    = 4,  // + scene_id
  CMD_ALL_OFF        = 5,  // turn every light off
  CMD_START_PAIRING  = 6,  // begin link-button pairing
  CMD_REQUEST_SCENES = 7,  // + room_id: send scenes for that room
} HueCommand;

// JS -> Watch: value carried in MESSAGE_KEY_bridge_status.
typedef enum {
  BRIDGE_NOT_PAIRED = 0,
  BRIDGE_PAIRING    = 1,  // waiting for the user to press the link button
  BRIDGE_READY      = 2,
  BRIDGE_ERROR      = 3,  // unreachable / failed
} BridgeStatus;

// JS -> Watch: value carried in MESSAGE_KEY_item_kind while streaming a list.
typedef enum {
  ITEM_ROOM  = 0,
  ITEM_SCENE = 1,
} ItemKind;

// ---- Watch-side command API (implemented in huemote.c) --------------------
void comm_init(void);
void comm_request_rooms(void);
void comm_request_scenes(const char *room_id);
void comm_toggle_room(const char *room_id);
void comm_set_bri(const char *room_id, int bri);
void comm_apply_scene(const char *scene_id);
void comm_all_off(void);
void comm_start_pairing(void);
