#pragma once
#include <pebble.h>
#include "comm.h"

// In-memory model of what the bridge told us. Populated from AppMessage and
// read by the windows. Kept deliberately small for the watch's RAM budget.

#define MAX_ROOMS  24
#define MAX_SCENES 48

#define HUE_ID_LEN   40
#define HUE_NAME_LEN 32

typedef struct {
  char id[HUE_ID_LEN];     // Hue group id ("1", "2", ...)
  char name[HUE_NAME_LEN];
  bool on;
  uint8_t bri;             // 0-254
} Room;

typedef struct {
  char id[HUE_ID_LEN];     // Hue scene id (long opaque string)
  char name[HUE_NAME_LEN];
  char group[8];           // group id this scene belongs to
} Scene;

void model_init(void);

// Bridge connection status.
BridgeStatus model_bridge_status(void);
void model_set_bridge_status(BridgeStatus status);

// Rooms.
int  model_room_count(void);
Room *model_room_at(int index);
Room *model_room_by_id(const char *id);
void model_clear_rooms(void);
void model_set_room(int index, const char *id, const char *name, bool on, uint8_t bri);

// Scenes (the set currently loaded, for whichever room was last requested).
int   model_scene_count(void);
Scene *model_scene_at(int index);
void  model_clear_scenes(void);
void  model_set_scene(int index, const char *id, const char *name, const char *group);
