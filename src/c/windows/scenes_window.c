#include "scenes_window.h"
#include "../comm.h"
#include "../model.h"

static Window *s_window;
static MenuLayer *s_menu;

static uint16_t get_num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  int count = model_scene_count();
  return count > 0 ? count : 1;  // 1 = "Loading…" placeholder
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *cb) {
  Scene *s = model_scene_at(idx->row);
  if (!s) {
    menu_cell_basic_draw(ctx, cell, "Loading scenes…", NULL, NULL);
    return;
  }
  menu_cell_basic_draw(ctx, cell, s->name, NULL, NULL);
}

static void select_click(MenuLayer *menu, MenuIndex *idx, void *cb) {
  Scene *s = model_scene_at(idx->row);
  if (s) {
    comm_apply_scene(s->id);
    vibes_short_pulse();
    window_stack_pop(true);  // back to the room screen
  }
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = get_num_rows,
    .draw_row = draw_row,
    .select_click = select_click,
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

void scenes_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = window_load,
      .unload = window_unload,
    });
  }
  window_stack_push(s_window, true);
}

void scenes_window_reload(void) {
  if (s_menu) {
    menu_layer_reload_data(s_menu);
  }
}
