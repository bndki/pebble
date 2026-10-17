var MessageKeys;
try {
  MessageKeys = require('message_keys');
} catch (e) {
  MessageKeys = null;
}

var FALLBACK_KEYS = {
  KEY_LOCATION_NAME: 0,
  KEY_IS_GPS: 1,
  KEY_CURRENT_TEMP_C: 2,
  KEY_CURRENT_FEELS_C: 3,
  KEY_CURRENT_ICON: 4,
  KEY_TODAY_HIGH_C: 5,
  KEY_TODAY_LOW_C: 6,
  KEY_TODAY_FEELS_HIGH_C: 7,
  KEY_TODAY_FEELS_LOW_C: 8,
  KEY_TODAY_PRECIP_PCT: 9,
  KEY_TODAY_HUMIDITY_PCT: 10,
  KEY_TODAY_WIND_SPEED_KMH: 11,
  KEY_TODAY_UV_INDEX: 12,
  KEY_HOURLY_DATA: 13,
  KEY_DAILY_DATA: 14,
  KEY_FETCH_STATUS: 15,
  KEY_REQUEST_REFRESH: 16,
  KEY_LOCATION_INDEX: 17,
  KEY_LOCATION_COUNT: 18,
  KEY_LOCATION_NAMES: 19,
  KEY_REQUEST_LOCATION_INDEX: 20,
  KEY_SUNRISE_TIME: 21,
  KEY_SUNSET_TIME: 22,
  KEY_MOON_PHASE_ICON: 23,
  KEY_ENABLE_CURRENT_TEMP_BAR: 24,
  KEY_ENABLE_PRECIP_HISTOGRAM: 25,
  KEY_HOURLY_PRECIP_DATA: 26,
  KEY_DAILY_PRECIP_DATA: 27,
  KEY_ENABLE_CURRENT_TIME_BAR: 28
};

var ICONS = {
  SUNNY: 0,
  CLEAR_NIGHT: 1,
  SUNNY_OVERCAST: 2,
  NIGHT_PARTLY_CLOUDY: 3,
  DAY_CLOUDY: 4,
  NIGHT_CLOUDY: 5,
  CLOUDY: 6,
  FOG: 7,
  SHOWERS: 8,
  RAIN: 9,
  SNOW: 10,
  THUNDERSTORM: 11
};

var SETTINGS_KEY = 'pebblerain_settings_v1';

var DEFAULT_SETTINGS = {
  useGps: true,
  activeLocationIndex: 0,
  locations: [],
  enableCurrentTimeBar: false,
  enableCurrentTempBar: false,
  enablePrecipHistogram: false
};

function getSettings() {
  try {
    var raw = localStorage.getItem(SETTINGS_KEY);
    if (raw) {
      var parsed = JSON.parse(raw);
      return {
        useGps: (parsed.useGps !== undefined) ? !!parsed.useGps : DEFAULT_SETTINGS.useGps,
        activeLocationIndex: parseInt(parsed.activeLocationIndex, 10) || 0,
        locations: (parsed.locations && Array.isArray(parsed.locations)) ? parsed.locations : [],
        enableCurrentTimeBar: (parsed.enableCurrentTimeBar !== undefined) ? !!parsed.enableCurrentTimeBar : DEFAULT_SETTINGS.enableCurrentTimeBar,
        enableCurrentTempBar: (parsed.enableCurrentTempBar !== undefined) ? !!parsed.enableCurrentTempBar : DEFAULT_SETTINGS.enableCurrentTempBar,
        enablePrecipHistogram: (parsed.enablePrecipHistogram !== undefined) ? !!parsed.enablePrecipHistogram : DEFAULT_SETTINGS.enablePrecipHistogram
      };
    }
  } catch (e) {
    console.log('[PEBBLERAIN] Error reading settings: ' + e);
  }
  return {
    useGps: DEFAULT_SETTINGS.useGps,
    activeLocationIndex: DEFAULT_SETTINGS.activeLocationIndex,
    locations: [],
    enableCurrentTimeBar: DEFAULT_SETTINGS.enableCurrentTimeBar,
    enableCurrentTempBar: DEFAULT_SETTINGS.enableCurrentTempBar,
    enablePrecipHistogram: DEFAULT_SETTINGS.enablePrecipHistogram
  };
}

function saveSettings(settings) {
  try {
    localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings));
    console.log('[PEBBLERAIN] Settings saved successfully: ' + JSON.stringify(settings));
  } catch (e) {
    console.log('[PEBBLERAIN] Error saving settings: ' + e);
  }
}

function getMessageKey(name) {
  if (MessageKeys) {
    if (MessageKeys[name] !== undefined) return MessageKeys[name];
    var alt1 = name.replace(/^KEY_/, '');
    if (MessageKeys[alt1] !== undefined) return MessageKeys[alt1];
    var camel = alt1.toLowerCase().replace(/_([a-z])/g, function(g) { return g[1].toUpperCase(); });
    if (MessageKeys[camel] !== undefined) return MessageKeys[camel];
    var pascal = camel.charAt(0).toUpperCase() + camel.slice(1);
    if (MessageKeys[pascal] !== undefined) return MessageKeys[pascal];
  }
  return undefined;
}

function setField(dict, keyName, fallbackInt, value) {
  var mk = getMessageKey(keyName);
  if (mk !== undefined) {
    dict[mk] = value;
  } else if (fallbackInt !== undefined) {
    dict[fallbackInt] = value;
  } else if (typeof Pebble !== 'undefined' && Pebble.keys && Pebble.keys[keyName] !== undefined) {
    dict[Pebble.keys[keyName]] = value;
  } else {
    dict[keyName] = value;
  }
}

function wmoToIcon(code, isDay) {
  var d = (isDay === 1 || isDay === true);
  switch (code) {
    case 0:
      return d ? ICONS.SUNNY : ICONS.CLEAR_NIGHT;
    case 1:
      return d ? ICONS.SUNNY_OVERCAST : ICONS.NIGHT_PARTLY_CLOUDY;
    case 2:
      return d ? ICONS.DAY_CLOUDY : ICONS.NIGHT_CLOUDY;
    case 3:
      return ICONS.CLOUDY;
    case 45:
    case 48:
      return ICONS.FOG;
    case 51:
    case 53:
    case 55:
    case 56:
    case 57:
      return ICONS.SHOWERS;
    case 61:
    case 63:
    case 65:
    case 66:
    case 67:
    case 80:
    case 81:
    case 82:
      return ICONS.RAIN;
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86:
      return ICONS.SNOW;
    case 95:
    case 96:
    case 99:
      return ICONS.THUNDERSTORM;
    default:
      return d ? ICONS.SUNNY : ICONS.CLEAR_NIGHT;
  }
}

function encodeInt8(val) {
  var v = Math.round(val);
  if (v < -128) v = -128;
  if (v > 127) v = 127;
  return (v < 0) ? (256 + v) : v;
}

