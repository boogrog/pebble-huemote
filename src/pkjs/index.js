// ---------------------------------------------------------------------------
// Huemote — phone-side bridge between the watch and the Hue Bridge v2.
//
// The watch can't reach the network, so this PebbleKit JS code does all of the
// HTTP. It talks the local Hue REST API v1 (plain HTTP, same WiFi as the
// bridge) and relays results back over AppMessage.
// ---------------------------------------------------------------------------

// ---- Protocol constants (mirror comm.h) -----------------------------------
var CMD_REQUEST_ROOMS  = 1;
var CMD_TOGGLE_ROOM    = 2;
var CMD_SET_BRI        = 3;
var CMD_APPLY_SCENE    = 4;
var CMD_ALL_OFF        = 5;
var CMD_START_PAIRING  = 6;
var CMD_REQUEST_SCENES = 7;

var BRIDGE_NOT_PAIRED = 0;
var BRIDGE_PAIRING    = 1;
var BRIDGE_READY      = 2;
var BRIDGE_ERROR      = 3;

var ITEM_ROOM  = 0;
var ITEM_SCENE = 1;

// ---- Demo mode ------------------------------------------------------------
// When true, serve canned rooms/scenes instead of talking to a real bridge —
// used to exercise the full UI in the emulator. Ships disabled.
var DEMO = false;
var demoRooms = [
  { id: '1', name: 'Living Room', on: 1, bri: 203 },
  { id: '2', name: 'Bedroom',     on: 0, bri: 120 },
  { id: '3', name: 'Office',      on: 1, bri: 114 },
  { id: '4', name: 'Kitchen',     on: 1, bri: 254 }
];
var demoScenes = [
  { id: 's1', name: 'Relax',       group: '1' },
  { id: 's2', name: 'Concentrate', group: '1' },
  { id: 's3', name: 'Energize',    group: '1' },
  { id: 's4', name: 'Nightlight',  group: '1' }
];

// ---- Persistent state -----------------------------------------------------
var bridgeIp = localStorage.getItem('hue_ip') || null;
var hueUser  = localStorage.getItem('hue_user') || null;

function apiBase() {
  return 'http://' + bridgeIp + '/api/' + hueUser;
}

// ---- AppMessage outbox queue (serialize sends) ----------------------------
var outQueue = [];
var sending = false;

function enqueue(dict) {
  outQueue.push(dict);
  pump();
}

function pump() {
  if (sending || outQueue.length === 0) {
    return;
  }
  sending = true;
  var dict = outQueue.shift();
  Pebble.sendAppMessage(dict,
    function() { sending = false; pump(); },
    function(e) { console.log('send failed: ' + JSON.stringify(e)); sending = false; pump(); });
}

function sendStatus(status) {
  enqueue({ bridge_status: status });
}

// ---- HTTP helper ----------------------------------------------------------
function http(method, url, body, ok, fail) {
  var xhr = new XMLHttpRequest();
  xhr.open(method, url, true);
  xhr.timeout = 8000;
  xhr.onload = function() {
    if (xhr.status >= 200 && xhr.status < 300) {
      var data = null;
      try { data = JSON.parse(xhr.responseText); } catch (e) {}
      ok(data);
    } else if (fail) {
      fail(xhr.status);
    }
  };
  xhr.onerror = function() { if (fail) fail(-1); };
  xhr.ontimeout = function() { if (fail) fail(-2); };
  if (body) {
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.send(JSON.stringify(body));
  } else {
    xhr.send();
  }
}

// ---- Bridge discovery + pairing -------------------------------------------
function ensureIp(cb) {
  if (bridgeIp) {
    cb(bridgeIp);
    return;
  }
  // Hue's cloud discovery returns the local IP of bridges on this network.
  http('GET', 'https://discovery.meethue.com', null, function(data) {
    if (data && data[0] && data[0].internalipaddress) {
      bridgeIp = data[0].internalipaddress;
      localStorage.setItem('hue_ip', bridgeIp);
      cb(bridgeIp);
    } else {
      cb(null);
    }
  }, function() { cb(null); });
}

