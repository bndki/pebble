#include <pebble.h>

#define STORAGE_KEY_USE_FEELS            100
#define STORAGE_KEY_USE_FAHRENHEIT       101
#define STORAGE_KEY_WEATHER_CACHE        102
#define STORAGE_KEY_ACTIVE_LOCATION_SLOT 103
#define STORAGE_KEY_WEATHER_CACHE_PART2  104
#define STORAGE_KEY_ENABLE_TEMP_BAR      105
#define STORAGE_KEY_ENABLE_HISTOGRAM     106
#define STORAGE_KEY_WEATHER_CACHE_PART3  107
#define STORAGE_KEY_ENABLE_TIME_BAR      108

typedef enum {
  PAGE_NOW = 0,
  PAGE_HOURLY = 1,
  PAGE_DAILY = 2,
  PAGE_COUNT = 3
} AppPages;

typedef enum {
  ICON_SUNNY = 0,
  ICON_CLEAR_NIGHT,
  ICON_SUNNY_OVERCAST,
  ICON_NIGHT_PARTLY_CLOUDY,
  ICON_DAY_CLOUDY,
  ICON_NIGHT_CLOUDY,
  ICON_CLOUDY,
  ICON_FOG,
  ICON_SHOWERS,
  ICON_RAIN,
  ICON_SNOW,
  ICON_THUNDERSTORM,
  ICON_COUNT = 12
} WeatherIconId;

typedef struct __attribute__((__packed__)) {
  uint8_t hour;
  int8_t temp_c;
  int8_t feels_c;
  uint8_t precip_pct;
  uint8_t icon;
} HourlyWeather;

typedef struct __attribute__((__packed__)) {
  uint8_t day_of_week;
  int8_t high_temp_c;
  int8_t low_temp_c;
  int8_t high_feels_c;
  int8_t low_feels_c;
  uint8_t precip_pct;
  uint8_t icon;
} DailyWeather;

typedef struct __attribute__((__packed__)) {
  bool has_data;
  bool is_gps;
  bool no_location;
  uint8_t location_slot;
  char location_name[32];
  int8_t current_temp_c;
  int8_t current_feels_c;
  uint8_t current_icon;
  int8_t today_high_c;
  int8_t today_low_c;
  int8_t today_feels_high_c;
  int8_t today_feels_low_c;
  uint8_t today_precip_pct;
  uint8_t today_humidity_pct;
  uint16_t today_wind_speed_kmh;
  uint8_t today_uv_index;
  char sunrise_time[8];
  char sunset_time[8];
  uint8_t moon_phase_icon;
  uint8_t hourly_count;
  HourlyWeather hourly[24];
  uint8_t daily_count;
  DailyWeather daily[14];
  uint8_t hourly_precip_count;
  uint16_t hourly_precip[96];
  uint8_t daily_precip_count;
  uint16_t daily_precip[14];
} WeatherData;

static const uint32_t LARGE_ICON_RES_IDS[ICON_COUNT] = {
  RESOURCE_ID_IMAGE_ICON_SUNNY_LARGE,
  RESOURCE_ID_IMAGE_ICON_CLEAR_NIGHT_LARGE,
  RESOURCE_ID_IMAGE_ICON_SUNNY_OVERCAST_LARGE,
  RESOURCE_ID_IMAGE_ICON_NIGHT_PARTLY_CLOUDY_LARGE,
  RESOURCE_ID_IMAGE_ICON_DAY_CLOUDY_LARGE,
  RESOURCE_ID_IMAGE_ICON_NIGHT_CLOUDY_LARGE,
  RESOURCE_ID_IMAGE_ICON_CLOUDY_LARGE,
  RESOURCE_ID_IMAGE_ICON_FOG_LARGE,
  RESOURCE_ID_IMAGE_ICON_SHOWERS_LARGE,
  RESOURCE_ID_IMAGE_ICON_RAIN_LARGE,
  RESOURCE_ID_IMAGE_ICON_SNOW_LARGE,
  RESOURCE_ID_IMAGE_ICON_THUNDERSTORM_LARGE
};

static const uint32_t SMALL_ICON_RES_IDS[ICON_COUNT] = {
  RESOURCE_ID_IMAGE_ICON_SUNNY_SMALL,
  RESOURCE_ID_IMAGE_ICON_CLEAR_NIGHT_SMALL,
  RESOURCE_ID_IMAGE_ICON_SUNNY_OVERCAST_SMALL,
  RESOURCE_ID_IMAGE_ICON_NIGHT_PARTLY_CLOUDY_SMALL,
  RESOURCE_ID_IMAGE_ICON_DAY_CLOUDY_SMALL,
  RESOURCE_ID_IMAGE_ICON_NIGHT_CLOUDY_SMALL,
  RESOURCE_ID_IMAGE_ICON_CLOUDY_SMALL,
  RESOURCE_ID_IMAGE_ICON_FOG_SMALL,
  RESOURCE_ID_IMAGE_ICON_SHOWERS_SMALL,
  RESOURCE_ID_IMAGE_ICON_RAIN_SMALL,
  RESOURCE_ID_IMAGE_ICON_SNOW_SMALL,
  RESOURCE_ID_IMAGE_ICON_THUNDERSTORM_SMALL
};

#define MOON_ICON_COUNT 28
static const uint32_t MOON_ICON_RES_IDS[MOON_ICON_COUNT] = {
  RESOURCE_ID_IMAGE_ICON_MOON_0,
  RESOURCE_ID_IMAGE_ICON_MOON_1,
  RESOURCE_ID_IMAGE_ICON_MOON_2,
  RESOURCE_ID_IMAGE_ICON_MOON_3,
  RESOURCE_ID_IMAGE_ICON_MOON_4,
  RESOURCE_ID_IMAGE_ICON_MOON_5,
  RESOURCE_ID_IMAGE_ICON_MOON_6,
  RESOURCE_ID_IMAGE_ICON_MOON_7,
  RESOURCE_ID_IMAGE_ICON_MOON_8,
  RESOURCE_ID_IMAGE_ICON_MOON_9,
  RESOURCE_ID_IMAGE_ICON_MOON_10,
  RESOURCE_ID_IMAGE_ICON_MOON_11,
  RESOURCE_ID_IMAGE_ICON_MOON_12,
  RESOURCE_ID_IMAGE_ICON_MOON_13,
  RESOURCE_ID_IMAGE_ICON_MOON_14,
  RESOURCE_ID_IMAGE_ICON_MOON_15,
  RESOURCE_ID_IMAGE_ICON_MOON_16,
  RESOURCE_ID_IMAGE_ICON_MOON_17,
  RESOURCE_ID_IMAGE_ICON_MOON_18,
  RESOURCE_ID_IMAGE_ICON_MOON_19,
  RESOURCE_ID_IMAGE_ICON_MOON_20,
  RESOURCE_ID_IMAGE_ICON_MOON_21,
  RESOURCE_ID_IMAGE_ICON_MOON_22,
  RESOURCE_ID_IMAGE_ICON_MOON_23,
  RESOURCE_ID_IMAGE_ICON_MOON_24,
  RESOURCE_ID_IMAGE_ICON_MOON_25,
  RESOURCE_ID_IMAGE_ICON_MOON_26,
  RESOURCE_ID_IMAGE_ICON_MOON_27
};

static const char *const DAY_NAMES[7] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static Window *s_main_window;
static Layer *s_page_layer;

#ifndef MESSAGE_KEY_KEY_ENABLE_CURRENT_TEMP_BAR
#define MESSAGE_KEY_KEY_ENABLE_CURRENT_TEMP_BAR 24
#endif
#ifndef MESSAGE_KEY_KEY_ENABLE_PRECIP_HISTOGRAM
#define MESSAGE_KEY_KEY_ENABLE_PRECIP_HISTOGRAM 25
#endif
#ifndef MESSAGE_KEY_KEY_HOURLY_PRECIP_DATA
#define MESSAGE_KEY_KEY_HOURLY_PRECIP_DATA 26
#endif
#ifndef MESSAGE_KEY_KEY_DAILY_PRECIP_DATA
#define MESSAGE_KEY_KEY_DAILY_PRECIP_DATA 27
#endif
#ifndef MESSAGE_KEY_KEY_ENABLE_CURRENT_TIME_BAR
#define MESSAGE_KEY_KEY_ENABLE_CURRENT_TIME_BAR 28
#endif

