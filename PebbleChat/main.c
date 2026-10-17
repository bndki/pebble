#include <pebble.h>

#ifndef MESSAGE_KEY_KEY_ACTION
  #define KEY_ACTION_VAL 0
  #define KEY_SESSION_ID_VAL 1
  #define KEY_PROMPT_VAL 2
  #define KEY_RESPONSE_CHUNK_VAL 3
  #define KEY_CHUNK_INDEX_VAL 4
  #define KEY_CHUNK_TOTAL_VAL 5
  #define KEY_SESSION_COUNT_VAL 6
  #define KEY_SESSION_TITLE_VAL 7
  #define KEY_ERROR_VAL 8
  #define KEY_TURN_INDEX_VAL 9
  #define KEY_TOTAL_TURNS_VAL 10
  #define KEY_ROLE_VAL 11
  #define KEY_SESSION_INDEX_VAL 12
  #define KEY_FONT_SIZE_VAL 13
  #define KEY_ANIMATE_TITLES_VAL 14
  #define KEY_LAUNCH_VOICE_DICTATION_VAL 15
#else
  #define KEY_ACTION_VAL MESSAGE_KEY_KEY_ACTION
  #define KEY_SESSION_ID_VAL MESSAGE_KEY_KEY_SESSION_ID
  #define KEY_PROMPT_VAL MESSAGE_KEY_KEY_PROMPT
  #define KEY_RESPONSE_CHUNK_VAL MESSAGE_KEY_KEY_RESPONSE_CHUNK
  #define KEY_CHUNK_INDEX_VAL MESSAGE_KEY_KEY_CHUNK_INDEX
  #define KEY_CHUNK_TOTAL_VAL MESSAGE_KEY_KEY_CHUNK_TOTAL
  #define KEY_SESSION_COUNT_VAL MESSAGE_KEY_KEY_SESSION_COUNT
  #define KEY_SESSION_TITLE_VAL MESSAGE_KEY_KEY_SESSION_TITLE
  #define KEY_ERROR_VAL MESSAGE_KEY_KEY_ERROR
  #define KEY_TURN_INDEX_VAL MESSAGE_KEY_KEY_TURN_INDEX
  #define KEY_TOTAL_TURNS_VAL MESSAGE_KEY_KEY_TOTAL_TURNS
  #define KEY_ROLE_VAL MESSAGE_KEY_KEY_ROLE
  #define KEY_SESSION_INDEX_VAL MESSAGE_KEY_KEY_SESSION_INDEX
  #define KEY_FONT_SIZE_VAL MESSAGE_KEY_KEY_FONT_SIZE
  #ifdef MESSAGE_KEY_KEY_ANIMATE_TITLES
    #define KEY_ANIMATE_TITLES_VAL MESSAGE_KEY_KEY_ANIMATE_TITLES
  #else
    #define KEY_ANIMATE_TITLES_VAL 14
  #endif
  #ifdef MESSAGE_KEY_KEY_LAUNCH_VOICE_DICTATION
    #define KEY_LAUNCH_VOICE_DICTATION_VAL MESSAGE_KEY_KEY_LAUNCH_VOICE_DICTATION
  #else
    #define KEY_LAUNCH_VOICE_DICTATION_VAL 15
  #endif
#endif

enum {
  ACTION_FETCH_SESSIONS = 1,
  ACTION_SEND_PROMPT = 2,
  ACTION_GET_TURN = 3,
  ACTION_LOAD_SESSION = 4,
  ACTION_DELETE_SESSION = 5,
  ACTION_DELETE_TURNS = 6,
  ACTION_SESSION_LIST = 10,
  ACTION_RESPONSE_CHUNK = 11,
  ACTION_ERROR = 12,
  ACTION_SETTINGS_UPDATE = 13
};

#define STORAGE_KEY_FONT_SIZE 100
#define STORAGE_KEY_ANIMATE_TITLES 101
#define STORAGE_KEY_LAUNCH_VOICE_DICTATION 102
static int s_font_size = 24;
static bool s_animate_titles = false;
static bool s_launch_voice_dictation = false;
static AppTimer *s_launch_dictation_timer = NULL;

static const char *get_font_key_for_size(int size) {
  if (size <= 20) {
    return FONT_KEY_GOTHIC_18;
  } else if (size <= 26) {
    return FONT_KEY_GOTHIC_24;
  } else {
    return FONT_KEY_GOTHIC_28;
  }
}

static const char *get_bold_font_key_for_size(int size) {
  if (size <= 20) {
    return FONT_KEY_GOTHIC_18_BOLD;
  } else if (size <= 26) {
    return FONT_KEY_GOTHIC_24_BOLD;
  } else {
    return FONT_KEY_GOTHIC_28_BOLD;
  }
}

#if defined(PBL_PLATFORM_EMERY)
void app_touch_navigation_enable(bool enable) __attribute__((weak));
void window_set_touch_bridge_disabled(Window *window, bool disabled) __attribute__((weak));
Recognizer *pan_recognizer_create(RecognizerEventCb event_cb, void *user_data, PanAxis axis) __attribute__((weak));
GPoint pan_recognizer_get_delta_since_start(const Recognizer *recognizer) __attribute__((weak));
void window_attach_recognizer(Window *window, Recognizer *recognizer) __attribute__((weak));
#endif

#define MAX_SESSIONS 5
#define TITLE_MAX_LEN 64

typedef struct {
  uint32_t id;
  char title[TITLE_MAX_LEN];
} ChatSessionInfo;

#define MARQUEE_PAUSE_MS 1200
#define MARQUEE_TICK_MS 50
#define MARQUEE_STEP_PX 2

typedef enum {
  MARQUEE_STATE_PAUSE_START = 0,
  MARQUEE_STATE_SCROLL_LEFT = 1,
  MARQUEE_STATE_PAUSE_END = 2,
  MARQUEE_STATE_SCROLL_RIGHT = 3
} MarqueeState;

static AppTimer *s_marquee_timer = NULL;
static int16_t s_title_scroll_offset = 0;
static MarqueeState s_marquee_state = MARQUEE_STATE_PAUSE_START;

static Window *s_menu_window = NULL;
static MenuLayer *s_menu_layer = NULL;

static Window *s_chat_window = NULL;
static ScrollLayer *s_chat_scroll_layer = NULL;
static TextLayer *s_chat_role_layer = NULL;
static TextLayer *s_chat_turn_count_layer = NULL;
static TextLayer *s_chat_text_layer = NULL;
static char s_turn_count_buf[16];

