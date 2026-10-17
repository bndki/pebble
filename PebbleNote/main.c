#include <pebble.h>

#ifndef MESSAGE_KEY_COMMAND
#define MESSAGE_KEY_COMMAND            0
#define MESSAGE_KEY_PATH               1
#define MESSAGE_KEY_NAME               2
#define MESSAGE_KEY_IS_DIR             3
#define MESSAGE_KEY_IS_PIN             4
#define MESSAGE_KEY_ITEM_CLEAR         5
#define MESSAGE_KEY_ITEM_INDEX         6
#define MESSAGE_KEY_ITEM_COUNT         7
#define MESSAGE_KEY_ITEM_NAME          8
#define MESSAGE_KEY_ITEM_PATH          9
#define MESSAGE_KEY_ITEM_IS_DIR        10
#define MESSAGE_KEY_ITEM_IS_PIN        11
#define MESSAGE_KEY_ITEM_IS_LAST       12
#define MESSAGE_KEY_CHUNK_DATA         13
#define MESSAGE_KEY_CHUNK_INDEX        14
#define MESSAGE_KEY_CHUNK_TOTAL        15
#define MESSAGE_KEY_CHUNK_IS_LAST      16
#define MESSAGE_KEY_ERROR              17
#define MESSAGE_KEY_STATUS             18
#define MESSAGE_KEY_SETTINGS_ANIMATE   19
#define MESSAGE_KEY_SETTINGS_FONT_SIZE 20
#define MESSAGE_KEY_CURRENT_DIR        21
#endif

#define CMD_GET_TREE                   1
#define CMD_GET_FILE                   2
#define CMD_TOGGLE_PIN                 3
#define CMD_STATUS_MSG                 4

#define MAX_PATH_LEN                   128
#define MAX_NAME_LEN                   64
#define MAX_STATUS_LEN                 48

typedef struct {
  char name[MAX_NAME_LEN];
  char path[MAX_PATH_LEN];
  bool is_dir;
  bool is_pin;
} NoteItem;

typedef enum {
  NOTE_FONT_SMALL = 0,
  NOTE_FONT_DEFAULT = 1,
  NOTE_FONT_LARGE = 2
} NoteFontSize;

static Window *s_browser_window = NULL;
static MenuLayer *s_browser_menu_layer = NULL;
static MenuLayerCallbacks s_menu_callbacks;
static ClickConfigProvider s_menu_click_config_provider = NULL;
static void *s_menu_click_config_context = NULL;

static NoteItem *s_items = NULL;
static uint16_t s_items_count = 0;
static uint16_t s_items_capacity = 0;
static char s_current_dir[MAX_PATH_LEN] = "";
static bool s_is_loading_dir = false;
static char s_dir_status[MAX_STATUS_LEN] = "Connecting to phone...";
static AppTimer *s_retry_timer = NULL;
static bool s_need_selection_reset = false;

static bool s_animate_long_titles = false;
static AppTimer *s_anim_timer = NULL;
static uint32_t s_anim_tick = 0;

static Window *s_viewer_window = NULL;
static ScrollLayer *s_viewer_scroll_layer = NULL;
static ScrollLayerCallbacks s_viewer_scroll_callbacks;
static TextLayer *s_viewer_text_layer = NULL;
static char *s_note_text = NULL;
static size_t s_note_text_len = 0;
static size_t s_note_text_capacity = 0;
static bool s_is_loading_note = false;
static char s_current_note_title[MAX_NAME_LEN] = "";
static char s_current_note_path[MAX_PATH_LEN] = "";
static NoteFontSize s_note_font_size = NOTE_FONT_DEFAULT;

static void request_directory(const char *path);
static void request_file(const char *path, const char *name);
static void request_toggle_pin(NoteItem *item);
static void update_viewer_layout(void);
static void note_viewer_window_push(const char *title, const char *path);
static void start_anim_timer(void);
static void stop_anim_timer(void);

static int32_t tuple_get_int(const Tuple *tuple) {
  if (!tuple) return 0;
  if (tuple->type == TUPLE_INT || tuple->type == TUPLE_UINT) {
    switch (tuple->length) {
      case 1: return (int32_t)tuple->value->uint8;
      case 2: return (int32_t)tuple->value->uint16;
      case 4: return (int32_t)tuple->value->uint32;
      default: return (int32_t)tuple->value->uint32;
    }
  }
  return 0;
}