static int s_current_page = PAGE_NOW;
static bool s_use_feels = false;
static bool s_use_fahrenheit = false;
static bool s_enable_temp_bar = false;
static bool s_enable_histogram = false;
static bool s_enable_time_bar = false;
static WeatherData s_weather;

#define MAX_SAVED_LOCATIONS 8
static char s_location_names[MAX_SAVED_LOCATIONS][32];
static uint8_t s_location_count = 1;
static uint8_t s_active_location_slot = 0;

static int16_t s_hourly_scroll_x = 0;
static int16_t s_daily_scroll_x = 0;
static int16_t s_base_scroll_x = 0;
static int16_t s_hourly_max_scroll = 760;
static int16_t s_daily_max_scroll = 360;

static GBitmap *s_gps_icon = NULL;
static GBitmap *s_small_icons[ICON_COUNT] = { NULL };
static GBitmap *s_curr_large_icon = NULL;
static int s_curr_large_icon_id = -1;

#if defined(PBL_PLATFORM_EMERY)
static Recognizer *s_pan_recognizer = NULL;
static int16_t s_touch_start_x = 0;
static bool s_touch_active = false;
#endif

static void up_click_handler(ClickRecognizerRef recognizer, void *context);
static void down_click_handler(ClickRecognizerRef recognizer, void *context);
static void select_single_click_handler(ClickRecognizerRef recognizer, void *context);
static void select_long_click_handler(ClickRecognizerRef recognizer, void *context);
static void click_config_provider(void *context);

static int convert_temp(int temp_c, bool use_f) {
  if (use_f) {
    float f = (float)temp_c * 1.8f + 32.0f;
    return (int)(f >= 0 ? f + 0.5f : f - 0.5f);
  }
  return temp_c;
}

static const char* get_uv_status(int uv) {
  if (uv <= 2) return "Low";
  if (uv <= 5) return "Moderate";
  if (uv <= 7) return "High";
  if (uv <= 10) return "Very High";
  return "Extreme";
}

static int convert_wind_speed(int speed_kmh, bool use_f) {
  if (use_f) {
    float mph = (float)speed_kmh * 0.621371f;
    return (int)(mph + 0.5f);
  }
  return speed_kmh;
}

static GBitmap *get_small_icon(uint8_t icon_id) {
  if (icon_id >= ICON_COUNT) icon_id = 0;
  if (!s_small_icons[icon_id]) {
    s_small_icons[icon_id] = gbitmap_create_with_resource(SMALL_ICON_RES_IDS[icon_id]);
  }
  return s_small_icons[icon_id];
}

static GBitmap *get_large_icon(uint8_t icon_id) {
  if (icon_id >= ICON_COUNT) icon_id = 0;
  if (s_curr_large_icon_id != (int)icon_id) {
    if (s_curr_large_icon) {
      gbitmap_destroy(s_curr_large_icon);
      s_curr_large_icon = NULL;
    }
    s_curr_large_icon = gbitmap_create_with_resource(LARGE_ICON_RES_IDS[icon_id]);
    s_curr_large_icon_id = icon_id;
  }
  return s_curr_large_icon;
}

static GBitmap *get_gps_icon(void) {
  if (!s_gps_icon) {
    s_gps_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_GPS_ICON);
  }
  return s_gps_icon;
}

static GBitmap *s_sunrise_icon = NULL;
static GBitmap *s_sunset_icon = NULL;
static GBitmap *s_curr_moon_icon = NULL;
static int s_curr_moon_icon_id = -1;

static GBitmap *get_sunrise_icon(void) {
  if (!s_sunrise_icon) {
    s_sunrise_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_SUNRISE);
  }
  return s_sunrise_icon;
}

static GBitmap *get_sunset_icon(void) {
  if (!s_sunset_icon) {
    s_sunset_icon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_SUNSET);
  }
  return s_sunset_icon;
}

static GBitmap *get_moon_icon(uint8_t icon_id) {
  if (icon_id >= MOON_ICON_COUNT) icon_id = 0;
  if (s_curr_moon_icon_id != (int)icon_id) {
    if (s_curr_moon_icon) {
      gbitmap_destroy(s_curr_moon_icon);
      s_curr_moon_icon = NULL;
    }
    s_curr_moon_icon = gbitmap_create_with_resource(MOON_ICON_RES_IDS[icon_id]);
    s_curr_moon_icon_id = icon_id;
  }
  return s_curr_moon_icon;
}