typedef enum {
  CONFIRM_MODE_NONE = 0,
  CONFIRM_MODE_DELETE_SESSION = 1,
  CONFIRM_MODE_DELETE_TURN = 2,
  CONFIRM_MODE_RETRY_ATTEMPT = 3
} ConfirmMode;

static Window *s_confirm_window = NULL;
static TextLayer *s_confirm_text_layer = NULL;
static ConfirmMode s_confirm_mode = CONFIRM_MODE_NONE;
static uint32_t s_confirm_session_id = 0;
static int s_confirm_turn_index = 0;
static char s_confirm_prompt_buf[100];

static DictationSession *s_dictation_session = NULL;
static AppTimer *s_watchdog_timer = NULL;

static ChatSessionInfo s_sessions[MAX_SESSIONS];
static int s_session_count = 0;

static uint32_t s_active_session_id = 0;
static int s_current_turn_index = 0;
static int s_total_turns = 1;
static int s_prev_total_turns = 1;
static int s_prev_turn_index = 0;
static int s_current_role = -1;
static bool s_waiting_for_api_response = false;
static bool s_is_error_screen = false;
static char s_last_prompt[512] = "";

#if defined(PBL_PLATFORM_EMERY)
static int16_t s_scroll_base = 0;
#endif

static char *s_response_buffer = NULL;
static uint32_t s_response_buffer_size = 0;
static uint32_t s_response_len = 0;

static void send_fetch_sessions(void);
static void send_prompt_to_phone(uint32_t session_id, const char *prompt);
static void submit_prompt(uint32_t session_id, const char *prompt);
static void send_get_turn(uint32_t session_id, int turn_index);
static void send_load_session(uint32_t session_id);
static void send_delete_session(uint32_t session_id);
static void send_delete_turns(uint32_t session_id, int turn_index);
static void chat_window_show(uint32_t session_id);
static void chat_window_update_display(int role, const char *text);
static void chat_window_update_text(const char *text);
static void start_dictation(uint32_t session_id);
static void confirm_window_show(ConfirmMode mode, uint32_t session_id, int turn_index);
static void marquee_animation_start(void);
static void marquee_animation_stop(void);
static void launch_dictation_timer_callback(void *data);

static Tuple *find_tuple_helper(DictionaryIterator *iter, uint32_t primary_key, uint32_t fallback_key) {
  Tuple *t = dict_find(iter, primary_key);
  if (!t && primary_key != fallback_key) {
    t = dict_find(iter, fallback_key);
  }
  return t;
}

static void watchdog_timer_cancel(void) {
  if (s_watchdog_timer != NULL) {
    app_timer_cancel(s_watchdog_timer);
    s_watchdog_timer = NULL;
  }
}

static void watchdog_timer_callback(void *data) {
  s_watchdog_timer = NULL;
  if (s_waiting_for_api_response) {
    vibes_short_pulse();
    s_waiting_for_api_response = false;
    s_total_turns = s_prev_total_turns;
    s_current_turn_index = s_prev_turn_index;
  }
  s_is_error_screen = true;
  APP_LOG(APP_LOG_LEVEL_WARNING, "Watchdog timer expired (30s)");
  chat_window_update_text("Request timed out (30s).\n\nPlease verify phone internet connection and OpenRouter API key in settings.\n\nSEL - Retry\nBACK - Menu");
}

static void watchdog_timer_start(void) {
  watchdog_timer_cancel();
  s_watchdog_timer = app_timer_register(35000, watchdog_timer_callback, NULL);
}

static void send_fetch_sessions(void) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK && iter) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Requesting session list from phone");
    dict_write_uint8(iter, KEY_ACTION_VAL, ACTION_FETCH_SESSIONS);
    if (KEY_ACTION_VAL != 0) {
      dict_write_uint8(iter, 0, ACTION_FETCH_SESSIONS);
    }
    app_message_outbox_send();
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "fetch_sessions outbox_begin result: %d", (int)result);
  }
}

static void send_prompt_to_phone(uint32_t session_id, const char *prompt) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK && iter) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Sending prompt to phone: session=%lu", (unsigned long)session_id);
    dict_write_uint8(iter, KEY_ACTION_VAL, ACTION_SEND_PROMPT);
    if (KEY_ACTION_VAL != 0) {
      dict_write_uint8(iter, 0, ACTION_SEND_PROMPT);
    }
    dict_write_uint32(iter, KEY_SESSION_ID_VAL, session_id);
    if (KEY_SESSION_ID_VAL != 1) {
      dict_write_uint32(iter, 1, session_id);
    }
    dict_write_cstring(iter, KEY_PROMPT_VAL, prompt);
    if (KEY_PROMPT_VAL != 2) {
      dict_write_cstring(iter, 2, prompt);
    }
    app_message_outbox_send();
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to begin outbox: %d", (int)result);
    watchdog_timer_cancel();
    if (s_waiting_for_api_response) {
      s_waiting_for_api_response = false;
      s_total_turns = s_prev_total_turns;
      s_current_turn_index = s_prev_turn_index;
    }
    s_is_error_screen = true;
    chat_window_update_text("Phone connection busy.\n\nPlease try again in a moment.\n\nSEL - Retry\nBACK - Menu");
  }
}

static void send_get_turn(uint32_t session_id, int turn_index) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK && iter) {
    dict_write_uint8(iter, KEY_ACTION_VAL, ACTION_GET_TURN);
    if (KEY_ACTION_VAL != 0) {
      dict_write_uint8(iter, 0, ACTION_GET_TURN);
    }
    dict_write_uint32(iter, KEY_SESSION_ID_VAL, session_id);
    if (KEY_SESSION_ID_VAL != 1) {
      dict_write_uint32(iter, 1, session_id);
    }
    dict_write_int16(iter, KEY_TURN_INDEX_VAL, (int16_t)turn_index);
    if (KEY_TURN_INDEX_VAL != 9) {
      dict_write_int16(iter, 9, (int16_t)turn_index);
    }
    app_message_outbox_send();
  }
}

static void send_load_session(uint32_t session_id) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK && iter) {
    dict_write_uint8(iter, KEY_ACTION_VAL, ACTION_LOAD_SESSION);
    if (KEY_ACTION_VAL != 0) {
      dict_write_uint8(iter, 0, ACTION_LOAD_SESSION);
    }
    dict_write_uint32(iter, KEY_SESSION_ID_VAL, session_id);
    if (KEY_SESSION_ID_VAL != 1) {
      dict_write_uint32(iter, 1, session_id);
    }
    app_message_outbox_send();
  }
}

