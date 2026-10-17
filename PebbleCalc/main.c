#include <pebble.h>
#include <stdio.h>
#include <string.h>
#include "calc_engine.h"

#if defined(PBL_TOUCH) || defined(PBL_PLATFORM_EMERY)
void app_touch_navigation_enable(bool enable) __attribute__((weak));
void window_set_touch_bridge_disabled(Window *window, bool disabled) __attribute__((weak));
Recognizer *tap_recognizer_create(RecognizerEventCb event_cb, void *user_data) __attribute__((weak));
GPoint tap_recognizer_get_tap_point(const Recognizer *recognizer) __attribute__((weak));
void window_attach_recognizer(Window *window, Recognizer *recognizer) __attribute__((weak));
#endif

#define EXPR_MAX_LEN 128
#define GRID_ROWS 5
#define GRID_COLS 4
#define TOTAL_BUTTONS (GRID_ROWS * GRID_COLS)

#if defined(PBL_COLOR)
  #define ACCENT_ORANGE GColorWindsorTan
#else
  #define ACCENT_ORANGE GColorBlack
#endif

typedef enum {
  BTN_TYPE_DIGIT,
  BTN_TYPE_OP,
  BTN_TYPE_CLEAR,
  BTN_TYPE_DEL,
  BTN_TYPE_PAREN,
  BTN_TYPE_MINUS,
  BTN_TYPE_DOT,
  BTN_TYPE_TIP,
  BTN_TYPE_EVAL
} ButtonType;

typedef struct {
  const char *label;
  ButtonType type;
} ButtonDef;

static const ButtonDef s_buttons[TOTAL_BUTTONS] = {
  { "C",   BTN_TYPE_CLEAR },
  { "DEL", BTN_TYPE_DEL },
  { "()",  BTN_TYPE_PAREN },
  { "/",   BTN_TYPE_OP },
  { "7",   BTN_TYPE_DIGIT },
  { "8",   BTN_TYPE_DIGIT },
  { "9",   BTN_TYPE_DIGIT },
  { "x",   BTN_TYPE_OP },
  { "4",   BTN_TYPE_DIGIT },
  { "5",   BTN_TYPE_DIGIT },
  { "6",   BTN_TYPE_DIGIT },
  { "-",   BTN_TYPE_MINUS },
  { "1",   BTN_TYPE_DIGIT },
  { "2",   BTN_TYPE_DIGIT },
  { "3",   BTN_TYPE_DIGIT },
  { "+",   BTN_TYPE_OP },
  { "0",   BTN_TYPE_DIGIT },
  { ".",   BTN_TYPE_DOT },
  { "TIP", BTN_TYPE_TIP },
  { "=",   BTN_TYPE_EVAL }
};

static GColor get_button_bg_color(int index) {
#if defined(PBL_COLOR)
  switch (s_buttons[index].type) {
    case BTN_TYPE_DIGIT:
    case BTN_TYPE_DOT:
      return GColorWhite;
    default:
      return ACCENT_ORANGE;
  }
#else
  switch (s_buttons[index].type) {
    case BTN_TYPE_DIGIT:
    case BTN_TYPE_DOT:
      return GColorWhite;
    default:
      return GColorBlack;
  }
#endif
}

static GColor get_button_text_color(int index) {
#if defined(PBL_COLOR)
  switch (s_buttons[index].type) {
    case BTN_TYPE_DIGIT:
    case BTN_TYPE_DOT:
      return GColorBlack;
    default:
      return GColorWhite;
  }
#else
  switch (s_buttons[index].type) {
    case BTN_TYPE_DIGIT:
    case BTN_TYPE_DOT:
      return GColorBlack;
    default:
      return GColorWhite;
  }
#endif
}

static const uint32_t s_short_vibe_durations[] = { 45 };
static const VibePattern s_short_vibe_pattern = {
  .durations = s_short_vibe_durations,
  .num_segments = 1,
};

static void trigger_short_vibe(void) {
  vibes_enqueue_custom_pattern(s_short_vibe_pattern);
}

static Window *s_main_window;
static Layer *s_display_layer;
static Layer *s_grid_layer;

static char s_expr[EXPR_MAX_LEN] = "";
static int s_cursor_pos = 0;
static bool s_is_result_showing = false;
static int s_focused_button_index = 19;
static int s_pressed_button_index = -1;
static int s_display_scroll_x = 0;
static bool s_cursor_blink_on = true;
static AppTimer *s_blink_timer = NULL;
static AppTimer *s_press_timer = NULL;
static AppTimer *s_hold_repeat_timer = NULL;
static int s_hold_direction = 0;