static void draw_top_section(GContext *ctx, GRect bounds) {
  GFont font_loc = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  graphics_context_set_text_color(ctx, GColorBlack);

  const char *loc_name = "PEBBLERAIN";
  if (s_weather.no_location) {
    loc_name = "NO LOCATION";
  } else if (s_weather.location_name[0] != '\0') {
    loc_name = s_weather.location_name;
  }

  if (!s_weather.no_location && s_weather.is_gps) {
    GBitmap *gps = get_gps_icon();
    GSize text_sz = graphics_text_layout_get_content_size(loc_name, font_loc,
                                                          GRect(0, 0, bounds.size.w - 28, 18),
                                                          GTextOverflowModeTrailingEllipsis,
                                                          GTextAlignmentLeft);
    int total_w = 16 + 4 + text_sz.w;
    int start_x = (bounds.size.w - total_w) / 2;
    if (start_x < 2) start_x = 2;

    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, gps, GRect(start_x, 2, 16, 16));

    graphics_draw_text(ctx, loc_name, font_loc,
                       GRect(start_x + 20, 1, bounds.size.w - (start_x + 20), 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  } else {
    graphics_draw_text(ctx, loc_name, font_loc,
                       GRect(0, 1, bounds.size.w, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, 20), GPoint(bounds.size.w, 20));
}

static void draw_section_header(GContext *ctx, GRect bounds, const char *title, GColor title_color) {
  graphics_context_set_text_color(ctx, title_color);
  int title_w = (bounds.size.h >= 200) ? 76 : 56;
  graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect((bounds.size.h >= 200) ? 8 : 4, 22, title_w, 20),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  const char *feel_label;
  if (s_use_feels) {
    feel_label = s_use_fahrenheit ? "FEELS-LIKE °F" : "FEELS-LIKE °C";
  } else {
    feel_label = s_use_fahrenheit ? "ACTUAL °F" : "ACTUAL °C";
  }

  graphics_context_set_text_color(ctx, GColorBlack);
  int label_w = (bounds.size.h >= 200) ? 106 : 84;
  int label_x = bounds.size.w - label_w - ((bounds.size.h >= 200) ? 8 : 4);
  graphics_draw_text(ctx, feel_label, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(label_x, 25, label_w, 18), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}

static void draw_no_location_message(GContext *ctx, GRect bounds) {
  graphics_context_set_text_color(ctx, GColorBlack);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  int y = (bounds.size.h >= 200) ? 95 : 68;
  graphics_draw_text(ctx, "Set location in settings", font,
                     GRect(10, y, bounds.size.w - 20, 60),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void draw_loading_message(GContext *ctx, GRect bounds) {
  graphics_context_set_text_color(ctx, GColorBlack);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  int y = (bounds.size.h >= 200) ? 95 : 68;
  graphics_draw_text(ctx, "Loading", font,
                     GRect(10, y, bounds.size.w - 20, 60),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void draw_page_now(GContext *ctx, GRect bounds) {
  draw_top_section(ctx, bounds);

  #if defined(PBL_COLOR)
  GColor orange_header = GColorOrange;
  #else
  GColor orange_header = GColorBlack;
  #endif

  draw_section_header(ctx, bounds, "NOW", orange_header);

  if (s_weather.no_location) {
    draw_no_location_message(ctx, bounds);
    return;
  }

  if (!s_weather.has_data) {
    draw_loading_message(ctx, bounds);
    return;
  }

  GBitmap *large_icon = get_large_icon(s_weather.current_icon);
  if (large_icon) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    int icon_y = (bounds.size.h >= 200) ? 49 : 42;
    graphics_draw_bitmap_in_rect(ctx, large_icon, GRect(10, icon_y, 48, 48));
  }

  int curr_temp_c = s_use_feels ? s_weather.current_feels_c : s_weather.current_temp_c;
  int curr_temp = convert_temp(curr_temp_c, s_use_fahrenheit);

  char num_buf[12];
  snprintf(num_buf, sizeof(num_buf), "%d", curr_temp);
  const char *unit_str = s_use_fahrenheit ? "°F" : "°C";

  GFont font_num;
  GFont font_unit;
  int num_y, unit_y;

  if (bounds.size.h >= 200) {
    bool is_large_num = (curr_temp >= 100 || curr_temp <= -10);
    font_num = fonts_get_system_font(is_large_num ? FONT_KEY_BITHAM_30_BLACK : FONT_KEY_BITHAM_42_BOLD);
    font_unit = fonts_get_system_font(is_large_num ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_28_BOLD);
    num_y = is_large_num ? 51 : 46;
    unit_y = is_large_num ? 50 : 48;
  } else {
    bool is_large_num = (curr_temp >= 100 || curr_temp <= -10);
    font_num = fonts_get_system_font(is_large_num ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_28_BOLD);
    font_unit = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    num_y = 42;
    unit_y = 42;
  }

  GSize num_size = graphics_text_layout_get_content_size(num_buf, font_num,
                                                         GRect(0, 0, 100, 48),
                                                         GTextOverflowModeTrailingEllipsis,
                                                         GTextAlignmentLeft);
  GSize unit_size = graphics_text_layout_get_content_size(unit_str, font_unit,
                                                          GRect(0, 0, 40, 36),
                                                          GTextOverflowModeTrailingEllipsis,
                                                          GTextAlignmentLeft);

  int total_temp_w = num_size.w + unit_size.w + 2;
  int mid_area_start = 58;
  int mid_area_w = (bounds.size.w - 44) - mid_area_start;
  int temp_start_x = mid_area_start + (mid_area_w - total_temp_w) / 2 + 4;
  if (temp_start_x < 60) temp_start_x = 60;

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, num_buf, font_num,
                     GRect(temp_start_x, num_y, num_size.w + 2, 48),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  graphics_draw_text(ctx, unit_str, font_unit,
                     GRect(temp_start_x + num_size.w + 2, unit_y, unit_size.w + 4, 34),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  int hi_c = s_use_feels ? s_weather.today_feels_high_c : s_weather.today_high_c;
  int lo_c = s_use_feels ? s_weather.today_feels_low_c : s_weather.today_low_c;
  int hi = convert_temp(hi_c, s_use_fahrenheit);
  int lo = convert_temp(lo_c, s_use_fahrenheit);

  char hi_buf[12];
  char lo_buf[12];
  snprintf(hi_buf, sizeof(hi_buf), "%d°", hi);
  snprintf(lo_buf, sizeof(lo_buf), "%d°", lo);

  int hi_y = (bounds.size.h >= 200) ? 49 : 44;
  int lo_y = (bounds.size.h >= 200) ? 73 : 66;

  graphics_draw_text(ctx, hi_buf, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(bounds.size.w - 44, hi_y, 40, 20), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, lo_buf, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(bounds.size.w - 44, lo_y, 40, 20), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  int div_y = (bounds.size.h >= 200) ? 106 : 82;
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, div_y), GPoint(bounds.size.w, div_y));

  int today_y = (bounds.size.h >= 200) ? 110 : 84;
  graphics_context_set_text_color(ctx, orange_header);
  int today_title_x = (bounds.size.h >= 200) ? 8 : 4;
  int today_title_w = (bounds.size.h >= 200) ? 70 : 54;
  graphics_draw_text(ctx, "TODAY", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(today_title_x, today_y, today_title_w, 20), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  if (s_weather.sunrise_time[0] != '\0' && s_weather.sunset_time[0] != '\0') {
    GFont font_sun_time = fonts_get_system_font(FONT_KEY_GOTHIC_14);
    GBitmap *icon_sr = get_sunrise_icon();
    GBitmap *icon_ss = get_sunset_icon();

    GSize sr_size = graphics_text_layout_get_content_size(s_weather.sunrise_time, font_sun_time,
                                                          GRect(0, 0, 50, 20),
                                                          GTextOverflowModeTrailingEllipsis,
                                                          GTextAlignmentLeft);
    GSize ss_size = graphics_text_layout_get_content_size(s_weather.sunset_time, font_sun_time,
                                                          GRect(0, 0, 50, 20),
                                                          GTextOverflowModeTrailingEllipsis,
                                                          GTextAlignmentLeft);

    int icon_w = 18;
    int gap_time = 2;
    int gap_sections = (bounds.size.w >= 200) ? 6 : 3;
    int right_margin = (bounds.size.w >= 200) ? 4 : 2;

    int ss_time_x = bounds.size.w - right_margin - ss_size.w;
    int ss_icon_x = ss_time_x - gap_time - icon_w;
    int sr_time_x = ss_icon_x - gap_sections - sr_size.w;
    int sr_icon_x = sr_time_x - gap_time - icon_w;

    int sun_icon_y = (bounds.size.h >= 200) ? 113 : 87;
    int sun_text_y = (bounds.size.h >= 200) ? 113 : 87;

    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    if (icon_sr) {
      graphics_draw_bitmap_in_rect(ctx, icon_sr, GRect(sr_icon_x, sun_icon_y, 18, 16));
    }
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, s_weather.sunrise_time, font_sun_time,
                       GRect(sr_time_x, sun_text_y, sr_size.w + 4, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    if (icon_ss) {
      graphics_draw_bitmap_in_rect(ctx, icon_ss, GRect(ss_icon_x, sun_icon_y, 18, 16));
    }
    graphics_draw_text(ctx, s_weather.sunset_time, font_sun_time,
                       GRect(ss_time_x, sun_text_y, ss_size.w + 4, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  if (s_weather.moon_phase_icon < MOON_ICON_COUNT) {
    GBitmap *moon_bmp = get_moon_icon(s_weather.moon_phase_icon);
    if (moon_bmp) {
      int moon_x = bounds.size.w - 18 - 48;
      int moon_y = (bounds.size.h >= 200) ? 143 : 109;
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(ctx, moon_bmp, GRect(moon_x, moon_y, 48, 48));
    }
  }

  GFont font_label = (bounds.size.h >= 200) ? fonts_get_system_font(FONT_KEY_GOTHIC_18) : fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont font_val = (bounds.size.h >= 200) ? fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD) : fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);

  int y_rows[4] = { 132, 155, 178, 201 };
  if (bounds.size.h < 200) {
    y_rows[0] = 104; y_rows[1] = 119; y_rows[2] = 134; y_rows[3] = 149;
  }

  int label_w = (bounds.size.w >= 200) ? 64 : 44;
  int val_start_x = (bounds.size.w >= 200) ? 74 : 50;
  int val_max_w = (bounds.size.w >= 200) ? (bounds.size.w - 66 - val_start_x - 2) : (bounds.size.w - val_start_x - 4);
  int row_h = (bounds.size.h >= 200) ? 22 : 18;

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, "PRECIP", font_label,
                     GRect(2, y_rows[0], label_w, row_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  char precip_buf[16];
  snprintf(precip_buf, sizeof(precip_buf), "%d%%", s_weather.today_precip_pct);
  graphics_draw_text(ctx, precip_buf, font_val,
                     GRect(val_start_x, y_rows[0], val_max_w, row_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  graphics_draw_text(ctx, "HUMID", font_label,
                     GRect(2, y_rows[1], label_w, row_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  char humid_buf[16];
  snprintf(humid_buf, sizeof(humid_buf), "%d%%", s_weather.today_humidity_pct);
  graphics_draw_text(ctx, humid_buf, font_val,
                     GRect(val_start_x, y_rows[1], val_max_w, row_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  graphics_draw_text(ctx, "WIND", font_label,
                     GRect(2, y_rows[2], label_w, row_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  int wind = convert_wind_speed(s_weather.today_wind_speed_kmh, s_use_fahrenheit);
  char wind_buf[24];
  snprintf(wind_buf, sizeof(wind_buf), "%d %s", wind, s_use_fahrenheit ? "mph" : "kph");
  graphics_draw_text(ctx, wind_buf, font_val,
                     GRect(val_start_x, y_rows[2], val_max_w, row_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  graphics_draw_text(ctx, "UV", font_label,
                     GRect(2, y_rows[3], label_w, row_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  char uv_buf[32];
  snprintf(uv_buf, sizeof(uv_buf), "%s (%d)", get_uv_status(s_weather.today_uv_index), s_weather.today_uv_index);
  graphics_draw_text(ctx, uv_buf, font_val,
                     GRect(val_start_x, y_rows[3], bounds.size.w - val_start_x - 4, row_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void draw_current_temp_bar(GContext *ctx, GRect bounds, int min_t, int max_t, int chart_top, int chart_bottom) {
  if (!s_enable_temp_bar) {
    return;
  }

  int curr_temp_c = s_use_feels ? s_weather.current_feels_c : s_weather.current_temp_c;
  int curr_temp = convert_temp(curr_temp_c, s_use_fahrenheit);

  int curr_y;
  if (max_t == min_t) {
    curr_y = (chart_top + chart_bottom) / 2;
  } else {
    curr_y = chart_bottom - ((curr_temp - min_t) * (chart_bottom - chart_top)) / (max_t - min_t);
  }

  if (curr_y < chart_top) curr_y = chart_top;
  if (curr_y > chart_bottom) curr_y = chart_bottom;

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);

  for (int x = 0; x < bounds.size.w; x += 3) {
    graphics_draw_pixel(ctx, GPoint(x, curr_y));
  }
}

static void draw_current_time_bar(GContext *ctx, GRect bounds, int chart_top, int chart_bottom, bool is_hourly) {
  if (!s_enable_time_bar) {
    return;
  }

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  if (!t) {
    return;
  }

  int time_x = -1;

  if (is_hourly) {
    if (s_weather.hourly_count == 0) {
      return;
    }
    int hour_idx = -1;
    for (int i = 0; i < s_weather.hourly_count; i++) {
      if (s_weather.hourly[i].hour == t->tm_hour) {
        hour_idx = i;
        break;
      }
    }
    if (hour_idx < 0) {
      return;
    }
    time_x = s_hourly_scroll_x + 20 + (hour_idx * 40) + ((t->tm_min * 40) / 60);
  } else {
    if (s_weather.daily_count == 0) {
      return;
    }
    int day_idx = -1;
    int max_check = s_weather.daily_count < 7 ? s_weather.daily_count : 7;
    for (int d = 0; d < max_check; d++) {
      if ((s_weather.daily[d].day_of_week % 7) == (t->tm_wday % 7)) {
        day_idx = d;
        break;
      }
    }
    if (day_idx < 0) {
      day_idx = 0;
    }
    int mins_in_day = t->tm_hour * 60 + t->tm_min;
    time_x = s_daily_scroll_x + 20 + (day_idx * 40) + ((mins_in_day * 40) / 1440);
  }

  if (time_x < 0 || time_x >= bounds.size.w) {
    return;
  }

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);

  for (int y = chart_top; y <= chart_bottom; y += 3) {
    graphics_draw_pixel(ctx, GPoint(time_x, y));
  }
}

static void draw_page_hourly(GContext *ctx, GRect bounds) {
  draw_top_section(ctx, bounds);

  #if defined(PBL_COLOR)
  GColor orange_header = GColorOrange;
  #else
  GColor orange_header = GColorBlack;
  #endif

  draw_section_header(ctx, bounds, "HOURLY", orange_header);

  if (s_weather.no_location) {
    draw_no_location_message(ctx, bounds);
    return;
  }

  if (!s_weather.has_data) {
    draw_loading_message(ctx, bounds);
    return;
  }

  if (s_weather.hourly_count == 0) {
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, "No hourly data", fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(0, 80, bounds.size.w, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  int min_t = 999;
  int max_t = -999;
  for (int i = 0; i < s_weather.hourly_count; i++) {
    int raw_t = s_use_feels ? s_weather.hourly[i].feels_c : s_weather.hourly[i].temp_c;
    int t = convert_temp(raw_t, s_use_fahrenheit);
    if (t < min_t) min_t = t;
    if (t > max_t) max_t = t;
  }

  int col_w = 40;
  s_hourly_max_scroll = (s_weather.hourly_count * col_w) - bounds.size.w;
  if (s_hourly_max_scroll < 0) s_hourly_max_scroll = 0;

  int chart_top = (bounds.size.h >= 200) ? 69 : 62;
  int chart_bottom = (bounds.size.h >= 200) ? 151 : 108;
  int dot_r = (bounds.size.h >= 200) ? 4 : 3;

  if (s_enable_histogram && s_weather.hourly_precip_count > 0) {
    uint16_t max_precip = 0;
    for (int k = 0; k < s_weather.hourly_precip_count; k++) {
      if (s_weather.hourly_precip[k] > max_precip) {
        max_precip = s_weather.hourly_precip[k];
      }
    }

    if (max_precip > 0) {
      int chart_height = chart_bottom - chart_top;
      int bar_w = 10;
      graphics_context_set_fill_color(ctx, GColorLightGray);

      for (int k = 0; k < s_weather.hourly_precip_count; k++) {
        int bar_x = s_hourly_scroll_x + 20 + (k * bar_w);
        if (bar_x + bar_w <= 0 || bar_x >= bounds.size.w) {
          continue;
        }

        uint16_t p = s_weather.hourly_precip[k];
        if (p > 0) {
          int bar_h = ((int32_t)p * chart_height) / max_precip;
          if (bar_h == 0) bar_h = 1;
          if (bar_h > chart_height) bar_h = chart_height;
          int bar_y = chart_bottom - bar_h;
#if defined(PBL_COLOR)
          graphics_context_set_fill_color(ctx, GColorLightGray);
          graphics_fill_rect(ctx, GRect(bar_x, bar_y, bar_w, bar_h), 0, GCornerNone);
#else
          graphics_context_set_stroke_color(ctx, GColorBlack);
          for (int ly = bar_y; ly < bar_y + bar_h; ly += 2) {
            graphics_draw_line(ctx, GPoint(bar_x, ly), GPoint(bar_x + bar_w - 1, ly));
          }
#endif
        }
      }
    }
  }

  GFont font_temp = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont font_precip = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont font_hour = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  for (int i = 0; i < s_weather.hourly_count; i++) {
    int cx = s_hourly_scroll_x + (col_w / 2) + (i * col_w);
    if (cx + (col_w / 2) < 0 || cx - (col_w / 2) > bounds.size.w) {
      continue;
    }

    int raw_t = s_use_feels ? s_weather.hourly[i].feels_c : s_weather.hourly[i].temp_c;
    int t = convert_temp(raw_t, s_use_fahrenheit);

    char tbuf[12];
    snprintf(tbuf, sizeof(tbuf), "%d°", t);
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, tbuf, font_temp,
                       GRect(cx - (col_w / 2), (bounds.size.h >= 200) ? 42 : 38, col_w, (bounds.size.h >= 200) ? 18 : 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    int dot_y;
    if (max_t == min_t) {
      dot_y = (chart_top + chart_bottom) / 2;
    } else {
      dot_y = chart_bottom - ((t - min_t) * (chart_bottom - chart_top)) / (max_t - min_t);
    }
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_circle(ctx, GPoint(cx, dot_y), dot_r);

    if (s_weather.hourly[i].precip_pct > 0) {
      char pbuf[12];
      snprintf(pbuf, sizeof(pbuf), "%d%%", s_weather.hourly[i].precip_pct);
      graphics_draw_text(ctx, pbuf, font_precip,
                         GRect(cx - (col_w / 2), (bounds.size.h >= 200) ? 158 : 114, col_w, (bounds.size.h >= 200) ? 16 : 14),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }

    GBitmap *icon = get_small_icon(s_weather.hourly[i].icon);
    if (icon) {
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(ctx, icon, GRect(cx - 16, (bounds.size.h >= 200) ? 176 : 130, 32, 32));
    }

    char hbuf[8];
    snprintf(hbuf, sizeof(hbuf), "%02d", s_weather.hourly[i].hour);
    graphics_draw_text(ctx, hbuf, font_hour,
                       GRect(cx - (col_w / 2), (bounds.size.h >= 200) ? 205 : 151, col_w, 20),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  draw_current_temp_bar(ctx, bounds, min_t, max_t, chart_top, chart_bottom);

  draw_current_time_bar(ctx, bounds, chart_top, chart_bottom, true);
}

static void draw_page_daily(GContext *ctx, GRect bounds) {
  draw_top_section(ctx, bounds);

  #if defined(PBL_COLOR)
  GColor orange_header = GColorOrange;
  #else
  GColor orange_header = GColorBlack;
  #endif

  draw_section_header(ctx, bounds, "DAILY", orange_header);

  if (s_weather.no_location) {
    draw_no_location_message(ctx, bounds);
    return;
  }

  if (!s_weather.has_data) {
    draw_loading_message(ctx, bounds);
    return;
  }

  if (s_weather.daily_count == 0) {
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, "No daily data", fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(0, 80, bounds.size.w, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  int min_t = 999;
  int max_t = -999;
  for (int i = 0; i < s_weather.daily_count; i++) {
    int raw_hi = s_use_feels ? s_weather.daily[i].high_feels_c : s_weather.daily[i].high_temp_c;
    int raw_lo = s_use_feels ? s_weather.daily[i].low_feels_c : s_weather.daily[i].low_temp_c;
    int hi = convert_temp(raw_hi, s_use_fahrenheit);
    int lo = convert_temp(raw_lo, s_use_fahrenheit);
    if (lo < min_t) min_t = lo;
    if (hi > max_t) max_t = hi;
  }

  int col_w = 40;
  s_daily_max_scroll = (s_weather.daily_count * col_w) - bounds.size.w;
  if (s_daily_max_scroll < 0) s_daily_max_scroll = 0;

  int chart_top = (bounds.size.h >= 200) ? 65 : 59;
  int chart_bottom = (bounds.size.h >= 200) ? 135 : 97;

  if (s_enable_histogram && s_weather.daily_precip_count > 0) {
    uint16_t max_precip = 0;
    for (int d = 0; d < s_weather.daily_precip_count; d++) {
      if (s_weather.daily_precip[d] > max_precip) {
        max_precip = s_weather.daily_precip[d];
      }
    }

    if (max_precip > 0) {
      int chart_height = chart_bottom - chart_top;
      int bar_w = col_w;
      graphics_context_set_fill_color(ctx, GColorLightGray);

      for (int d = 0; d < s_weather.daily_precip_count; d++) {
        int bar_x = s_daily_scroll_x + 20 + (d * bar_w);
        if (bar_x + bar_w <= 0 || bar_x >= bounds.size.w) {
          continue;
        }

        uint16_t p = s_weather.daily_precip[d];
        if (p > 0) {
          int bar_h = ((int32_t)p * chart_height) / max_precip;
          if (bar_h == 0) bar_h = 1;
          if (bar_h > chart_height) bar_h = chart_height;
          int bar_y = chart_bottom - bar_h;
#if defined(PBL_COLOR)
          graphics_context_set_fill_color(ctx, GColorLightGray);
          graphics_fill_rect(ctx, GRect(bar_x, bar_y, bar_w, bar_h), 0, GCornerNone);
#else
          graphics_context_set_stroke_color(ctx, GColorBlack);
          for (int ly = bar_y; ly < bar_y + bar_h; ly += 2) {
            graphics_draw_line(ctx, GPoint(bar_x, ly), GPoint(bar_x + bar_w - 1, ly));
          }
#endif
        }
      }
    }
  }

  GFont font_temp = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont font_precip = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont font_day = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  for (int i = 0; i < s_weather.daily_count; i++) {
    int cx = s_daily_scroll_x + (col_w / 2) + (i * col_w);
    if (cx + (col_w / 2) < 0 || cx - (col_w / 2) > bounds.size.w) {
      continue;
    }

    int raw_hi = s_use_feels ? s_weather.daily[i].high_feels_c : s_weather.daily[i].high_temp_c;
    int raw_lo = s_use_feels ? s_weather.daily[i].low_feels_c : s_weather.daily[i].low_temp_c;
    int hi = convert_temp(raw_hi, s_use_fahrenheit);
    int lo = convert_temp(raw_lo, s_use_fahrenheit);

    char hbuf[12];
    snprintf(hbuf, sizeof(hbuf), "%d°", hi);
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, hbuf, font_temp,
                       GRect(cx - (col_w / 2), (bounds.size.h >= 200) ? 42 : 38, col_w, (bounds.size.h >= 200) ? 18 : 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    int bar_top, bar_bottom;
    if (max_t == min_t) {
      bar_top = (chart_top + chart_bottom) / 2 - 3;
      bar_bottom = (chart_top + chart_bottom) / 2 + 3;
    } else {
      bar_top = chart_bottom - ((hi - min_t) * (chart_bottom - chart_top)) / (max_t - min_t);
      bar_bottom = chart_bottom - ((lo - min_t) * (chart_bottom - chart_top)) / (max_t - min_t);
    }
    int line_w = (bounds.size.h >= 200) ? 6 : 5;
    int line_r = (bounds.size.h >= 200) ? 3 : 2;
    if (bar_bottom - bar_top < line_w) {
      int mid = (bar_top + bar_bottom) / 2;
      bar_top = mid - (line_w / 2);
      bar_bottom = bar_top + line_w;
      if (bar_top < chart_top) {
        bar_top = chart_top;
        bar_bottom = bar_top + line_w;
      }
      if (bar_bottom > chart_bottom) {
        bar_bottom = chart_bottom;
        bar_top = bar_bottom - line_w;
      }
    }

    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(cx - (line_w / 2), bar_top, line_w, bar_bottom - bar_top), line_r, GCornersAll);

    char lbuf[12];
    snprintf(lbuf, sizeof(lbuf), "%d°", lo);
    graphics_draw_text(ctx, lbuf, font_temp,
                       GRect(cx - (col_w / 2), (bounds.size.h >= 200) ? 134 : 96, col_w, (bounds.size.h >= 200) ? 18 : 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    if (s_weather.daily[i].precip_pct > 0) {
      char pbuf[12];
      snprintf(pbuf, sizeof(pbuf), "%d%%", s_weather.daily[i].precip_pct);
      graphics_draw_text(ctx, pbuf, font_precip,
                         GRect(cx - (col_w / 2), (bounds.size.h >= 200) ? 158 : 114, col_w, (bounds.size.h >= 200) ? 16 : 14),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }

    GBitmap *icon = get_small_icon(s_weather.daily[i].icon);
    if (icon) {
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(ctx, icon, GRect(cx - 16, (bounds.size.h >= 200) ? 176 : 130, 32, 32));
    }

    graphics_context_set_text_color(ctx, GColorBlack);
    int dow = s_weather.daily[i].day_of_week % 7;
    const char *day_str = DAY_NAMES[dow];
    int y_day = (bounds.size.h >= 200) ? 205 : 151;

    if (i < 7) {
      graphics_draw_text(ctx, day_str, font_day,
                         GRect(cx - (col_w / 2), y_day, col_w, 20),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    } else {
      GFont font_sup = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
      GSize name_sz = graphics_text_layout_get_content_size(day_str, font_day,
                                                            GRect(0, 0, col_w, 20),
                                                            GTextOverflowModeTrailingEllipsis,
                                                            GTextAlignmentLeft);
      GSize plus_sz = graphics_text_layout_get_content_size("+", font_sup,
                                                            GRect(0, 0, 16, 16),
                                                            GTextOverflowModeTrailingEllipsis,
                                                            GTextAlignmentLeft);
      int total_w = name_sz.w + plus_sz.w;
      int start_x = cx - (total_w / 2);
      int y_plus = (bounds.size.h >= 200) ? 201 : 148;

      graphics_draw_text(ctx, day_str, font_day,
                         GRect(start_x, y_day, name_sz.w + 4, 20),
                         GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      graphics_draw_text(ctx, "+", font_sup,
                         GRect(start_x + name_sz.w, y_plus, plus_sz.w + 6, 16),
                         GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    }
  }

  draw_current_temp_bar(ctx, bounds, min_t, max_t, chart_top, chart_bottom);

  draw_current_time_bar(ctx, bounds, chart_top, chart_bottom, false);
}

static void page_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  switch (s_current_page) {
    case PAGE_NOW:
      draw_page_now(ctx, bounds);
      break;
    case PAGE_HOURLY:
      draw_page_hourly(ctx, bounds);
      break;
    case PAGE_DAILY:
      draw_page_daily(ctx, bounds);
      break;
    default:
      break;
  }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  s_current_page = (s_current_page + PAGE_COUNT - 1) % PAGE_COUNT;
  s_hourly_scroll_x = 0;
  s_daily_scroll_x = 0;
  s_base_scroll_x = 0;
  layer_mark_dirty(s_page_layer);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  s_current_page = (s_current_page + 1) % PAGE_COUNT;
  s_hourly_scroll_x = 0;
  s_daily_scroll_x = 0;
  s_base_scroll_x = 0;
  layer_mark_dirty(s_page_layer);
}

static void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  vibes_short_pulse();
  s_use_feels = !s_use_feels;
  persist_write_bool(STORAGE_KEY_USE_FEELS, s_use_feels);
  layer_mark_dirty(s_page_layer);
}

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  vibes_short_pulse();
  s_use_fahrenheit = !s_use_fahrenheit;
  persist_write_bool(STORAGE_KEY_USE_FAHRENHEIT, s_use_fahrenheit);
  layer_mark_dirty(s_page_layer);
}

static void request_location_slot(uint8_t slot) {
  s_weather.has_data = false;
  layer_mark_dirty(s_page_layer);

  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_KEY_REQUEST_LOCATION_INDEX, slot);
    app_message_outbox_send();
  }
}

static void switch_location(int step) {
  if (s_weather.no_location || s_location_count == 0) {
    return;
  }
  if (s_location_count <= 1) {
    s_weather.has_data = false;
    s_hourly_scroll_x = 0;
    s_daily_scroll_x = 0;
    s_base_scroll_x = 0;
    layer_mark_dirty(s_page_layer);
    request_location_slot(s_active_location_slot);
    return;
  }

  if (step > 0) {
    s_active_location_slot = (s_active_location_slot + 1) % s_location_count;
  } else {
    s_active_location_slot = (s_active_location_slot + s_location_count - 1) % s_location_count;
  }

  if (s_location_names[s_active_location_slot][0] != '\0') {
    strncpy(s_weather.location_name, s_location_names[s_active_location_slot], sizeof(s_weather.location_name) - 1);
    s_weather.location_name[sizeof(s_weather.location_name) - 1] = '\0';
  } else {
    snprintf(s_weather.location_name, sizeof(s_weather.location_name), "LOCATION %d...", s_active_location_slot + 1);
  }

  if (s_active_location_slot == 0 && (strstr(s_weather.location_name, "GPS") != NULL || strcmp(s_weather.location_name, "PEBBLERAIN") == 0)) {
    s_weather.is_gps = true;
  } else {
    s_weather.is_gps = false;
  }

  s_weather.has_data = false;
  s_hourly_scroll_x = 0;
  s_daily_scroll_x = 0;
  s_base_scroll_x = 0;

  layer_mark_dirty(s_page_layer);

  request_location_slot(s_active_location_slot);
}

static void up_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  switch_location(-1);
}

static void down_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  switch_location(1);
}

static void click_config_provider(void *context) {
  (void)context;
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);

  window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_long_click_handler, NULL);

  window_long_click_subscribe(BUTTON_ID_UP, 500, up_long_click_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 500, down_long_click_handler, NULL);
}

#if defined(PBL_PLATFORM_EMERY)
static void touch_handler(const TouchEvent *event, void *context) {
  (void)context;
  if (!event) return;
  if (s_current_page != PAGE_HOURLY && s_current_page != PAGE_DAILY) return;

  int16_t max_scroll = (s_current_page == PAGE_HOURLY) ? s_hourly_max_scroll : s_daily_max_scroll;

  switch (event->type) {
    case TouchEvent_Touchdown:
      s_touch_start_x = event->x;
      s_base_scroll_x = (s_current_page == PAGE_HOURLY) ? s_hourly_scroll_x : s_daily_scroll_x;
      s_touch_active = true;
      break;

    case TouchEvent_PositionUpdate:
      if (!s_touch_active) break;
      {
        int16_t dx = event->x - s_touch_start_x;
        int16_t new_x = s_base_scroll_x + dx;
        if (new_x > 0) new_x = 0;
        if (new_x < -max_scroll) new_x = -max_scroll;

        if (s_current_page == PAGE_HOURLY) {
          s_hourly_scroll_x = new_x;
        } else {
          s_daily_scroll_x = new_x;
        }
        layer_mark_dirty(s_page_layer);
      }
      break;

    case TouchEvent_Liftoff:
      if (s_touch_active) {
        s_touch_active = false;
        s_base_scroll_x = (s_current_page == PAGE_HOURLY) ? s_hourly_scroll_x : s_daily_scroll_x;
        layer_mark_dirty(s_page_layer);
      }
      break;

    default:
      break;
  }
}

static void pan_handler(const Recognizer *recognizer, RecognizerEvent event) {
  if (s_current_page != PAGE_HOURLY && s_current_page != PAGE_DAILY) return;

  switch (event) {
    case RecognizerEvent_Updated: {
      GPoint d = pan_recognizer_get_delta_since_start(recognizer);
      int16_t new_x = s_base_scroll_x + d.x;
      int16_t max_scroll = (s_current_page == PAGE_HOURLY) ? s_hourly_max_scroll : s_daily_max_scroll;
      if (new_x > 0) new_x = 0;
      if (new_x < -max_scroll) new_x = -max_scroll;

      if (s_current_page == PAGE_HOURLY) {
        s_hourly_scroll_x = new_x;
      } else {
        s_daily_scroll_x = new_x;
      }
      layer_mark_dirty(s_page_layer);
      break;
    }
    case RecognizerEvent_Completed:
      s_base_scroll_x = (s_current_page == PAGE_HOURLY) ? s_hourly_scroll_x : s_daily_scroll_x;
      break;
    case RecognizerEvent_Cancelled:
      if (s_current_page == PAGE_HOURLY) {
        s_hourly_scroll_x = s_base_scroll_x;
      } else {
        s_daily_scroll_x = s_base_scroll_x;
      }
      layer_mark_dirty(s_page_layer);
      break;
    default:
      break;
  }
}
#endif

#define WEATHER_CACHE_PART1_SIZE 200
#define WEATHER_CACHE_PART2_SIZE 200
#define WEATHER_CACHE_PART3_SIZE ((int)sizeof(WeatherData) - WEATHER_CACHE_PART1_SIZE - WEATHER_CACHE_PART2_SIZE)

static void save_weather_cache(void) {
  const uint8_t *p = (const uint8_t *)&s_weather;
  persist_write_data(STORAGE_KEY_WEATHER_CACHE, p, WEATHER_CACHE_PART1_SIZE);
  persist_write_data(STORAGE_KEY_WEATHER_CACHE_PART2, p + WEATHER_CACHE_PART1_SIZE, WEATHER_CACHE_PART2_SIZE);
  persist_write_data(STORAGE_KEY_WEATHER_CACHE_PART3, p + WEATHER_CACHE_PART1_SIZE + WEATHER_CACHE_PART2_SIZE, WEATHER_CACHE_PART3_SIZE);
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  (void)context;

  Tuple *status_t = dict_find(iterator, MESSAGE_KEY_KEY_FETCH_STATUS);
  Tuple *curr_t = dict_find(iterator, MESSAGE_KEY_KEY_CURRENT_TEMP_C);
  Tuple *count_t = dict_find(iterator, MESSAGE_KEY_KEY_LOCATION_COUNT);

  Tuple *time_bar_t = dict_find(iterator, MESSAGE_KEY_KEY_ENABLE_CURRENT_TIME_BAR);
  if (!time_bar_t) {
    time_bar_t = dict_find(iterator, 28);
  }

  Tuple *bar_t = dict_find(iterator, MESSAGE_KEY_KEY_ENABLE_CURRENT_TEMP_BAR);
  if (!bar_t) {
    bar_t = dict_find(iterator, 24);
  }

  Tuple *hist_t = dict_find(iterator, MESSAGE_KEY_KEY_ENABLE_PRECIP_HISTOGRAM);
  if (!hist_t) {
    hist_t = dict_find(iterator, 25);
  }

  Tuple *is_gps_t = dict_find(iterator, MESSAGE_KEY_KEY_IS_GPS);
  if (is_gps_t) {
    s_weather.is_gps = ((is_gps_t->value->int32 & 1) != 0);
  }

  if (time_bar_t) {
    s_enable_time_bar = (time_bar_t->value->int32 != 0);
    persist_write_bool(STORAGE_KEY_ENABLE_TIME_BAR, s_enable_time_bar);
  } else if (is_gps_t) {
    s_enable_time_bar = ((is_gps_t->value->int32 & 8) != 0);
    persist_write_bool(STORAGE_KEY_ENABLE_TIME_BAR, s_enable_time_bar);
  }

  if (bar_t) {
    s_enable_temp_bar = (bar_t->value->int32 != 0);
    persist_write_bool(STORAGE_KEY_ENABLE_TEMP_BAR, s_enable_temp_bar);
  } else if (is_gps_t) {
    s_enable_temp_bar = ((is_gps_t->value->int32 & 2) != 0);
    persist_write_bool(STORAGE_KEY_ENABLE_TEMP_BAR, s_enable_temp_bar);
  }

  if (hist_t) {
    s_enable_histogram = (hist_t->value->int32 != 0);
    persist_write_bool(STORAGE_KEY_ENABLE_HISTOGRAM, s_enable_histogram);
  } else if (is_gps_t) {
    s_enable_histogram = ((is_gps_t->value->int32 & 4) != 0);
    persist_write_bool(STORAGE_KEY_ENABLE_HISTOGRAM, s_enable_histogram);
  }

  if (status_t && status_t->value->int32 == 0) {
    s_weather.has_data = false;
    layer_mark_dirty(s_page_layer);
    return;
  }

  if ((status_t && status_t->value->int32 == 3) ||
      (!curr_t && count_t && count_t->value->int32 == 0)) {
    s_weather.no_location = true;
    s_weather.has_data = false;
    s_weather.is_gps = false;
    strncpy(s_weather.location_name, "NO LOCATION", sizeof(s_weather.location_name) - 1);
    s_weather.location_name[sizeof(s_weather.location_name) - 1] = '\0';
    s_location_count = 0;
    save_weather_cache();
    layer_mark_dirty(s_page_layer);
    return;
  }

  Tuple *loc_t = dict_find(iterator, MESSAGE_KEY_KEY_LOCATION_NAME);
  if (loc_t) {
    strncpy(s_weather.location_name, loc_t->value->cstring, sizeof(s_weather.location_name) - 1);
    s_weather.location_name[sizeof(s_weather.location_name) - 1] = '\0';
  }

  if (curr_t) s_weather.current_temp_c = (int8_t)curr_t->value->int32;

  Tuple *feels_t = dict_find(iterator, MESSAGE_KEY_KEY_CURRENT_FEELS_C);
  if (feels_t) s_weather.current_feels_c = (int8_t)feels_t->value->int32;

  Tuple *icon_t = dict_find(iterator, MESSAGE_KEY_KEY_CURRENT_ICON);
  if (icon_t) s_weather.current_icon = (uint8_t)icon_t->value->int32;

  Tuple *hi_t = dict_find(iterator, MESSAGE_KEY_KEY_TODAY_HIGH_C);
  if (hi_t) s_weather.today_high_c = (int8_t)hi_t->value->int32;

  Tuple *lo_t = dict_find(iterator, MESSAGE_KEY_KEY_TODAY_LOW_C);
  if (lo_t) s_weather.today_low_c = (int8_t)lo_t->value->int32;

  Tuple *f_hi_t = dict_find(iterator, MESSAGE_KEY_KEY_TODAY_FEELS_HIGH_C);
  if (f_hi_t) s_weather.today_feels_high_c = (int8_t)f_hi_t->value->int32;

  Tuple *f_lo_t = dict_find(iterator, MESSAGE_KEY_KEY_TODAY_FEELS_LOW_C);
  if (f_lo_t) s_weather.today_feels_low_c = (int8_t)f_lo_t->value->int32;

  Tuple *precip_t = dict_find(iterator, MESSAGE_KEY_KEY_TODAY_PRECIP_PCT);
  if (precip_t) s_weather.today_precip_pct = (uint8_t)precip_t->value->int32;

  Tuple *hum_t = dict_find(iterator, MESSAGE_KEY_KEY_TODAY_HUMIDITY_PCT);
  if (hum_t) s_weather.today_humidity_pct = (uint8_t)hum_t->value->int32;

  Tuple *wind_t = dict_find(iterator, MESSAGE_KEY_KEY_TODAY_WIND_SPEED_KMH);
  if (wind_t) s_weather.today_wind_speed_kmh = (uint16_t)wind_t->value->int32;

  Tuple *uv_t = dict_find(iterator, MESSAGE_KEY_KEY_TODAY_UV_INDEX);
  if (uv_t) s_weather.today_uv_index = (uint8_t)uv_t->value->int32;

  Tuple *sunrise_t = dict_find(iterator, MESSAGE_KEY_KEY_SUNRISE_TIME);
  if (sunrise_t) {
    strncpy(s_weather.sunrise_time, sunrise_t->value->cstring, sizeof(s_weather.sunrise_time) - 1);
    s_weather.sunrise_time[sizeof(s_weather.sunrise_time) - 1] = '\0';
  }

  Tuple *sunset_t = dict_find(iterator, MESSAGE_KEY_KEY_SUNSET_TIME);
  if (sunset_t) {
    strncpy(s_weather.sunset_time, sunset_t->value->cstring, sizeof(s_weather.sunset_time) - 1);
    s_weather.sunset_time[sizeof(s_weather.sunset_time) - 1] = '\0';
  }

  Tuple *moon_t = dict_find(iterator, MESSAGE_KEY_KEY_MOON_PHASE_ICON);
  if (moon_t) {
    s_weather.moon_phase_icon = (uint8_t)moon_t->value->int32;
  }

  Tuple *hourly_t = dict_find(iterator, MESSAGE_KEY_KEY_HOURLY_DATA);
  if (hourly_t && hourly_t->type == TUPLE_BYTE_ARRAY) {
    uint8_t count = hourly_t->length / 5;
    if (count > 24) count = 24;
    s_weather.hourly_count = count;
    uint8_t *raw = hourly_t->value->data;
    for (int i = 0; i < count; i++) {
      s_weather.hourly[i].hour = raw[i * 5 + 0];
      s_weather.hourly[i].temp_c = (int8_t)raw[i * 5 + 1];
      s_weather.hourly[i].feels_c = (int8_t)raw[i * 5 + 2];
      s_weather.hourly[i].precip_pct = raw[i * 5 + 3];
      s_weather.hourly[i].icon = raw[i * 5 + 4];
    }
    if (hourly_t->length >= 120 + 192) {
      uint8_t *precip_raw = raw + 120;
      s_weather.hourly_precip_count = 96;
      for (int k = 0; k < 96; k++) {
        s_weather.hourly_precip[k] = (uint16_t)precip_raw[k * 2 + 0] | ((uint16_t)precip_raw[k * 2 + 1] << 8);
      }
    }
  }

  Tuple *daily_t = dict_find(iterator, MESSAGE_KEY_KEY_DAILY_DATA);
  if (daily_t && daily_t->type == TUPLE_BYTE_ARRAY) {
    uint8_t count = daily_t->length / 7;
    if (count > 14) count = 14;
    s_weather.daily_count = count;
    uint8_t *raw = daily_t->value->data;
    for (int i = 0; i < count; i++) {
      s_weather.daily[i].day_of_week = raw[i * 7 + 0];
      s_weather.daily[i].high_temp_c = (int8_t)raw[i * 7 + 1];
      s_weather.daily[i].low_temp_c = (int8_t)raw[i * 7 + 2];
      s_weather.daily[i].high_feels_c = (int8_t)raw[i * 7 + 3];
      s_weather.daily[i].low_feels_c = (int8_t)raw[i * 7 + 4];
      s_weather.daily[i].precip_pct = raw[i * 7 + 5];
      s_weather.daily[i].icon = raw[i * 7 + 6];
    }
    if (daily_t->length >= 98 + 28) {
      uint8_t *precip_raw = raw + 98;
      s_weather.daily_precip_count = 14;
      for (int d = 0; d < 14; d++) {
        s_weather.daily_precip[d] = (uint16_t)precip_raw[d * 2 + 0] | ((uint16_t)precip_raw[d * 2 + 1] << 8);
      }
    }
  } else {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Inbox: daily_t missing or not byte array");
  }

  Tuple *h_precip_t = dict_find(iterator, MESSAGE_KEY_KEY_HOURLY_PRECIP_DATA);
  if (!h_precip_t) {
    h_precip_t = dict_find(iterator, 26);
  }
  if (h_precip_t && h_precip_t->type == TUPLE_BYTE_ARRAY) {
    uint8_t count = h_precip_t->length / 2;
    if (count > 96) count = 96;
    s_weather.hourly_precip_count = count;
    uint8_t *raw = h_precip_t->value->data;
    for (int i = 0; i < count; i++) {
      s_weather.hourly_precip[i] = (uint16_t)raw[i * 2 + 0] | ((uint16_t)raw[i * 2 + 1] << 8);
    }
  }

  Tuple *d_precip_t = dict_find(iterator, MESSAGE_KEY_KEY_DAILY_PRECIP_DATA);
  if (!d_precip_t) {
    d_precip_t = dict_find(iterator, 27);
  }
  if (d_precip_t && d_precip_t->type == TUPLE_BYTE_ARRAY) {
    uint8_t count = d_precip_t->length / 2;
    if (count > 14) count = 14;
    s_weather.daily_precip_count = count;
    uint8_t *raw = d_precip_t->value->data;
    for (int i = 0; i < count; i++) {
      s_weather.daily_precip[i] = (uint16_t)raw[i * 2 + 0] | ((uint16_t)raw[i * 2 + 1] << 8);
    }
  }

  uint16_t max_h_p = 0;
  for (int i = 0; i < s_weather.hourly_precip_count; i++) {
    if (s_weather.hourly_precip[i] > max_h_p) max_h_p = s_weather.hourly_precip[i];
  }
  uint16_t max_d_p = 0;
  for (int i = 0; i < s_weather.daily_precip_count; i++) {
    if (s_weather.daily_precip[i] > max_d_p) max_d_p = s_weather.daily_precip[i];
  }

  APP_LOG(APP_LOG_LEVEL_INFO, "Inbox: hours=%d, hourly_precip=%d (max=%d), days=%d, daily_precip=%d (max=%d)",
          (int)s_weather.hourly_count, (int)s_weather.hourly_precip_count, (int)max_h_p,
          (int)s_weather.daily_count, (int)s_weather.daily_precip_count, (int)max_d_p);
  APP_LOG(APP_LOG_LEVEL_INFO, "Inbox: histogram=%d, temp_bar=%d, time_bar=%d, is_gps=%d",
          (int)s_enable_histogram, (int)s_enable_temp_bar, (int)s_enable_time_bar, (int)s_weather.is_gps);

  Tuple *slot_t = dict_find(iterator, MESSAGE_KEY_KEY_LOCATION_INDEX);
  if (slot_t) {
    s_active_location_slot = (uint8_t)slot_t->value->int32;
  }

  if (count_t) {
    s_location_count = (uint8_t)count_t->value->int32;
    if (s_location_count > MAX_SAVED_LOCATIONS) {
      s_location_count = MAX_SAVED_LOCATIONS;
    }
  }

  Tuple *names_t = dict_find(iterator, MESSAGE_KEY_KEY_LOCATION_NAMES);
  if (names_t) {
    const char *p = names_t->value->cstring;
    for (int i = 0; i < s_location_count; i++) {
      const char *sep = strchr(p, '|');
      if (sep) {
        size_t len = sep - p;
        if (len >= sizeof(s_location_names[i])) len = sizeof(s_location_names[i]) - 1;
        strncpy(s_location_names[i], p, len);
        s_location_names[i][len] = '\0';
        p = sep + 1;
      } else {
        strncpy(s_location_names[i], p, sizeof(s_location_names[i]) - 1);
        s_location_names[i][sizeof(s_location_names[i]) - 1] = '\0';
        break;
      }
    }
  }

  s_weather.location_slot = s_active_location_slot;
  s_weather.no_location = false;
  s_weather.has_data = true;

  if (s_active_location_slot == 0) {
    save_weather_cache();
  }

  layer_mark_dirty(s_page_layer);
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  (void)reason;
  (void)context;
  APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage dropped: %d", (int)reason);
}

static void request_weather_refresh(void) {
  s_weather.has_data = false;
  layer_mark_dirty(s_page_layer);

  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_KEY_REQUEST_REFRESH, 1);
    dict_write_uint8(iter, MESSAGE_KEY_KEY_REQUEST_LOCATION_INDEX, s_active_location_slot);
    app_message_outbox_send();
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_page_layer = layer_create(bounds);
  layer_set_update_proc(s_page_layer, page_layer_update_proc);
  layer_add_child(window_layer, s_page_layer);

#if defined(PBL_PLATFORM_EMERY)
  window_set_touch_bridge_disabled(window, true);
  touch_service_subscribe(touch_handler, NULL);
  s_pan_recognizer = pan_recognizer_create(pan_handler, NULL, PanAxis_Horizontal);
  if (s_pan_recognizer) {
    window_attach_recognizer(window, s_pan_recognizer);
  }
#endif
}

static void window_unload(Window *window) {
  (void)window;
#if defined(PBL_PLATFORM_EMERY)
  touch_service_unsubscribe();
#endif

  if (s_curr_large_icon) {
    gbitmap_destroy(s_curr_large_icon);
    s_curr_large_icon = NULL;
  }
  for (int i = 0; i < ICON_COUNT; i++) {
    if (s_small_icons[i]) {
      gbitmap_destroy(s_small_icons[i]);
      s_small_icons[i] = NULL;
    }
  }
  if (s_gps_icon) {
    gbitmap_destroy(s_gps_icon);
    s_gps_icon = NULL;
  }
  if (s_sunrise_icon) {
    gbitmap_destroy(s_sunrise_icon);
    s_sunrise_icon = NULL;
  }
  if (s_sunset_icon) {
    gbitmap_destroy(s_sunset_icon);
    s_sunset_icon = NULL;
  }
  if (s_curr_moon_icon) {
    gbitmap_destroy(s_curr_moon_icon);
    s_curr_moon_icon = NULL;
    s_curr_moon_icon_id = -1;
  }

  layer_destroy(s_page_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  (void)tick_time;
  (void)units_changed;
  if (s_enable_time_bar && (s_current_page == PAGE_HOURLY || s_current_page == PAGE_DAILY)) {
    layer_mark_dirty(s_page_layer);
  }
}

static void init(void) {
  if (persist_exists(STORAGE_KEY_USE_FEELS)) {
    s_use_feels = persist_read_bool(STORAGE_KEY_USE_FEELS);
  } else {
    s_use_feels = false;
  }

  if (persist_exists(STORAGE_KEY_USE_FAHRENHEIT)) {
    s_use_fahrenheit = persist_read_bool(STORAGE_KEY_USE_FAHRENHEIT);
  } else {
    s_use_fahrenheit = false;
  }

  if (persist_exists(STORAGE_KEY_ENABLE_TEMP_BAR)) {
    s_enable_temp_bar = persist_read_bool(STORAGE_KEY_ENABLE_TEMP_BAR);
  } else {
    s_enable_temp_bar = false;
  }

  if (persist_exists(STORAGE_KEY_ENABLE_HISTOGRAM)) {
    s_enable_histogram = persist_read_bool(STORAGE_KEY_ENABLE_HISTOGRAM);
  } else {
    s_enable_histogram = false;
  }

  if (persist_exists(STORAGE_KEY_ENABLE_TIME_BAR)) {
    s_enable_time_bar = persist_read_bool(STORAGE_KEY_ENABLE_TIME_BAR);
  } else {
    s_enable_time_bar = false;
  }

  s_active_location_slot = 0;
  if (persist_exists(STORAGE_KEY_ACTIVE_LOCATION_SLOT)) {
    persist_delete(STORAGE_KEY_ACTIVE_LOCATION_SLOT);
  }

  if (persist_exists(STORAGE_KEY_WEATHER_CACHE) && persist_exists(STORAGE_KEY_WEATHER_CACHE_PART2) && persist_exists(STORAGE_KEY_WEATHER_CACHE_PART3)) {
    uint8_t *p = (uint8_t *)&s_weather;
    int read_bytes1 = persist_read_data(STORAGE_KEY_WEATHER_CACHE, p, WEATHER_CACHE_PART1_SIZE);
    int read_bytes2 = persist_read_data(STORAGE_KEY_WEATHER_CACHE_PART2, p + WEATHER_CACHE_PART1_SIZE, WEATHER_CACHE_PART2_SIZE);
    int read_bytes3 = persist_read_data(STORAGE_KEY_WEATHER_CACHE_PART3, p + WEATHER_CACHE_PART1_SIZE + WEATHER_CACHE_PART2_SIZE, WEATHER_CACHE_PART3_SIZE);
    if (read_bytes1 != WEATHER_CACHE_PART1_SIZE || read_bytes2 != WEATHER_CACHE_PART2_SIZE || read_bytes3 != WEATHER_CACHE_PART3_SIZE || s_weather.location_slot != 0) {
      memset(&s_weather, 0, sizeof(WeatherData));
      strncpy(s_weather.location_name, "PEBBLERAIN", sizeof(s_weather.location_name) - 1);
    }
  } else {
    memset(&s_weather, 0, sizeof(WeatherData));
    strncpy(s_weather.location_name, "PEBBLERAIN", sizeof(s_weather.location_name) - 1);
  }
  s_weather.has_data = false;

  s_main_window = window_create();
  window_set_click_config_provider(s_main_window, click_config_provider);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });

  window_stack_push(s_main_window, true);

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  request_weather_refresh();
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