static void send_delete_session(uint32_t session_id) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK && iter) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Sending delete_session to phone: session=%lu", (unsigned long)session_id);
    dict_write_uint8(iter, KEY_ACTION_VAL, ACTION_DELETE_SESSION);
    if (KEY_ACTION_VAL != 0) {
      dict_write_uint8(iter, 0, ACTION_DELETE_SESSION);
    }
    dict_write_uint32(iter, KEY_SESSION_ID_VAL, session_id);
    if (KEY_SESSION_ID_VAL != 1) {
      dict_write_uint32(iter, 1, session_id);
    }
    app_message_outbox_send();
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "delete_session outbox_begin result: %d", (int)result);
  }
}

static void send_delete_turns(uint32_t session_id, int turn_index) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK && iter) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Sending delete_turns to phone: session=%lu, turn=%d", (unsigned long)session_id, turn_index);
    dict_write_uint8(iter, KEY_ACTION_VAL, ACTION_DELETE_TURNS);
    if (KEY_ACTION_VAL != 0) {
      dict_write_uint8(iter, 0, ACTION_DELETE_TURNS);
    }
    dict_write_uint32(iter, KEY_SESSION_ID_VAL, session_id);
    if (KEY_SESSION_ID_VAL != 1) {
      dict_write_uint32(iter, 1, session_id);
    }
    dict_write_int16(iter, KEY_TURN_INDEX_VAL, (int16_t)turn_index);
    if (KEY_TURN_INDEX_VAL != 9) {
      dict_write_int16(iter, 9, (int16_t)turn_index);
    }
    app_message_outbox_send();
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "delete_turns outbox_begin result: %d", (int)result);
  }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *action_tuple = find_tuple_helper(iterator, KEY_ACTION_VAL, 0);
  if (!action_tuple) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Inbox received tuple without action key");
    return;
  }

  uint8_t action = action_tuple->value->uint8;
  APP_LOG(APP_LOG_LEVEL_INFO, "Inbox received action: %d", (int)action);

  if (action == ACTION_SESSION_LIST) {
    Tuple *count_tuple = find_tuple_helper(iterator, KEY_SESSION_COUNT_VAL, 6);
    Tuple *index_tuple = find_tuple_helper(iterator, KEY_SESSION_INDEX_VAL, 12);
    Tuple *id_tuple = find_tuple_helper(iterator, KEY_SESSION_ID_VAL, 1);
    Tuple *title_tuple = find_tuple_helper(iterator, KEY_SESSION_TITLE_VAL, 7);

    if (count_tuple) {
      int count = count_tuple->value->int32;
      if (count <= 0) {
        s_session_count = 0;
        marquee_animation_stop();
        if (s_menu_layer) {
          menu_layer_reload_data(s_menu_layer);
        }
        return;
      }
      if (index_tuple && index_tuple->value->int32 == 0) {
        s_session_count = (count > MAX_SESSIONS) ? MAX_SESSIONS : count;
      }
    }

    if (index_tuple && id_tuple && title_tuple) {
      int idx = index_tuple->value->int32;
      if (idx >= 0 && idx < MAX_SESSIONS) {
        s_sessions[idx].id = id_tuple->value->uint32;
        strncpy(s_sessions[idx].title, title_tuple->value->cstring, TITLE_MAX_LEN - 1);
        s_sessions[idx].title[TITLE_MAX_LEN - 1] = '\0';
        if (idx + 1 > s_session_count) {
          s_session_count = idx + 1;
        }
        if (s_menu_layer) {
          menu_layer_reload_data(s_menu_layer);
          marquee_animation_start();
        }
      }
    }
  } else if (action == ACTION_RESPONSE_CHUNK) {
    Tuple *chunk_idx_tuple = find_tuple_helper(iterator, KEY_CHUNK_INDEX_VAL, 4);
    Tuple *chunk_tot_tuple = find_tuple_helper(iterator, KEY_CHUNK_TOTAL_VAL, 5);
    Tuple *chunk_data_tuple = find_tuple_helper(iterator, KEY_RESPONSE_CHUNK_VAL, 3);
    Tuple *turn_idx_tuple = find_tuple_helper(iterator, KEY_TURN_INDEX_VAL, 9);
    Tuple *tot_turns_tuple = find_tuple_helper(iterator, KEY_TOTAL_TURNS_VAL, 10);
    Tuple *session_id_tuple = find_tuple_helper(iterator, KEY_SESSION_ID_VAL, 1);
    Tuple *role_tuple = find_tuple_helper(iterator, KEY_ROLE_VAL, 11);

    if (session_id_tuple) {
      s_active_session_id = session_id_tuple->value->uint32;
    }
    if (turn_idx_tuple) {
      s_current_turn_index = turn_idx_tuple->value->int32;
    }
    if (tot_turns_tuple) {
      s_total_turns = tot_turns_tuple->value->int32;
    }
    if (role_tuple) {
      s_current_role = role_tuple->value->int32;
    } else {
      s_current_role = 1;
    }

    if (!chunk_idx_tuple || !chunk_tot_tuple || !chunk_data_tuple) {
      return;
    }

    int chunk_index = chunk_idx_tuple->value->int32;
    int chunk_total = chunk_tot_tuple->value->int32;
    const char *chunk_str = chunk_data_tuple->value->cstring;
    size_t chunk_len = strlen(chunk_str);

    APP_LOG(APP_LOG_LEVEL_INFO, "Received chunk %d/%d (%u bytes)", chunk_index + 1, chunk_total, (unsigned int)chunk_len);

    if (chunk_index == 0) {
      if (s_response_buffer) {
        free(s_response_buffer);
        s_response_buffer = NULL;
      }
      s_response_buffer_size = (chunk_total * 450) + 64;
      s_response_buffer = malloc(s_response_buffer_size);
      if (!s_response_buffer && s_response_buffer_size > 4096) {
        s_response_buffer_size = 4096;
        s_response_buffer = malloc(s_response_buffer_size);
      }
      if (!s_response_buffer && s_response_buffer_size > 2048) {
        s_response_buffer_size = 2048;
        s_response_buffer = malloc(s_response_buffer_size);
      }
      if (s_response_buffer) {
        s_response_buffer[0] = '\0';
        s_response_len = 0;
      }
    }

    if (s_response_buffer && (s_response_len + chunk_len < s_response_buffer_size)) {
      memcpy(s_response_buffer + s_response_len, chunk_str, chunk_len);
      s_response_len += chunk_len;
      s_response_buffer[s_response_len] = '\0';
    }

    if (chunk_index == chunk_total - 1) {
      watchdog_timer_cancel();
      if (s_waiting_for_api_response) {
        vibes_short_pulse();
        s_waiting_for_api_response = false;
      }
      s_is_error_screen = false;
      s_last_prompt[0] = '\0';
      if (s_response_buffer) {
        chat_window_update_display(s_current_role, s_response_buffer);
      }
    }
  } else if (action == ACTION_SETTINGS_UPDATE) {
    Tuple *font_tuple = find_tuple_helper(iterator, KEY_FONT_SIZE_VAL, 13);
    if (font_tuple) {
      s_font_size = font_tuple->value->int32;
      persist_write_int(STORAGE_KEY_FONT_SIZE, s_font_size);
      APP_LOG(APP_LOG_LEVEL_INFO, "Updated font size to: %d", s_font_size);
      if (s_chat_text_layer && s_response_buffer) {
        chat_window_update_display(s_current_role, s_response_buffer);
      }
    }
    Tuple *anim_tuple = find_tuple_helper(iterator, KEY_ANIMATE_TITLES_VAL, 14);
    if (anim_tuple) {
      s_animate_titles = (anim_tuple->value->int32 != 0);
      persist_write_bool(STORAGE_KEY_ANIMATE_TITLES, s_animate_titles);
      APP_LOG(APP_LOG_LEVEL_INFO, "Updated animate titles to: %d", (int)s_animate_titles);
      if (s_menu_layer) {
        if (s_animate_titles) {
          marquee_animation_start();
        } else {
          marquee_animation_stop();
        }
        layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
      }
    }
    Tuple *launch_dict_tuple = find_tuple_helper(iterator, KEY_LAUNCH_VOICE_DICTATION_VAL, 15);
    if (launch_dict_tuple) {
      s_launch_voice_dictation = (launch_dict_tuple->value->int32 != 0);
      persist_write_bool(STORAGE_KEY_LAUNCH_VOICE_DICTATION, s_launch_voice_dictation);
      APP_LOG(APP_LOG_LEVEL_INFO, "Updated launch voice dictation to: %d", (int)s_launch_voice_dictation);
    }
  } else if (action == ACTION_ERROR) {
    watchdog_timer_cancel();
    if (s_waiting_for_api_response) {
      vibes_short_pulse();
      s_waiting_for_api_response = false;
      s_total_turns = s_prev_total_turns;
      s_current_turn_index = s_prev_turn_index;
    }
    s_is_error_screen = true;
    Tuple *err_tuple = find_tuple_helper(iterator, KEY_ERROR_VAL, 8);
    if (err_tuple) {
      static char err_display[180];
      snprintf(err_display, sizeof(err_display), "Error:\n%s\n\nSEL - Retry\nBACK - Menu", err_tuple->value->cstring);
      APP_LOG(APP_LOG_LEVEL_ERROR, "Received error from phone: %s", err_tuple->value->cstring);
      chat_window_update_text(err_display);
    }
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage inbox dropped, reason: %d", (int)reason);
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "AppMessage outbox sent successfully to phone");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage outbox failed, reason: %d", (int)reason);
  watchdog_timer_cancel();
  if (s_waiting_for_api_response) {
    vibes_short_pulse();
    s_waiting_for_api_response = false;
    s_total_turns = s_prev_total_turns;
    s_current_turn_index = s_prev_turn_index;
  }
  s_is_error_screen = true;
  chat_window_update_text("Phone connection error.\n\nCould not deliver prompt to phone app.\n\n\n\nSEL - Retry\nBACK - Menu");
}