static GFont get_current_note_font(void) {
  switch (s_note_font_size) {
    case NOTE_FONT_SMALL:
      return fonts_get_system_font(FONT_KEY_GOTHIC_18);
    case NOTE_FONT_LARGE:
      return fonts_get_system_font(FONT_KEY_GOTHIC_28);
    case NOTE_FONT_DEFAULT:
    default:
      return fonts_get_system_font(FONT_KEY_GOTHIC_24);
  }
}

static void free_directory_items(void) {
  if (s_items != NULL) {
    free(s_items);
    s_items = NULL;
  }
  s_items_count = 0;
  s_items_capacity = 0;
}

static bool append_or_update_directory_item(const char *name, const char *path, bool is_dir, bool is_pin) {
  for (uint16_t i = 0; i < s_items_count; i++) {
    if (strcmp(s_items[i].path, path) == 0) {
      strncpy(s_items[i].name, name, MAX_NAME_LEN - 1);
      s_items[i].name[MAX_NAME_LEN - 1] = '\0';
      s_items[i].is_dir = is_dir;
      s_items[i].is_pin = is_pin;
      return true;
    }
  }

  if (s_items_count >= s_items_capacity) {
    uint16_t new_cap = (s_items_capacity == 0) ? 8 : (s_items_capacity * 2);
    NoteItem *new_buf = (NoteItem *)realloc(s_items, new_cap * sizeof(NoteItem));
    if (!new_buf) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Out of memory expanding directory items buffer");
      return false;
    }
    s_items = new_buf;
    s_items_capacity = new_cap;
  }

  NoteItem *item = &s_items[s_items_count];
  strncpy(item->name, name, MAX_NAME_LEN - 1);
  item->name[MAX_NAME_LEN - 1] = '\0';

  strncpy(item->path, path, MAX_PATH_LEN - 1);
  item->path[MAX_PATH_LEN - 1] = '\0';

  item->is_dir = is_dir;
  item->is_pin = is_pin;
  s_items_count++;
  return true;
}

static void free_note_text(void) {
  if (s_note_text != NULL) {
    free(s_note_text);
    s_note_text = NULL;
  }
  s_note_text_len = 0;
  s_note_text_capacity = 0;
}

static bool append_note_chunk(const char *chunk) {
  size_t chunk_len = strlen(chunk);
  if (chunk_len == 0) {
    return true;
  }

  size_t required = s_note_text_len + chunk_len + 1;
  if (required > s_note_text_capacity) {
    size_t new_cap = (s_note_text_capacity == 0) ? 512 : (s_note_text_capacity * 2);
    while (new_cap < required) {
      new_cap *= 2;
    }
    char *new_buf = (char *)realloc(s_note_text, new_cap);
    if (!new_buf) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Out of memory expanding note buffer");
      return false;
    }
    s_note_text = new_buf;
    s_note_text_capacity = new_cap;
  }

  memcpy(s_note_text + s_note_text_len, chunk, chunk_len);
  s_note_text_len += chunk_len;
  s_note_text[s_note_text_len] = '\0';
  return true;
}

static void retry_timer_callback(void *data) {
  s_retry_timer = NULL;
  if (s_items_count == 0 && !s_is_loading_note) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Auto-retrying directory fetch...");
    request_directory(s_current_dir);
  }
}

static void request_directory(const char *path) {
  free_directory_items();
  s_is_loading_dir = true;
  snprintf(s_dir_status, sizeof(s_dir_status), "Loading files...");
  s_need_selection_reset = true;
  if (s_browser_menu_layer) {
    menu_layer_reload_data(s_browser_menu_layer);
    menu_layer_set_selected_index(s_browser_menu_layer, (MenuIndex){ .section = 0, .row = 0 }, MenuRowAlignTop, false);
  }

  if (s_retry_timer) {
    app_timer_cancel(s_retry_timer);
    s_retry_timer = NULL;
  }
  s_retry_timer = app_timer_register(2500, retry_timer_callback, NULL);

  DictionaryIterator *iter = NULL;
  AppMessageResult res = app_message_outbox_begin(&iter);
  if (res != APP_MSG_OK || !iter) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox begin failed for GET_TREE: %d", res);
    return;
  }

  dict_write_uint8(iter, MESSAGE_KEY_COMMAND, CMD_GET_TREE);
  dict_write_cstring(iter, MESSAGE_KEY_PATH, path ? path : "");
  dict_write_end(iter);

  res = app_message_outbox_send();
  if (res != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed for GET_TREE: %d", res);
  }
}

