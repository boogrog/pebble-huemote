#include "room_window.h"
#include "scenes_window.h"
#include "../comm.h"
#include "../model.h"

#define BRI_STEP 25
#define BRI_MIN  1
#define BRI_MAX  254

static Window *s_window;
static Layer  *s_canvas;
static int     s_room_index = -1;

static Room *current_room(void) {
  return model_room_at(s_room_index);
}

// ---- Rendering ------------------------------------------------------------

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  Room *r = current_room();

  graphics_context_set_text_color(ctx, GColorBlack);

  const char *name = r ? r->name : "Room";
  graphics_draw_text(ctx, name, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     GRect(4, 8, b.size.w - 8, 34),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Status line.
  static char status[16];
  int pct = 0;
  bool on = r ? r->on : false;
  if (r && r->on) {
    pct = (r->bri * 100) / 254;
    snprintf(status, sizeof(status), "On · %d%%", pct);
  } else {
    strncpy(status, "Off", sizeof(status));
  }
  graphics_draw_text(ctx, status, fonts_get_system_font(FONT_KEY_GOTHIC_24),
                     GRect(4, 44, b.size.w - 8, 28),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  // Brightness bar.
  int bar_w = b.size.w - 32;
  GRect track = GRect(16, 80, bar_w, 14);
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_round_rect(ctx, track, 4);
  if (on && pct > 0) {
    int fill_w = (bar_w * pct) / 100;
    GRect fill = GRect(16, 80, fill_w, 14);
#if defined(PBL_COLOR)
    graphics_context_set_fill_color(ctx, GColorChromeYellow);
#else
    graphics_context_set_fill_color(ctx, GColorBlack);
#endif
    graphics_fill_rect(ctx, fill, 4, GCornersAll);
  }

  // Button hints.
  graphics_draw_text(ctx, "Brighter",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(0, 104, b.size.w, 22),
                     GTextOverflowModeFill, GTextAlignmentRight, NULL);
  graphics_draw_text(ctx, on ? "Turn off" : "Turn on",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(0, 126, b.size.w, 22),
                     GTextOverflowModeFill, GTextAlignmentRight, NULL);
  graphics_draw_text(ctx, "Dimmer",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(0, 148, b.size.w, 22),
                     GTextOverflowModeFill, GTextAlignmentRight, NULL);
}

// ---- Click handlers -------------------------------------------------------

static void set_bri(int new_bri) {
  Room *r = current_room();
  if (!r) {
    return;
  }
  if (new_bri < BRI_MIN) new_bri = BRI_MIN;
  if (new_bri > BRI_MAX) new_bri = BRI_MAX;
  r->bri = new_bri;
  r->on = true;  // adjusting brightness implies on
  comm_set_bri(r->id, new_bri);
  layer_mark_dirty(s_canvas);
}

static void up_click(ClickRecognizerRef rec, void *ctx) {
  Room *r = current_room();
  if (r) {
    set_bri(r->on ? r->bri + BRI_STEP : BRI_STEP);
  }
}

static void down_click(ClickRecognizerRef rec, void *ctx) {
  Room *r = current_room();
  if (r) {
    set_bri(r->bri - BRI_STEP);
  }
}

static void select_click(ClickRecognizerRef rec, void *ctx) {
  Room *r = current_room();
  if (r) {
    r->on = !r->on;
    comm_toggle_room(r->id);
    vibes_short_pulse();
    layer_mark_dirty(s_canvas);
  }
}

static void select_long_click(ClickRecognizerRef rec, void *ctx) {
  Room *r = current_room();
  if (r) {
    comm_request_scenes(r->id);
    scenes_window_push();
  }
}

static void click_config(void *ctx) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 150, up_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, select_long_click, NULL);
}

// ---- Window lifecycle -----------------------------------------------------

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);
  window_set_click_config_provider(window, click_config);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
  window_destroy(s_window);
  s_window = NULL;
  s_room_index = -1;
}

void room_window_push(int room_index) {
  s_room_index = room_index;
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = window_load,
      .unload = window_unload,
    });
  }
  window_stack_push(s_window, true);
}

void room_window_update(void) {
  if (s_canvas) {
    layer_mark_dirty(s_canvas);
  }
}