function encodeUInt8(val) {
  var v = Math.round(val);
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  return v;
}

function reverseGeocode(lat, lon, callback) {
  var url = 'https://api.bigdatacloud.net/data/reverse-geocode-client?latitude=' +
            lat + '&longitude=' + lon + '&localityLanguage=en';
  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, true);
  xhr.timeout = 7000;
  xhr.onload = function() {
    if (xhr.status >= 200 && xhr.status < 300) {
      try {
        var res = JSON.parse(xhr.responseText);
        var city = res.city || res.locality || res.principalSubdivision || 'LOCAL WEATHER';
        var state = res.principalSubdivisionCode ? res.principalSubdivisionCode.replace(/^[A-Z]+-/, '') : (res.countryCode || '');
        var full = city;
        if (state) {
          full += ', ' + state;
        }
        callback(full.toUpperCase());
        return;
      } catch (e) {
        console.log('[PEBBLERAIN] JSON parse error in reverse geocode: ' + e);
      }
    }
    callback('GPS LOCATION');
  };
  xhr.onerror = xhr.ontimeout = function() {
    callback('GPS LOCATION');
  };
  xhr.send();
}

function fetchWeatherForCoordinates(lat, lon, locationName, isGps, slotIndex) {
  console.log('[PEBBLERAIN] Fetching weather for (' + lat + ', ' + lon + ') name: ' + locationName + ' isGps=' + isGps + ' slot=' + slotIndex);

  var url = 'https://api.open-meteo.com/v1/forecast?latitude=' + lat.toFixed(4) +
            '&longitude=' + lon.toFixed(4) +
            '&daily=weather_code,temperature_2m_max,temperature_2m_min,apparent_temperature_max,apparent_temperature_min,uv_index_max,relative_humidity_2m_mean,wind_speed_10m_mean,precipitation_probability_max,sunrise,sunset,moon_phase,precipitation_sum' +
            '&hourly=temperature_2m,apparent_temperature,precipitation_probability,weather_code' +
            '&models=best_match' +
            '&current=temperature_2m,apparent_temperature,weather_code,is_day' +
            '&timezone=auto&forecast_days=14&forecast_hours=24' +
            '&minutely_15=precipitation&forecast_minutely_15=96&past_minutely_15=4';

  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, true);
  xhr.timeout = 15000;
  xhr.onload = function() {
    if (xhr.status >= 200 && xhr.status < 300) {
      try {
        var data = JSON.parse(xhr.responseText);
        processAndSendWeather(data, locationName, isGps, slotIndex);
      } catch (err) {
        console.log('[PEBBLERAIN] Error parsing weather JSON: ' + err);
        sendFetchStatus(2);
      }
    } else {
      console.log('[PEBBLERAIN] HTTP status error: ' + xhr.status);
      sendFetchStatus(2);
    }
  };
  xhr.onerror = xhr.ontimeout = function() {
    console.log('[PEBBLERAIN] Network request failed or timed out');
    sendFetchStatus(2);
  };
  xhr.send();
}