static void request_file(const char *path, const char *name) {
  free_note_text();
  s_is_loading_note = true;

  strncpy(s_current_note_path, path, MAX_PATH_LEN - 1);
  s_current_note_path[MAX_PATH_LEN - 1] = '\0';

  strncpy(s_current_note_title, name, MAX_NAME_LEN - 1);
  s_current_note_title[MAX_NAME_LEN - 1] = '\0';

  note_viewer_window_push(s_current_note_title, s_current_note_path);

  DictionaryIterator *iter = NULL;
  AppMessageResult res = app_message_outbox_begin(&iter);
  if (res != APP_MSG_OK || !iter) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox begin failed for GET_FILE: %d", res);
    return;
  }

  dict_write_uint8(iter, MESSAGE_KEY_COMMAND, CMD_GET_FILE);
  dict_write_cstring(iter, MESSAGE_KEY_PATH, path);
  dict_write_end(iter);

  res = app_message_outbox_send();
  if (res != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed for GET_FILE: %d", res);
  }
}

static void request_toggle_pin(NoteItem *item) {
  if (!item) return;

  vibes_short_pulse();

  item->is_pin = !item->is_pin;
  if (s_browser_menu_layer) {
    menu_layer_reload_data(s_browser_menu_layer);
  }

  DictionaryIterator *iter = NULL;
  AppMessageResult res = app_message_outbox_begin(&iter);
  if (res != APP_MSG_OK || !iter) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox begin failed for TOGGLE_PIN: %d", res);
    return;
  }

  dict_write_uint8(iter, MESSAGE_KEY_COMMAND, CMD_TOGGLE_PIN);
  dict_write_cstring(iter, MESSAGE_KEY_PATH, item->path);
  dict_write_cstring(iter, MESSAGE_KEY_NAME, item->name);
  dict_write_uint8(iter, MESSAGE_KEY_IS_DIR, item->is_dir ? 1 : 0);
  dict_write_cstring(iter, MESSAGE_KEY_CURRENT_DIR, s_current_dir);
  dict_write_end(iter);

  res = app_message_outbox_send();
  if (res != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed for TOGGLE_PIN: %d", res);
  }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *tuple_error = dict_find(iterator, MESSAGE_KEY_ERROR);
  if (tuple_error) {
    const char *err = tuple_error->value->cstring;
    APP_LOG(APP_LOG_LEVEL_WARNING, "Server error: %s", err);
    snprintf(s_dir_status, sizeof(s_dir_status), "%s", err);
    s_is_loading_dir = false;
    s_is_loading_note = false;
    if (s_browser_menu_layer) {
      menu_layer_reload_data(s_browser_menu_layer);
    }
    if (s_viewer_text_layer && window_stack_contains_window(s_viewer_window)) {
      text_layer_set_text(s_viewer_text_layer, err);
      update_viewer_layout();
    }
    return;
  }

  Tuple *tuple_anim = dict_find(iterator, MESSAGE_KEY_SETTINGS_ANIMATE);
  if (tuple_anim) {
    s_animate_long_titles = (tuple_get_int(tuple_anim) != 0);
    if (s_animate_long_titles) {
      start_anim_timer();
    } else {
      stop_anim_timer();
    }
    if (s_browser_menu_layer) {
      layer_mark_dirty(menu_layer_get_layer(s_browser_menu_layer));
    }
  }

  Tuple *tuple_font = dict_find(iterator, MESSAGE_KEY_SETTINGS_FONT_SIZE);
  if (tuple_font) {
    s_note_font_size = (NoteFontSize)tuple_get_int(tuple_font);
    if (s_viewer_text_layer && window_stack_contains_window(s_viewer_window)) {
      text_layer_set_font(s_viewer_text_layer, get_current_note_font());
      update_viewer_layout();
    }
  }

  Tuple *tuple_clear = dict_find(iterator, MESSAGE_KEY_ITEM_CLEAR);
  Tuple *tuple_index = dict_find(iterator, MESSAGE_KEY_ITEM_INDEX);
  uint16_t item_idx = tuple_index ? (uint16_t)tuple_get_int(tuple_index) : 0;

  if ((tuple_clear && tuple_get_int(tuple_clear) == 1) || (tuple_index && item_idx == 0)) {
    free_directory_items();
  }

  Tuple *tuple_name = dict_find(iterator, MESSAGE_KEY_ITEM_NAME);
  if (tuple_name) {
    if (s_retry_timer) {
      app_timer_cancel(s_retry_timer);
      s_retry_timer = NULL;
    }

    Tuple *tuple_path = dict_find(iterator, MESSAGE_KEY_ITEM_PATH);
    Tuple *tuple_is_dir = dict_find(iterator, MESSAGE_KEY_ITEM_IS_DIR);
    Tuple *tuple_is_pin = dict_find(iterator, MESSAGE_KEY_ITEM_IS_PIN);

    const char *name = tuple_name->value->cstring;
    const char *path = tuple_path ? tuple_path->value->cstring : name;
    bool is_dir = tuple_is_dir ? (tuple_get_int(tuple_is_dir) != 0) : false;
    bool is_pin = tuple_is_pin ? (tuple_get_int(tuple_is_pin) != 0) : false;

    append_or_update_directory_item(name, path, is_dir, is_pin);

    if (s_browser_menu_layer) {
      menu_layer_reload_data(s_browser_menu_layer);
      if (s_need_selection_reset) {
        menu_layer_set_selected_index(s_browser_menu_layer, (MenuIndex){ .section = 0, .row = 0 }, MenuRowAlignTop, false);
      }
    }
  }

  Tuple *tuple_item_last = dict_find(iterator, MESSAGE_KEY_ITEM_IS_LAST);
  if (tuple_item_last && tuple_get_int(tuple_item_last) == 1) {
    if (s_retry_timer) {
      app_timer_cancel(s_retry_timer);
      s_retry_timer = NULL;
    }
    s_is_loading_dir = false;
    if (s_items_count == 0) {
      snprintf(s_dir_status, sizeof(s_dir_status), "No .txt or .md files");
    }
    if (s_browser_menu_layer) {
      menu_layer_reload_data(s_browser_menu_layer);
      if (s_need_selection_reset) {
        s_need_selection_reset = false;
        menu_layer_set_selected_index(s_browser_menu_layer, (MenuIndex){ .section = 0, .row = 0 }, MenuRowAlignTop, false);
      }
    }
  }

  Tuple *tuple_chunk = dict_find(iterator, MESSAGE_KEY_CHUNK_DATA);
  if (tuple_chunk) {
    Tuple *tuple_chunk_idx = dict_find(iterator, MESSAGE_KEY_CHUNK_INDEX);
    Tuple *tuple_total = dict_find(iterator, MESSAGE_KEY_CHUNK_TOTAL);
    Tuple *tuple_last = dict_find(iterator, MESSAGE_KEY_CHUNK_IS_LAST);

    uint16_t chunk_idx = tuple_chunk_idx ? (uint16_t)tuple_get_int(tuple_chunk_idx) : 0;
    uint16_t chunk_total = tuple_total ? (uint16_t)tuple_get_int(tuple_total) : 0;
    bool is_last = tuple_last ? (tuple_get_int(tuple_last) != 0) : false;

    if (chunk_idx == 0) {
      free_note_text();
    }

    append_note_chunk(tuple_chunk->value->cstring);

    if (s_viewer_text_layer && window_stack_contains_window(s_viewer_window)) {
      if (is_last || (chunk_total > 0 && chunk_idx >= chunk_total - 1)) {
        s_is_loading_note = false;
        text_layer_set_text(s_viewer_text_layer, s_note_text ? s_note_text : "(Empty note)");
      } else {
        text_layer_set_text(s_viewer_text_layer, s_note_text ? s_note_text : "Loading...");
      }
      update_viewer_layout();
    }
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "AppMessage inbox dropped: %d", reason);
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage outbox send failed: %d", reason);
  snprintf(s_dir_status, sizeof(s_dir_status), "Phone offline (tap to retry)");
  s_is_loading_dir = false;
  s_is_loading_note = false;
  if (s_browser_menu_layer) {
    menu_layer_reload_data(s_browser_menu_layer);
  }
}