static Window *s_tip_window;
static Layer *s_tip_layer;
static double s_tip_subtotal = 0.0;
static int s_tip_percent = 15;
static int s_split_count = 1;
static AppTimer *s_tip_hold_repeat_timer = NULL;
static int s_tip_hold_direction = 0;

static void open_tip_window(void);
static void main_click_config_provider(void *context);
static void tip_click_config_provider(void *context);

static void reset_blink_timer(void);

static void blink_timer_callback(void *data) {
  (void)data;
  s_cursor_blink_on = !s_cursor_blink_on;
  if (s_display_layer) {
    layer_mark_dirty(s_display_layer);
  }
  s_blink_timer = app_timer_register(600, blink_timer_callback, NULL);
}

static void reset_blink_timer(void) {
  s_cursor_blink_on = true;
  if (s_blink_timer) {
    app_timer_cancel(s_blink_timer);
  }
  s_blink_timer = app_timer_register(600, blink_timer_callback, NULL);
}

static void unpress_timer_callback(void *data) {
  (void)data;
  s_pressed_button_index = -1;
  s_press_timer = NULL;
  if (s_grid_layer) {
    layer_mark_dirty(s_grid_layer);
  }
}

static void trigger_button_visual_feedback(int index) {
  s_pressed_button_index = index;
  trigger_short_vibe();
  if (s_press_timer) {
    app_timer_cancel(s_press_timer);
  }
  s_press_timer = app_timer_register(120, unpress_timer_callback, NULL);
  if (s_grid_layer) {
    layer_mark_dirty(s_grid_layer);
  }
}

static int get_display_height(GRect bounds) {
  int row_h = bounds.size.h / 6;
  return bounds.size.h - (row_h * GRID_ROWS);
}

static GRect get_button_rect(GRect bounds, int index) {
  int disp_h = get_display_height(bounds);
  int grid_h = bounds.size.h - disp_h;
  int row_h = grid_h / GRID_ROWS;
  int col_w = bounds.size.w / GRID_COLS;

  int row = index / GRID_COLS;
  int col = index % GRID_COLS;

  int x = col * col_w;
  int y = disp_h + (row * row_h);
  int w = (col == GRID_COLS - 1) ? (bounds.size.w - x) : col_w;
  int h = (row == GRID_ROWS - 1) ? (bounds.size.h - y) : row_h;

  return GRect(x, y, w, h);
}

static int get_substr_width(const char *str, int count, GFont font) {
  if (count <= 0 || !str || *str == '\0') return 0;
  char temp[EXPR_MAX_LEN];
  if (count >= (int)sizeof(temp)) count = sizeof(temp) - 1;
  strncpy(temp, str, count);
  temp[count] = '\0';
  GSize size = graphics_text_layout_get_content_size(temp, font, GRect(0, 0, 1000, 40),
                                                    GTextOverflowModeTrailingEllipsis,
                                                    GTextAlignmentLeft);
  return size.w;
}

static void update_display_scroll(int disp_w, GFont font) {
  int cursor_x = 8 + get_substr_width(s_expr, s_cursor_pos, font);

  if (cursor_x - s_display_scroll_x > disp_w - 24) {
    s_display_scroll_x = cursor_x - (disp_w - 24);
  } else if (cursor_x - s_display_scroll_x < 12) {
    s_display_scroll_x = cursor_x - 12;
  }
  if (s_display_scroll_x < 0) {
    s_display_scroll_x = 0;
  }
}

