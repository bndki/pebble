var FALLBACK_API_KEY = '';

var MessageKeys;
try {
  MessageKeys = require('message_keys');
} catch (e) {
  MessageKeys = null;
}

var FALLBACK_KEYS = {
  KEY_ACTION: 0,
  KEY_SESSION_ID: 1,
  KEY_PROMPT: 2,
  KEY_RESPONSE_CHUNK: 3,
  KEY_CHUNK_INDEX: 4,
  KEY_CHUNK_TOTAL: 5,
  KEY_SESSION_COUNT: 6,
  KEY_SESSION_TITLE: 7,
  KEY_ERROR: 8,
  KEY_TURN_INDEX: 9,
  KEY_TOTAL_TURNS: 10,
  KEY_ROLE: 11,
  KEY_SESSION_INDEX: 12,
  KEY_FONT_SIZE: 13,
  KEY_ANIMATE_TITLES: 14,
  KEY_LAUNCH_VOICE_DICTATION: 15
};

var ACTIONS = {
  ACTION_FETCH_SESSIONS: 1,
  ACTION_SEND_PROMPT: 2,
  ACTION_GET_TURN: 3,
  ACTION_LOAD_SESSION: 4,
  ACTION_DELETE_SESSION: 5,
  ACTION_DELETE_TURNS: 6,
  ACTION_SESSION_LIST: 10,
  ACTION_RESPONSE_CHUNK: 11,
  ACTION_ERROR: 12,
  ACTION_SETTINGS_UPDATE: 13
};

var SETTINGS_KEY = 'pebblechat_settings_v1';
var SESSIONS_KEY = 'pebblechat_sessions_v1';

var DEFAULT_SYSTEM_PROMPT = 'You are a helpful assistant. You must respond in plain text without markdowns. You must respond in short and concise manner.';

var DEFAULT_SETTINGS = {
  apiKey: '',
  model: '',
  systemPrompt: DEFAULT_SYSTEM_PROMPT,
  reasoning: 'none',
  maxTokens: 1024,
  fontSize: 24,
  onlineAccess: false,
  includeTime: false,
  includeLocation: false,
  animateTitles: false,
  launchVoiceDictation: false
};

function getField(dict, keyName, fallbackInt) {
  if (!dict) return undefined;

  if (dict[keyName] !== undefined) return dict[keyName];

  if (dict[keyName.toLowerCase()] !== undefined) return dict[keyName.toLowerCase()];

  if (MessageKeys && MessageKeys[keyName] !== undefined && dict[MessageKeys[keyName]] !== undefined) {
    return dict[MessageKeys[keyName]];
  }

  if (typeof Pebble !== 'undefined' && Pebble.keys && Pebble.keys[keyName] !== undefined && dict[Pebble.keys[keyName]] !== undefined) {
    return dict[Pebble.keys[keyName]];
  }

  if (fallbackInt !== undefined) {
    if (dict[fallbackInt] !== undefined) return dict[fallbackInt];
    if (dict[String(fallbackInt)] !== undefined) return dict[String(fallbackInt)];
  }

  return undefined;
}

function setField(dict, keyName, fallbackInt, value) {
  dict[keyName] = value;

  if (fallbackInt !== undefined) {
    dict[fallbackInt] = value;
  }

  if (MessageKeys && MessageKeys[keyName] !== undefined) {
    dict[MessageKeys[keyName]] = value;
  }

  if (typeof Pebble !== 'undefined' && Pebble.keys && Pebble.keys[keyName] !== undefined) {
    dict[Pebble.keys[keyName]] = value;
  }
}

function getSettings() {
  try {
    var raw = localStorage.getItem(SETTINGS_KEY);
    if (raw) {
      var parsed = JSON.parse(raw);
      return {
        apiKey: (parsed.apiKey !== undefined && parsed.apiKey !== null) ? parsed.apiKey : (FALLBACK_API_KEY || DEFAULT_SETTINGS.apiKey),
        model: (parsed.model !== undefined && parsed.model !== null) ? parsed.model : DEFAULT_SETTINGS.model,
        systemPrompt: (parsed.systemPrompt !== undefined && parsed.systemPrompt !== null && parsed.systemPrompt.trim() !== '') ? parsed.systemPrompt : DEFAULT_SETTINGS.systemPrompt,
        reasoning: parsed.reasoning || DEFAULT_SETTINGS.reasoning,
        maxTokens: parseInt(parsed.maxTokens, 10) || DEFAULT_SETTINGS.maxTokens,
        fontSize: parseInt(parsed.fontSize, 10) || DEFAULT_SETTINGS.fontSize,
        onlineAccess: !!parsed.onlineAccess,
        includeTime: !!parsed.includeTime,
        includeLocation: !!parsed.includeLocation,
        animateTitles: (parsed.animateTitles !== undefined) ? !!parsed.animateTitles : DEFAULT_SETTINGS.animateTitles,
        launchVoiceDictation: (parsed.launchVoiceDictation !== undefined) ? !!parsed.launchVoiceDictation : DEFAULT_SETTINGS.launchVoiceDictation
      };
    }
  } catch (e) {
    console.log('[PHONE] Error reading settings: ' + e);
  }
  return {
    apiKey: FALLBACK_API_KEY || DEFAULT_SETTINGS.apiKey,
    model: DEFAULT_SETTINGS.model,
    systemPrompt: DEFAULT_SYSTEM_PROMPT,
    reasoning: DEFAULT_SETTINGS.reasoning,
    maxTokens: DEFAULT_SETTINGS.maxTokens,
    fontSize: DEFAULT_SETTINGS.fontSize,
    onlineAccess: DEFAULT_SETTINGS.onlineAccess,
    includeTime: DEFAULT_SETTINGS.includeTime,
    includeLocation: DEFAULT_SETTINGS.includeLocation,
    animateTitles: DEFAULT_SETTINGS.animateTitles,
    launchVoiceDictation: DEFAULT_SETTINGS.launchVoiceDictation
  };
}