static int16_t get_bounce_offset(int16_t overflow, uint32_t ticks) {
  if (overflow <= 0) return 0;
  int16_t pause_ticks = 16;
  int16_t travel_ticks = overflow;
  int16_t half_cycle = pause_ticks + travel_ticks;
  int16_t full_cycle = half_cycle * 2;

  int16_t phase = ticks % full_cycle;
  if (phase < pause_ticks) {
    return 0;
  } else if (phase < half_cycle) {
    return -(phase - pause_ticks);
  } else if (phase < half_cycle + pause_ticks) {
    return -overflow;
  } else {
    return -overflow + (phase - (half_cycle + pause_ticks));
  }
}

static void anim_timer_callback(void *data) {
  s_anim_timer = NULL;
  if (!s_animate_long_titles || !s_browser_window || !window_is_loaded(s_browser_window)) {
    return;
  }
  s_anim_tick++;
  if (s_browser_menu_layer) {
    layer_mark_dirty(menu_layer_get_layer(s_browser_menu_layer));
  }
  s_anim_timer = app_timer_register(35, anim_timer_callback, NULL);
}

static void start_anim_timer(void) {
  if (s_animate_long_titles && !s_anim_timer) {
    s_anim_timer = app_timer_register(35, anim_timer_callback, NULL);
  }
}