static void handle_button_action(int index) {
  if (index < 0 || index >= TOTAL_BUTTONS) return;
  const ButtonDef *btn = &s_buttons[index];

  reset_blink_timer();

  switch (btn->type) {
    case BTN_TYPE_CLEAR: {
      s_expr[0] = '\0';
      s_cursor_pos = 0;
      s_is_result_showing = false;
      s_display_scroll_x = 0;
      break;
    }

    case BTN_TYPE_DEL: {
      if (s_is_result_showing) {
        s_expr[0] = '\0';
        s_cursor_pos = 0;
        s_is_result_showing = false;
        s_display_scroll_x = 0;
      } else {
        calc_backspace(s_expr, &s_cursor_pos);
      }
      break;
    }

    case BTN_TYPE_PAREN: {
      if (s_is_result_showing) {
        s_expr[0] = '\0';
        s_cursor_pos = 0;
        s_is_result_showing = false;
      }
      const char *bracket = calc_get_smart_bracket(s_expr, s_cursor_pos);
      calc_insert_str(s_expr, &s_cursor_pos, sizeof(s_expr), bracket);
      break;
    }

    case BTN_TYPE_OP: {
      if (s_is_result_showing) {
        s_is_result_showing = false;
      }
      calc_insert_str(s_expr, &s_cursor_pos, sizeof(s_expr), btn->label);
      break;
    }

    case BTN_TYPE_MINUS: {
      if (s_is_result_showing) {
        s_is_result_showing = false;
      }
      if (s_cursor_pos == 0) {
        calc_insert_str(s_expr, &s_cursor_pos, sizeof(s_expr), "-");
      } else {
        char prev = s_expr[s_cursor_pos - 1];
        if (prev == '+') {
          s_expr[s_cursor_pos - 1] = '-';
        } else if (prev == '-') {
          if (s_cursor_pos < 2 || s_expr[s_cursor_pos - 2] != '-') {
            calc_insert_str(s_expr, &s_cursor_pos, sizeof(s_expr), "-");
          }
        } else {
          calc_insert_str(s_expr, &s_cursor_pos, sizeof(s_expr), "-");
        }
      }
      break;
    }

    case BTN_TYPE_DIGIT: {
      if (s_is_result_showing) {
        s_expr[0] = '\0';
        s_cursor_pos = 0;
        s_is_result_showing = false;
        s_display_scroll_x = 0;
      }
      calc_insert_str(s_expr, &s_cursor_pos, sizeof(s_expr), btn->label);
      break;
    }

    case BTN_TYPE_DOT: {
      if (s_is_result_showing) {
        s_expr[0] = '\0';
        s_cursor_pos = 0;
        s_is_result_showing = false;
        s_display_scroll_x = 0;
      }
      bool has_dot = false;
      for (int i = s_cursor_pos - 1; i >= 0; i--) {
        char c = s_expr[i];
        if (c == '.') {
          has_dot = true;
          break;
        }
        if (c == '+' || c == '-' || c == 'x' || c == 'X' || c == '*' || c == '/' || c == '(' || c == ')') {
          break;
        }
      }
      if (!has_dot) {
        if (s_cursor_pos == 0 || s_expr[s_cursor_pos - 1] == '+' || s_expr[s_cursor_pos - 1] == '-' ||
            s_expr[s_cursor_pos - 1] == 'x' || s_expr[s_cursor_pos - 1] == '/' || s_expr[s_cursor_pos - 1] == '(') {
          calc_insert_str(s_expr, &s_cursor_pos, sizeof(s_expr), "0.");
        } else {
          calc_insert_str(s_expr, &s_cursor_pos, sizeof(s_expr), ".");
        }
      }
      break;
    }

    case BTN_TYPE_EVAL: {
      if (strlen(s_expr) == 0) break;
      double result = 0.0;
      CalcError err = calc_evaluate(s_expr, &result);
      if (err == CALC_OK) {
        calc_format_result(result, s_expr, sizeof(s_expr));
        s_cursor_pos = (int)strlen(s_expr);
        s_is_result_showing = true;
        s_display_scroll_x = 0;
      } else if (err == CALC_ERR_DIV_ZERO) {
        snprintf(s_expr, sizeof(s_expr), "Error: Div/0");
        s_cursor_pos = (int)strlen(s_expr);
        s_is_result_showing = true;
        s_display_scroll_x = 0;
      } else {
        snprintf(s_expr, sizeof(s_expr), "Error: Syntax");
        s_cursor_pos = (int)strlen(s_expr);
        s_is_result_showing = true;
        s_display_scroll_x = 0;
      }
      break;
    }

    case BTN_TYPE_TIP: {
      open_tip_window();
      break;
    }
  }

  if (s_display_layer) layer_mark_dirty(s_display_layer);
  if (s_grid_layer) layer_mark_dirty(s_grid_layer);
}

static void move_cursor_left(void) {
  if (s_cursor_pos > 0) {
    s_cursor_pos--;
    s_is_result_showing = false;
    reset_blink_timer();
    if (s_display_layer) layer_mark_dirty(s_display_layer);
  }
}

static void move_cursor_right(void) {
  int len = (int)strlen(s_expr);
  if (s_cursor_pos < len) {
    s_cursor_pos++;
    s_is_result_showing = false;
    reset_blink_timer();
    if (s_display_layer) layer_mark_dirty(s_display_layer);
  }
}