static void submit_prompt(uint32_t session_id, const char *prompt) {
  if (!prompt || strlen(prompt) == 0) return;

  chat_window_show(session_id);

  s_prev_total_turns = s_total_turns;
  s_prev_turn_index = s_current_turn_index;

  if (prompt != s_last_prompt) {
    strncpy(s_last_prompt, prompt, sizeof(s_last_prompt) - 1);
    s_last_prompt[sizeof(s_last_prompt) - 1] = '\0';
  }
  s_is_error_screen = false;

  if (session_id == 0) {
    s_current_turn_index = 0;
    s_total_turns = 1;
  } else {
    s_total_turns++;
    s_current_turn_index = s_total_turns - 1;
  }

  static char s_initial_prompt_buf[600];
  snprintf(s_initial_prompt_buf, sizeof(s_initial_prompt_buf), "%s\n\nProcessing...", prompt);
  chat_window_update_display(0, s_initial_prompt_buf);

  s_waiting_for_api_response = true;

  watchdog_timer_start();

  send_prompt_to_phone(session_id, prompt);
}

static void dictation_status_callback(DictationSession *session, DictationSessionStatus status, char *transcription, void *context) {
  if (status == DictationSessionStatusSuccess) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Dictation accepted: %s", transcription);
    submit_prompt(s_active_session_id, transcription);
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Dictation ended with status: %d", (int)status);
  }
}

static void start_dictation(uint32_t session_id) {
  s_active_session_id = session_id;
  if (s_dictation_session) {
    dictation_session_start(s_dictation_session);
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Dictation session is NULL");
  }
}

static void launch_dictation_timer_callback(void *data) {
  s_launch_dictation_timer = NULL;
  s_is_error_screen = false;
  s_last_prompt[0] = '\0';
  start_dictation(0);
}

#if defined(PBL_PLATFORM_EMERY)
static void chat_pan_handler(const Recognizer *recognizer, RecognizerEvent event) {
  if (!s_chat_scroll_layer || !pan_recognizer_get_delta_since_start) return;
  switch (event) {
    case RecognizerEvent_Updated: {
      GPoint d = pan_recognizer_get_delta_since_start(recognizer);
      scroll_layer_set_content_offset(s_chat_scroll_layer, GPoint(0, s_scroll_base + d.y), false);
      break;
    }
    case RecognizerEvent_Completed:
      s_scroll_base = scroll_layer_get_content_offset(s_chat_scroll_layer).y;
      break;
    case RecognizerEvent_Cancelled:
      scroll_layer_set_content_offset(s_chat_scroll_layer, GPoint(0, s_scroll_base), true);
      break;
    default:
      break;
  }
}
#endif