static void stop_anim_timer(void) {
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
  }
  s_anim_tick = 0;
}

static void draw_folder_icon(GContext *ctx, int16_t x, int16_t y, bool is_highlighted) {
  GColor body_color = is_highlighted ? GColorWhite : GColorChromeYellow;
  GColor border_color = is_highlighted ? GColorWhite : GColorBlack;

  graphics_context_set_fill_color(ctx, body_color);
  graphics_context_set_stroke_color(ctx, border_color);

  graphics_fill_rect(ctx, GRect(x, y, 6, 3), 0, GCornerNone);
  graphics_draw_rect(ctx, GRect(x, y, 6, 3));

  graphics_fill_rect(ctx, GRect(x, y + 2, 15, 10), 1, GCornersAll);
  graphics_draw_rect(ctx, GRect(x, y + 2, 15, 10));
}

static void draw_file_icon(GContext *ctx, int16_t x, int16_t y, bool is_highlighted) {
  GColor page_color = is_highlighted ? GColorWhite : GColorWhite;
  GColor border_color = is_highlighted ? GColorWhite : GColorBlack;
  GColor line_color = is_highlighted ? GColorCobaltBlue : GColorBlack;

  graphics_context_set_fill_color(ctx, page_color);
  graphics_context_set_stroke_color(ctx, border_color);

  graphics_fill_rect(ctx, GRect(x + 1, y, 12, 14), 0, GCornerNone);
  graphics_draw_rect(ctx, GRect(x + 1, y, 12, 14));

  graphics_context_set_stroke_color(ctx, line_color);
  graphics_draw_line(ctx, GPoint(x + 3, y + 4), GPoint(x + 9, y + 4));
  graphics_draw_line(ctx, GPoint(x + 3, y + 7), GPoint(x + 10, y + 7));
  graphics_draw_line(ctx, GPoint(x + 3, y + 10), GPoint(x + 7, y + 10));
}

static void draw_pin_star(GContext *ctx, int16_t x, int16_t y, bool is_highlighted) {
  GColor star_color = is_highlighted ? GColorYellow : GColorOrange;
  graphics_context_set_fill_color(ctx, star_color);
  graphics_context_set_stroke_color(ctx, is_highlighted ? GColorWhite : GColorBlack);

  graphics_fill_circle(ctx, GPoint(x + 4, y + 5), 3);
  graphics_draw_circle(ctx, GPoint(x + 4, y + 5), 3);
}

static void draw_parent_folder_row(GContext *ctx, GRect bounds, bool is_highlighted) {
  GColor fg_color = is_highlighted ? GColorWhite : GColorBlack;
  graphics_context_set_stroke_color(ctx, fg_color);
  graphics_context_set_text_color(ctx, fg_color);

  int16_t x = 8;
  int16_t y = (bounds.size.h - 12) / 2;
  graphics_draw_line(ctx, GPoint(x + 4, y), GPoint(x, y + 5));
  graphics_draw_line(ctx, GPoint(x + 4, y), GPoint(x + 8, y + 5));
  graphics_draw_line(ctx, GPoint(x + 4, y), GPoint(x + 4, y + 11));

  graphics_draw_text(ctx, "..", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(24, (bounds.size.h - 22) / 2, bounds.size.w - 28, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return 1;
}

static bool is_in_subfolder(void) {
  return strlen(s_current_dir) > 0;
}

static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  bool in_sub = is_in_subfolder();
  if (s_items_count == 0) {
    return in_sub ? 2 : 1;
  }
  return in_sub ? (s_items_count + 1) : s_items_count;
}

static int16_t menu_get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  return 34;
}