function processAndSendWeather(data, locationName, isGps, slotIndex) {
  if (!data || !data.current || !data.daily || !data.hourly) {
    console.log('[PEBBLERAIN] Incomplete weather data structure');
    sendFetchStatus(2);
    return;
  }

  var current = data.current;
  var daily = data.daily;
  var hourly = data.hourly;

  var currentCode = current.weather_code || 0;
  var currentIsDay = (current.is_day !== undefined) ? current.is_day : 1;
  var currentIcon = wmoToIcon(currentCode, currentIsDay);

  var currentTempC = Math.round(current.temperature_2m || 0);
  var currentFeelsC = Math.round(current.apparent_temperature || 0);

  var todayHighC = Math.round((daily.temperature_2m_max && daily.temperature_2m_max[0]) || 0);
  var todayLowC = Math.round((daily.temperature_2m_min && daily.temperature_2m_min[0]) || 0);
  var todayFeelsHighC = Math.round((daily.apparent_temperature_max && daily.apparent_temperature_max[0]) || 0);
  var todayFeelsLowC = Math.round((daily.apparent_temperature_min && daily.apparent_temperature_min[0]) || 0);

  var todayPrecipPct = Math.round((daily.precipitation_probability_max && daily.precipitation_probability_max[0]) || 0);
  var todayHumidityPct = Math.round((daily.relative_humidity_2m_mean && daily.relative_humidity_2m_mean[0]) || 0);
  var todayWindSpeedKmh = Math.round((daily.wind_speed_10m_mean && daily.wind_speed_10m_mean[0]) || 0);
  var todayUvIndex = Math.round((daily.uv_index_max && daily.uv_index_max[0]) || 0);

  var sunriseStr = '--:--';
  if (daily.sunrise && daily.sunrise[0]) {
    var srParts = daily.sunrise[0].split('T');
    if (srParts.length > 1) {
      sunriseStr = srParts[1].substring(0, 5);
    }
  }

  var sunsetStr = '--:--';
  if (daily.sunset && daily.sunset[0]) {
    var ssParts = daily.sunset[0].split('T');
    if (ssParts.length > 1) {
      sunsetStr = ssParts[1].substring(0, 5);
    }
  }

  var moonPhaseVal = (daily.moon_phase && daily.moon_phase[0] !== undefined) ? daily.moon_phase[0] : 0;
  var moonIconIdx = Math.floor(moonPhaseVal * 28 + 0.5) % 28;
  if (isNaN(moonIconIdx) || moonIconIdx < 0 || moonIconIdx >= 28) {
    moonIconIdx = 0;
  }

  var hourlyBytes = [];
  var hourlyCount = Math.min(24, (hourly.time ? hourly.time.length : 0));
  for (var h = 0; h < hourlyCount; h++) {
    var timeStr = hourly.time[h];
    var hourNum = 0;
    if (timeStr && timeStr.indexOf('T') !== -1) {
      hourNum = parseInt(timeStr.split('T')[1].split(':')[0], 10) || 0;
    } else {
      hourNum = (new Date()).getHours() + h;
    }
    hourNum = hourNum % 24;

    var hTemp = Math.round(hourly.temperature_2m[h] || 0);
    var hFeels = Math.round(hourly.apparent_temperature[h] || 0);
    var hPrecip = Math.round(hourly.precipitation_probability[h] || 0);
    var hCode = hourly.weather_code[h] || 0;
    var hIsDay = (hourNum >= 6 && hourNum < 20) ? 1 : 0;
    var hIcon = wmoToIcon(hCode, hIsDay);

    hourlyBytes.push(encodeUInt8(hourNum));
    hourlyBytes.push(encodeInt8(hTemp));
    hourlyBytes.push(encodeInt8(hFeels));
    hourlyBytes.push(encodeUInt8(hPrecip));
    hourlyBytes.push(encodeUInt8(hIcon));
  }

  var dailyBytes = [];
  var dailyCount = Math.min(14, (daily.time ? daily.time.length : 0));
  for (var d = 0; d < dailyCount; d++) {
    var dayDateStr = daily.time[d];
    var dayOfWeek = 0;
    if (dayDateStr) {
      var dObj = new Date(dayDateStr + 'T12:00:00');
      dayOfWeek = dObj.getDay();
    } else {
      dayOfWeek = ((new Date()).getDay() + d) % 7;
    }

    var dHigh = Math.round(daily.temperature_2m_max[d] || 0);
    var dLow = Math.round(daily.temperature_2m_min[d] || 0);
    var dFeelsHigh = Math.round(daily.apparent_temperature_max[d] || 0);
    var dFeelsLow = Math.round(daily.apparent_temperature_min[d] || 0);
    var dPrecip = Math.round(daily.precipitation_probability_max[d] || 0);
    var dCode = daily.weather_code[d] || 0;
    var dIcon = wmoToIcon(dCode, 1);

    dailyBytes.push(encodeUInt8(dayOfWeek));
    dailyBytes.push(encodeInt8(dHigh));
    dailyBytes.push(encodeInt8(dLow));
    dailyBytes.push(encodeInt8(dFeelsHigh));
    dailyBytes.push(encodeInt8(dFeelsLow));
    dailyBytes.push(encodeUInt8(dPrecip));
    dailyBytes.push(encodeUInt8(dIcon));
  }

  var hourlyPrecipBytes = [];
  var m15 = (data.minutely_15 && data.minutely_15.precipitation) ? data.minutely_15.precipitation : [];
  var m15Times = (data.minutely_15 && data.minutely_15.time) ? data.minutely_15.time : [];
  var targetHourTime = (hourly.time && hourly.time[0]) ? hourly.time[0] : null;

  var startIdx = -1;
  if (targetHourTime && m15Times.length > 0) {
    startIdx = m15Times.indexOf(targetHourTime);
    if (startIdx === -1) {
      var prefix = targetHourTime.substring(0, 13);
      for (var ti = 0; ti < m15Times.length; ti++) {
        if (m15Times[ti].substring(0, 13) === prefix && m15Times[ti].slice(-2) === '00') {
          startIdx = ti;
          break;
        }
      }
    }
  }
  if (startIdx < 0) {
    startIdx = 0;
  }
  console.log('[PEBBLERAIN] Aligned 15-min precip startIdx=' + startIdx + ' for targetHour=' + targetHourTime + ' (m15Total=' + m15.length + ')');

  for (var k = 0; k < 96; k++) {
    var pVal = 0;
    var srcIdx = startIdx + k;
    if (m15 && m15[srcIdx] !== undefined && m15[srcIdx] !== null) {
      pVal = Math.round(m15[srcIdx] * 100);
    }
    if (pVal < 0) pVal = 0;
    if (pVal > 65535) pVal = 65535;
    hourlyPrecipBytes.push(pVal & 0xFF);
    hourlyPrecipBytes.push((pVal >> 8) & 0xFF);
  }

  var dailyPrecipBytes = [];
  var dPrecipList = (daily && daily.precipitation_sum) ? daily.precipitation_sum : [];
  for (var d = 0; d < 14; d++) {
    var dpVal = 0;
    if (dPrecipList && dPrecipList[d] !== undefined && dPrecipList[d] !== null) {
      dpVal = Math.round(dPrecipList[d] * 100);
    }
    if (dpVal < 0) dpVal = 0;
    if (dpVal > 65535) dpVal = 65535;
    dailyPrecipBytes.push(dpVal & 0xFF);
    dailyPrecipBytes.push((dpVal >> 8) & 0xFF);
  }

  while (hourlyBytes.length < 120) {
    hourlyBytes.push(0);
  }
  for (var hi = 0; hi < hourlyPrecipBytes.length; hi++) {
    hourlyBytes.push(hourlyPrecipBytes[hi]);
  }

  while (dailyBytes.length < 98) {
    dailyBytes.push(0);
  }
  for (var di = 0; di < dailyPrecipBytes.length; di++) {
    dailyBytes.push(dailyPrecipBytes[di]);
  }

  var settings = getSettings();
  var isGpsVal = (isGps ? 1 : 0) | (settings.enableCurrentTempBar ? 2 : 0) | (settings.enablePrecipHistogram ? 4 : 0) | (settings.enableCurrentTimeBar ? 8 : 0);

  var dict = {};
  setField(dict, 'KEY_LOCATION_NAME', FALLBACK_KEYS.KEY_LOCATION_NAME, locationName || 'WEATHER');
  setField(dict, 'KEY_IS_GPS', FALLBACK_KEYS.KEY_IS_GPS, isGpsVal);
  setField(dict, 'KEY_CURRENT_TEMP_C', FALLBACK_KEYS.KEY_CURRENT_TEMP_C, currentTempC);
  setField(dict, 'KEY_CURRENT_FEELS_C', FALLBACK_KEYS.KEY_CURRENT_FEELS_C, currentFeelsC);
  setField(dict, 'KEY_CURRENT_ICON', FALLBACK_KEYS.KEY_CURRENT_ICON, currentIcon);
  setField(dict, 'KEY_TODAY_HIGH_C', FALLBACK_KEYS.KEY_TODAY_HIGH_C, todayHighC);
  setField(dict, 'KEY_TODAY_LOW_C', FALLBACK_KEYS.KEY_TODAY_LOW_C, todayLowC);
  setField(dict, 'KEY_TODAY_FEELS_HIGH_C', FALLBACK_KEYS.KEY_TODAY_FEELS_HIGH_C, todayFeelsHighC);
  setField(dict, 'KEY_TODAY_FEELS_LOW_C', FALLBACK_KEYS.KEY_TODAY_FEELS_LOW_C, todayFeelsLowC);
  setField(dict, 'KEY_TODAY_PRECIP_PCT', FALLBACK_KEYS.KEY_TODAY_PRECIP_PCT, todayPrecipPct);
  setField(dict, 'KEY_TODAY_HUMIDITY_PCT', FALLBACK_KEYS.KEY_TODAY_HUMIDITY_PCT, todayHumidityPct);
  setField(dict, 'KEY_TODAY_WIND_SPEED_KMH', FALLBACK_KEYS.KEY_TODAY_WIND_SPEED_KMH, todayWindSpeedKmh);
  setField(dict, 'KEY_TODAY_UV_INDEX', FALLBACK_KEYS.KEY_TODAY_UV_INDEX, todayUvIndex);
  setField(dict, 'KEY_SUNRISE_TIME', FALLBACK_KEYS.KEY_SUNRISE_TIME, sunriseStr);
  setField(dict, 'KEY_SUNSET_TIME', FALLBACK_KEYS.KEY_SUNSET_TIME, sunsetStr);
  setField(dict, 'KEY_MOON_PHASE_ICON', FALLBACK_KEYS.KEY_MOON_PHASE_ICON, moonIconIdx);
  setField(dict, 'KEY_HOURLY_DATA', FALLBACK_KEYS.KEY_HOURLY_DATA, hourlyBytes);
  setField(dict, 'KEY_DAILY_DATA', FALLBACK_KEYS.KEY_DAILY_DATA, dailyBytes);
  setField(dict, 'KEY_FETCH_STATUS', FALLBACK_KEYS.KEY_FETCH_STATUS, 1);
  setField(dict, 'KEY_ENABLE_CURRENT_TIME_BAR', FALLBACK_KEYS.KEY_ENABLE_CURRENT_TIME_BAR, settings.enableCurrentTimeBar ? 1 : 0);
  setField(dict, 'KEY_ENABLE_CURRENT_TEMP_BAR', FALLBACK_KEYS.KEY_ENABLE_CURRENT_TEMP_BAR, settings.enableCurrentTempBar ? 1 : 0);
  setField(dict, 'KEY_ENABLE_PRECIP_HISTOGRAM', FALLBACK_KEYS.KEY_ENABLE_PRECIP_HISTOGRAM, settings.enablePrecipHistogram ? 1 : 0);
  setField(dict, 'KEY_HOURLY_PRECIP_DATA', FALLBACK_KEYS.KEY_HOURLY_PRECIP_DATA, hourlyPrecipBytes);
  setField(dict, 'KEY_DAILY_PRECIP_DATA', FALLBACK_KEYS.KEY_DAILY_PRECIP_DATA, dailyPrecipBytes);

  var locs = settings.locations || [];
  var names = [];
  if (settings.useGps) {
    names.push(isGps ? locationName : 'GPS LOCATION');
    for (var li = 0; li < locs.length; li++) {
      names.push((locs[li].name || ('LOCATION ' + (li + 1))).toUpperCase());
    }
  } else {
    for (var li = 0; li < locs.length; li++) {
      names.push((locs[li].name || ('LOCATION ' + (li + 1))).toUpperCase());
    }
  }
  var namesStr = names.join('|');

  var currentSlot = (slotIndex !== undefined && slotIndex !== null) ? slotIndex : 0;
  setField(dict, 'KEY_LOCATION_INDEX', FALLBACK_KEYS.KEY_LOCATION_INDEX, currentSlot);
  setField(dict, 'KEY_LOCATION_COUNT', FALLBACK_KEYS.KEY_LOCATION_COUNT, names.length);
  setField(dict, 'KEY_LOCATION_NAMES', FALLBACK_KEYS.KEY_LOCATION_NAMES, namesStr);

  console.log('[PEBBLERAIN] Sending weather packet to watch: temp=' + currentTempC + '°C, icon=' + currentIcon +
              ', dailyDays=' + dailyCount + ' (' + dailyBytes.length + ' bytes), hourlyHours=' + hourlyCount + ' (' + hourlyBytes.length + ' bytes)' +
              ', currentTimeBar=' + (settings.enableCurrentTimeBar ? 'ON' : 'OFF') +
              ', currentTempBar=' + (settings.enableCurrentTempBar ? 'ON' : 'OFF') +
              ', precipHistogram=' + (settings.enablePrecipHistogram ? 'ON' : 'OFF') +
              ' (hPrecip=' + (hourlyPrecipBytes.length / 2) + ', dPrecip=' + (dailyPrecipBytes.length / 2) + ')' +
              ', location=' + locationName + ', slot=' + currentSlot + '/' + names.length);

  Pebble.sendAppMessage(dict, function() {
    console.log('[PEBBLERAIN] Weather packet successfully delivered to watch (' + dailyCount + ' daily days, ' + hourlyCount + ' hourly hours, precipHistogram=' + (settings.enablePrecipHistogram ? 'ON' : 'OFF') + ')');
  }, function(err) {
    console.log('[PEBBLERAIN] Failed to deliver weather to watch: ' + JSON.stringify(err));
  });
}