#if defined(PBL_TOUCH) || defined(PBL_PLATFORM_EMERY)
static void cursor_set_from_tap_x(int tap_x, GFont font) {
  int len = (int)strlen(s_expr);
  if (len == 0) {
    s_cursor_pos = 0;
    return;
  }

  int target_x = tap_x - 8 + s_display_scroll_x;
  int best_pos = 0;
  int min_diff = 999999;

  for (int i = 0; i <= len; i++) {
    int w = get_substr_width(s_expr, i, font);
    int diff = w - target_x;
    if (diff < 0) diff = -diff;
    if (diff < min_diff) {
      min_diff = diff;
      best_pos = i;
    }
  }

  s_cursor_pos = best_pos;
  s_is_result_showing = false;
  reset_blink_timer();
  if (s_display_layer) layer_mark_dirty(s_display_layer);
}
#endif

static void display_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));

  update_display_scroll(bounds.size.w, font);

  graphics_context_set_text_color(ctx, GColorBlack);
  GRect text_box = GRect(8 - s_display_scroll_x, (bounds.size.h - 26) / 2 - 2, 2000, 30);
  graphics_draw_text(ctx, s_expr, font, text_box, GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  if (s_cursor_blink_on) {
    int cursor_x = 8 + get_substr_width(s_expr, s_cursor_pos, font) - s_display_scroll_x;
    if (cursor_x >= 0 && cursor_x <= bounds.size.w) {
      graphics_context_set_fill_color(ctx, ACCENT_ORANGE);
      graphics_fill_rect(ctx, GRect(cursor_x, 6, 2, bounds.size.h - 12), 0, GCornerNone);
    }
  }
}

