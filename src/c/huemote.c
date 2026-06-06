#include <pebble.h>
#include "comm.h"
#include "model.h"
#include "windows/rooms_window.h"
#include "windows/room_window.h"
#include "windows/scenes_window.h"

// ---------------------------------------------------------------------------
// App entry point + the watch half of the AppMessage protocol.
// ---------------------------------------------------------------------------

// ---- Outgoing commands (comm.h API) ---------------------------------------

static void send_cmd(HueCommand cmd, const char *room_id,
                     const char *scene_id, int bri) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "outbox busy, dropping cmd %d", cmd);
    return;
  }
  int cmd_val = (int)cmd;
  dict_write_int(iter, MESSAGE_KEY_cmd, &cmd_val, sizeof(int), true);
  if (room_id) {
    dict_write_cstring(iter, MESSAGE_KEY_room_id, room_id);
  }
  if (scene_id) {
    dict_write_cstring(iter, MESSAGE_KEY_scene_id, scene_id);
  }
  if (bri >= 0) {
    dict_write_int(iter, MESSAGE_KEY_bri, &bri, sizeof(int), true);
  }
  app_message_outbox_send();
}

void comm_request_rooms(void)                     { send_cmd(CMD_REQUEST_ROOMS, NULL, NULL, -1); }
void comm_request_scenes(const char *room_id)     { send_cmd(CMD_REQUEST_SCENES, room_id, NULL, -1); }
void comm_toggle_room(const char *room_id)        { send_cmd(CMD_TOGGLE_ROOM, room_id, NULL, -1); }
void comm_set_bri(const char *room_id, int bri)   { send_cmd(CMD_SET_BRI, room_id, NULL, bri); }
void comm_apply_scene(const char *scene_id)       { send_cmd(CMD_APPLY_SCENE, NULL, scene_id, -1); }
void comm_all_off(void)                           { send_cmd(CMD_ALL_OFF, NULL, NULL, -1); }
void comm_start_pairing(void)                     { send_cmd(CMD_START_PAIRING, NULL, NULL, -1); }

// ---- Incoming messages ----------------------------------------------------

static void handle_item(DictionaryIterator *iter) {
  Tuple *kind_t  = dict_find(iter, MESSAGE_KEY_item_kind);
  Tuple *index_t = dict_find(iter, MESSAGE_KEY_item_index);
  Tuple *count_t = dict_find(iter, MESSAGE_KEY_item_count);
  Tuple *name_t  = dict_find(iter, MESSAGE_KEY_item_name);
  Tuple *id_t    = dict_find(iter, MESSAGE_KEY_item_id);
  if (!kind_t || !index_t || !count_t || !name_t || !id_t) {
    return;
  }

  int kind  = kind_t->value->int32;
  int index = index_t->value->int32;
  int count = count_t->value->int32;
  const char *name = name_t->value->cstring;
  const char *id   = id_t->value->cstring;
  bool last = (index >= count - 1);

  if (kind == ITEM_ROOM) {
    if (index == 0) {
      model_clear_rooms();
    }
    Tuple *on_t  = dict_find(iter, MESSAGE_KEY_item_on);
    Tuple *bri_t = dict_find(iter, MESSAGE_KEY_item_bri);
    bool on = on_t && on_t->value->int32;
    uint8_t bri = bri_t ? (uint8_t)bri_t->value->int32 : 0;
    model_set_room(index, id, name, on, bri);
    if (last) {
      rooms_window_reload();
      room_window_update();
    }
  } else if (kind == ITEM_SCENE) {
    if (index == 0) {
      model_clear_scenes();
    }
    Tuple *group_t = dict_find(iter, MESSAGE_KEY_item_group);
    const char *group = group_t ? group_t->value->cstring : "0";
    model_set_scene(index, id, name, group);
    if (last) {
      scenes_window_reload();
    }
  }
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *status_t = dict_find(iter, MESSAGE_KEY_bridge_status);
  if (status_t) {
    BridgeStatus prev = model_bridge_status();
    BridgeStatus now = (BridgeStatus)status_t->value->int32;
    model_set_bridge_status(now);
    rooms_window_reload();
    // Just became usable: pull the room list.
    if (now == BRIDGE_READY && prev != BRIDGE_READY) {
      comm_request_rooms();
    }
  }

  if (dict_find(iter, MESSAGE_KEY_item_kind)) {
    handle_item(iter);
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "inbox dropped: %d", reason);
}

static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "outbox failed: %d", reason);
}

void comm_init(void) {
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(256, 128);
}

// ---- Boot -----------------------------------------------------------------

static void init(void) {
  model_init();
  comm_init();
  rooms_window_push();
}

static void deinit(void) {
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