static void chat_window_update_display(int role, const char *text) {
  if (!s_chat_text_layer || !s_chat_role_layer || !s_chat_scroll_layer || !s_chat_window) return;
  if (!text) text = "";

  if (strncmp(text, "USER", 4) == 0) {
    role = 0;
    text += 4;
  } else if (strncmp(text, "ASSISTANT", 9) == 0) {
    role = 1;
    text += 9;
  }

  s_current_role = role;

  const char *body_font_key = get_font_key_for_size(s_font_size);
  const char *bold_font_key = get_bold_font_key_for_size(s_font_size);
  text_layer_set_font(s_chat_text_layer, fonts_get_system_font(body_font_key));
  text_layer_set_font(s_chat_role_layer, fonts_get_system_font(bold_font_key));
  if (s_chat_turn_count_layer) {
    text_layer_set_font(s_chat_turn_count_layer, fonts_get_system_font(body_font_key));
  }

  Layer *window_root = window_get_root_layer(s_chat_window);
  GRect bounds = layer_get_bounds(window_root);

  int16_t margin = 4;
  int16_t width = bounds.size.w - (margin * 2);
  int16_t content_height = bounds.size.h;

  if (role == 0 || role == 1) {
    const char *role_str = (role == 0) ? "USER" : "ASSISTANT";
    text_layer_set_text(s_chat_role_layer, role_str);
    layer_set_hidden(text_layer_get_layer(s_chat_role_layer), false);

    GSize role_size = graphics_text_layout_get_content_size(
      role_str,
      fonts_get_system_font(bold_font_key),
      GRect(0, 0, width, 200),
      GTextOverflowModeWordWrap,
      GTextAlignmentLeft
    );
    int16_t role_height = role_size.h + 2;
    layer_set_frame(text_layer_get_layer(s_chat_role_layer), GRect(margin, margin, width, role_height));

    if (s_chat_turn_count_layer) {
      int current_exchange = (s_current_turn_index / 2) + 1;
      int total_exchanges = (s_total_turns + 1) / 2;
      if (total_exchanges < 1) total_exchanges = 1;
      if (current_exchange < 1) current_exchange = 1;
      if (current_exchange > total_exchanges) current_exchange = total_exchanges;

      snprintf(s_turn_count_buf, sizeof(s_turn_count_buf), "%d/%d", current_exchange, total_exchanges);
      text_layer_set_text(s_chat_turn_count_layer, s_turn_count_buf);
      layer_set_frame(text_layer_get_layer(s_chat_turn_count_layer), GRect(margin, margin, width, role_height));
      layer_set_hidden(text_layer_get_layer(s_chat_turn_count_layer), false);
    }

    text_layer_set_text(s_chat_text_layer, text);
    GSize text_size = graphics_text_layout_get_content_size(
      text,
      fonts_get_system_font(body_font_key),
      GRect(0, 0, width, 10000),
      GTextOverflowModeWordWrap,
      GTextAlignmentLeft
    );
    int16_t body_y = margin + role_height + 4;
    int16_t body_height = text_size.h + 8;
    layer_set_frame(text_layer_get_layer(s_chat_text_layer), GRect(margin, body_y, width, body_height));

    content_height = body_y + body_height + margin;
  } else {
    text_layer_set_text(s_chat_role_layer, "");
    layer_set_hidden(text_layer_get_layer(s_chat_role_layer), true);

    if (s_chat_turn_count_layer) {
      text_layer_set_text(s_chat_turn_count_layer, "");
      layer_set_hidden(text_layer_get_layer(s_chat_turn_count_layer), true);
    }

    text_layer_set_text(s_chat_text_layer, text);
    GSize text_size = graphics_text_layout_get_content_size(
      text,
      fonts_get_system_font(body_font_key),
      GRect(0, 0, width, 10000),
      GTextOverflowModeWordWrap,
      GTextAlignmentLeft
    );
    int16_t body_height = text_size.h + 8;
    layer_set_frame(text_layer_get_layer(s_chat_text_layer), GRect(margin, margin, width, body_height));

    content_height = margin + body_height + margin;
  }

  if (content_height < bounds.size.h) {
    content_height = bounds.size.h;
  }

  scroll_layer_set_content_size(s_chat_scroll_layer, GSize(bounds.size.w, content_height));
  scroll_layer_set_content_offset(s_chat_scroll_layer, GPointZero, false);
#if defined(PBL_PLATFORM_EMERY)
  s_scroll_base = 0;
#endif
}

static void chat_window_update_text(const char *text) {
  chat_window_update_display(-1, text);
}

static void chat_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  start_dictation(s_active_session_id);
}

static void chat_select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_is_error_screen) {
    if (strlen(s_last_prompt) > 0) {
      confirm_window_show(CONFIRM_MODE_RETRY_ATTEMPT, s_active_session_id, s_current_turn_index);
    }
    return;
  }
  if (s_active_session_id != 0 && s_current_role != -1) {
    confirm_window_show(CONFIRM_MODE_DELETE_TURN, s_active_session_id, s_current_turn_index);
  }
}

static void chat_up_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_is_error_screen) {
    if (s_active_session_id != 0 && s_total_turns > 0) {
      s_is_error_screen = false;
      s_waiting_for_api_response = false;
      chat_window_update_text("Loading turn...");
      watchdog_timer_start();
      send_get_turn(s_active_session_id, s_current_turn_index);
    }
    return;
  }
  if (s_current_turn_index > 0) {
    s_waiting_for_api_response = false;
    s_current_turn_index--;
    chat_window_update_text("Loading previous turn...");
    watchdog_timer_start();
    send_get_turn(s_active_session_id, s_current_turn_index);
  }
}

static void chat_down_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_is_error_screen) {
    return;
  }
  if (s_current_turn_index < s_total_turns - 1) {
    s_waiting_for_api_response = false;
    s_current_turn_index++;
    chat_window_update_text("Loading next turn...");
    watchdog_timer_start();
    send_get_turn(s_active_session_id, s_current_turn_index);
  }
}

static void chat_scroll_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, chat_select_click_handler);

  window_long_click_subscribe(BUTTON_ID_SELECT, 500, chat_select_long_click_handler, NULL);

  window_long_click_subscribe(BUTTON_ID_UP, 500, chat_up_long_click_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 500, chat_down_long_click_handler, NULL);
}

static void chat_scroll_content_offset_changed_handler(ScrollLayer *scroll_layer, void *context) {
#if defined(PBL_PLATFORM_EMERY)
  s_scroll_base = scroll_layer_get_content_offset(scroll_layer).y;
#endif
}