function sendFetchStatus(status) {
  var dict = {};
  var settings = getSettings();
  setField(dict, 'KEY_FETCH_STATUS', FALLBACK_KEYS.KEY_FETCH_STATUS, status);
  var isGpsVal = (settings.enableCurrentTempBar ? 2 : 0) | (settings.enablePrecipHistogram ? 4 : 0) | (settings.enableCurrentTimeBar ? 8 : 0);
  setField(dict, 'KEY_IS_GPS', FALLBACK_KEYS.KEY_IS_GPS, isGpsVal);
  setField(dict, 'KEY_ENABLE_CURRENT_TIME_BAR', FALLBACK_KEYS.KEY_ENABLE_CURRENT_TIME_BAR, settings.enableCurrentTimeBar ? 1 : 0);
  setField(dict, 'KEY_ENABLE_CURRENT_TEMP_BAR', FALLBACK_KEYS.KEY_ENABLE_CURRENT_TEMP_BAR, settings.enableCurrentTempBar ? 1 : 0);
  setField(dict, 'KEY_ENABLE_PRECIP_HISTOGRAM', FALLBACK_KEYS.KEY_ENABLE_PRECIP_HISTOGRAM, settings.enablePrecipHistogram ? 1 : 0);
  Pebble.sendAppMessage(dict);
}

function sendNoLocationStatus() {
  console.log('[PEBBLERAIN] No location available (GPS disabled & no manual locations).');
  var dict = {};
  var settings = getSettings();
  setField(dict, 'KEY_LOCATION_NAME', FALLBACK_KEYS.KEY_LOCATION_NAME, 'NO LOCATION');
  var isGpsVal = (settings.enableCurrentTempBar ? 2 : 0) | (settings.enablePrecipHistogram ? 4 : 0) | (settings.enableCurrentTimeBar ? 8 : 0);
  setField(dict, 'KEY_IS_GPS', FALLBACK_KEYS.KEY_IS_GPS, isGpsVal);
  setField(dict, 'KEY_LOCATION_INDEX', FALLBACK_KEYS.KEY_LOCATION_INDEX, 0);
  setField(dict, 'KEY_LOCATION_COUNT', FALLBACK_KEYS.KEY_LOCATION_COUNT, 0);
  setField(dict, 'KEY_LOCATION_NAMES', FALLBACK_KEYS.KEY_LOCATION_NAMES, 'NO LOCATION');
  setField(dict, 'KEY_FETCH_STATUS', FALLBACK_KEYS.KEY_FETCH_STATUS, 3);
  setField(dict, 'KEY_ENABLE_CURRENT_TIME_BAR', FALLBACK_KEYS.KEY_ENABLE_CURRENT_TIME_BAR, settings.enableCurrentTimeBar ? 1 : 0);
  setField(dict, 'KEY_ENABLE_CURRENT_TEMP_BAR', FALLBACK_KEYS.KEY_ENABLE_CURRENT_TEMP_BAR, settings.enableCurrentTempBar ? 1 : 0);
  setField(dict, 'KEY_ENABLE_PRECIP_HISTOGRAM', FALLBACK_KEYS.KEY_ENABLE_PRECIP_HISTOGRAM, settings.enablePrecipHistogram ? 1 : 0);
  Pebble.sendAppMessage(dict);
}