function sendSettingsToWatch(settings) {
  if (!settings) settings = getSettings();
  var dict = {};
  setField(dict, 'KEY_ACTION', FALLBACK_KEYS.KEY_ACTION, ACTIONS.ACTION_SETTINGS_UPDATE);
  setField(dict, 'KEY_FONT_SIZE', FALLBACK_KEYS.KEY_FONT_SIZE, settings.fontSize || 24);
  setField(dict, 'KEY_ANIMATE_TITLES', FALLBACK_KEYS.KEY_ANIMATE_TITLES, settings.animateTitles ? 1 : 0);
  setField(dict, 'KEY_LAUNCH_VOICE_DICTATION', FALLBACK_KEYS.KEY_LAUNCH_VOICE_DICTATION, settings.launchVoiceDictation ? 1 : 0);
  console.log('[PHONE] Syncing settings to watch: fontSize=' + (settings.fontSize || 24) + ', animateTitles=' + (!!settings.animateTitles) + ', launchVoiceDictation=' + (!!settings.launchVoiceDictation));
  Pebble.sendAppMessage(dict);
}

function saveSettings(settings) {
  try {
    localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings));
    console.log('[PHONE] Settings successfully saved');
    sendSettingsToWatch(settings);
  } catch (e) {
    console.log('[PHONE] Error saving settings: ' + e);
  }
}

function getSessions() {
  try {
    var raw = localStorage.getItem(SESSIONS_KEY);
    if (raw) {
      return JSON.parse(raw);
    }
  } catch (e) {
    console.log('[PHONE] Error reading sessions: ' + e);
  }
  return [];
}

function saveSessions(sessions) {
  try {
    if (sessions.length > 5) {
      sessions = sessions.slice(0, 5);
    }
    localStorage.setItem(SESSIONS_KEY, JSON.stringify(sessions));
  } catch (e) {
    console.log('[PHONE] Error saving sessions: ' + e);
  }
}

function sendErrorToWatch(errorMsg) {
  console.log('[PHONE] Dispatching error to watch: ' + errorMsg);
  var dict = {};
  setField(dict, 'KEY_ACTION', FALLBACK_KEYS.KEY_ACTION, ACTIONS.ACTION_ERROR);
  setField(dict, 'KEY_ERROR', FALLBACK_KEYS.KEY_ERROR, String(errorMsg || 'Unknown error'));

  Pebble.sendAppMessage(dict, function() {
    console.log('[PHONE] Error successfully delivered to watch');
  }, function(e) {
    console.log('[PHONE] Failed to deliver error to watch: ' + JSON.stringify(e));
  });
}

function sendChunks(sessionId, turnIndex, totalTurns, role, text) {
  var CHUNK_SIZE = 380;
  var fullText = text || '';
  var chunks = [];
  if (!fullText || fullText.length === 0) {
    chunks.push('(Empty response)');
  } else {
    for (var i = 0; i < fullText.length; i += CHUNK_SIZE) {
      chunks.push(fullText.substring(i, i + CHUNK_SIZE));
    }
  }

  console.log('[PHONE] Preparing to send ' + chunks.length + ' chunks (total length: ' + fullText.length + ')');

  var chunkIndex = 0;
  function sendNextChunk() {
    if (chunkIndex >= chunks.length) {
      console.log('[PHONE] Finished transmitting all ' + chunks.length + ' chunks.');
      return;
    }

    var dict = {};
    setField(dict, 'KEY_ACTION', FALLBACK_KEYS.KEY_ACTION, ACTIONS.ACTION_RESPONSE_CHUNK);
    setField(dict, 'KEY_SESSION_ID', FALLBACK_KEYS.KEY_SESSION_ID, sessionId);
    setField(dict, 'KEY_TURN_INDEX', FALLBACK_KEYS.KEY_TURN_INDEX, turnIndex);
    setField(dict, 'KEY_TOTAL_TURNS', FALLBACK_KEYS.KEY_TOTAL_TURNS, totalTurns);
    setField(dict, 'KEY_ROLE', FALLBACK_KEYS.KEY_ROLE, role);
    setField(dict, 'KEY_CHUNK_INDEX', FALLBACK_KEYS.KEY_CHUNK_INDEX, chunkIndex);
    setField(dict, 'KEY_CHUNK_TOTAL', FALLBACK_KEYS.KEY_CHUNK_TOTAL, chunks.length);
    setField(dict, 'KEY_RESPONSE_CHUNK', FALLBACK_KEYS.KEY_RESPONSE_CHUNK, chunks[chunkIndex]);

    console.log('[PHONE] Sending chunk ' + (chunkIndex + 1) + '/' + chunks.length + ' (' + chunks[chunkIndex].length + ' bytes)');

    Pebble.sendAppMessage(dict, function() {
      chunkIndex++;
      sendNextChunk();
    }, function(err) {
      console.log('[PHONE] Failed to send chunk ' + chunkIndex + ', retrying once: ' + JSON.stringify(err));
      setTimeout(function() {
        Pebble.sendAppMessage(dict, function() {
          chunkIndex++;
          sendNextChunk();
        }, function(fatalErr) {
          console.log('[PHONE] Fatal error sending chunk ' + chunkIndex + ': ' + JSON.stringify(fatalErr));
          sendErrorToWatch('Failed transmitting response chunks to watch.');
        });
      }, 300);
    });
  }

  sendNextChunk();
}