function pair() {
  sendStatus(BRIDGE_PAIRING);
  ensureIp(function(ip) {
    if (!ip) {
      sendStatus(BRIDGE_ERROR);
      return;
    }
    var attempts = 0;
    var maxAttempts = 15;  // ~30s window to press the link button
    (function tryOnce() {
      attempts++;
      http('POST', 'http://' + ip + '/api', { devicetype: 'huemote#pebble' },
        function(data) {
          if (data && data[0] && data[0].success) {
            hueUser = data[0].success.username;
            localStorage.setItem('hue_user', hueUser);
            sendStatus(BRIDGE_READY);
            requestRooms();
            return;
          }
          // error 101 = link button not pressed yet; keep polling.
          if (data && data[0] && data[0].error && data[0].error.type === 101 && attempts < maxAttempts) {
            setTimeout(tryOnce, 2000);
            return;
          }
          sendStatus(BRIDGE_ERROR);
        },
        function() {
          if (attempts < maxAttempts) { setTimeout(tryOnce, 2000); }
          else { sendStatus(BRIDGE_ERROR); }
        });
    })();
  });
}

// ---- Reads: rooms + scenes ------------------------------------------------
function streamRooms(rooms) {
  rooms.forEach(function(r, i) {
    enqueue({
      item_kind: ITEM_ROOM, item_index: i, item_count: rooms.length,
      item_name: String(r.name), item_id: String(r.id),
      item_on: r.on, item_bri: r.bri
    });
  });
}

function requestRooms() {
  if (DEMO) { streamRooms(demoRooms); return; }
  if (!bridgeIp || !hueUser) { sendStatus(BRIDGE_NOT_PAIRED); return; }
  http('GET', apiBase() + '/groups', null, function(groups) {
    if (!groups) { return; }
    var rooms = Object.keys(groups)
      .filter(function(k) {
        var t = groups[k].type;
        return t === 'Room' || t === 'Zone';
      })
      .map(function(k) {
        var g = groups[k];
        return {
          id: k,
          name: g.name,
          on: (g.state && g.state.any_on) ? 1 : 0,
          bri: (g.action && typeof g.action.bri === 'number') ? g.action.bri : 0
        };
      });
    rooms.forEach(function(r, i) {
      enqueue({
        item_kind: ITEM_ROOM,
        item_index: i,
        item_count: rooms.length,
        item_name: String(r.name),
        item_id: String(r.id),
        item_on: r.on,
        item_bri: r.bri
      });
    });
  }, function(status) {
    sendStatus(BRIDGE_ERROR);
  });
}

function requestScenes(roomId) {
  if (DEMO) {
    demoScenes.forEach(function(s, i) {
      enqueue({
        item_kind: ITEM_SCENE, item_index: i, item_count: demoScenes.length,
        item_name: String(s.name), item_id: String(s.id), item_group: String(s.group)
      });
    });
    return;
  }
  if (!bridgeIp || !hueUser) { return; }
  http('GET', apiBase() + '/scenes', null, function(scenes) {
    if (!scenes) { return; }
    var all = Object.keys(scenes).map(function(k) {
      var s = scenes[k];
      return { id: k, name: s.name, group: s.group || '0' };
    });
    var matched = all.filter(function(s) { return s.group === String(roomId); });
    var list = matched.length ? matched : all;
    list.forEach(function(s, i) {
      enqueue({
        item_kind: ITEM_SCENE,
        item_index: i,
        item_count: list.length,
        item_name: String(s.name),
        item_id: String(s.id),
        item_group: String(s.group)
      });
    });
  });
}

