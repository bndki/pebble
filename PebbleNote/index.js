var KEYS = {
  COMMAND: 0,
  PATH: 1,
  NAME: 2,
  IS_DIR: 3,
  IS_PIN: 4,
  ITEM_CLEAR: 5,
  ITEM_INDEX: 6,
  ITEM_COUNT: 7,
  ITEM_NAME: 8,
  ITEM_PATH: 9,
  ITEM_IS_DIR: 10,
  ITEM_IS_PIN: 11,
  ITEM_IS_LAST: 12,
  CHUNK_DATA: 13,
  CHUNK_INDEX: 14,
  CHUNK_TOTAL: 15,
  CHUNK_IS_LAST: 16,
  ERROR: 17,
  STATUS: 18,
  SETTINGS_ANIMATE: 19,
  SETTINGS_FONT_SIZE: 20,
  CURRENT_DIR: 21
};

var CMD = {
  GET_TREE: 1,
  GET_FILE: 2,
  TOGGLE_PIN: 3,
  STATUS_MSG: 4
};

var messageKeys = null;
try {
  messageKeys = require('message_keys');
} catch (e) {
  messageKeys = null;
}

var DEFAULT_HOST = '127.0.0.1';
var DEFAULT_PORT = 8765;
var CHUNK_SIZE = 200;

var s_last_tree_fetch_time = 0;
var s_last_tree_fetch_path = null;
var s_active_stream_token = 0;

function getSettings() {
  try {
    var raw = localStorage.getItem('pebble_note_settings');
    if (raw) {
      var s = JSON.parse(raw);
      return {
        port: s.port || DEFAULT_PORT,
        animate_titles: !!s.animate_titles,
        font_size: s.font_size || 'default',
        order: s.order || 'a-z',
        hide_extension: !!s.hide_extension
      };
    }
  } catch (err) {
    console.log('Error parsing settings: ' + err);
  }
  return { port: DEFAULT_PORT, animate_titles: false, font_size: 'default', order: 'a-z', hide_extension: false };
}

function naturalCompare(strA, strB) {
  if (strA === strB) return 0;
  var sA = (strA || '').toString();
  var sB = (strB || '').toString();

  try {
    if (typeof Intl !== 'undefined' && Intl.Collator) {
      if (!naturalCompare._collator) {
        naturalCompare._collator = new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' });
      }
      return naturalCompare._collator.compare(sA, sB);
    }
  } catch (e) {}

  var re = /(\d+)|(\D+)/g;
  var tokensA = sA.match(re) || [];
  var tokensB = sB.match(re) || [];

  var len = Math.min(tokensA.length, tokensB.length);
  for (var i = 0; i < len; i++) {
    var tA = tokensA[i];
    var tB = tokensB[i];

    var isNumA = /^\d+$/.test(tA);
    var isNumB = /^\d+$/.test(tB);

    if (isNumA && isNumB) {
      var numA = parseInt(tA, 10);
      var numB = parseInt(tB, 10);
      if (numA !== numB) {
        return numA - numB;
      }
      if (tA.length !== tB.length) {
        return tA.length - tB.length;
      }
    } else {
      var cmp = tA.toLowerCase().localeCompare(tB.toLowerCase());
      if (cmp !== 0) {
        return cmp;
      }
      if (tA !== tB) {
        return tA < tB ? -1 : 1;
      }
    }
  }

  return tokensA.length - tokensB.length;
}

function compareItems(a, b, order) {
  if (a.isDir && !b.isDir) return -1;
  if (!a.isDir && b.isDir) return 1;

  if (order === 'z-a') {
    return naturalCompare(b.name, a.name);
  } else if (order === 'last-modified') {
    var modA = a.lastModified || 0;
    var modB = b.lastModified || 0;
    if (modB !== modA) {
      return modB - modA;
    }
    return naturalCompare(a.name, b.name);
  } else if (order === 'first-modified') {
    var modA = a.lastModified || 0;
    var modB = b.lastModified || 0;
    if (modA !== modB) {
      return modA - modB;
    }
    return naturalCompare(a.name, b.name);
  } else {
    return naturalCompare(a.name, b.name);
  }
}