function updateWeather(desiredSlot) {
  var settings = getSettings();
  var locs = settings.locations || [];

  if (!settings.useGps && locs.length === 0) {
    sendNoLocationStatus();
    return;
  }

  sendFetchStatus(0);

  if (settings.useGps) {
    var totalSlots = 1 + locs.length;
    var slot = 0;
    if (desiredSlot !== undefined && desiredSlot !== null) {
      slot = parseInt(desiredSlot, 10);
      if (isNaN(slot) || slot < 0 || slot >= totalSlots) {
        slot = 0;
      }
    }

    if (slot === 0) {
      if (navigator.geolocation) {
        console.log('[PEBBLERAIN] Attempting GPS fetch for Slot 0...');
        navigator.geolocation.getCurrentPosition(
          function(pos) {
            var lat = pos.coords.latitude;
            var lon = pos.coords.longitude;
            console.log('[PEBBLERAIN] GPS success: (' + lat + ', ' + lon + ')');
            reverseGeocode(lat, lon, function(locName) {
              fetchWeatherForCoordinates(lat, lon, locName, true, 0);
            });
          },
          function(err) {
            console.log('[PEBBLERAIN] Geolocation failed (' + err.code + ': ' + err.message + ').');
            if (locs.length > 0) {
              var fallbackLoc = locs[0];
              var name = (fallbackLoc.name || 'WEATHER').toUpperCase();
              fetchWeatherForCoordinates(fallbackLoc.lat, fallbackLoc.lon, name, false, 0);
            } else {
              sendNoLocationStatus();
            }
          },
          { timeout: 10000, maximumAge: 60000, enableHighAccuracy: false }
        );
      } else {
        if (locs.length > 0) {
          var fallbackLoc = locs[0];
          var name = (fallbackLoc.name || 'WEATHER').toUpperCase();
          fetchWeatherForCoordinates(fallbackLoc.lat, fallbackLoc.lon, name, false, 0);
        } else {
          sendNoLocationStatus();
        }
      }
    } else {
      var manualIdx = slot - 1;
      var loc = locs[manualIdx];
      var name = (loc.name || ('LOCATION ' + slot)).toUpperCase();
      console.log('[PEBBLERAIN] Fetching manual location for Slot ' + slot + ': ' + name);
      fetchWeatherForCoordinates(loc.lat, loc.lon, name, false, slot);
    }
  } else {
    if (locs.length === 0) {
      sendNoLocationStatus();
      return;
    }

    var totalSlots = locs.length;
    var slot = 0;
    if (desiredSlot !== undefined && desiredSlot !== null) {
      slot = parseInt(desiredSlot, 10);
      if (isNaN(slot) || slot < 0 || slot >= totalSlots) {
        slot = 0;
      }
    } else {
      slot = 0;
    }

    var loc = locs[slot];
    var name = (loc.name || ('LOCATION ' + (slot + 1))).toUpperCase();
    console.log('[PEBBLERAIN] Fetching manual location (GPS off) for Slot ' + slot + ': ' + name);
    fetchWeatherForCoordinates(loc.lat, loc.lon, name, false, slot);
  }
}

