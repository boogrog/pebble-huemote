#include "model.h"

static BridgeStatus s_bridge_status = BRIDGE_NOT_PAIRED;

static Room  s_rooms[MAX_ROOMS];
static int   s_room_count = 0;

static Scene s_scenes[MAX_SCENES];
static int   s_scene_count = 0;

void model_init(void) {
  s_bridge_status = BRIDGE_NOT_PAIRED;
  s_room_count = 0;
  s_scene_count = 0;
}

// ---- Bridge status --------------------------------------------------------

BridgeStatus model_bridge_status(void) {
  return s_bridge_status;
}

void model_set_bridge_status(BridgeStatus status) {
  s_bridge_status = status;
}

// ---- Rooms ----------------------------------------------------------------

int model_room_count(void) {
  return s_room_count;
}

Room *model_room_at(int index) {
  if (index < 0 || index >= s_room_count) {
    return NULL;
  }
  return &s_rooms[index];
}

Room *model_room_by_id(const char *id) {
  for (int i = 0; i < s_room_count; i++) {
    if (strncmp(s_rooms[i].id, id, HUE_ID_LEN) == 0) {
      return &s_rooms[i];
    }
  }
  return NULL;
}

void model_clear_rooms(void) {
  s_room_count = 0;
}

void model_set_room(int index, const char *id, const char *name, bool on, uint8_t bri) {
  if (index < 0 || index >= MAX_ROOMS) {
    return;
  }
  Room *r = &s_rooms[index];
  strncpy(r->id, id, HUE_ID_LEN - 1);
  r->id[HUE_ID_LEN - 1] = '\0';
  strncpy(r->name, name, HUE_NAME_LEN - 1);
  r->name[HUE_NAME_LEN - 1] = '\0';
  r->on = on;
  r->bri = bri;
  if (index + 1 > s_room_count) {
    s_room_count = index + 1;
  }
}

// ---- Scenes ---------------------------------------------------------------

int model_scene_count(void) {
  return s_scene_count;
}

Scene *model_scene_at(int index) {
  if (index < 0 || index >= s_scene_count) {
    return NULL;
  }
  return &s_scenes[index];
}

void model_clear_scenes(void) {
  s_scene_count = 0;
}

void model_set_scene(int index, const char *id, const char *name, const char *group) {
  if (index < 0 || index >= MAX_SCENES) {
    return;
  }
  Scene *s = &s_scenes[index];
  strncpy(s->id, id, HUE_ID_LEN - 1);
  s->id[HUE_ID_LEN - 1] = '\0';
  strncpy(s->name, name, HUE_NAME_LEN - 1);
  s->name[HUE_NAME_LEN - 1] = '\0';
  strncpy(s->group, group, sizeof(s->group) - 1);
  s->group[sizeof(s->group) - 1] = '\0';
  if (index + 1 > s_scene_count) {
    s_scene_count = index + 1;
  }
}