static void grid_layer_update_proc(Layer *layer, GContext *ctx) {
  (void)layer;
  GRect window_bounds = layer_get_bounds(window_get_root_layer(s_main_window));
  int disp_h = get_display_height(window_bounds);
  int grid_h = window_bounds.size.h - disp_h;
  int row_h = grid_h / GRID_ROWS;
  int col_w = window_bounds.size.w / GRID_COLS;

  GFont font_digit = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont font_action = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  for (int i = 0; i < TOTAL_BUTTONS; i++) {
    GRect btn_rect = get_button_rect(window_bounds, i);
    btn_rect.origin.y -= disp_h;

    GColor bg = get_button_bg_color(i);
    if (i == s_pressed_button_index) {
      bg = GColorBlack;
    }

    graphics_context_set_fill_color(ctx, bg);
    graphics_fill_rect(ctx, btn_rect, 0, GCornerNone);
  }

  graphics_context_set_stroke_color(ctx, GColorBlack);

  for (int c = 1; c < GRID_COLS; c++) {
    int x = c * col_w;
    graphics_draw_line(ctx, GPoint(x, 0), GPoint(x, grid_h));
  }

  for (int r = 1; r < GRID_ROWS; r++) {
    int y = r * row_h;
    graphics_draw_line(ctx, GPoint(0, y), GPoint(window_bounds.size.w, y));
  }

  graphics_draw_line(ctx, GPoint(0, grid_h - 1), GPoint(window_bounds.size.w, grid_h - 1));

  if (s_focused_button_index >= 0 && s_focused_button_index < TOTAL_BUTTONS) {
    GRect focus_rect = get_button_rect(window_bounds, s_focused_button_index);
    focus_rect.origin.y -= disp_h;

    bool is_white_btn = (s_buttons[s_focused_button_index].type == BTN_TYPE_DIGIT ||
                         s_buttons[s_focused_button_index].type == BTN_TYPE_DOT);
    GColor ring_color = is_white_btn ? GColorBlack : GColorWhite;
    graphics_context_set_stroke_color(ctx, ring_color);
    graphics_draw_rect(ctx, GRect(focus_rect.origin.x + 2, focus_rect.origin.y + 2,
                                  focus_rect.size.w - 4, focus_rect.size.h - 4));
  }

  for (int i = 0; i < TOTAL_BUTTONS; i++) {
    GRect btn_rect = get_button_rect(window_bounds, i);
    btn_rect.origin.y -= disp_h;

    const ButtonDef *btn = &s_buttons[i];
    GColor fg = get_button_text_color(i);
    if (i == s_pressed_button_index) {
      fg = GColorWhite;
    }

    GFont font_to_use = (btn->type == BTN_TYPE_DIGIT || btn->type == BTN_TYPE_OP ||
                         btn->type == BTN_TYPE_MINUS || btn->type == BTN_TYPE_EVAL ||
                         btn->type == BTN_TYPE_DOT)
                          ? font_digit : font_action;

    int text_h = (font_to_use == font_digit) ? 28 : 22;
    int y_offset = (btn_rect.size.h - text_h) / 2;
    if (font_to_use == font_digit) {
      y_offset -= 2;
    } else {
      y_offset -= 1;
    }

    graphics_context_set_text_color(ctx, fg);
    GRect text_box = GRect(btn_rect.origin.x, btn_rect.origin.y + y_offset, btn_rect.size.w, text_h + 4);
    graphics_draw_text(ctx, btn->label, font_to_use, text_box,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

#if defined(PBL_TOUCH) || defined(PBL_PLATFORM_EMERY)
static void main_tap_handler(const Recognizer *recognizer, RecognizerEvent event) {
  if (event != RecognizerEvent_Completed || !tap_recognizer_get_tap_point) return;

  GPoint pt = tap_recognizer_get_tap_point(recognizer);
  GRect bounds = layer_get_bounds(window_get_root_layer(s_main_window));
  int disp_h = get_display_height(bounds);

  if (pt.y < disp_h) {
    GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    cursor_set_from_tap_x(pt.x, font);
  } else {
    int grid_h = bounds.size.h - disp_h;
    int row_h = grid_h / GRID_ROWS;
    int col_w = bounds.size.w / GRID_COLS;

    int row = (pt.y - disp_h) / row_h;
    int col = pt.x / col_w;

    if (row >= 0 && row < GRID_ROWS && col >= 0 && col < GRID_COLS) {
      int index = row * GRID_COLS + col;
      s_focused_button_index = index;
      trigger_button_visual_feedback(index);
      handle_button_action(index);
    }
  }
}
#endif

static void hold_repeat_timer_callback(void *data) {
  (void)data;
  if (s_hold_direction == -1) {
    move_cursor_left();
    s_hold_repeat_timer = app_timer_register(150, hold_repeat_timer_callback, NULL);
  } else if (s_hold_direction == 1) {
    move_cursor_right();
    s_hold_repeat_timer = app_timer_register(150, hold_repeat_timer_callback, NULL);
  }
}

static void main_up_single_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  s_focused_button_index--;
  if (s_focused_button_index < 0) {
    s_focused_button_index = TOTAL_BUTTONS - 1;
  }
  if (s_grid_layer) layer_mark_dirty(s_grid_layer);
}

static void main_down_single_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  s_focused_button_index++;
  if (s_focused_button_index >= TOTAL_BUTTONS) {
    s_focused_button_index = 0;
  }
  if (s_grid_layer) layer_mark_dirty(s_grid_layer);
}

static void main_select_single_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  trigger_button_visual_feedback(s_focused_button_index);
  handle_button_action(s_focused_button_index);
}

static void main_up_long_click_down(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  s_hold_direction = -1;
  move_cursor_left();
  if (s_hold_repeat_timer) app_timer_cancel(s_hold_repeat_timer);
  s_hold_repeat_timer = app_timer_register(300, hold_repeat_timer_callback, NULL);
}

static void main_up_long_click_up(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  s_hold_direction = 0;
  if (s_hold_repeat_timer) {
    app_timer_cancel(s_hold_repeat_timer);
    s_hold_repeat_timer = NULL;
  }
}

static void main_down_long_click_down(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  s_hold_direction = 1;
  move_cursor_right();
  if (s_hold_repeat_timer) app_timer_cancel(s_hold_repeat_timer);
  s_hold_repeat_timer = app_timer_register(300, hold_repeat_timer_callback, NULL);
}

static void main_down_long_click_up(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  s_hold_direction = 0;
  if (s_hold_repeat_timer) {
    app_timer_cancel(s_hold_repeat_timer);
    s_hold_repeat_timer = NULL;
  }
}

static void main_click_config_provider(void *context) {
  (void)context;
  window_single_click_subscribe(BUTTON_ID_UP, main_up_single_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, main_down_single_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, main_select_single_click);

  window_long_click_subscribe(BUTTON_ID_UP, 400, main_up_long_click_down, main_up_long_click_up);
  window_long_click_subscribe(BUTTON_ID_DOWN, 400, main_down_long_click_down, main_down_long_click_up);
}