static void navigate_to_parent(void) {
  char *last_slash = strrchr(s_current_dir, '/');
  if (last_slash) {
    *last_slash = '\0';
  } else {
    s_current_dir[0] = '\0';
  }
  free_directory_items();
  request_directory(s_current_dir);
}

static void menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  bool in_sub = is_in_subfolder();
  uint16_t row = cell_index->row;
  GRect bounds = layer_get_bounds(cell_layer);
  bool is_highlighted = menu_cell_layer_is_highlighted(cell_layer);

  GColor fg_color = is_highlighted ? GColorWhite : GColorBlack;
  GColor bg_color = is_highlighted ? GColorCobaltBlue : GColorWhite;
  graphics_context_set_text_color(ctx, fg_color);

  if (in_sub && row == 0) {
    draw_parent_folder_row(ctx, bounds, is_highlighted);
    return;
  }

  uint16_t item_index = in_sub ? (row - 1) : row;

  if (s_items_count == 0) {
    graphics_draw_text(ctx, s_dir_status, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(8, (bounds.size.h - 22) / 2, bounds.size.w - 16, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  if (item_index >= s_items_count) return;

  NoteItem *item = &s_items[item_index];
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  int16_t icon_x = 6;
  int16_t icon_y = (bounds.size.h - 14) / 2;
  int16_t text_x = 24;

  if (item->is_pin) {
    text_x += 10;
  }

  int16_t available_w = bounds.size.w - text_x - 6;
  GSize text_size = graphics_text_layout_get_content_size(item->name, font, GRect(0, 0, 1000, bounds.size.h),
                                                          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);

  int16_t overflow = text_size.w - available_w + 4;
  int16_t bounce_x = 0;
  if (s_animate_long_titles && is_highlighted && overflow > 0) {
    bounce_x = get_bounce_offset(overflow, s_anim_tick);
  }

  if (bounce_x != 0) {
    GRect text_rect = GRect(text_x + bounce_x, (bounds.size.h - 22) / 2, text_size.w + 12, 22);
    graphics_draw_text(ctx, item->name, font, text_rect, GTextOverflowModeFill, GTextAlignmentLeft, NULL);

    graphics_context_set_fill_color(ctx, bg_color);
    graphics_fill_rect(ctx, GRect(0, 0, text_x, bounds.size.h), 0, GCornerNone);

    graphics_fill_rect(ctx, GRect(bounds.size.w - 4, 0, 4, bounds.size.h), 0, GCornerNone);
  } else {
    GRect text_rect = GRect(text_x, (bounds.size.h - 22) / 2, available_w, 22);
    graphics_draw_text(ctx, item->name, font, text_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  if (item->is_dir) {
    draw_folder_icon(ctx, icon_x, icon_y, is_highlighted);
  } else {
    draw_file_icon(ctx, icon_x, icon_y, is_highlighted);
  }

  if (item->is_pin) {
    draw_pin_star(ctx, 22, (bounds.size.h - 10) / 2, is_highlighted);
  }

  if (!in_sub && item->is_pin && (item_index + 1 < s_items_count) && !s_items[item_index + 1].is_pin) {
    graphics_context_set_stroke_color(ctx, is_highlighted ? GColorWhite : GColorDarkGray);
    graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w - 1, bounds.size.h - 1));
  }
}

static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  bool in_sub = is_in_subfolder();
  uint16_t row = cell_index->row;

  if (in_sub && row == 0) {
    navigate_to_parent();
    return;
  }

  if (s_items_count == 0) {
    request_directory(s_current_dir);
    return;
  }

  uint16_t item_index = in_sub ? (row - 1) : row;
  if (item_index >= s_items_count) return;

  NoteItem *item = &s_items[item_index];
  if (item->is_dir) {
    strncpy(s_current_dir, item->path, sizeof(s_current_dir) - 1);
    s_current_dir[sizeof(s_current_dir) - 1] = '\0';
    free_directory_items();
    request_directory(s_current_dir);
  } else {
    request_file(item->path, item->name);
  }
}

static void menu_select_long_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  bool in_sub = is_in_subfolder();
  uint16_t row = cell_index->row;

  if (in_sub && row == 0) return;
  if (s_items_count == 0) return;

  uint16_t item_index = in_sub ? (row - 1) : row;
  if (item_index >= s_items_count) return;

  NoteItem *item = &s_items[item_index];
  request_toggle_pin(item);
}