function getServerUrl() {
  var s = getSettings();
  var port = s.port || DEFAULT_PORT;
  return 'http://' + DEFAULT_HOST + ':' + port;
}

function getPins() {
  try {
    var raw = localStorage.getItem('pebble_note_pins');
    if (raw) {
      var parsed = JSON.parse(raw);
      if (Array.isArray(parsed)) {
        var uniquePins = [];
        var seen = {};
        for (var i = 0; i < parsed.length; i++) {
          var p = parsed[i];
          if (p && p.path && !seen[p.path]) {
            seen[p.path] = true;
            uniquePins.push(p);
          }
        }
        return uniquePins;
      }
    }
  } catch (err) {
    console.log('Error parsing pins: ' + err);
  }
  return [];
}

function savePins(pins) {
  try {
    localStorage.setItem('pebble_note_pins', JSON.stringify(pins));
  } catch (err) {
    console.log('Error saving pins: ' + err);
  }
}

function makePayload(entries) {
  var payload = {};
  for (var key in entries) {
    if (entries.hasOwnProperty(key)) {
      var val = entries[key];
      if (KEYS[key] !== undefined) {
        payload[KEYS[key]] = val;
      }
      payload[key] = val;
      if (messageKeys && messageKeys[key] !== undefined) {
        payload[messageKeys[key]] = val;
      }
    }
  }
  return payload;
}

function getValue(payload, name) {
  if (!payload) return undefined;
  var id = KEYS[name];
  if (payload[name] !== undefined) return payload[name];
  if (payload['KEY_' + name] !== undefined) return payload['KEY_' + name];
  if (id !== undefined) {
    if (payload[id] !== undefined) return payload[id];
    if (payload['' + id] !== undefined) return payload['' + id];
  }
  if (messageKeys && messageKeys[name] !== undefined) {
    var mkId = messageKeys[name];
    if (payload[mkId] !== undefined) return payload[mkId];
    if (payload['' + mkId] !== undefined) return payload['' + mkId];
  }
  return undefined;
}

function sendMessage(dict, onSuccess, onFailure) {
  Pebble.sendAppMessage(dict, onSuccess, onFailure);
}

function sendError(errMsg) {
  var dict = makePayload({
    ERROR: errMsg
  });
  sendMessage(dict, function() {}, function(err) {
    console.log('Failed sending error message: ' + JSON.stringify(err));
  });
}

function sendSettingsToWatch() {
  var s = getSettings();
  var fontCode = 1;
  if (s.font_size === 'small') fontCode = 0;
  else if (s.font_size === 'large') fontCode = 2;

  var dict = makePayload({
    SETTINGS_ANIMATE: s.animate_titles ? 1 : 0,
    SETTINGS_FONT_SIZE: fontCode
  });

  sendMessage(dict, function() {
    console.log('Settings synced to watch: animate=' + s.animate_titles + ', font=' + s.font_size);
  }, function(err) {
    console.log('Failed syncing settings to watch: ' + JSON.stringify(err));
  });
}

function fetchAndStreamTree(path) {
  path = path || '';
  var now = Date.now();

  if (path === s_last_tree_fetch_path && (now - s_last_tree_fetch_time) < 1000) {
    console.log('Debouncing duplicate fetchAndStreamTree for path: ' + path);
    return;
  }
  s_last_tree_fetch_time = now;
  s_last_tree_fetch_path = path;

  s_active_stream_token++;
  var fetchToken = s_active_stream_token;

  var url = getServerUrl() + '/tree?path=' + encodeURIComponent(path);
  console.log('Fetching tree: ' + url);

  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, true);
  xhr.timeout = 10000;

  xhr.onload = function() {
    if (fetchToken !== s_active_stream_token) {
      console.log('Discarding stale XHR tree response');
      return;
    }

    if (xhr.status === 200) {
      try {
        var items = JSON.parse(xhr.responseText);
        handleDirectoryResponse(path, items);
      } catch (e) {
        console.log('JSON parse error: ' + e);
        sendError('Invalid server JSON');
      }
    } else {
      console.log('HTTP error: ' + xhr.status);
      sendError('HTTP ' + xhr.status);
    }
  };

  xhr.onerror = function() {
    if (fetchToken !== s_active_stream_token) return;
    console.log('Network error fetching tree');
    sendError('Android server offline');
  };

  xhr.ontimeout = function() {
    if (fetchToken !== s_active_stream_token) return;
    console.log('Timeout fetching tree');
    sendError('Request timed out');
  };

  xhr.send();
}