function generateTitle(userPrompt, callback) {
  var settings = getSettings();
  if (!settings.apiKey) {
    if (callback) callback(null);
    return;
  }

  var modelName = settings.model || DEFAULT_SETTINGS.model || 'google/gemini-3.5-flash-lite';
  console.log('[PHONE] Generating title using model: ' + modelName);

  var xhr = new XMLHttpRequest();
  xhr.open('POST', 'https://openrouter.ai/api/v1/chat/completions', true);
  xhr.timeout = 30000;
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.setRequestHeader('Authorization', 'Bearer ' + settings.apiKey);
  xhr.setRequestHeader('HTTP-Referer', 'https://rebble.io');
  xhr.setRequestHeader('X-Title', 'PebbleChat');

  var body = {
    model: modelName,
    messages: [
      {
        role: 'system',
        content: 'Generate short title. Must respond with exactly 4 words summarizing the user query. Do not include quotes, punctuation, or conversational filler.'
      },
      {
        role: 'user',
        content: userPrompt.substring(0, 120)
      }
    ],
    max_tokens: 512,
    max_completion_tokens: 512,
    temperature: 1.0,
  };

  var completed = false;
  function finish(newTitle) {
    if (!completed) {
      completed = true;
      if (callback) callback(newTitle);
    }
  }

  xhr.onload = function() {
    console.log('[PHONE] generateTitle HTTP status: ' + xhr.status);
    if (xhr.status === 200) {
      try {
        var res = JSON.parse(xhr.responseText);
        var raw = res.choices && res.choices[0] && res.choices[0].message && res.choices[0].message.content;
        console.log('[PHONE] generateTitle response: ' + raw);
        if (raw) {
          var clean = raw.replace(/^(Title|Summary|Here is a 4-word title|Here is a title|Here's a title)\s*:\s*/i, '');
          clean = clean.replace(/[\*\"\'\`\#\.\,\:\;\!\?\(\)\[\]\n\r]/g, ' ').trim();
          var words = clean.split(/\s+/).filter(function(w) { return w.length > 0; }).slice(0, 4).join(' ');
          if (words && words.length > 0) {
            finish(words);
            return;
          }
        }
      } catch (e) {
        console.log('[PHONE] Error parsing title: ' + e);
      }
    } else {
      console.log('[PHONE] generateTitle failed: HTTP ' + xhr.status + ' - ' + xhr.responseText);
    }
    finish(null);
  };

  xhr.onerror = function() {
    console.log('[PHONE] generateTitle network error');
    finish(null);
  };

  xhr.ontimeout = function() {
    console.log('[PHONE] generateTitle timed out (30s)');
    finish(null);
  };

  xhr.send(JSON.stringify(body));
}

function sendSessionListToWatch() {
  var sessions = getSessions();
  var count = Math.min(sessions.length, 5);
  console.log('[PHONE] Syncing session list to watch (count=' + count + ')');

  if (count === 0) {
    var dict = {};
    setField(dict, 'KEY_ACTION', FALLBACK_KEYS.KEY_ACTION, ACTIONS.ACTION_SESSION_LIST);
    setField(dict, 'KEY_SESSION_COUNT', FALLBACK_KEYS.KEY_SESSION_COUNT, 0);
    Pebble.sendAppMessage(dict);
    return;
  }

  var index = 0;
  function sendNextSessionItem() {
    if (index >= count) {
      return;
    }
    var dict = {};
    setField(dict, 'KEY_ACTION', FALLBACK_KEYS.KEY_ACTION, ACTIONS.ACTION_SESSION_LIST);
    setField(dict, 'KEY_SESSION_COUNT', FALLBACK_KEYS.KEY_SESSION_COUNT, count);
    setField(dict, 'KEY_SESSION_INDEX', FALLBACK_KEYS.KEY_SESSION_INDEX, index);
    setField(dict, 'KEY_SESSION_ID', FALLBACK_KEYS.KEY_SESSION_ID, sessions[index].id);
    setField(dict, 'KEY_SESSION_TITLE', FALLBACK_KEYS.KEY_SESSION_TITLE, (sessions[index].title || 'Chat ' + (index + 1)).substring(0, 63));

    Pebble.sendAppMessage(dict, function() {
      setTimeout(function() {
        index++;
        sendNextSessionItem();
      }, 50);
    }, function(err) {
      console.log('[PHONE] Failed to send session item ' + index + ', retrying once...');
      setTimeout(function() {
        Pebble.sendAppMessage(dict, function() {
          index++;
          sendNextSessionItem();
        }, function(e2) {
          console.log('[PHONE] Retry failed for session item ' + index);
          index++;
          sendNextSessionItem();
        });
      }, 150);
    });
  }

  sendNextSessionItem();
}

function handleSendPrompt(sessionId, promptText) {
  var settings = getSettings();
  console.log('[PHONE] handleSendPrompt - API Key present? ' + (!!settings.apiKey) + ', Model: ' + (settings.model || DEFAULT_SETTINGS.model || 'google/gemini-3.5-flash-lite'));

  if (!settings.apiKey || settings.apiKey.trim().length === 0) {
    console.log('[PHONE] ERROR: OpenRouter API key is missing!');
    sendErrorToWatch('Missing API Key!\n\nConfigure in watchapp settings.');
    return;
  }

  var sessions = getSessions();
  var isNewSession = (sessionId === 0 || !sessionId);
  var activeSessionId = isNewSession ? Math.floor(Date.now() / 1000) : sessionId;
  var sessionTitle = (promptText.length > 50) ? (promptText.substring(0, 50) + '...') : promptText;
  var previousMessages = [];

  if (!isNewSession) {
    for (var i = 0; i < sessions.length; i++) {
      if (sessions[i].id === activeSessionId) {
        previousMessages = sessions[i].messages ? sessions[i].messages.slice() : [];
        if (sessions[i].title) {
          sessionTitle = sessions[i].title;
        }
        break;
      }
    }
  }

  function executeApiCall(locContext) {
    var modelName = settings.model || DEFAULT_SETTINGS.model || 'google/gemini-3.5-flash-lite';
    if (settings.onlineAccess && modelName.indexOf(':online') === -1) {
      modelName = modelName + ':online';
    }

    var apiMessages = [];

    var sysParts = [];
    if (settings.systemPrompt && settings.systemPrompt.trim().length > 0) {
      sysParts.push(settings.systemPrompt.trim());
    }
    if (settings.includeTime) {
      var now = new Date();
      sysParts.push('[Context: Local Time is ' + now.toLocaleString() + ']');
    }
    if (locContext) {
      sysParts.push('[Context: User Coordinates ' + locContext + ']');
    }

    if (sysParts.length > 0) {
      apiMessages.push({
        role: 'system',
        content: sysParts.join('\n\n')
      });
    }

    for (var m = 0; m < previousMessages.length; m++) {
      apiMessages.push({
        role: previousMessages[m].role,
        content: previousMessages[m].content
      });
    }

    apiMessages.push({
      role: 'user',
      content: promptText
    });

    var tokenBudget = settings.maxTokens || 1024;
    var requestPayload = {
      model: modelName,
      messages: apiMessages,
      max_tokens: tokenBudget,
      max_completion_tokens: tokenBudget,
      temperature: 1.0
    };

    if (settings.reasoning) {
      requestPayload.reasoning = {
        effort: settings.reasoning
      };
    }

    console.log('[PHONE] Querying OpenRouter (' + modelName + ', tokens: ' + tokenBudget + ')...');

    var xhr = new XMLHttpRequest();
    xhr.open('POST', 'https://openrouter.ai/api/v1/chat/completions', true);
    xhr.timeout = 30000;
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.setRequestHeader('Authorization', 'Bearer ' + settings.apiKey);
    xhr.setRequestHeader('HTTP-Referer', 'https://rebble.io');
    xhr.setRequestHeader('X-Title', 'PebbleChat');

    xhr.ontimeout = function() {
      console.log('[PHONE] OpenRouter request timed out after 30 seconds');
      sendErrorToWatch('OpenRouter timed out (30s).\nCheck connection or try a faster model.');
    };

    xhr.onerror = function() {
      console.log('[PHONE] Network connection error calling OpenRouter');
      sendErrorToWatch('Network error.\nPlease check phone internet connection.');
    };

    xhr.onload = function() {
      console.log('[PHONE] OpenRouter HTTP status: ' + xhr.status);
      if (xhr.status === 200) {
        try {
          var res = JSON.parse(xhr.responseText);
          var reply = '';
          var finishReason = '';
          if (res.choices && res.choices[0]) {
            finishReason = res.choices[0].finish_reason || '';
            if (res.choices[0].message) {
              reply = res.choices[0].message.content || '';
            }
          }
          if (!reply) {
            reply = '(No response content received)';
          }

          var usageInfo = '';
          if (res.usage) {
            usageInfo = ' [tokens: ' + (res.usage.completion_tokens || '?') + ' completion, ' + (res.usage.total_tokens || '?') + ' total]';
          }
          console.log('[PHONE] Received reply (' + reply.length + ' chars, finish_reason: ' + (finishReason || 'none') + ')' + usageInfo);

          if (finishReason === 'length') {
            console.log('[PHONE] Warning: Response was truncated by token limit (finish_reason: length)');
            reply += '\n\n... [max tokens reached]';
          }

          var currentSessions = getSessions();
          var targetSession = null;

          if (isNewSession) {
            targetSession = {
              id: activeSessionId,
              title: sessionTitle,
              messages: []
            };
            currentSessions.unshift(targetSession);
          } else {
            for (var s = 0; s < currentSessions.length; s++) {
              if (currentSessions[s].id === activeSessionId) {
                targetSession = currentSessions[s];
                break;
              }
            }
            if (!targetSession) {
              targetSession = {
                id: activeSessionId,
                title: sessionTitle,
                messages: []
              };
              currentSessions.unshift(targetSession);
            }
          }

          targetSession.messages.push({
            role: 'user',
            content: promptText
          });
          targetSession.messages.push({
            role: 'assistant',
            content: reply
          });
          if (sessionTitle && targetSession.title !== sessionTitle) {
            targetSession.title = sessionTitle;
          }

          if (currentSessions.length > 5) {
            currentSessions = currentSessions.slice(0, 5);
          }
          saveSessions(currentSessions);

          sendSessionListToWatch();

          var turnIndex = targetSession.messages.length - 1;
          var totalTurns = targetSession.messages.length;

          sendChunks(activeSessionId, turnIndex, totalTurns, 1, reply);
        } catch (err) {
          console.log('[PHONE] JSON parse error: ' + err);
          sendErrorToWatch('Failed to parse API response: ' + err);
        }
      } else {
        var errDetail = 'HTTP ' + xhr.status;
        try {
          var errObj = JSON.parse(xhr.responseText);
          if (errObj.error && errObj.error.message) {
            errDetail += ': ' + errObj.error.message;
          }
        } catch (e) {}
        console.log('[PHONE] OpenRouter API error: ' + errDetail);
        sendErrorToWatch(errDetail);
      }
    };

    xhr.send(JSON.stringify(requestPayload));
  }

  function startChatFlow() {
    if (settings.includeLocation && navigator.geolocation) {
      navigator.geolocation.getCurrentPosition(function(pos) {
        var loc = 'Lat ' + pos.coords.latitude.toFixed(3) + ', Lon ' + pos.coords.longitude.toFixed(3);
        executeApiCall(loc);
      }, function(err) {
        console.log('[PHONE] Location lookup skipped: ' + err.message);
        executeApiCall(null);
      }, {
        timeout: 4000,
        maximumAge: 600000
      });
    } else {
      executeApiCall(null);
    }
  }

  if (isNewSession) {
    console.log('[PHONE] New session detected: generating title in background');
    generateTitle(promptText, function(newTitle) {
      if (newTitle) {
        sessionTitle = newTitle;
      }
      startChatFlow();
    });
  } else {
    startChatFlow();
  }
}

function handleGetTurn(sessionId, turnIndex) {
  var sessions = getSessions();
  var session = null;
  for (var i = 0; i < sessions.length; i++) {
    if (sessions[i].id === sessionId) {
      session = sessions[i];
      break;
    }
  }

  if (!session || !session.messages || session.messages.length === 0) {
    sendErrorToWatch('Session history not found.');
    return;
  }

  if (turnIndex < 0) turnIndex = 0;
  if (turnIndex >= session.messages.length) turnIndex = session.messages.length - 1;

  var msg = session.messages[turnIndex];
  var role = (msg.role === 'assistant') ? 1 : 0;
  sendChunks(sessionId, turnIndex, session.messages.length, role, msg.content);
}

function handleLoadSession(sessionId) {
  var sessions = getSessions();
  var session = null;
  for (var i = 0; i < sessions.length; i++) {
    if (sessions[i].id === sessionId) {
      session = sessions[i];
      break;
    }
  }

  if (!session || !session.messages || session.messages.length === 0) {
    sendErrorToWatch('Empty session history.');
    return;
  }

  var latestIndex = session.messages.length - 1;
  var msg = session.messages[latestIndex];
  var role = (msg.role === 'assistant') ? 1 : 0;
  sendChunks(sessionId, latestIndex, session.messages.length, role, msg.content);
}

function handleDeleteSession(sessionId) {
  console.log('[PHONE] Deleting session: ' + sessionId);
  var sessions = getSessions();
  var filtered = [];
  for (var i = 0; i < sessions.length; i++) {
    if (sessions[i].id !== sessionId) {
      filtered.push(sessions[i]);
    }
  }
  saveSessions(filtered);
  sendSessionListToWatch();
}

function handleDeleteTurns(sessionId, turnIndex) {
  console.log('[PHONE] Deleting turns from index ' + turnIndex + ' in session ' + sessionId);
  var sessions = getSessions();
  var session = null;
  for (var i = 0; i < sessions.length; i++) {
    if (sessions[i].id === sessionId) {
      session = sessions[i];
      break;
    }
  }

  if (!session || !session.messages) {
    sendErrorToWatch('Session history not found.');
    return;
  }

  var exchangeNum = Math.floor(turnIndex / 2) + 1;
  if (exchangeNum <= 1) {
    handleDeleteSession(sessionId);
    return;
  }

  var keepCount = (exchangeNum - 1) * 2;
  if (keepCount <= 0) {
    handleDeleteSession(sessionId);
    return;
  }

  session.messages = session.messages.slice(0, keepCount);
  saveSessions(sessions);

  var latestIndex = session.messages.length - 1;
  var msg = session.messages[latestIndex];
  var role = (msg.role === 'assistant') ? 1 : 0;
  sendChunks(sessionId, latestIndex, session.messages.length, role, msg.content);
}

Pebble.addEventListener('appmessage', function(e) {
  var dict = e.payload;
  console.log('[PHONE] Received AppMessage payload: ' + JSON.stringify(dict));

  var action = getField(dict, 'KEY_ACTION', FALLBACK_KEYS.KEY_ACTION);
  console.log('[PHONE] Resolved Action: ' + action);

  if (action === ACTIONS.ACTION_FETCH_SESSIONS) {
    sendSessionListToWatch();
  } else if (action === ACTIONS.ACTION_SEND_PROMPT) {
    var sessionId = getField(dict, 'KEY_SESSION_ID', FALLBACK_KEYS.KEY_SESSION_ID) || 0;
    var prompt = getField(dict, 'KEY_PROMPT', FALLBACK_KEYS.KEY_PROMPT) || '';
    console.log('[PHONE] Action SEND_PROMPT: session=' + sessionId + ', prompt=' + prompt);
    handleSendPrompt(sessionId, prompt);
  } else if (action === ACTIONS.ACTION_GET_TURN) {
    var sId = getField(dict, 'KEY_SESSION_ID', FALLBACK_KEYS.KEY_SESSION_ID) || 0;
    var tIdx = getField(dict, 'KEY_TURN_INDEX', FALLBACK_KEYS.KEY_TURN_INDEX) || 0;
    handleGetTurn(sId, tIdx);
  } else if (action === ACTIONS.ACTION_LOAD_SESSION) {
    var targetSessionId = getField(dict, 'KEY_SESSION_ID', FALLBACK_KEYS.KEY_SESSION_ID) || 0;
    handleLoadSession(targetSessionId);
  } else if (action === ACTIONS.ACTION_DELETE_SESSION) {
    var delSessionId = getField(dict, 'KEY_SESSION_ID', FALLBACK_KEYS.KEY_SESSION_ID) || 0;
    handleDeleteSession(delSessionId);
  } else if (action === ACTIONS.ACTION_DELETE_TURNS) {
    var targetSId = getField(dict, 'KEY_SESSION_ID', FALLBACK_KEYS.KEY_SESSION_ID) || 0;
    var turnIdx = getField(dict, 'KEY_TURN_INDEX', FALLBACK_KEYS.KEY_TURN_INDEX) || 0;
    handleDeleteTurns(targetSId, turnIdx);
  } else {
    console.log('[PHONE] Unhandled action code: ' + action);
  }
});

function buildConfigPageHtml(settings) {
  var currentFontSize = parseInt(settings.fontSize, 10) || 24;
  var selectedSize = 24;
  if (currentFontSize <= 20) {
    selectedSize = 18;
  } else if (currentFontSize <= 26) {
    selectedSize = 24;
  } else {
    selectedSize = 28;
  }

  return '<!DOCTYPE html>\n' +
    '<html>\n' +
    '<head>\n' +
    '  <meta charset="utf-8">\n' +
    '  <meta name="viewport" content="width=device-width, initial-scale=1">\n' +
    '  <title>PebbleChat Settings</title>\n' +
    '  <style>\n' +
    '    * { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }\n' +
    '    body { background-color: #f2f2f7; color: #1c1c1e; margin: 0; padding: 16px; }\n' +
    '    h1 { font-size: 22px; font-weight: 700; margin: 0 0 16px 0; color: #ff4700; }\n' +
    '    .card { background: #ffffff; border-radius: 12px; padding: 16px; margin-bottom: 16px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }\n' +
    '    .field { margin-bottom: 14px; }\n' +
    '    .field:last-child { margin-bottom: 0; }\n' +
    '    label { display: block; font-size: 13px; font-weight: 600; text-transform: uppercase; color: #6e6e73; margin-bottom: 6px; }\n' +
    '    input[type="text"], input[type="password"], input[type="number"], select, textarea {\n' +
    '      width: 100%; padding: 10px 12px; font-size: 15px; border: 1px solid #d1d1d6; border-radius: 8px; background: #fff; color: #000;\n' +
    '    }\n' +
    '    textarea { resize: vertical; min-height: 80px; }\n' +
    '    .checkbox-row { display: flex; align-items: center; justify-content: space-between; padding: 10px 0; border-bottom: 1px solid #e5e5ea; }\n' +
    '    .checkbox-row:last-child { border-bottom: none; }\n' +
    '    .checkbox-label { font-size: 15px; font-weight: 500; color: #1c1c1e; }\n' +
    '    .checkbox-desc { font-size: 12px; color: #8e8e93; display: block; margin-top: 2px; }\n' +
    '    input[type="checkbox"] { width: 22px; height: 22px; accent-color: #ff4700; }\n' +
    '    .btn-save { width: 100%; padding: 14px; background-color: #ff4700; color: #ffffff; border: none; border-radius: 10px; font-size: 16px; font-weight: 600; cursor: pointer; margin-top: 10px; }\n' +
    '    .btn-save:active { opacity: 0.8; }\n' +
    '    .btn-reset { width: 100%; padding: 13px; background-color: #ffffff; color: #ff4700; border: 1.5px solid #ff4700; border-radius: 10px; font-size: 15px; font-weight: 600; cursor: pointer; margin-top: 10px; }\n' +
    '    .btn-reset:active { background-color: #fff0eb; }\n' +
    '    .btn-clear { width: 100%; padding: 13px; background-color: #ffffff; color: #ff3b30; border: 1.5px solid #ff3b30; border-radius: 10px; font-size: 15px; font-weight: 600; cursor: pointer; margin-top: 10px; }\n' +
    '    .btn-clear:active { background-color: #ffe5e5; }\n' +
    '    .hint { font-size: 12px; color: #8e8e93; margin-top: 4px; }\n' +
    '  </style>\n' +
    '</head>\n' +
    '<body>\n' +
    '  <h1>PebbleChat Settings</h1>\n' +
    '  <form id="settingsForm">\n' +
    '    <div class="card">\n' +
    '      <div class="field">\n' +
    '        <label for="apiKey">OpenRouter API Key</label>\n' +
    '        <input type="password" id="apiKey" placeholder="sk-or-v1-..." value="' + (settings.apiKey || '') + '">\n' +
    '        <div class="hint">Obtain from <a href="https://openrouter.ai/keys" target="_blank">openrouter.ai/keys</a></div>\n' +
    '      </div>\n' +
    '      <div class="field">\n' +
    '        <label for="model">Custom Model</label>\n' +
    '        <input type="text" id="model" placeholder="google/gemini-3.5-flash-lite" value="' + (settings.model || '') + '">\n' +
    '      </div>\n' +
    '      <div class="field">\n' +
    '        <label for="systemPrompt">Custom System Instructions</label>\n' +
    '        <textarea id="systemPrompt" placeholder="' + DEFAULT_SYSTEM_PROMPT + '">' + (settings.systemPrompt || DEFAULT_SYSTEM_PROMPT) + '</textarea>\n' +
    '      </div>\n' +
    '      <div class="field">\n' +
    '        <label for="reasoning">Reasoning Effort</label>\n' +
    '        <select id="reasoning">\n' +
    '          <option value="none"' + (settings.reasoning === 'none' ? ' selected' : '') + '>None</option>\n' +
    '          <option value="low"' + (settings.reasoning === 'low' ? ' selected' : '') + '>Low</option>\n' +
    '          <option value="medium"' + (settings.reasoning === 'medium' ? ' selected' : '') + '>Medium</option>\n' +
    '          <option value="high"' + (settings.reasoning === 'high' ? ' selected' : '') + '>High</option>\n' +
    '        </select>\n' +
    '      </div>\n' +
    '      <div class="field">\n' +
    '        <label for="maxTokens">Max Output Tokens</label>\n' +
    '        <input type="number" id="maxTokens" min="64" max="4096" value="' + (settings.maxTokens || 1024) + '">\n' +
    '      </div>\n' +
    '      <div class="field">\n' +
    '        <label for="fontSize">Font Size</label>\n' +
    '        <select id="fontSize">\n' +
    '          <option value="18"' + (selectedSize === 18 ? ' selected' : '') + '>Small</option>\n' +
    '          <option value="24"' + (selectedSize === 24 ? ' selected' : '') + '>Default</option>\n' +
    '          <option value="28"' + (selectedSize === 28 ? ' selected' : '') + '>Large</option>\n' +
    '        </select>\n' +
    '      </div>\n' +
    '    </div>\n' +
    '\n' +
    '    <div class="card">\n' +
    '      <div class="checkbox-row">\n' +
    '        <div>\n' +
    '          <span class="checkbox-label">Online Access</span>\n' +
    '          <span class="checkbox-desc">Allow model to perform web search</span>\n' +
    '        </div>\n' +
    '        <input type="checkbox" id="onlineAccess"' + (settings.onlineAccess ? ' checked' : '') + '>\n' +
    '      </div>\n' +
    '      <div class="checkbox-row">\n' +
    '        <div>\n' +
    '          <span class="checkbox-label">Include Time Context</span>\n' +
    '          <span class="checkbox-desc">Adds current local time and date to instructions</span>\n' +
    '        </div>\n' +
    '        <input type="checkbox" id="includeTime"' + (settings.includeTime ? ' checked' : '') + '>\n' +
    '      </div>\n' +
    '      <div class="checkbox-row">\n' +
    '        <div>\n' +
    '          <span class="checkbox-label">Include Location Context</span>\n' +
    '          <span class="checkbox-desc">Adds GPS coordinates to instructions</span>\n' +
    '        </div>\n' +
    '        <input type="checkbox" id="includeLocation"' + (settings.includeLocation ? ' checked' : '') + '>\n' +
    '      </div>\n' +
    '      <div class="checkbox-row">\n' +
    '        <div>\n' +
    '          <span class="checkbox-label">Animate Long Titles</span>\n' +
    '          <span class="checkbox-desc">Scrolls title text back and forth in recent chats</span>\n' +
    '        </div>\n' +
    '        <input type="checkbox" id="animateTitles"' + (settings.animateTitles ? ' checked' : '') + '>\n' +
    '      </div>\n' +
    '      <div class="checkbox-row">\n' +
    '        <div>\n' +
    '          <span class="checkbox-label">App Launch Voice Dictation</span>\n' +
    '          <span class="checkbox-desc">Starts dictation upon opening the app</span>\n' +
    '        </div>\n' +
    '        <input type="checkbox" id="launchVoiceDictation"' + (settings.launchVoiceDictation ? ' checked' : '') + '>\n' +
    '      </div>\n' +
    '    </div>\n' +
    '\n' +
    '    <button type="button" class="btn-save" id="btnSave">Save Settings</button>\n' +
    '    <button type="button" class="btn-reset" id="btnReset">Reset to Default</button>\n' +
    '    <button type="button" class="btn-clear" id="btnClear">Clear Chat Sessions</button>\n' +
    '  </form>\n' +
    '  <script>\n' +
    '    function getQueryParam(variable, defaultValue) {\n' +
    '      var query = location.search.substring(1);\n' +
    '      var vars = query.split("&");\n' +
    '      for (var i = 0; i < vars.length; i++) {\n' +
    '        var pair = vars[i].split("=");\n' +
    '        if (pair[0] === variable) {\n' +
    '          return decodeURIComponent(pair[1]);\n' +
    '        }\n' +
    '      }\n' +
    '      return defaultValue;\n' +
    '    }\n' +
    '    document.getElementById("btnSave").addEventListener("click", function() {\n' +
    '      var config = {\n' +
    '        apiKey: document.getElementById("apiKey").value.trim(),\n' +
    '        model: document.getElementById("model").value.trim(),\n' +
    '        systemPrompt: document.getElementById("systemPrompt").value.trim(),\n' +
    '        reasoning: document.getElementById("reasoning").value,\n' +
    '        maxTokens: parseInt(document.getElementById("maxTokens").value, 10) || 1024,\n' +
    '        fontSize: parseInt(document.getElementById("fontSize").value, 10) || 24,\n' +
    '        onlineAccess: document.getElementById("onlineAccess").checked,\n' +
    '        includeTime: document.getElementById("includeTime").checked,\n' +
    '        includeLocation: document.getElementById("includeLocation").checked,\n' +
    '        animateTitles: document.getElementById("animateTitles").checked,\n' +
    '        launchVoiceDictation: document.getElementById("launchVoiceDictation").checked\n' +
    '      };\n' +
    '      var returnTo = getQueryParam("return_to", "pebblejs://close#");\n' +
    '      document.location = returnTo + encodeURIComponent(JSON.stringify(config));\n' +
    '    });\n' +
    '    document.getElementById("btnReset").addEventListener("click", function() {\n' +
    '      if (confirm("Are you sure you want to reset all settings to default?")) {\n' +
    '        document.getElementById("apiKey").value = "";\n' +
    '        document.getElementById("model").value = "";\n' +
    '        document.getElementById("systemPrompt").value = ' + JSON.stringify(DEFAULT_SYSTEM_PROMPT) + ';\n' +
    '        document.getElementById("reasoning").value = "none";\n' +
    '        document.getElementById("maxTokens").value = 1024;\n' +
    '        document.getElementById("fontSize").value = "24";\n' +
    '        document.getElementById("onlineAccess").checked = false;\n' +
    '        document.getElementById("includeTime").checked = false;\n' +
    '        document.getElementById("includeLocation").checked = false;\n' +
    '        document.getElementById("animateTitles").checked = false;\n' +
    '        document.getElementById("launchVoiceDictation").checked = false;\n' +
    '        var config = {\n' +
    '          apiKey: "",\n' +
    '          model: "",\n' +
    '          systemPrompt: ' + JSON.stringify(DEFAULT_SYSTEM_PROMPT) + ',\n' +
    '          reasoning: "none",\n' +
    '          maxTokens: 1024,\n' +
    '          fontSize: 24,\n' +
    '          onlineAccess: false,\n' +
    '          includeTime: false,\n' +
    '          includeLocation: false,\n' +
    '          animateTitles: false,\n' +
    '          launchVoiceDictation: false\n' +
    '        };\n' +
    '        var returnTo = getQueryParam("return_to", "pebblejs://close#");\n' +
    '        document.location = returnTo + encodeURIComponent(JSON.stringify(config));\n' +
    '      }\n' +
    '    });\n' +
    '    document.getElementById("btnClear").addEventListener("click", function() {\n' +
    '      if (confirm("Are you sure you want to clear all saved chat sessions?")) {\n' +
    '        var config = {\n' +
    '          apiKey: document.getElementById("apiKey").value.trim(),\n' +
    '          model: document.getElementById("model").value.trim(),\n' +
    '          systemPrompt: document.getElementById("systemPrompt").value.trim(),\n' +
    '          reasoning: document.getElementById("reasoning").value,\n' +
    '          maxTokens: parseInt(document.getElementById("maxTokens").value, 10) || 1024,\n' +
    '          fontSize: parseInt(document.getElementById("fontSize").value, 10) || 24,\n' +
    '          onlineAccess: document.getElementById("onlineAccess").checked,\n' +
    '          includeTime: document.getElementById("includeTime").checked,\n' +
    '          includeLocation: document.getElementById("includeLocation").checked,\n' +
    '          animateTitles: document.getElementById("animateTitles").checked,\n' +
    '          launchVoiceDictation: document.getElementById("launchVoiceDictation").checked,\n' +
    '          clearSessions: true\n' +
    '        };\n' +
    '        var returnTo = getQueryParam("return_to", "pebblejs://close#");\n' +
    '        document.location = returnTo + encodeURIComponent(JSON.stringify(config));\n' +
    '      }\n' +
    '    });\n' +
    '  </script>\n' +
    '</body>\n' +
    '</html>';
}

Pebble.addEventListener('showConfiguration', function(e) {
  var settings = getSettings();
  var html = buildConfigPageHtml(settings);
  var dataUri = 'data:text/html;charset=utf-8,' + encodeURIComponent(html);
  Pebble.openURL(dataUri);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && e.response) {
    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      if (config.clearSessions) {
        console.log('[PHONE] Clearing all chat sessions');
        localStorage.removeItem(SESSIONS_KEY);
        sendSessionListToWatch();
      }
      saveSettings(config);
      console.log('[PHONE] Updated settings: ' + JSON.stringify(config));
    } catch (err) {
      console.log('[PHONE] Failed to parse returned settings: ' + err);
    }
  }
});

Pebble.addEventListener('ready', function(e) {
  console.log('[PHONE] PebbleChat JS bridge fully ready on phone');
  sendSettingsToWatch();
  sendSessionListToWatch();
});