static void browser_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (is_in_subfolder()) {
    navigate_to_parent();
  } else {
    window_stack_pop(true);
  }
}

static void menu_selection_changed_callback(MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context) {
  s_anim_tick = 0;
}

static void browser_back_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  window_stack_pop_all(true);
}

static void browser_click_config_provider(void *context) {
  if (s_menu_click_config_provider) {
    s_menu_click_config_provider(s_menu_click_config_context);
  }
  window_single_click_subscribe(BUTTON_ID_BACK, browser_back_click_handler);
  window_long_click_subscribe(BUTTON_ID_BACK, 2000, browser_back_long_click_handler, NULL);
}

static void browser_window_load(Window *window) {
  free_directory_items();

  Layer *root_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root_layer);

  s_browser_menu_layer = menu_layer_create(bounds);

  s_menu_callbacks = (MenuLayerCallbacks) {
    .get_num_sections = menu_get_num_sections_callback,
    .get_num_rows = menu_get_num_rows_callback,
    .get_cell_height = menu_get_cell_height_callback,
    .draw_row = menu_draw_row_callback,
    .select_click = menu_select_callback,
    .select_long_click = menu_select_long_callback,
    .selection_changed = menu_selection_changed_callback,
  };
  menu_layer_set_callbacks(s_browser_menu_layer, NULL, s_menu_callbacks);

  menu_layer_set_normal_colors(s_browser_menu_layer, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_browser_menu_layer, GColorCobaltBlue, GColorWhite);

  menu_layer_set_click_config_onto_window(s_browser_menu_layer, window);

  s_menu_click_config_provider = window_get_click_config_provider(window);
  s_menu_click_config_context = window_get_click_config_context(window);
  window_set_click_config_provider_with_context(window, browser_click_config_provider, s_menu_click_config_context);

  layer_add_child(root_layer, menu_layer_get_layer(s_browser_menu_layer));

  start_anim_timer();
  request_directory("");
}

static void browser_window_unload(Window *window) {
  stop_anim_timer();
  if (s_retry_timer) {
    app_timer_cancel(s_retry_timer);
    s_retry_timer = NULL;
  }
  if (s_browser_menu_layer) {
    menu_layer_destroy(s_browser_menu_layer);
    s_browser_menu_layer = NULL;
  }
  free_directory_items();
}

static void note_viewer_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Note viewer: Single SELECT clicked");
}

static void note_viewer_long_select_handler(ClickRecognizerRef recognizer, void *context) {
  vibes_short_pulse();

  s_note_font_size = (NoteFontSize)((s_note_font_size + 1) % 3);

  if (s_viewer_text_layer && s_viewer_window && window_stack_contains_window(s_viewer_window)) {
    text_layer_set_font(s_viewer_text_layer, get_current_note_font());
    update_viewer_layout();
  }

  DictionaryIterator *iter = NULL;
  AppMessageResult res = app_message_outbox_begin(&iter);
  if (res == APP_MSG_OK && iter) {
    dict_write_uint8(iter, MESSAGE_KEY_SETTINGS_FONT_SIZE, (uint8_t)s_note_font_size);
    dict_write_end(iter);
    app_message_outbox_send();
  }
}

static void note_viewer_long_up_stub(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Note viewer stub: Long UP triggered");
  if (s_viewer_scroll_layer) {
    scroll_layer_set_content_offset(s_viewer_scroll_layer, GPointZero, true);
  }
}

static void note_viewer_long_down_stub(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Note viewer stub: Long DOWN triggered");
}

static void note_viewer_back_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  window_stack_pop_all(true);
}

static void note_viewer_scroll_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, note_viewer_select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, note_viewer_long_select_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_BACK, 2000, note_viewer_back_long_click_handler, NULL);
}