static void chat_window_load(Window *window) {
  Layer *window_root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_root);

  s_chat_scroll_layer = scroll_layer_create(bounds);
  scroll_layer_set_callbacks(s_chat_scroll_layer, (ScrollLayerCallbacks){
    .click_config_provider = chat_scroll_click_config_provider,
    .content_offset_changed_handler = chat_scroll_content_offset_changed_handler
  });
  scroll_layer_set_click_config_onto_window(s_chat_scroll_layer, window);

  s_chat_role_layer = text_layer_create(GRect(4, 4, bounds.size.w - 8, 24));
  text_layer_set_font(s_chat_role_layer, fonts_get_system_font(get_bold_font_key_for_size(s_font_size)));
  text_layer_set_text_color(s_chat_role_layer, GColorBlack);
  text_layer_set_background_color(s_chat_role_layer, GColorClear);
  text_layer_set_text_alignment(s_chat_role_layer, GTextAlignmentLeft);
  text_layer_set_overflow_mode(s_chat_role_layer, GTextOverflowModeWordWrap);

  s_chat_turn_count_layer = text_layer_create(GRect(4, 4, bounds.size.w - 8, 24));
  text_layer_set_font(s_chat_turn_count_layer, fonts_get_system_font(get_font_key_for_size(s_font_size)));
  text_layer_set_text_color(s_chat_turn_count_layer, GColorBlack);
  text_layer_set_background_color(s_chat_turn_count_layer, GColorClear);
  text_layer_set_text_alignment(s_chat_turn_count_layer, GTextAlignmentRight);
  text_layer_set_overflow_mode(s_chat_turn_count_layer, GTextOverflowModeWordWrap);

  s_chat_text_layer = text_layer_create(GRect(4, 30, bounds.size.w - 8, bounds.size.h - 30));
  text_layer_set_font(s_chat_text_layer, fonts_get_system_font(get_font_key_for_size(s_font_size)));
  text_layer_set_text_color(s_chat_text_layer, GColorBlack);
  text_layer_set_background_color(s_chat_text_layer, GColorClear);
  text_layer_set_overflow_mode(s_chat_text_layer, GTextOverflowModeWordWrap);

  scroll_layer_add_child(s_chat_scroll_layer, text_layer_get_layer(s_chat_role_layer));
  scroll_layer_add_child(s_chat_scroll_layer, text_layer_get_layer(s_chat_turn_count_layer));
  scroll_layer_add_child(s_chat_scroll_layer, text_layer_get_layer(s_chat_text_layer));
  layer_add_child(window_root, scroll_layer_get_layer(s_chat_scroll_layer));

#if defined(PBL_PLATFORM_EMERY)
  if (window_set_touch_bridge_disabled) {
    window_set_touch_bridge_disabled(window, true);
  }
  if (pan_recognizer_create && window_attach_recognizer) {
    Recognizer *pan = pan_recognizer_create(chat_pan_handler, NULL, PanAxis_Vertical);
    if (pan) {
      window_attach_recognizer(window, pan);
    }
  }
#endif
}

static void chat_window_unload(Window *window) {
  watchdog_timer_cancel();
  s_waiting_for_api_response = false;
  s_is_error_screen = false;
  s_last_prompt[0] = '\0';
  if (s_chat_turn_count_layer) {
    text_layer_destroy(s_chat_turn_count_layer);
    s_chat_turn_count_layer = NULL;
  }
  if (s_chat_role_layer) {
    text_layer_destroy(s_chat_role_layer);
    s_chat_role_layer = NULL;
  }
  if (s_chat_text_layer) {
    text_layer_destroy(s_chat_text_layer);
    s_chat_text_layer = NULL;
  }
  if (s_chat_scroll_layer) {
    scroll_layer_destroy(s_chat_scroll_layer);
    s_chat_scroll_layer = NULL;
  }
  if (s_response_buffer) {
    free(s_response_buffer);
    s_response_buffer = NULL;
    s_response_buffer_size = 0;
    s_response_len = 0;
  }
}

static void chat_window_show(uint32_t session_id) {
  s_active_session_id = session_id;
  if (!s_chat_window) {
    s_chat_window = window_create();
    window_set_background_color(s_chat_window, GColorWhite);
    window_set_window_handlers(s_chat_window, (WindowHandlers){
      .load = chat_window_load,
      .unload = chat_window_unload
    });
  }
  if (!window_is_loaded(s_chat_window) || window_stack_get_top_window() != s_chat_window) {
    window_stack_push(s_chat_window, true);
  }
}

static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return (s_session_count > 0) ? 2 : 1;
}

static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  if (section_index == 0) {
    return 1;
  } else if (section_index == 1) {
    return s_session_count;
  }
  return 0;
}

static int16_t menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void menu_draw_header_callback(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  if (section_index == 0) {
    menu_cell_basic_header_draw(ctx, cell_layer, "PebbleChat");
  } else if (section_index == 1) {
    menu_cell_basic_header_draw(ctx, cell_layer, "Recent Chats");
  }
}

static int16_t menu_get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  return 36;
}

static void marquee_animation_stop(void) {
  if (s_marquee_timer != NULL) {
    app_timer_cancel(s_marquee_timer);
    s_marquee_timer = NULL;
  }
  s_title_scroll_offset = 0;
  s_marquee_state = MARQUEE_STATE_PAUSE_START;
}

static void marquee_timer_callback(void *data) {
  s_marquee_timer = NULL;

  if (!s_menu_layer || !s_animate_titles) {
    s_title_scroll_offset = 0;
    return;
  }

  if (!s_menu_window || !window_is_loaded(s_menu_window) || window_stack_get_top_window() != s_menu_window) {
    s_title_scroll_offset = 0;
    return;
  }

  MenuIndex selected = menu_layer_get_selected_index(s_menu_layer);
  if (selected.section != 1 || selected.row >= s_session_count) {
    s_title_scroll_offset = 0;
    return;
  }

  Layer *root_layer = menu_layer_get_layer(s_menu_layer);
  GRect bounds = layer_get_bounds(root_layer);
  const int16_t margin = 8;
  int16_t avail_w = bounds.size.w - (margin * 2);

  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  const char *title = s_sessions[selected.row].title;
  GSize text_size = graphics_text_layout_get_content_size(
    title,
    font,
    GRect(0, 0, 1000, bounds.size.h),
    GTextOverflowModeWordWrap,
    GTextAlignmentLeft
  );

  int16_t max_scroll = text_size.w - avail_w;
  if (max_scroll <= 0) {
    s_title_scroll_offset = 0;
    return;
  }

  uint32_t next_timeout_ms = MARQUEE_TICK_MS;

  switch (s_marquee_state) {
    case MARQUEE_STATE_PAUSE_START:
      s_marquee_state = MARQUEE_STATE_SCROLL_LEFT;
      s_title_scroll_offset = 0;
      next_timeout_ms = MARQUEE_TICK_MS;
      break;

    case MARQUEE_STATE_SCROLL_LEFT:
      s_title_scroll_offset += MARQUEE_STEP_PX;
      if (s_title_scroll_offset >= max_scroll) {
        s_title_scroll_offset = max_scroll;
        s_marquee_state = MARQUEE_STATE_PAUSE_END;
        next_timeout_ms = MARQUEE_PAUSE_MS;
      } else {
        next_timeout_ms = MARQUEE_TICK_MS;
      }
      break;

    case MARQUEE_STATE_PAUSE_END:
      s_marquee_state = MARQUEE_STATE_SCROLL_RIGHT;
      next_timeout_ms = MARQUEE_TICK_MS;
      break;

    case MARQUEE_STATE_SCROLL_RIGHT:
      s_title_scroll_offset -= MARQUEE_STEP_PX;
      if (s_title_scroll_offset <= 0) {
        s_title_scroll_offset = 0;
        s_marquee_state = MARQUEE_STATE_PAUSE_START;
        next_timeout_ms = MARQUEE_PAUSE_MS;
      } else {
        next_timeout_ms = MARQUEE_TICK_MS;
      }
      break;
  }

  layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
  s_marquee_timer = app_timer_register(next_timeout_ms, marquee_timer_callback, NULL);
}