static void main_window_load(Window *window) {
  Layer *root_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root_layer);

  int disp_h = get_display_height(bounds);

  s_display_layer = layer_create(GRect(0, 0, bounds.size.w, disp_h));
  layer_set_update_proc(s_display_layer, display_layer_update_proc);
  layer_add_child(root_layer, s_display_layer);

  s_grid_layer = layer_create(GRect(0, disp_h, bounds.size.w, bounds.size.h - disp_h));
  layer_set_update_proc(s_grid_layer, grid_layer_update_proc);
  layer_add_child(root_layer, s_grid_layer);

  reset_blink_timer();

#if defined(PBL_TOUCH) || defined(PBL_PLATFORM_EMERY)
  if (window_set_touch_bridge_disabled) {
    window_set_touch_bridge_disabled(window, true);
  }
  if (tap_recognizer_create && window_attach_recognizer) {
    Recognizer *tap = tap_recognizer_create(main_tap_handler, NULL);
    if (tap) {
      window_attach_recognizer(window, tap);
    }
  }
#endif
}

static void main_window_unload(Window *window) {
  (void)window;
  if (s_blink_timer) {
    app_timer_cancel(s_blink_timer);
    s_blink_timer = NULL;
  }
  if (s_press_timer) {
    app_timer_cancel(s_press_timer);
    s_press_timer = NULL;
  }
  if (s_hold_repeat_timer) {
    app_timer_cancel(s_hold_repeat_timer);
    s_hold_repeat_timer = NULL;
  }

  layer_destroy(s_display_layer);
  layer_destroy(s_grid_layer);
  s_display_layer = NULL;
  s_grid_layer = NULL;
}

static void tip_hold_repeat_timer_callback(void *data) {
  (void)data;
  if (s_tip_hold_direction == 1) {
    if (s_split_count < 20) {
      s_split_count++;
      trigger_short_vibe();
      if (s_tip_layer) layer_mark_dirty(s_tip_layer);
    }
    s_tip_hold_repeat_timer = app_timer_register(350, tip_hold_repeat_timer_callback, NULL);
  } else if (s_tip_hold_direction == -1) {
    if (s_split_count > 1) {
      s_split_count--;
      trigger_short_vibe();
      if (s_tip_layer) layer_mark_dirty(s_tip_layer);
    }
    s_tip_hold_repeat_timer = app_timer_register(350, tip_hold_repeat_timer_callback, NULL);
  }
}

static void tip_up_single_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  if (s_tip_percent < 50) {
    s_tip_percent++;
    trigger_short_vibe();
    if (s_tip_layer) layer_mark_dirty(s_tip_layer);
  }
}

static void tip_down_single_click(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  if (s_tip_percent > 1) {
    s_tip_percent--;
    trigger_short_vibe();
    if (s_tip_layer) layer_mark_dirty(s_tip_layer);
  }
}

static void tip_up_long_down(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  s_tip_hold_direction = 1;
  if (s_split_count < 20) {
    s_split_count++;
    trigger_short_vibe();
    if (s_tip_layer) layer_mark_dirty(s_tip_layer);
  }
  if (s_tip_hold_repeat_timer) app_timer_cancel(s_tip_hold_repeat_timer);
  s_tip_hold_repeat_timer = app_timer_register(500, tip_hold_repeat_timer_callback, NULL);
}

static void tip_up_long_up(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  s_tip_hold_direction = 0;
  if (s_tip_hold_repeat_timer) {
    app_timer_cancel(s_tip_hold_repeat_timer);
    s_tip_hold_repeat_timer = NULL;
  }
}

static void tip_down_long_down(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  s_tip_hold_direction = -1;
  if (s_split_count > 1) {
    s_split_count--;
    trigger_short_vibe();
    if (s_tip_layer) layer_mark_dirty(s_tip_layer);
  }
  if (s_tip_hold_repeat_timer) app_timer_cancel(s_tip_hold_repeat_timer);
  s_tip_hold_repeat_timer = app_timer_register(500, tip_hold_repeat_timer_callback, NULL);
}

static void tip_down_long_up(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer; (void)context;
  s_tip_hold_direction = 0;
  if (s_tip_hold_repeat_timer) {
    app_timer_cancel(s_tip_hold_repeat_timer);
    s_tip_hold_repeat_timer = NULL;
  }
}

static void tip_click_config_provider(void *context) {
  (void)context;
  window_single_click_subscribe(BUTTON_ID_UP, tip_up_single_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, tip_down_single_click);

  window_long_click_subscribe(BUTTON_ID_UP, 400, tip_up_long_down, tip_up_long_up);
  window_long_click_subscribe(BUTTON_ID_DOWN, 400, tip_down_long_down, tip_down_long_up);
}