function handleDirectoryResponse(currentPath, serverItems) {
  if (!Array.isArray(serverItems)) {
    serverItems = [];
  }

  var settings = getSettings();
  var order = settings.order || 'a-z';
  var pins = getPins();

  if (!currentPath || currentPath === '') {
    for (var p = 0; p < pins.length; p++) {
      for (var s = 0; s < serverItems.length; s++) {
        if (pins[p].name === serverItems[s].name || pins[p].path === serverItems[s].name) {
          if (serverItems[s].lastModified) {
            pins[p].lastModified = serverItems[s].lastModified;
          }
          break;
        }
      }
    }
  }

  pins.sort(function(a, b) {
    return compareItems(a, b, order);
  });

  serverItems.sort(function(a, b) {
    return compareItems(a, b, order);
  });

  function formatDisplayName(name, isDir) {
    if (!settings.hide_extension || isDir) {
      return name;
    }
    return name.replace(/\.(txt|md)$/i, '');
  }

  var itemsToSend = [];

  if (!currentPath || currentPath === '') {
    for (var i = 0; i < pins.length; i++) {
      itemsToSend.push({
        name: formatDisplayName(pins[i].name, pins[i].isDir),
        path: pins[i].path,
        isDir: !!pins[i].isDir,
        isPin: true
      });
    }

    for (var j = 0; j < serverItems.length; j++) {
      var sItem = serverItems[j];
      var relPath = sItem.name;
      var isPinned = pins.some(function(pin) {
        return pin.path === relPath;
      });

      if (!isPinned) {
        itemsToSend.push({
          name: formatDisplayName(sItem.name, sItem.isDir),
          path: relPath,
          isDir: !!sItem.isDir,
          isPin: false
        });
      }
    }
  } else {
    for (var k = 0; k < serverItems.length; k++) {
      var item = serverItems[k];
      var fullPath = currentPath + '/' + item.name;
      var isItemPinned = pins.some(function(pin) {
        return pin.path === fullPath;
      });

      itemsToSend.push({
        name: formatDisplayName(item.name, item.isDir),
        path: fullPath,
        isDir: !!item.isDir,
        isPin: isItemPinned
      });
    }
  }

  var uniqueItems = [];
  var seenPaths = {};
  for (var k = 0; k < itemsToSend.length; k++) {
    var pth = itemsToSend[k].path;
    if (!seenPaths[pth]) {
      seenPaths[pth] = true;
      uniqueItems.push(itemsToSend[k]);
    }
  }

  streamItemsToWatch(uniqueItems);
}

function streamItemsToWatch(items) {
  s_active_stream_token++;
  var streamToken = s_active_stream_token;

  if (items.length === 0) {
    var emptyDict = makePayload({
      ITEM_CLEAR: 1,
      ITEM_COUNT: 0,
      ITEM_IS_LAST: 1
    });
    sendMessage(emptyDict, function() {}, function() {});
    return;
  }

  var index = 0;

  function sendNext() {
    if (streamToken !== s_active_stream_token) {
      console.log('Aborting superseded stream #' + streamToken);
      return;
    }

    if (index >= items.length) {
      console.log('Finished streaming ' + items.length + ' items');
      return;
    }

    var item = items[index];
    var isLast = (index === items.length - 1) ? 1 : 0;
    var dict = makePayload({
      ITEM_INDEX: index,
      ITEM_COUNT: items.length,
      ITEM_NAME: item.name,
      ITEM_PATH: item.path,
      ITEM_IS_DIR: item.isDir ? 1 : 0,
      ITEM_IS_PIN: item.isPin ? 1 : 0,
      ITEM_IS_LAST: isLast
    });

    if (index === 0) {
      dict[KEYS.ITEM_CLEAR] = 1;
      dict['ITEM_CLEAR'] = 1;
      if (messageKeys && messageKeys.ITEM_CLEAR !== undefined) {
        dict[messageKeys.ITEM_CLEAR] = 1;
      }
    }

    sendMessage(dict, function() {
      if (streamToken !== s_active_stream_token) return;
      index++;
      sendNext();
    }, function(err) {
      if (streamToken !== s_active_stream_token) return;
      console.log('Failed sending item ' + index + ', retrying once...');
      sendMessage(dict, function() {
        if (streamToken !== s_active_stream_token) return;
        index++;
        sendNext();
      }, function(e2) {
        console.log('Failed retry for item ' + index + ': ' + JSON.stringify(e2));
      });
    });
  }

  sendNext();
}