function buildConfigPageHtml(settings) {
  var locationsJson = JSON.stringify(settings.locations || []);
  var useGpsChecked = settings.useGps ? 'checked' : '';
  var enableCurrentTimeBarChecked = settings.enableCurrentTimeBar ? 'checked' : '';
  var enableCurrentTempBarChecked = settings.enableCurrentTempBar ? 'checked' : '';
  var enablePrecipHistogramChecked = settings.enablePrecipHistogram ? 'checked' : '';

  return '<!DOCTYPE html>\n' +
    '<html>\n' +
    '<head>\n' +
    '  <meta charset="utf-8">\n' +
    '  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">\n' +
    '  <title>PebbleRain Settings</title>\n' +
    '  <style>\n' +
    '    * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }\n' +
    '    body {\n' +
    '      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;\n' +
    '      background: #f4f5f7; color: #222; margin: 0; padding: 16px;\n' +
    '    }\n' +
    '    .header {\n' +
    '      text-align: center; margin-bottom: 20px;\n' +
    '    }\n' +
    '    .header h1 { margin: 4px 0; font-size: 24px; color: #ff8000; }\n' +
    '    .card {\n' +
    '      background: #fff; border-radius: 12px; padding: 16px;\n' +
    '      box-shadow: 0 2px 8px rgba(0,0,0,0.06); margin-bottom: 16px;\n' +
    '    }\n' +
    '    .card h2 {\n' +
    '      font-size: 16px; margin: 0 0 12px 0; color: #333;\n' +
    '      border-bottom: 1px solid #eee; padding-bottom: 8px;\n' +
    '    }\n' +
    '    .row { display: flex; align-items: center; justify-content: space-between; margin: 0; }\n' +
    '    .switch {\n' +
    '      position: relative; display: inline-block; width: 50px; height: 30px; flex-shrink: 0;\n' +
    '    }\n' +
    '    .switch input { opacity: 0; width: 0; height: 0; position: absolute; }\n' +
    '    .slider {\n' +
    '      position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;\n' +
    '      background-color: #d1d5db; transition: background-color .25s ease; border-radius: 30px;\n' +
    '    }\n' +
    '    .slider:before {\n' +
    '      position: absolute; content: ""; height: 24px; width: 24px;\n' +
    '      left: 3px; bottom: 3px; background-color: white;\n' +
    '      transition: transform .25s ease; border-radius: 50%;\n' +
    '      box-shadow: 0 2px 4px rgba(0,0,0,0.25);\n' +
    '    }\n' +
    '    input:checked + .slider { background-color: #ff8000; }\n' +
    '    input:checked + .slider:before { transform: translateX(20px); }\n' +
    '    .search-box { display: flex; gap: 8px; margin-bottom: 10px; }\n' +
    '    input[type="text"] {\n' +
    '      flex: 1; padding: 10px 12px; font-size: 14px; border: 1px solid #ddd;\n' +
    '      border-radius: 8px; outline: none;\n' +
    '    }\n' +
    '    input[type="text"]:focus { border-color: #ff8000; }\n' +
    '    button.btn {\n' +
    '      background: #ff8000; color: #fff; border: none; padding: 10px 16px;\n' +
    '      border-radius: 8px; font-size: 14px; font-weight: 600; cursor: pointer;\n' +
    '    }\n' +
    '    button.btn:active { background: #e07000; }\n' +
    '    .search-results { max-height: 180px; overflow-y: auto; margin-bottom: 12px; }\n' +
    '    .result-item {\n' +
    '      padding: 8px 12px; border-bottom: 1px solid #f0f0f0; display: flex;\n' +
    '      justify-content: space-between; align-items: center; font-size: 13px;\n' +
    '    }\n' +
    '    .result-item:hover { background: #fbfbfb; }\n' +
    '    .result-item button {\n' +
    '      background: #28a745; color: #fff; border: none; padding: 4px 10px;\n' +
    '      border-radius: 6px; font-size: 12px; font-weight: 600; cursor: pointer;\n' +
    '    }\n' +
    '    .loc-list { margin-top: 10px; }\n' +
    '    .loc-item {\n' +
    '      display: flex; align-items: center; justify-content: space-between;\n' +
    '      padding: 10px 12px; background: #fafafa; border: 1px solid #eee;\n' +
    '      border-radius: 8px; margin-bottom: 8px; transition: background .15s, border-color .15s;\n' +
    '    }\n' +
    '    .loc-item.dragging {\n' +
    '      opacity: 0.5; background: #fff3e6; border: 1px dashed #ff8000;\n' +
    '    }\n' +
    '    .loc-item.drag-over {\n' +
    '      border-top: 2px solid #ff8000;\n' +
    '    }\n' +
    '    .drag-handle {\n' +
    '      display: flex; align-items: center; justify-content: center;\n' +
    '      cursor: grab; cursor: -webkit-grab; padding: 4px 8px 4px 0;\n' +
    '      touch-action: none; -webkit-user-select: none; user-select: none;\n' +
    '      color: #888; flex-shrink: 0;\n' +
    '    }\n' +
    '    .drag-handle:active {\n' +
    '      cursor: grabbing; cursor: -webkit-grabbing;\n' +
    '    }\n' +
    '    .drag-handle svg { display: block; pointer-events: none; }\n' +
    '    .loc-details { flex: 1; font-size: 14px; font-weight: 500; margin-left: 6px; margin-right: 10px; }\n' +
    '    .loc-coords { font-size: 11px; color: #888; font-weight: normal; margin-top: 2px; }\n' +
    '    .btn-delete {\n' +
    '      background: #dc3545; color: #fff; border: none; padding: 6px 10px;\n' +
    '      border-radius: 6px; font-size: 12px; cursor: pointer; flex-shrink: 0;\n' +
    '    }\n' +
    '    .hint {\n' +
    '      font-size: 12px; color: #777; margin-top: 6px; line-height: 1.4;\n' +
    '    }\n' +
    '    .btn-save {\n' +
    '      width: 100%; padding: 14px; background: #ff8000; color: #fff;\n' +
    '      border: none; border-radius: 10px; font-size: 16px; font-weight: bold;\n' +
    '      cursor: pointer; box-shadow: 0 4px 12px rgba(255,128,0,0.3);\n' +
    '      margin-top: 8px; margin-bottom: 24px;\n' +
    '    }\n' +
    '    .btn-save:active { background: #e07000; }\n' +
    '    .manual-latlon {\n' +
    '      display: flex; gap: 8px; margin-top: 8px;\n' +
    '    }\n' +
    '    .manual-latlon input {\n' +
    '      width: 50%; padding: 10px 12px; font-size: 14px; border: 1px solid #ddd;\n' +
    '      border-radius: 8px; outline: none;\n' +
    '    }\n' +
    '    .manual-latlon input:focus { border-color: #ff8000; }\n' +
    '  </style>\n' +
    '</head>\n' +
    '<body>\n' +
    '  <div class="header">\n' +
    '    <h1>PebbleRain</h1>\n' +
    '  </div>\n' +
    '\n' +
    '  <div class="card">\n' +
    '    <div class="row">\n' +
    '      <span style="font-size: 16px; font-weight: 600; color: #222;">Enable GPS</span>\n' +
    '      <label class="switch">\n' +
    '        <input type="checkbox" id="useGps" ' + useGpsChecked + '>\n' +
    '        <span class="slider"></span>\n' +
    '      </label>\n' +
    '    </div>\n' +
    '  </div>\n' +
    '\n' +
    '  <div class="card">\n' +
    '    <h2>Add Location</h2>\n' +
    '    <div class="search-box">\n' +
    '      <input type="text" id="cityInput" placeholder="Search location...">\n' +
    '      <button class="btn" id="btnSearch">Find</button>\n' +
    '    </div>\n' +
    '    <div id="searchResults" class="search-results"></div>\n' +
    '    <details style="margin-top: 10px; font-size: 13px; color: #555;">\n' +
    '      <summary style="cursor: pointer;">Or enter coordinates manually</summary>\n' +
    '      <input type="text" id="manualName" placeholder="Location Name (e.g. Home)" style="margin-top: 8px; width: 100%;">\n' +
    '      <div class="manual-latlon">\n' +
    '        <input type="number" step="any" id="manualLat" placeholder="Latitude (e.g. 12.34)">\n' +
    '        <input type="number" step="any" id="manualLon" placeholder="Longitude (e.g. -12.34)">\n' +
    '      </div>\n' +
    '      <button class="btn" id="btnAddManual" style="margin-top: 8px; width: 100%;">Add Coordinates</button>\n' +
    '    </details>\n' +
    '  </div>\n' +
    '\n' +
    '  <div class="card">\n' +
    '    <h2>Saved Locations</h2>\n' +
    '    <div id="locList" class="loc-list"></div>\n' +
    '  </div>\n' +
    '\n' +
    '  <div class="card">\n' +
    '    <h2>Advanced Settings</h2>\n' +
    '    <label style="display: flex; align-items: center; gap: 10px; cursor: pointer; font-size: 15px; font-weight: 500; color: #222;">\n' +
    '      <input type="checkbox" id="enableCurrentTimeBar" ' + enableCurrentTimeBarChecked + ' style="width: 18px; height: 18px; cursor: pointer; accent-color: #ff8000;">\n' +
    '      <span>Enable Current Time Bar</span>\n' +
    '    </label>\n' +
    '    <label style="display: flex; align-items: center; gap: 10px; cursor: pointer; font-size: 15px; font-weight: 500; color: #222; margin-top: 12px;">\n' +
    '      <input type="checkbox" id="enableCurrentTempBar" ' + enableCurrentTempBarChecked + ' style="width: 18px; height: 18px; cursor: pointer; accent-color: #ff8000;">\n' +
    '      <span>Enable Current Temp Bar</span>\n' +
    '    </label>\n' +
    '    <label style="display: flex; align-items: center; gap: 10px; cursor: pointer; font-size: 15px; font-weight: 500; color: #222; margin-top: 12px;">\n' +
    '      <input type="checkbox" id="enablePrecipHistogram" ' + enablePrecipHistogramChecked + ' style="width: 18px; height: 18px; cursor: pointer; accent-color: #ff8000;">\n' +
    '      <span>Enable Precipitation Histogram</span>\n' +
    '    </label>\n' +
    '  </div>\n' +
    '\n' +
    '  <button class="btn-save" id="btnSave">Save &amp; Apply</button>\n' +
    '\n' +
    '  <script>\n' +
    '    var locations = ' + locationsJson + ';\n' +
    '\n' +
    '    function renderLocations() {\n' +
    '      var list = document.getElementById("locList");\n' +
    '      list.innerHTML = "";\n' +
    '      if (!locations || locations.length === 0) {\n' +
    '        list.innerHTML = "<div class=\'hint\'>No locations saved yet.</div>";\n' +
    '        return;\n' +
    '      }\n' +
    '      locations.forEach(function(loc, i) {\n' +
    '        var item = document.createElement("div");\n' +
    '        item.className = "loc-item";\n' +
    '        item.setAttribute("data-index", i);\n' +
    '        item.innerHTML = "<div class=\'drag-handle\' title=\'Drag to reorder\' data-index=\'" + i + "\'>" +\n' +
    '            "<svg width=\'16\' height=\'16\' viewBox=\'0 0 24 24\' fill=\'none\'>" +\n' +
    '              "<circle cx=\'8\' cy=\'5\' r=\'2\' fill=\'#888\'/>" +\n' +
    '              "<circle cx=\'16\' cy=\'5\' r=\'2\' fill=\'#888\'/>" +\n' +
    '              "<circle cx=\'8\' cy=\'12\' r=\'2\' fill=\'#888\'/>" +\n' +
    '              "<circle cx=\'16\' cy=\'12\' r=\'2\' fill=\'#888\'/>" +\n' +
    '              "<circle cx=\'8\' cy=\'19\' r=\'2\' fill=\'#888\'/>" +\n' +
    '              "<circle cx=\'16\' cy=\'19\' r=\'2\' fill=\'#888\'/>" +\n' +
    '            "</svg>" +\n' +
    '          "</div>" +\n' +
    '          "<div class=\'loc-details\'>" +\n' +
    '            "<div>" + loc.name + "</div>" +\n' +
    '            "<div class=\'loc-coords\'>" + Number(loc.lat).toFixed(4) + ", " + Number(loc.lon).toFixed(4) + "</div>" +\n' +
    '          "</div>" +\n' +
    '          "<button class=\'btn-delete\' data-index=\'" + i + "\'>Delete</button>";\n' +
    '        list.appendChild(item);\n' +
    '      });\n' +
    '\n' +
    '      // Bind delete buttons\n' +
    '      var deletes = list.querySelectorAll(".btn-delete");\n' +
    '      deletes.forEach(function(d) {\n' +
    '        d.addEventListener("click", function(e) {\n' +
    '          var idx = parseInt(e.target.getAttribute("data-index"), 10);\n' +
    '          locations.splice(idx, 1);\n' +
    '          renderLocations();\n' +
    '        });\n' +
    '      });\n' +
    '\n' +
    '      // Bind reordering (both desktop HTML5 drag & drop and mobile touch drag)\n' +
    '      var dragSrc = null;\n' +
    '      var items = list.querySelectorAll(".loc-item");\n' +
    '      items.forEach(function(item) {\n' +
    '        var handle = item.querySelector(".drag-handle");\n' +
    '        var idx = parseInt(item.getAttribute("data-index"), 10);\n' +
    '\n' +
    '        // Desktop drag-and-drop\n' +
    '        item.setAttribute("draggable", "true");\n' +
    '        item.addEventListener("dragstart", function(e) {\n' +
    '          dragSrc = idx;\n' +
    '          e.dataTransfer.effectAllowed = "move";\n' +
    '          item.classList.add("dragging");\n' +
    '        });\n' +
    '        item.addEventListener("dragover", function(e) {\n' +
    '          e.preventDefault();\n' +
    '          e.dataTransfer.dropEffect = "move";\n' +
    '          item.classList.add("drag-over");\n' +
    '        });\n' +
    '        item.addEventListener("dragleave", function(e) {\n' +
    '          item.classList.remove("drag-over");\n' +
    '        });\n' +
    '        item.addEventListener("drop", function(e) {\n' +
    '          e.preventDefault();\n' +
    '          item.classList.remove("drag-over");\n' +
    '          var targetIdx = parseInt(item.getAttribute("data-index"), 10);\n' +
    '          if (dragSrc !== null && targetIdx !== dragSrc) {\n' +
    '            var moved = locations.splice(dragSrc, 1)[0];\n' +
    '            locations.splice(targetIdx, 0, moved);\n' +
    '            renderLocations();\n' +
    '          }\n' +
    '        });\n' +
    '        item.addEventListener("dragend", function() {\n' +
    '          item.classList.remove("dragging");\n' +
    '          list.querySelectorAll(".loc-item").forEach(function(el) { el.classList.remove("drag-over"); });\n' +
    '        });\n' +
    '\n' +
    '        // Mobile Touch drag\n' +
    '        if (handle) {\n' +
    '          handle.addEventListener("touchstart", function(e) {\n' +
    '            dragSrc = idx;\n' +
    '            item.classList.add("dragging");\n' +
    '          }, { passive: true });\n' +
    '\n' +
    '          handle.addEventListener("touchmove", function(e) {\n' +
    '            if (dragSrc === null) return;\n' +
    '            var touch = e.touches[0];\n' +
    '            var elem = document.elementFromPoint(touch.clientX, touch.clientY);\n' +
    '            if (!elem) return;\n' +
    '            var targetItem = elem.closest(".loc-item");\n' +
    '            if (targetItem) {\n' +
    '              var targetIdx = parseInt(targetItem.getAttribute("data-index"), 10);\n' +
    '              if (!isNaN(targetIdx) && targetIdx !== dragSrc) {\n' +
    '                var moved = locations.splice(dragSrc, 1)[0];\n' +
    '                locations.splice(targetIdx, 0, moved);\n' +
    '                dragSrc = targetIdx;\n' +
    '                renderLocations();\n' +
    '              }\n' +
    '            }\n' +
    '          }, { passive: true });\n' +
    '\n' +
    '          handle.addEventListener("touchend", function(e) {\n' +
    '            dragSrc = null;\n' +
    '            renderLocations();\n' +
    '          });\n' +
    '        }\n' +
    '      });\n' +
    '    }\n' +
    '\n' +
    '    // Geocoding Search\n' +
    '    document.getElementById("btnSearch").addEventListener("click", function() {\n' +
    '      var q = document.getElementById("cityInput").value.trim();\n' +
    '      if (!q) return;\n' +
    '      var resultsDiv = document.getElementById("searchResults");\n' +
    '      resultsDiv.innerHTML = "<div class=\'hint\'>Searching...</div>";\n' +
    '      var url = "https://geocoding-api.open-meteo.com/v1/search?name=" + encodeURIComponent(q) + "&count=5&language=en&format=json";\n' +
    '      var xhr = new XMLHttpRequest();\n' +
    '      xhr.open("GET", url, true);\n' +
    '      xhr.onload = function() {\n' +
    '        if (xhr.status >= 200 && xhr.status < 300) {\n' +
    '          var data = JSON.parse(xhr.responseText);\n' +
    '          if (data && data.results && data.results.length > 0) {\n' +
    '            resultsDiv.innerHTML = "";\n' +
    '            data.results.forEach(function(item) {\n' +
    '              var div = document.createElement("div");\n' +
    '              div.className = "result-item";\n' +
    '              var parts = [item.name];\n' +
    '              if (item.admin1) parts.push(item.admin1);\n' +
    '              if (item.country) parts.push(item.country);\n' +
    '              var label = parts.join(", ");\n' +
    '              div.innerHTML = "<span>" + label + "</span>" +\n' +
    '                "<button>+ Add</button>";\n' +
    '              div.querySelector("button").addEventListener("click", function() {\n' +
    '                var shortName = item.name;\n' +
    '                if (item.admin1) {\n' +
    '                  shortName += ", " + (item.country_code === "US" ? (item.admin1.length <= 3 ? item.admin1 : item.country_code) : item.country_code);\n' +
    '                } else if (item.country_code) {\n' +
    '                  shortName += ", " + item.country_code;\n' +
    '                }\n' +
    '                locations.push({\n' +
    '                  name: shortName,\n' +
    '                  lat: item.latitude,\n' +
    '                  lon: item.longitude,\n' +
    '                  country: item.country || ""\n' +
    '                });\n' +
    '                renderLocations();\n' +
    '                resultsDiv.innerHTML = "";\n' +
    '                document.getElementById("cityInput").value = "";\n' +
    '              });\n' +
    '              resultsDiv.appendChild(div);\n' +
    '            });\n' +
    '          } else {\n' +
    '            resultsDiv.innerHTML = "<div class=\'hint\'>No locations found. Try another query.</div>";\n' +
    '          }\n' +
    '        } else {\n' +
    '          resultsDiv.innerHTML = "<div class=\'hint\'>Search failed. Please try again.</div>";\n' +
    '        }\n' +
    '      };\n' +
    '      xhr.onerror = function() {\n' +
    '        resultsDiv.innerHTML = "<div class=\'hint\'>Network error during search.</div>";\n' +
    '      };\n' +
    '      xhr.send();\n' +
    '    });\n' +
    '\n' +
    '    // Add manual lat/lon\n' +
    '    document.getElementById("btnAddManual").addEventListener("click", function() {\n' +
    '      var name = document.getElementById("manualName").value.trim() || "Manual Location";\n' +
    '      var lat = parseFloat(document.getElementById("manualLat").value);\n' +
    '      var lon = parseFloat(document.getElementById("manualLon").value);\n' +
    '      if (isNaN(lat) || isNaN(lon)) {\n' +
    '        alert("Please enter valid latitude and longitude numbers.");\n' +
    '        return;\n' +
    '      }\n' +
    '      locations.push({ name: name, lat: lat, lon: lon, country: "" });\n' +
    '      renderLocations();\n' +
    '      document.getElementById("manualName").value = "";\n' +
    '      document.getElementById("manualLat").value = "";\n' +
    '      document.getElementById("manualLon").value = "";\n' +
    '    });\n' +
    '\n' +
    '    // Initial render\n' +
    '    renderLocations();\n' +
    '\n' +
    '    // Save & Return\n' +
    '    function getQueryParam(variable, defaultValue) {\n' +
    '      var query = location.search.substring(1);\n' +
    '      var vars = query.split("&");\n' +
    '      for (var i = 0; i < vars.length; i++) {\n' +
    '        var pair = vars[i].split("=");\n' +
    '        if (pair[0] === variable) {\n' +
    '          return decodeURIComponent(pair[1]);\n' +
    '        }\n' +
    '      }\n' +
    '      return defaultValue || false;\n' +
    '    }\n' +
    '\n' +
    '    document.getElementById("btnSave").addEventListener("click", function() {\n' +
    '      var config = {\n' +
    '        useGps: document.getElementById("useGps").checked,\n' +
    '        locations: locations,\n' +
    '        activeLocationIndex: 0,\n' +
    '        enableCurrentTimeBar: document.getElementById("enableCurrentTimeBar").checked,\n' +
    '        enableCurrentTempBar: document.getElementById("enableCurrentTempBar").checked,\n' +
    '        enablePrecipHistogram: document.getElementById("enablePrecipHistogram").checked\n' +
    '      };\n' +
    '      var returnTo = getQueryParam("return_to", "pebblejs://close#");\n' +
    '      document.location = returnTo + encodeURIComponent(JSON.stringify(config));\n' +
    '    });\n' +
    '  </script>\n' +
    '</body>\n' +
    '</html>';
}