// ---- Writes: toggle / brightness / scene / all-off ------------------------
function demoRoom(roomId) {
  for (var i = 0; i < demoRooms.length; i++) {
    if (demoRooms[i].id === String(roomId)) { return demoRooms[i]; }
  }
  return null;
}

function toggleRoom(roomId) {
  if (DEMO) {
    var r = demoRoom(roomId);
    if (r) { r.on = r.on ? 0 : 1; }
    streamRooms(demoRooms);
    return;
  }
  http('GET', apiBase() + '/groups/' + roomId, null, function(g) {
    var on = g && g.state && g.state.any_on;
    http('PUT', apiBase() + '/groups/' + roomId + '/action', { on: !on }, function() {
      requestRooms();
    });
  });
}

function setBri(roomId, bri) {
  if (DEMO) {
    var r = demoRoom(roomId);
    if (r) { r.bri = bri; r.on = 1; }
    return;
  }
  http('PUT', apiBase() + '/groups/' + roomId + '/action', { on: true, bri: bri }, function() {});
}

function applyScene(sceneId) {
  if (DEMO) { return; }
  http('PUT', apiBase() + '/groups/0/action', { scene: sceneId }, function() {
    requestRooms();
  });
}

function allOff() {
  if (DEMO) {
    demoRooms.forEach(function(r) { r.on = 0; });
    streamRooms(demoRooms);
    return;
  }
  http('PUT', apiBase() + '/groups/0/action', { on: false }, function() {
    requestRooms();
  });
}

// ---- Pebble event wiring --------------------------------------------------
Pebble.addEventListener('ready', function() {
  if (DEMO) { sendStatus(BRIDGE_READY); return; }
  if (bridgeIp && hueUser) {
    sendStatus(BRIDGE_READY);  // watch will follow up with REQUEST_ROOMS
  } else {
    sendStatus(BRIDGE_NOT_PAIRED);
  }
});

Pebble.addEventListener('appmessage', function(e) {
  var d = e.payload;
  switch (d.cmd) {
    case CMD_REQUEST_ROOMS:  requestRooms(); break;
    case CMD_TOGGLE_ROOM:    toggleRoom(d.room_id); break;
    case CMD_SET_BRI:        setBri(d.room_id, d.bri); break;
    case CMD_APPLY_SCENE:    applyScene(d.scene_id); break;
    case CMD_ALL_OFF:        allOff(); break;
    case CMD_START_PAIRING:  pair(); break;
    case CMD_REQUEST_SCENES: requestScenes(d.room_id); break;
  }
});

// ---- Settings page: manual bridge IP + re-pair ----------------------------
Pebble.addEventListener('showConfiguration', function() {
  var ipVal = bridgeIp || '';
  var html =
    '<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<style>body{font-family:sans-serif;padding:20px;}input{font-size:18px;padding:8px;width:100%;box-sizing:border-box;}' +
    'button{font-size:18px;padding:12px;width:100%;margin-top:14px;}</style></head><body>' +
    '<h2>Huemote</h2>' +
    '<label>Bridge IP (optional — auto-detected if blank)</label>' +
    '<input id="ip" value="' + ipVal + '" placeholder="192.168.1.x">' +
    '<button onclick="save(true)">Save &amp; Pair (press link button)</button>' +
    '<button onclick="save(false)">Save only</button>' +
    '<script>function save(p){var cfg={ip:document.getElementById("ip").value.trim(),pair:p};' +
    'document.location="pebblejs://close#"+encodeURIComponent(JSON.stringify(cfg));}</script>' +
    '</body></html>';
  Pebble.openURL('data:text/html,' + encodeURIComponent(html));
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) { return; }
  var cfg;
  try { cfg = JSON.parse(decodeURIComponent(e.response)); } catch (err) { return; }
  if (cfg.ip) {
    bridgeIp = cfg.ip;
    localStorage.setItem('hue_ip', bridgeIp);
  }
  if (cfg.pair) {
    pair();
  }
});