static void marquee_animation_start(void) {
  marquee_animation_stop();

  if (!s_menu_layer || !s_animate_titles) {
    return;
  }

  if (!s_menu_window || !window_is_loaded(s_menu_window) || window_stack_get_top_window() != s_menu_window) {
    return;
  }

  MenuIndex selected = menu_layer_get_selected_index(s_menu_layer);
  if (selected.section != 1 || selected.row >= s_session_count) {
    return;
  }

  Layer *root_layer = menu_layer_get_layer(s_menu_layer);
  GRect bounds = layer_get_bounds(root_layer);
  const int16_t margin = 8;
  int16_t avail_w = bounds.size.w - (margin * 2);

  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  const char *title = s_sessions[selected.row].title;
  GSize text_size = graphics_text_layout_get_content_size(
    title,
    font,
    GRect(0, 0, 1000, bounds.size.h),
    GTextOverflowModeWordWrap,
    GTextAlignmentLeft
  );

  if (text_size.w > avail_w) {
    s_title_scroll_offset = 0;
    s_marquee_state = MARQUEE_STATE_PAUSE_START;
    s_marquee_timer = app_timer_register(MARQUEE_PAUSE_MS, marquee_timer_callback, NULL);
  }
}

static void menu_selection_changed_callback(MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *data) {
  marquee_animation_start();
  layer_mark_dirty(menu_layer_get_layer(menu_layer));
}

static void menu_draw_recent_chat_row(GContext *ctx, const Layer *cell_layer, const char *title, bool is_highlighted) {
  if (!title) title = "";

  GRect bounds = layer_get_bounds(cell_layer);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

  const int16_t margin = 8;
  int16_t avail_w = bounds.size.w - (margin * 2);

  GSize text_size = graphics_text_layout_get_content_size(
    title,
    font,
    GRect(0, 0, 1000, bounds.size.h),
    GTextOverflowModeWordWrap,
    GTextAlignmentLeft
  );

  int16_t y = (bounds.size.h - text_size.h) / 2;
  if (y > 2) {
    y -= 2;
  }

#ifdef PBL_COLOR
  graphics_context_set_text_color(ctx, is_highlighted ? GColorWhite : GColorBlack);
#else
  graphics_context_set_text_color(ctx, is_highlighted ? GColorWhite : GColorBlack);
#endif

  if (is_highlighted && s_animate_titles && text_size.w > avail_w) {
    int16_t draw_x = margin - s_title_scroll_offset;
    GRect text_box = GRect(draw_x, y, text_size.w + 20, bounds.size.h);
    graphics_draw_text(ctx, title, font, text_box, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  } else {
    GRect text_box = GRect(margin, y, avail_w, bounds.size.h);
    graphics_draw_text(ctx, title, font, text_box, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  if (cell_index->section == 0) {
    menu_cell_basic_draw(ctx, cell_layer, "New Session", NULL, NULL);
  } else if (cell_index->section == 1) {
    if (cell_index->row < s_session_count) {
      bool is_highlighted = menu_cell_layer_is_highlighted(cell_layer);
      menu_draw_recent_chat_row(ctx, cell_layer, s_sessions[cell_index->row].title, is_highlighted);
    }
  }
}

static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  if (cell_index->section == 0) {
    s_is_error_screen = false;
    s_last_prompt[0] = '\0';
    start_dictation(0);
  } else if (cell_index->section == 1) {
    if (cell_index->row < s_session_count) {
      s_waiting_for_api_response = false;
      s_is_error_screen = false;
      s_last_prompt[0] = '\0';
      uint32_t chosen_id = s_sessions[cell_index->row].id;
      chat_window_show(chosen_id);
      chat_window_update_text("Loading session...");
      watchdog_timer_start();
      send_load_session(chosen_id);
    }
  }
}

static void confirm_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_confirm_mode == CONFIRM_MODE_DELETE_SESSION) {
    uint32_t target_id = s_confirm_session_id;
    send_delete_session(target_id);

    for (int i = 0; i < s_session_count; i++) {
      if (s_sessions[i].id == target_id) {
        for (int j = i; j < s_session_count - 1; j++) {
          s_sessions[j] = s_sessions[j + 1];
        }
        s_session_count--;
        break;
      }
    }
    if (s_menu_layer) {
      menu_layer_reload_data(s_menu_layer);
      marquee_animation_start();
    }
    window_stack_pop(true);
  } else if (s_confirm_mode == CONFIRM_MODE_DELETE_TURN) {
    int exchange_num = (s_confirm_turn_index / 2) + 1;
    if (exchange_num <= 1) {
      uint32_t target_id = s_confirm_session_id;
      send_delete_session(target_id);

      for (int i = 0; i < s_session_count; i++) {
        if (s_sessions[i].id == target_id) {
          for (int j = i; j < s_session_count - 1; j++) {
            s_sessions[j] = s_sessions[j + 1];
          }
          s_session_count--;
          break;
        }
      }
      if (s_menu_layer) {
        menu_layer_reload_data(s_menu_layer);
        marquee_animation_start();
      }

      window_stack_pop(false);
      if (s_chat_window && window_is_loaded(s_chat_window)) {
        window_stack_pop(true);
      }
    } else {
      s_waiting_for_api_response = false;
      send_delete_turns(s_confirm_session_id, s_confirm_turn_index);
      chat_window_update_text("Resuming chat...");
      watchdog_timer_start();
      window_stack_pop(true);
    }
  } else if (s_confirm_mode == CONFIRM_MODE_RETRY_ATTEMPT) {
    window_stack_pop(true);
    if (strlen(s_last_prompt) > 0) {
      submit_prompt(s_active_session_id, s_last_prompt);
    }
  }
}

static void confirm_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  window_stack_pop(true);
}

static void confirm_select_click_handler(ClickRecognizerRef recognizer, void *context) {
}

static void confirm_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, confirm_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, confirm_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, confirm_select_click_handler);
}