static void update_viewer_layout(void) {
  if (!s_viewer_window || !s_viewer_text_layer || !s_viewer_scroll_layer) {
    return;
  }

  Layer *root_layer = window_get_root_layer(s_viewer_window);
  GRect bounds = layer_get_bounds(root_layer);

  text_layer_set_size(s_viewer_text_layer, GSize(bounds.size.w - 8, 30000));
  GSize max_size = text_layer_get_content_size(s_viewer_text_layer);

  max_size.w = bounds.size.w - 8;
  max_size.h += 16;
  if (max_size.h < bounds.size.h) {
    max_size.h = bounds.size.h;
  }

  text_layer_set_size(s_viewer_text_layer, max_size);
  scroll_layer_set_content_size(s_viewer_scroll_layer, GSize(bounds.size.w, max_size.h + 8));
}

static void viewer_window_load(Window *window) {
  Layer *root_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root_layer);

  window_set_background_color(window, GColorWhite);

  s_viewer_scroll_layer = scroll_layer_create(bounds);
  scroll_layer_set_shadow_hidden(s_viewer_scroll_layer, false);

  s_viewer_scroll_callbacks = (ScrollLayerCallbacks) {
    .click_config_provider = note_viewer_scroll_click_config_provider,
    .content_offset_changed_handler = NULL
  };
  scroll_layer_set_callbacks(s_viewer_scroll_layer, s_viewer_scroll_callbacks);

  scroll_layer_set_click_config_onto_window(s_viewer_scroll_layer, window);

  s_viewer_text_layer = text_layer_create(GRect(4, 4, bounds.size.w - 8, bounds.size.h));
  text_layer_set_font(s_viewer_text_layer, get_current_note_font());
  text_layer_set_text_color(s_viewer_text_layer, GColorBlack);
  text_layer_set_background_color(s_viewer_text_layer, GColorClear);
  text_layer_set_overflow_mode(s_viewer_text_layer, GTextOverflowModeWordWrap);

  if (s_is_loading_note) {
    text_layer_set_text(s_viewer_text_layer, "Loading note...\nStreaming from phone...");
  } else if (s_note_text) {
    text_layer_set_text(s_viewer_text_layer, s_note_text);
  } else {
    text_layer_set_text(s_viewer_text_layer, "(Empty note)");
  }

  scroll_layer_add_child(s_viewer_scroll_layer, text_layer_get_layer(s_viewer_text_layer));
  layer_add_child(root_layer, scroll_layer_get_layer(s_viewer_scroll_layer));

  update_viewer_layout();
}

static void viewer_window_unload(Window *window) {
  if (s_viewer_text_layer) {
    text_layer_destroy(s_viewer_text_layer);
    s_viewer_text_layer = NULL;
  }
  if (s_viewer_scroll_layer) {
    scroll_layer_destroy(s_viewer_scroll_layer);
    s_viewer_scroll_layer = NULL;
  }

  free_note_text();
  s_is_loading_note = false;
}

static void note_viewer_window_push(const char *title, const char *path) {
  if (s_viewer_window && !window_stack_contains_window(s_viewer_window)) {
    window_stack_push(s_viewer_window, true);
  } else if (s_viewer_text_layer) {
    text_layer_set_text(s_viewer_text_layer, "Loading note...\nStreaming from phone...");
    update_viewer_layout();
  }
}

static void init(void) {
  app_touch_navigation_enable(true);

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_open(1024, 256);

  s_browser_window = window_create();
  window_set_window_handlers(s_browser_window, (WindowHandlers) {
    .load = browser_window_load,
    .unload = browser_window_unload,
  });

  s_viewer_window = window_create();
  window_set_window_handlers(s_viewer_window, (WindowHandlers) {
    .load = viewer_window_load,
    .unload = viewer_window_unload,
  });

  window_stack_push(s_browser_window, true);
}

static void deinit(void) {
  stop_anim_timer();
  app_message_deregister_callbacks();
  if (s_retry_timer) {
    app_timer_cancel(s_retry_timer);
    s_retry_timer = NULL;
  }
  if (s_viewer_window) {
    if (window_stack_contains_window(s_viewer_window)) {
      window_stack_remove(s_viewer_window, false);
    }
    window_destroy(s_viewer_window);
    s_viewer_window = NULL;
  }
  if (s_browser_window) {
    if (window_stack_contains_window(s_browser_window)) {
      window_stack_remove(s_browser_window, false);
    }
    window_destroy(s_browser_window);
    s_browser_window = NULL;
  }
  free_directory_items();
  free_note_text();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
