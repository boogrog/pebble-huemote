#include "rooms_window.h"
#include "room_window.h"
#include "../comm.h"
#include "../model.h"

static Window *s_window;
static MenuLayer *s_menu;

// Section layout when the bridge is READY.
#define SECTION_QUICK 0
#define SECTION_ROOMS 1

static bool is_ready(void) {
  return model_bridge_status() == BRIDGE_READY;
}

// ---- MenuLayer callbacks --------------------------------------------------

static uint16_t get_num_sections(MenuLayer *menu, void *ctx) {
  return is_ready() ? 2 : 1;
}

static uint16_t get_num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  if (!is_ready()) {
    return 1;  // status / pairing row
  }
  if (section == SECTION_QUICK) {
    return 1;  // "All Off"
  }
  int count = model_room_count();
  return count > 0 ? count : 1;  // 1 = "No rooms found" placeholder
}

static void draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *cb) {
  if (!is_ready()) {
    return;
  }
  menu_cell_basic_header_draw(ctx, cell, section == SECTION_QUICK ? "Quick" : "Rooms");
}

static int16_t get_header_height(MenuLayer *menu, uint16_t section, void *ctx) {
  return is_ready() ? MENU_CELL_BASIC_HEADER_HEIGHT : 0;
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *cb) {
  if (!is_ready()) {
    const char *msg;
    switch (model_bridge_status()) {
      case BRIDGE_PAIRING: msg = "Press the link button on your bridge"; break;
      case BRIDGE_ERROR:   msg = "Bridge unreachable — tap to retry"; break;
      default:             msg = "Tap to pair your Hue Bridge"; break;
    }
    menu_cell_basic_draw(ctx, cell, "Huemote", msg, NULL);
    return;
  }

  if (idx->section == SECTION_QUICK) {
    menu_cell_basic_draw(ctx, cell, "All Lights Off", NULL, NULL);
    return;
  }

  Room *r = model_room_at(idx->row);
  if (!r) {
    menu_cell_basic_draw(ctx, cell, "No rooms found", "Tap to refresh", NULL);
    return;
  }

  static char subtitle[16];
  if (r->on) {
    int pct = (r->bri * 100) / 254;
    snprintf(subtitle, sizeof(subtitle), "On · %d%%", pct);
  } else {
    strncpy(subtitle, "Off", sizeof(subtitle));
  }
  menu_cell_basic_draw(ctx, cell, r->name, subtitle, NULL);
}

static void select_click(MenuLayer *menu, MenuIndex *idx, void *cb) {
  if (!is_ready()) {
    comm_start_pairing();
    return;
  }
  if (idx->section == SECTION_QUICK) {
    comm_all_off();
    vibes_short_pulse();
    return;
  }
  Room *r = model_room_at(idx->row);
  if (r) {
    room_window_push(idx->row);
  } else {
    comm_request_rooms();
  }
}

// Long-press on a room row toggles it without drilling in.
static void select_long_click(MenuLayer *menu, MenuIndex *idx, void *cb) {
  if (!is_ready() || idx->section != SECTION_ROOMS) {
    return;
  }
  Room *r = model_room_at(idx->row);
  if (r) {
    comm_toggle_room(r->id);
    r->on = !r->on;  // optimistic; JS will resync on next room push
    vibes_short_pulse();
    menu_layer_reload_data(menu);
  }
}

// ---- Window lifecycle -----------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_sections = get_num_sections,
    .get_num_rows = get_num_rows,
    .get_header_height = get_header_height,
    .draw_header = draw_header,
    .draw_row = draw_row,
    .select_click = select_click,
    .select_long_click = select_long_click,
  });
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void rooms_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = window_load,
      .unload = window_unload,
    });
  }
  window_stack_push(s_window, true);
}

void rooms_window_reload(void) {
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
}