static void confirm_window_load(Window *window) {
  Layer *window_root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_root);

  int16_t margin = 10;
  int16_t y = (bounds.size.h > 180) ? 28 : 12;
  s_confirm_text_layer = text_layer_create(GRect(margin, y, bounds.size.w - (margin * 2), bounds.size.h - y - 8));
  text_layer_set_text_color(s_confirm_text_layer, GColorBlack);
  text_layer_set_background_color(s_confirm_text_layer, GColorClear);
  text_layer_set_text_alignment(s_confirm_text_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_confirm_text_layer, GTextOverflowModeWordWrap);
  text_layer_set_font(s_confirm_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_confirm_text_layer, s_confirm_prompt_buf);

  layer_add_child(window_root, text_layer_get_layer(s_confirm_text_layer));
}

static void confirm_window_unload(Window *window) {
  s_confirm_mode = CONFIRM_MODE_NONE;
  if (s_confirm_text_layer) {
    text_layer_destroy(s_confirm_text_layer);
    s_confirm_text_layer = NULL;
  }
}

static void confirm_window_show(ConfirmMode mode, uint32_t session_id, int turn_index) {
  s_confirm_mode = mode;
  s_confirm_session_id = session_id;
  s_confirm_turn_index = turn_index;

  if (mode == CONFIRM_MODE_DELETE_SESSION) {
    snprintf(s_confirm_prompt_buf, sizeof(s_confirm_prompt_buf), "Delete this chat session?\n\nUP - Yes\nDOWN - No");
  } else if (mode == CONFIRM_MODE_DELETE_TURN) {
    snprintf(s_confirm_prompt_buf, sizeof(s_confirm_prompt_buf), "Delete this chat turn?\n\nUP - Yes\nDOWN - No");
  } else if (mode == CONFIRM_MODE_RETRY_ATTEMPT) {
    snprintf(s_confirm_prompt_buf, sizeof(s_confirm_prompt_buf), "Retry last attempt?\n\nUP - Yes\nDOWN - No");
  } else {
    return;
  }

  if (!s_confirm_window) {
    s_confirm_window = window_create();
    window_set_background_color(s_confirm_window, GColorWhite);
    window_set_window_handlers(s_confirm_window, (WindowHandlers){
      .load = confirm_window_load,
      .unload = confirm_window_unload
    });
    window_set_click_config_provider(s_confirm_window, confirm_click_config_provider);
  }

  if (s_confirm_text_layer) {
    text_layer_set_text(s_confirm_text_layer, s_confirm_prompt_buf);
  }

  if (!window_is_loaded(s_confirm_window) || window_stack_get_top_window() != s_confirm_window) {
    window_stack_push(s_confirm_window, true);
  }
}

static void menu_select_long_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  if (cell_index->section == 1 && cell_index->row < s_session_count) {
    uint32_t chosen_id = s_sessions[cell_index->row].id;
    confirm_window_show(CONFIRM_MODE_DELETE_SESSION, chosen_id, 0);
  }
}

static void menu_window_appear(Window *window) {
  send_fetch_sessions();
  marquee_animation_start();
}

static void menu_window_disappear(Window *window) {
  marquee_animation_stop();
}

static void menu_window_load(Window *window) {
  Layer *window_root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_root);

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_sections = menu_get_num_sections_callback,
    .get_num_rows = menu_get_num_rows_callback,
    .get_header_height = menu_get_header_height_callback,
    .draw_header = menu_draw_header_callback,
    .get_cell_height = menu_get_cell_height_callback,
    .draw_row = menu_draw_row_callback,
    .select_click = menu_select_callback,
    .select_long_click = menu_select_long_callback,
    .selection_changed = menu_selection_changed_callback
  });

#ifdef PBL_COLOR
  menu_layer_set_highlight_colors(s_menu_layer, GColorCobaltBlue, GColorWhite);
#endif

  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(window_root, menu_layer_get_layer(s_menu_layer));

  menu_layer_set_selected_index(s_menu_layer, (MenuIndex){.section = 0, .row = 0}, MenuRowAlignTop, false);
}

static void menu_window_unload(Window *window) {
  marquee_animation_stop();
  if (s_menu_layer) {
    menu_layer_destroy(s_menu_layer);
    s_menu_layer = NULL;
  }
}

static void init(void) {
#if defined(PBL_PLATFORM_EMERY)
  if (app_touch_navigation_enable) {
    app_touch_navigation_enable(true);
  }
#endif

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_register_outbox_failed(outbox_failed_callback);

  app_message_open(2048, 1024);

  if (persist_exists(STORAGE_KEY_FONT_SIZE)) {
    s_font_size = persist_read_int(STORAGE_KEY_FONT_SIZE);
  } else {
    s_font_size = 24;
  }

  if (persist_exists(STORAGE_KEY_ANIMATE_TITLES)) {
    s_animate_titles = persist_read_bool(STORAGE_KEY_ANIMATE_TITLES);
  } else {
    s_animate_titles = false;
  }

  if (persist_exists(STORAGE_KEY_LAUNCH_VOICE_DICTATION)) {
    s_launch_voice_dictation = persist_read_bool(STORAGE_KEY_LAUNCH_VOICE_DICTATION);
  } else {
    s_launch_voice_dictation = false;
  }

  s_dictation_session = dictation_session_create(512, dictation_status_callback, NULL);
  if (s_dictation_session) {
    dictation_session_enable_confirmation(s_dictation_session, true);
  }

  s_menu_window = window_create();
  window_set_window_handlers(s_menu_window, (WindowHandlers){
    .load = menu_window_load,
    .appear = menu_window_appear,
    .disappear = menu_window_disappear,
    .unload = menu_window_unload
  });
  window_stack_push(s_menu_window, !s_launch_voice_dictation);

  if (s_launch_voice_dictation) {
    s_launch_dictation_timer = app_timer_register(100, launch_dictation_timer_callback, NULL);
  }
}

static void deinit(void) {
  watchdog_timer_cancel();
  marquee_animation_stop();

  if (s_launch_dictation_timer != NULL) {
    app_timer_cancel(s_launch_dictation_timer);
    s_launch_dictation_timer = NULL;
  }

  if (s_dictation_session) {
    dictation_session_destroy(s_dictation_session);
    s_dictation_session = NULL;
  }

  if (s_confirm_window) {
    window_destroy(s_confirm_window);
    s_confirm_window = NULL;
  }

  if (s_chat_window) {
    window_destroy(s_chat_window);
    s_chat_window = NULL;
  }

  if (s_menu_window) {
    window_destroy(s_menu_window);
    s_menu_window = NULL;
  }

  app_message_deregister_callbacks();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