#if defined(PBL_TOUCH) || defined(PBL_PLATFORM_EMERY)
static void tip_tap_handler(const Recognizer *recognizer, RecognizerEvent event) {
  if (event != RecognizerEvent_Completed || !tap_recognizer_get_tap_point) return;

  GPoint pt = tap_recognizer_get_tap_point(recognizer);
  GRect bounds = layer_get_bounds(window_get_root_layer(s_tip_window));

  int r0_h = 44;
  int r1_h = 48;
  int r2_h = 44;
  int r3_h = 44;

  if (pt.y >= r0_h && pt.y < r0_h + r1_h) {
    int w = bounds.size.w;
    int third = w / 3;
    if (pt.x < third) {
      s_tip_percent = 10;
    } else if (pt.x < third * 2) {
      s_tip_percent = 15;
    } else {
      s_tip_percent = 20;
    }
    trigger_short_vibe();
    if (s_tip_layer) layer_mark_dirty(s_tip_layer);
  }
  else if (pt.y >= r0_h + r1_h + r2_h && pt.y < r0_h + r1_h + r2_h + r3_h) {
    if (pt.x < bounds.size.w / 2) {
      if (s_split_count > 1) {
        s_split_count--;
        trigger_short_vibe();
        if (s_tip_layer) layer_mark_dirty(s_tip_layer);
      }
    } else {
      if (s_split_count < 20) {
        s_split_count++;
        trigger_short_vibe();
        if (s_tip_layer) layer_mark_dirty(s_tip_layer);
      }
    }
  }
}
#endif

static void tip_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  int r0_h = 44;
  int r1_h = 48;
  int r2_h = 44;
  int r3_h = 44;
  int r4_h = bounds.size.h - (r0_h + r1_h + r2_h + r3_h);

  GFont font_uniform = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont font_preset = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  char buf[64];

  GRect r0 = GRect(0, 0, bounds.size.w, r0_h);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, r0, 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, r0_h - 1), GPoint(bounds.size.w, r0_h - 1));

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, "SUBTOTAL:", font_uniform, GRect(8, (r0_h - 26) / 2 - 2, 100, 26),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  char subtotal_buf[32];
  calc_format_currency(subtotal_buf, sizeof(subtotal_buf), s_tip_subtotal);
  graphics_draw_text(ctx, subtotal_buf, font_uniform, GRect(108, (r0_h - 26) / 2 - 2, bounds.size.w - 116, 26),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  int r1_y = r0_h;
  GRect r1 = GRect(0, r1_y, bounds.size.w, r1_h);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, r1, 0, GCornerNone);

  int presets[3] = { 10, 15, 20 };
  int margin = 8;
  int gap = 8;
  int btn_w = (bounds.size.w - (margin * 2) - (gap * 2)) / 3;

  for (int i = 0; i < 3; i++) {
    int bx = margin + i * (btn_w + gap);
    GRect b_rect = GRect(bx, r1_y + 6, btn_w, r1_h - 12);
    bool selected = (s_tip_percent == presets[i]);

    graphics_context_set_fill_color(ctx, selected ? ACCENT_ORANGE : GColorWhite);
    graphics_fill_rect(ctx, b_rect, 4, GCornersAll);

    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_round_rect(ctx, b_rect, 4);

    snprintf(buf, sizeof(buf), "%d%%", presets[i]);
    graphics_context_set_text_color(ctx, selected ? GColorWhite : GColorBlack);
    graphics_draw_text(ctx, buf, font_preset, GRect(b_rect.origin.x, b_rect.origin.y + (b_rect.size.h - 20) / 2 - 1, b_rect.size.w, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, r1_y + r1_h - 1), GPoint(bounds.size.w, r1_y + r1_h - 1));

  double tip_amt = s_tip_subtotal * (s_tip_percent / 100.0);
  double total_amt = s_tip_subtotal + tip_amt;

  int r2_y = r1_y + r1_h;
  GRect r2 = GRect(0, r2_y, bounds.size.w, r2_h);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, r2, 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, r2_y + r2_h - 1), GPoint(bounds.size.w, r2_y + r2_h - 1));

  snprintf(buf, sizeof(buf), "TIP (%d%%):", s_tip_percent);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, buf, font_uniform, GRect(8, r2_y + (r2_h - 26) / 2 - 2, 110, 26),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  char tip_cur_buf[32];
  calc_format_currency(tip_cur_buf, sizeof(tip_cur_buf), tip_amt);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, tip_cur_buf, font_uniform, GRect(118, r2_y + (r2_h - 26) / 2 - 2, bounds.size.w - 126, 26),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  int r3_y = r2_y + r2_h;
  GRect r3 = GRect(0, r3_y, bounds.size.w, r3_h);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, r3, 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, r3_y + r3_h - 1), GPoint(bounds.size.w, r3_y + r3_h - 1));

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, "SPLIT:", font_uniform, GRect(8, r3_y + (r3_h - 26) / 2 - 2, 60, 26),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  if (s_split_count > 1) {
    double per_person = total_amt / s_split_count;
    char per_person_buf[32];
    calc_format_currency(per_person_buf, sizeof(per_person_buf), per_person);
    snprintf(buf, sizeof(buf), "/%d = %s", s_split_count, per_person_buf);
  } else {
    snprintf(buf, sizeof(buf), "/1");
  }
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, buf, font_uniform, GRect(68, r3_y + (r3_h - 26) / 2 - 2, bounds.size.w - 76, 26),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  int r4_y = r3_y + r3_h;
  GRect r4 = GRect(0, r4_y, bounds.size.w, r4_h);
  graphics_context_set_fill_color(ctx, ACCENT_ORANGE);
  graphics_fill_rect(ctx, r4, 0, GCornerNone);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "TOTAL:", font_uniform, GRect(8, r4_y + (r4_h - 26) / 2 - 2, 80, 26),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  char total_cur_buf[32];
  calc_format_currency(total_cur_buf, sizeof(total_cur_buf), total_amt);
  graphics_draw_text(ctx, total_cur_buf, font_uniform, GRect(88, r4_y + (r4_h - 26) / 2 - 2, bounds.size.w - 96, 26),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}