Pebble.addEventListener('ready', function(e) {
  console.log('[PEBBLERAIN] PebbleKit JS ready. Triggering weather fetch...');
  updateWeather();
});

Pebble.addEventListener('appmessage', function(e) {
  console.log('[PEBBLERAIN] Received message from watch: ' + JSON.stringify(e.payload));
  var reqSlot = undefined;
  if (e && e.payload) {
    if (e.payload.KEY_REQUEST_LOCATION_INDEX !== undefined) {
      reqSlot = e.payload.KEY_REQUEST_LOCATION_INDEX;
    } else if (e.payload[20] !== undefined) {
      reqSlot = e.payload[20];
    } else if (e.payload['20'] !== undefined) {
      reqSlot = e.payload['20'];
    }
  }
  if (reqSlot !== undefined) {
    console.log('[PEBBLERAIN] Watch requested location switch to slot: ' + reqSlot);
    updateWeather(reqSlot);
  } else {
    updateWeather();
  }
});

Pebble.addEventListener('showConfiguration', function(e) {
  var settings = getSettings();
  var html = buildConfigPageHtml(settings);
  var dataUri = 'data:text/html;charset=utf-8,' + encodeURIComponent(html);
  Pebble.openURL(dataUri);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var respStr = e.response;
      if (typeof respStr === 'string' && respStr.charAt(0) === '#') {
        respStr = respStr.substring(1);
      }
      var config;
      try {
        config = JSON.parse(decodeURIComponent(respStr));
      } catch (err1) {
        config = JSON.parse(respStr);
      }
      saveSettings(config);
      console.log('[PEBBLERAIN] Updated configuration received: ' + JSON.stringify(config));
      updateWeather();
    } catch (err) {
      console.log('[PEBBLERAIN] Error parsing settings from webview: ' + err);
    }
  }
});