function fetchAndStreamFile(path) {
  var url = getServerUrl() + '/file?path=' + encodeURIComponent(path);
  console.log('Fetching note: ' + url);

  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, true);
  xhr.timeout = 15000;

  xhr.onload = function() {
    if (xhr.status === 200) {
      var content = xhr.responseText || '';
      streamNoteChunks(content);
    } else {
      console.log('HTTP error: ' + xhr.status);
      sendError('HTTP ' + xhr.status + ' on file');
    }
  };

  xhr.onerror = function() {
    console.log('Network error fetching file');
    sendError('Failed to read file');
  };

  xhr.ontimeout = function() {
    console.log('Timeout fetching file');
    sendError('File read timed out');
  };

  xhr.send();
}

function streamNoteChunks(text) {
  var chunks = [];
  if (!text || text.length === 0) {
    chunks.push('(Empty note)');
  } else {
    for (var i = 0; i < text.length; i += CHUNK_SIZE) {
      chunks.push(text.slice(i, i + CHUNK_SIZE));
    }
  }

  var chunkIndex = 0;

  function sendNextChunk() {
    if (chunkIndex >= chunks.length) {
      console.log('Finished streaming ' + chunks.length + ' chunks');
      return;
    }

    var chunk = chunks[chunkIndex];
    var isLast = (chunkIndex === chunks.length - 1) ? 1 : 0;
    var dict = makePayload({
      CHUNK_DATA: chunk,
      CHUNK_INDEX: chunkIndex,
      CHUNK_TOTAL: chunks.length,
      CHUNK_IS_LAST: isLast
    });

    sendMessage(dict, function() {
      chunkIndex++;
      sendNextChunk();
    }, function(err) {
      console.log('Chunk ' + chunkIndex + ' failed, retrying once...');
      sendMessage(dict, function() {
        chunkIndex++;
        sendNextChunk();
      }, function(e2) {
        console.log('Retry failed for chunk ' + chunkIndex + ': ' + JSON.stringify(e2));
      });
    });
  }

  sendNextChunk();
}

function handleTogglePin(path, name, isDir, currentDir) {
  if (!path) return;

  var pins = getPins();
  var existingIndex = -1;

  for (var i = 0; i < pins.length; i++) {
    if (pins[i].path === path) {
      existingIndex = i;
      break;
    }
  }

  if (existingIndex >= 0) {
    pins.splice(existingIndex, 1);
    console.log('Unpinned: ' + path);
  } else {
    pins.push({
      path: path,
      name: name || path,
      isDir: !!isDir,
      lastModified: Date.now()
    });
    console.log('Pinned: ' + path);
  }

  savePins(pins);

  var dirToRefresh = (currentDir !== undefined && currentDir !== null) ? currentDir : '';
  console.log('Refreshing directory after pin toggle: "' + dirToRefresh + '"');
  fetchAndStreamTree(dirToRefresh);
}

var _global = (typeof global !== 'undefined') ? global : (typeof window !== 'undefined' ? window : this);