static void tip_window_load(Window *window) {
  Layer *root_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root_layer);

  s_tip_layer = layer_create(bounds);
  layer_set_update_proc(s_tip_layer, tip_layer_update_proc);
  layer_add_child(root_layer, s_tip_layer);

#if defined(PBL_TOUCH) || defined(PBL_PLATFORM_EMERY)
  if (window_set_touch_bridge_disabled) {
    window_set_touch_bridge_disabled(window, true);
  }
  if (tap_recognizer_create && window_attach_recognizer) {
    Recognizer *tap = tap_recognizer_create(tip_tap_handler, NULL);
    if (tap) {
      window_attach_recognizer(window, tap);
    }
  }
#endif
}

static void tip_window_unload(Window *window) {
  (void)window;
  if (s_tip_hold_repeat_timer) {
    app_timer_cancel(s_tip_hold_repeat_timer);
    s_tip_hold_repeat_timer = NULL;
  }
  layer_destroy(s_tip_layer);
  s_tip_layer = NULL;

  calc_format_result(s_tip_subtotal, s_expr, sizeof(s_expr));
  s_cursor_pos = (int)strlen(s_expr);
  s_is_result_showing = true;
  s_display_scroll_x = 0;
  if (s_display_layer) layer_mark_dirty(s_display_layer);
}

static void open_tip_window(void) {
  double subtotal = 0.0;
  if (strlen(s_expr) > 0) {
    CalcError err = calc_evaluate(s_expr, &subtotal);
    if (err != CALC_OK || subtotal < 0) {
      subtotal = 0.0;
    }
  }
  s_tip_subtotal = subtotal;
  s_tip_percent = 15;
  s_split_count = 1;

  if (!s_tip_window) {
    s_tip_window = window_create();
    window_set_click_config_provider(s_tip_window, tip_click_config_provider);
    window_set_window_handlers(s_tip_window, (WindowHandlers) {
      .load = tip_window_load,
      .unload = tip_window_unload,
    });
  }
  window_stack_push(s_tip_window, true);
}

static void init(void) {
#if defined(PBL_TOUCH) || defined(PBL_PLATFORM_EMERY)
  if (app_touch_navigation_enable) {
    app_touch_navigation_enable(true);
  }
#endif

  s_main_window = window_create();
  window_set_click_config_provider(s_main_window, main_click_config_provider);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });

  window_stack_push(s_main_window, true);
}

static void deinit(void) {
  if (s_tip_window) {
    window_destroy(s_tip_window);
    s_tip_window = NULL;
  }
  if (s_main_window) {
    window_destroy(s_main_window);
    s_main_window = NULL;
  }
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