if (!_global._pebblenote_listeners_registered) {
  _global._pebblenote_listeners_registered = true;

  Pebble.addEventListener('ready', function(e) {
    console.log('PebbleKit JS ready for PebbleNote');
    sendSettingsToWatch();
    fetchAndStreamTree('');
  });

  Pebble.addEventListener('appmessage', function(e) {
    var payload = e.payload;
    if (!payload) return;

    var command = getValue(payload, 'COMMAND');
    var path = getValue(payload, 'PATH') || '';
    var name = getValue(payload, 'NAME') || '';
    var isDir = getValue(payload, 'IS_DIR');
    var currentDir = getValue(payload, 'CURRENT_DIR');

    console.log('Received AppMessage: cmd=' + command + ', path=' + path + ', currentDir=' + currentDir);

    var newFontSize = getValue(payload, 'SETTINGS_FONT_SIZE');
    if (newFontSize !== undefined) {
      var s = getSettings();
      var sizeStr = 'default';
      var numCode = parseInt(newFontSize, 10);
      if (numCode === 0) sizeStr = 'small';
      else if (numCode === 2) sizeStr = 'large';
      else sizeStr = 'default';
      s.font_size = sizeStr;
      localStorage.setItem('pebble_note_settings', JSON.stringify(s));
      console.log('Font size updated and synced from watch: ' + sizeStr + ' (code ' + numCode + ')');
    }

    switch (command) {
      case CMD.GET_TREE:
        fetchAndStreamTree(path);
        break;

      case CMD.GET_FILE:
        fetchAndStreamFile(path);
        break;

      case CMD.TOGGLE_PIN:
        if (currentDir === undefined) {
          if (path && path.lastIndexOf('/') !== -1) {
            currentDir = path.substring(0, path.lastIndexOf('/'));
          } else {
            currentDir = '';
          }
        }
        handleTogglePin(path, name, isDir, currentDir);
        break;

      default:
        if (newFontSize === undefined) {
          console.log('Unknown command opcode: ' + command);
        }
        break;
    }
  });

  Pebble.addEventListener('showConfiguration', function(e) {
    var settings = getSettings();
    var port = settings.port || DEFAULT_PORT;
    var animateChecked = settings.animate_titles ? 'checked' : '';
    var hideExtChecked = settings.hide_extension ? 'checked' : '';
    var fontSize = settings.font_size || 'default';
    var order = settings.order || 'a-z';

    var html = '<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1">' +
      '<title>PebbleNote Settings</title>' +
      '<style>' +
      'body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; padding: 20px; background: #f4f4f7; color: #333; margin: 0; }' +
      '.card { background: white; border-radius: 10px; padding: 18px; margin-bottom: 16px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }' +
      'h2 { margin-top: 0; color: #ff4700; font-size: 20px; }' +
      'label { display: block; margin-top: 12px; font-weight: 600; font-size: 14px; }' +
      'input[type="text"], input[type="number"], select { width: 100%; box-sizing: border-box; padding: 10px; margin-top: 5px; border: 1px solid #ccc; border-radius: 6px; font-size: 15px; background: white; }' +
      'input:focus, select:focus { border-color: #ff4700; outline: none; }' +
      '.checkbox-row { display: flex; align-items: center; margin-top: 15px; }' +
      '.checkbox-row input { width: 20px; height: 20px; margin: 0 10px 0 0; accent-color: #ff4700; }' +
      '.checkbox-row label { margin-top: 0; font-weight: 500; font-size: 15px; cursor: pointer; }' +
      '.btn-primary { width: 100%; margin-top: 20px; padding: 13px; background: #ff4700; color: white; border: none; border-radius: 6px; font-size: 16px; font-weight: bold; cursor: pointer; }' +
      '.btn-danger { width: 100%; margin-top: 12px; padding: 13px; background: #ffffff; color: #d32f2f; border: 1px solid #d32f2f; border-radius: 6px; font-size: 15px; font-weight: 600; cursor: pointer; }' +
      '</style></head><body>' +
      '<div class="card">' +
      '<h2>Connection Settings</h2>' +
      '<label>Server Port:</label>' +
      '<input id="port" type="number" value="' + port + '" placeholder="8765">' +
      '</div>' +
      '<div class="card">' +
      '<h2>Display Settings</h2>' +
      '<label>Item Ordering:</label>' +
      '<select id="order">' +
      '<option value="a-z"' + (order === 'a-z' ? ' selected' : '') + '>A-Z</option>' +
      '<option value="z-a"' + (order === 'z-a' ? ' selected' : '') + '>Z-A</option>' +
      '<option value="last-modified"' + (order === 'last-modified' ? ' selected' : '') + '>Last Modified</option>' +
      '<option value="first-modified"' + (order === 'first-modified' ? ' selected' : '') + '>First Modified</option>' +
      '</select>' +
      '<label>Note Text Size:</label>' +
      '<select id="fontSize">' +
      '<option value="small"' + (fontSize === 'small' ? ' selected' : '') + '>Small</option>' +
      '<option value="default"' + (fontSize === 'default' ? ' selected' : '') + '>Default</option>' +
      '<option value="large"' + (fontSize === 'large' ? ' selected' : '') + '>Large</option>' +
      '</select>' +
      '<div class="checkbox-row">' +
      '<input id="animate" type="checkbox" ' + animateChecked + '>' +
      '<label for="animate">Animate Long Titles</label>' +
      '</div>' +
      '<div class="checkbox-row">' +
      '<input id="hideExtension" type="checkbox" ' + hideExtChecked + '>' +
      '<label for="hideExtension">Hide File Extension</label>' +
      '</div>' +
      '</div>' +
      '<button id="saveBtn" class="btn-primary">Save & Apply</button>' +
      '<button id="resetSettingsBtn" class="btn-danger">Reset Settings</button>' +
      '<button id="resetPinsBtn" class="btn-danger" style="margin-bottom: 24px;">Reset Note Pins</button>' +
      '<script>' +
      'document.getElementById("saveBtn").addEventListener("click", function() {' +
      '  var p = parseInt(document.getElementById("port").value, 10) || 8765;' +
      '  var anim = document.getElementById("animate").checked;' +
      '  var hideExt = document.getElementById("hideExtension").checked;' +
      '  var fSize = document.getElementById("fontSize").value || "default";' +
      '  var ord = document.getElementById("order").value || "a-z";' +
      '  var options = { port: p, animate_titles: anim, font_size: fSize, order: ord, hide_extension: hideExt };' +
      '  var returnUri = "pebblejs://close#" + encodeURIComponent(JSON.stringify(options));' +
      '  document.location = returnUri;' +
      '});' +
      'document.getElementById("resetSettingsBtn").addEventListener("click", function() {' +
      '  if (confirm("Reset all settings to default values? This will apply immediately.")) {' +
      '    var defaultOptions = { port: 8765, animate_titles: false, font_size: "default", order: "a-z", hide_extension: false };' +
      '    var returnUri = "pebblejs://close#" + encodeURIComponent(JSON.stringify(defaultOptions));' +
      '    document.location = returnUri;' +
      '  }' +
      '});' +
      'document.getElementById("resetPinsBtn").addEventListener("click", function() {' +
      '  if (confirm("Reset and forget all pinned notes? This will clear all shortcuts and start clean.")) {' +
      '    var p = parseInt(document.getElementById("port").value, 10) || 8765;' +
      '    var anim = document.getElementById("animate").checked;' +
      '    var hideExt = document.getElementById("hideExtension").checked;' +
      '    var fSize = document.getElementById("fontSize").value || "default";' +
      '    var ord = document.getElementById("order").value || "a-z";' +
      '    var options = { port: p, animate_titles: anim, font_size: fSize, order: ord, hide_extension: hideExt, reset_pins: true };' +
      '    var returnUri = "pebblejs://close#" + encodeURIComponent(JSON.stringify(options));' +
      '    document.location = returnUri;' +
      '  }' +
      '});' +
      '</script></body></html>';

    Pebble.openURL('data:text/html;charset=utf-8,' + encodeURIComponent(html));
  });

  Pebble.addEventListener('webviewclosed', function(e) {
    if (e && e.response) {
      try {
        var options = JSON.parse(decodeURIComponent(e.response));
        console.log('Settings returned from webview: ' + JSON.stringify(options));

        if (options.reset_pins) {
          console.log('Resetting all pinned notes to empty state');
          localStorage.removeItem('pebble_note_pins');
          savePins([]);
        }

        var s = {
          port: options.port || DEFAULT_PORT,
          animate_titles: !!options.animate_titles,
          font_size: options.font_size || 'default',
          order: options.order || 'a-z',
          hide_extension: !!options.hide_extension
        };
        localStorage.setItem('pebble_note_settings', JSON.stringify(s));

        sendSettingsToWatch();
        fetchAndStreamTree(s_last_tree_fetch_path || '');
      } catch (err) {
        console.log('Error parsing configuration response: ' + err);
      }
    }
  });
}
