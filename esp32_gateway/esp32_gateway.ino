#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

/* TODO: Replace with your own WiFi credentials before flashing */
const char *WIFI_SSID = "111";
const char *WIFI_PASSWORD = "amg1408700";

#define LED_BB_RED 4
#define LED_BB_GREEN 5
#define LED_BB_BLUE 18
#define LED_BB_YELLOW 19

#define RXD2 16
#define TXD2 17

/* 数据缓存：断网期间缓存最近 10 条数据，重连后批量发送 */
#define DATA_CACHE_SIZE  10
#define CRC16_POLY        0xA001

WebServer server(80);
WebSocketsServer webSocket(81);

static String stm32_data_json = "{}";
static String data_cache[DATA_CACHE_SIZE];
static uint8_t data_cache_head = 0;
static uint8_t data_cache_count = 0;
static bool wifi_was_down = false;

static bool bb_led_state[4] = { false, false, false, false };
static const int bb_led_pins[4] = { LED_BB_RED, LED_BB_GREEN, LED_BB_BLUE, LED_BB_YELLOW };

/* CRC16-Modbus 校验 */
static uint16_t crc16_modbus(const uint8_t *data, uint16_t len) {
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) crc = (crc >> 1) ^ CRC16_POLY;
      else crc >>= 1;
    }
  }
  return crc;
}

/* 缓存一条数据（环形缓冲区） */
static void cache_data(const String &json) {
  data_cache[data_cache_head] = json;
  data_cache_head = (data_cache_head + 1) % DATA_CACHE_SIZE;
  if (data_cache_count < DATA_CACHE_SIZE) data_cache_count++;
}

/* 发送缓存的历史数据 */
static void send_cached_data(uint8_t client_num) {
  if (data_cache_count == 0) return;
  uint8_t start = (data_cache_head >= data_cache_count)
    ? (data_cache_head - data_cache_count)
    : (DATA_CACHE_SIZE - (data_cache_count - data_cache_head));
  for (uint8_t i = 0; i < data_cache_count; i++) {
    uint8_t idx = (start + i) % DATA_CACHE_SIZE;
    if (data_cache[idx].length() > 4) {
      webSocket.sendTXT(client_num, data_cache[idx]);
    }
  }
  Serial.printf("[WS] sent %u cached data items to client %u\n", data_cache_count, client_num);
}

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Gateway 控制面板</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0f172a;color:#e2e8f0;font:14px Arial;padding:12px;min-height:100vh}
h2{text-align:center;color:#38bdf8;margin:0 0 12px;font-size:20px}
h3{color:#94a3b8;font-size:13px;margin:14px 0 8px;border-bottom:1px solid #334155;padding-bottom:4px}
.g{display:grid;grid-template-columns:1fr 1fr;gap:6px}
.c{background:#1e293b;border-radius:8px;padding:10px;text-align:center}
.l{font-size:10px;color:#64748b}.v{font-size:22px;font-weight:700}
.lb{display:flex;flex-wrap:wrap;gap:6px;justify-content:center}
.btn{flex:1 1 60px;padding:12px 6px;border:none;border-radius:8px;font-size:13px;font-weight:600;
  cursor:pointer;transition:all .15s;min-width:50px;color:#fff}
.btn:active{transform:scale(.95)}
.on{background:#22c55e}.off{background:#ef4444}.toggle{background:#38bdf8}
.all-on{background:#22c55e}.all-off{background:#ef4444}
.bt{background:#64748b}.bt-r{background:#ef4444}.bt-g{background:#22c55e}
.bt-b{background:#3b82f6}.bt-y{background:#eab308;color:#000}
.bb{display:flex;flex-direction:column;gap:6px}
.bb .row{display:flex;gap:6px}
.bb .row .btn{flex:1}
.status-bar{text-align:center;font-size:10px;color:#475569;margin-top:12px}
.ip-info{background:#1e293b;border-radius:8px;padding:8px;text-align:center;margin-bottom:10px;border:1px solid #334155}
.ip-info .ip{color:#38bdf8;font-size:16px;font-weight:700}
.ip-info .tip{color:#64748b;font-size:10px}
.alarm{background:#ef4444;color:#fff;padding:4px 8px;border-radius:4px;font-size:10px;font-weight:600}
.normal{background:#22c55e;color:#fff;padding:4px 8px;border-radius:4px;font-size:10px;font-weight:600}
</style>
</head>
<body>

<h2>Gateway 控制面板</h2>

<div class="ip-info">
  <div class="tip">ESP32 IP 地址</div>
  <div class="ip" id="esp_ip">---</div>
</div>

<h3>实时采样数据</h3>
<div class="g">
  <div class="c"><div class="l">ADC (PB15)</div><div class="v" style="color:#38bdf8" id="c0">--</div><div class="l" style="font-size:9px" id="v0">--V</div></div>
  <div class="c"><div class="l">DHT11 温度</div><div class="v" style="color:#f59e0b" id="temp">--C</div></div>
  <div class="c"><div class="l">DHT11 湿度</div><div class="v" style="color:#38bdf8" id="hum">--</div></div>
  <div class="c"><div class="l">远程寄存器 0</div><div class="v" style="color:#a3e635" id="r">--</div></div>
  <div class="c"><div class="l">远程寄存器 1</div><div class="v" style="color:#a3e635" id="r1">--</div></div>
</div>

<h3>系统状态</h3>
<div class="g">
  <div class="c"><div class="l">Modbus</div><div class="v" style="font-size:14px" id="mb">--</div></div>
  <div class="c"><div class="l">云端</div><div class="v" style="font-size:14px" id="cl">--</div></div>
  <div class="c"><div class="l">告警状态</div><div class="v" style="font-size:12px" id="alarm">--</div></div>
</div>

<h3>统计信息</h3>
<div class="g">
  <div class="c"><div class="l">采样次数</div><div class="v" style="color:#f59e0b" id="s">--</div></div>
  <div class="c"><div class="l">通信成功</div><div class="v" style="color:#a3e635" id="ok">--</div></div>
  <div class="c"><div class="l">通信失败</div><div class="v" style="color:#ef4444" id="f">--</div></div>
  <div class="c"><div class="l">上传次数</div><div class="v" style="color:#f472b6" id="up">--</div></div>
</div>

<h3>面包板 LED (ESP32 GPIO)</h3>
<div class="bb">
  <div class="row">
    <button class="btn bt-r" onclick="bbCmd(0,'on')">红 ON</button>
    <button class="btn bt-r" onclick="bbCmd(0,'off')" style="opacity:0.7">红 OFF</button>
    <button class="btn bt-g" onclick="bbCmd(1,'on')">绿 ON</button>
    <button class="btn bt-g" onclick="bbCmd(1,'off')" style="opacity:0.7">绿 OFF</button>
  </div>
  <div class="row">
    <button class="btn bt-b" onclick="bbCmd(2,'on')">蓝 ON</button>
    <button class="btn bt-b" onclick="bbCmd(2,'off')" style="opacity:0.7">蓝 OFF</button>
    <button class="btn bt-y" onclick="bbCmd(3,'on')">黄 ON</button>
    <button class="btn bt-y" onclick="bbCmd(3,'off')" style="opacity:0.7">黄 OFF</button>
  </div>
</div>

<h3>开发板 LED (STM32 板载 8灯)</h3>
<div class="lb">
  <button class="btn on" onclick="stmCmd(1,'on')">1 ON</button>
  <button class="btn off" onclick="stmCmd(1,'off')">1 OFF</button>
  <button class="btn on" onclick="stmCmd(2,'on')">2 ON</button>
  <button class="btn off" onclick="stmCmd(2,'off')">2 OFF</button>
  <button class="btn on" onclick="stmCmd(3,'on')">3 ON</button>
  <button class="btn off" onclick="stmCmd(3,'off')">3 OFF</button>
  <button class="btn on" onclick="stmCmd(4,'on')">4 ON</button>
  <button class="btn off" onclick="stmCmd(4,'off')">4 OFF</button>
  <button class="btn on" onclick="stmCmd(5,'on')">5 ON</button>
  <button class="btn off" onclick="stmCmd(5,'off')">5 OFF</button>
  <button class="btn on" onclick="stmCmd(6,'on')">6 ON</button>
  <button class="btn off" onclick="stmCmd(6,'off')">6 OFF</button>
  <button class="btn on" onclick="stmCmd(7,'on')">7 ON</button>
  <button class="btn off" onclick="stmCmd(7,'off')">7 OFF</button>
  <button class="btn on" onclick="stmCmd(8,'on')">8 ON</button>
  <button class="btn off" onclick="stmCmd(8,'off')">8 OFF</button>
</div>
<div style="display:flex;gap:6px;margin-top:6px">
  <button class="btn all-on" onclick="stmAll('on')" style="flex:1">开发板全部 ON</button>
  <button class="btn all-off" onclick="stmAll('off')" style="flex:1">开发板全部 OFF</button>
</div>

<div class="status-bar" id="status">等待连接...</div>

<script>
var ws = null;
var wsUrl = 'ws://' + location.hostname + ':81/';

function connectWS() {
  ws = new WebSocket(wsUrl);
  ws.onopen = function() { document.getElementById('status').textContent = '已连接'; };
  ws.onclose = function() { document.getElementById('status').textContent = '断开, 3秒重连...'; setTimeout(connectWS,3000); };
  ws.onerror = function() { ws.close(); };
  ws.onmessage = function(e) {
    try {
      var d = JSON.parse(e.data);
      if (d.t == 'd') updateData(d);
      else if (d.t == 'query') updateData(d);
      else if (d.t == 'info') document.getElementById('esp_ip').textContent = d.ip;
      else if (d.t == 'alarm') updateAlarm(d);
    } catch(ex) {}
  };
}

function updateData(d) {
  document.getElementById('c0').textContent = d.c0 || '--';
  document.getElementById('v0').textContent = (d.v0 || 0).toFixed(2) + 'V';
  document.getElementById('temp').textContent = (d.temp || 0).toFixed(1) + 'C';
  document.getElementById('hum').textContent = (d.hum || '--') + '%';
  document.getElementById('r').textContent = d.r || '--';
  document.getElementById('r1').textContent = d.r1 || '--';
  document.getElementById('mb').innerHTML = d.mb ? '<span style=color:#22c55e>ONLINE</span>' : '<span style=color:#ef4444>OFFLINE</span>';
  document.getElementById('cl').innerHTML = d.cl ? '<span style=color:#22c55e>ONLINE</span>' : '<span style=color:#ef4444>OFFLINE</span>';
  document.getElementById('s').textContent = d.s || '--';
  document.getElementById('ok').textContent = d.ok || '--';
  document.getElementById('f').textContent = d.f || '--';
  document.getElementById('up').textContent = d.up || '--';
  updateAlarm(d);
}

function updateAlarm(d) {
  var al = d.al || 0;
  var alarmStr = 'NORMAL';
  if (al & 1) alarmStr = 'TEMP HIGH';
  else if (al & 2) alarmStr = 'TEMP LOW';
  else if (al & 4) alarmStr = 'VOLT HIGH';
  else if (al & 8) alarmStr = 'VOLT LOW';
  document.getElementById('alarm').innerHTML = al ? '<span class="alarm">' + alarmStr + '</span>' : '<span class="normal">NORMAL</span>';
}

function bbCmd(idx, act) { ws.send(JSON.stringify({t:'bb',id:idx,act:act})); }
function stmCmd(id, act) { ws.send(JSON.stringify({t:'stm',dev:'led',id:id,act:act})); }
function stmAll(act) { ws.send(JSON.stringify({t:'stm',dev:'led_all',act:act})); }

connectWS();
</script>

</body>
</html>
)rawliteral";

void bb_led_set(int idx, bool on) {
  if (idx < 0 || idx > 3) return;
  bb_led_state[idx] = on;
  digitalWrite(bb_led_pins[idx], on ? HIGH : LOW);
  Serial.printf("[BB] LED%d (GPIO%d) -> %s\n", idx + 1, bb_led_pins[idx], on ? "ON" : "OFF");
}

void bb_led_toggle(int idx) {
  if (idx < 0 || idx > 3) return;
  bb_led_set(idx, !bb_led_state[idx]);
}

void send_cmd_to_stm32(const char *dev, int id, const char *act)
{
    char json[128];
    snprintf(json, sizeof(json),
        "{\"t\":\"c\",\"dev\":\"%s\",\"id\":%d,\"act\":\"%s\"}\n", dev, id, act);
    Serial2.print(json);
    Serial2.flush();
    Serial.printf("[UART->STM32] %s", json);
}

void send_cmd_all_to_stm32(const char *dev, const char *act)
{
    char json[128];
    snprintf(json, sizeof(json),
        "{\"t\":\"c\",\"dev\":\"%s\",\"act\":\"%s\"}\n", dev, act);
    Serial2.print(json);
    Serial2.flush();
    Serial.printf("[UART->STM32] %s", json);
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WS] client %u disconnected\n", num);
      break;

    case WStype_CONNECTED:
      Serial.printf("[WS] client %u connected from %s\n", num, webSocket.remoteIP(num).toString().c_str());
      {
        String info = "{\"t\":\"info\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
        webSocket.sendTXT(num, info);
      }
      /* 发送最新一条数据 */
      if (stm32_data_json.length() > 4) {
        webSocket.sendTXT(num, stm32_data_json);
      }
      /* 发送缓存的历史数据 */
      send_cached_data(num);
      break;

    case WStype_TEXT:
      {
        String msg = String((char *)payload);
        Serial.printf("[WS] received: %s\n", msg.c_str());

        DynamicJsonDocument doc(256);
        DeserializationError err = deserializeJson(doc, msg);
        if (err) {
          Serial.printf("[WS] JSON parse error: %s\n", err.c_str());
          break;
        }

        const char *type = doc["t"];

        if (strcmp(type, "bb") == 0) {
          int idx = doc["id"];
          const char *act = doc["act"];
          if (strcmp(act, "on") == 0) bb_led_set(idx, true);
          else if (strcmp(act, "off") == 0) bb_led_set(idx, false);
          else if (strcmp(act, "toggle") == 0) bb_led_toggle(idx);
        }
        else if (strcmp(type, "stm") == 0) {
          const char *dev = doc["dev"];
          const char *act = doc["act"];
          if (strcmp(dev, "led_all") == 0) {
            send_cmd_all_to_stm32(dev, act);
          } else {
            int id = doc["id"];
            send_cmd_to_stm32(dev, id, act);
          }
        }
        break;
      }

    default: break;
  }
}

/* 验证 JSON 数据的 CRC16 校验
 * 约定：STM32 对「去掉 ,"crc":N 之后的完整 JSON（含结尾 }）」计算 CRC */
static bool validate_crc(const String &json) {
  int crc_pos = json.lastIndexOf(",\"crc\":");
  if (crc_pos < 0) {
    crc_pos = json.lastIndexOf("\"crc\":");
    if (crc_pos < 0) return true;  /* 无 CRC，兼容旧包 */
  }

  int val_start = json.indexOf(':', crc_pos) + 1;
  int val_end = json.indexOf('}', val_start);
  if (val_start <= 0 || val_end < 0) return false;

  String crc_str = json.substring(val_start, val_end);
  crc_str.trim();
  uint16_t received_crc = (uint16_t)crc_str.toInt();

  /* 还原为不含 crc 的完整 JSON：前缀 + '}' */
  String check_str = json.substring(0, crc_pos);
  check_str += '}';

  uint16_t calc_crc = crc16_modbus((const uint8_t *)check_str.c_str(),
                                   (uint16_t)check_str.length());
  if (calc_crc != received_crc) {
    Serial.printf("[CRC] mismatch: recv=%u calc=%u\n", received_crc, calc_crc);
    return false;
  }
  return true;
}

void handleRoot() {
  server.send(200, "text/html; charset=utf-8", HTML_PAGE);
}

void handleNotFound() {
  server.send(404, "text/plain", "404 Not Found");
}

/* 处理 STM32 发来的数据，带 CRC16 校验 */
void handleSTM32Data() {
  while (Serial2.available()) {
    String line = Serial2.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    /* 收到任意完整行就回 ping，打通 ESP32→STM32，供 Cloud:ON */
    Serial2.print("{\"t\":\"ping\"}\n");

    /* 检查是否为数据消息 */
    if (line.indexOf("\"t\":\"d\"") >= 0 || line.indexOf("\"t\":\"query\"") >= 0) {
      /* CRC16 校验 */
      if (!validate_crc(line)) {
        Serial.printf("[CRC] data rejected, len=%d\n", line.length());
        continue;
      }

      stm32_data_json = line;
      cache_data(line);

      /* WiFi 在线时才广播 */
      if (WiFi.status() == WL_CONNECTED) {
        webSocket.broadcastTXT(line);
        Serial.printf("[STM32->WS] broadcast: %s\n", line.c_str());
      } else {
        Serial.printf("[STM32] data cached (WiFi down), len=%d\n", line.length());
      }
    }
    /* 检查是否为告警消息 */
    else if (line.indexOf("\"t\":\"alarm\"") >= 0) {
      if (WiFi.status() == WL_CONNECTED) {
        webSocket.broadcastTXT(line);
        Serial.printf("[STM32->WS] alarm broadcast: %s\n", line.c_str());
      }
    }
    /* 检查是否为 ACK 响应 */
    else if (line.indexOf("\"t\":\"ack\"") >= 0) {
      if (WiFi.status() == WL_CONNECTED) {
        webSocket.broadcastTXT(line);
        Serial.printf("[STM32->WS] ack: %s\n", line.c_str());
      }
    }
    else {
      Serial.printf("[UART] unknown: %s\n", line.c_str());
    }
  }
}

/* WiFi 状态检查和自动重连 */
static void check_wifi(void) {
  static unsigned long last_check = 0;
  unsigned long now = millis();

  /* 每 10 秒检查一次 WiFi 状态 */
  if (now - last_check < 10000) return;
  last_check = now;

  if (WiFi.status() != WL_CONNECTED) {
    if (!wifi_was_down) {
      Serial.println("[WiFi] Connection lost! Attempting reconnect...");
      wifi_was_down = true;
    }
    WiFi.reconnect();
    /* 等待连接，最多 5 秒 */
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 50) {
      delay(100);
      retry++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WiFi] Reconnected!");
      Serial.printf("[WiFi] IP Address: %s\n", WiFi.localIP().toString().c_str());
      wifi_was_down = false;
    } else {
      Serial.printf("[WiFi] Reconnect failed, retry in 10s\n");
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n==========================");
  Serial.println("  ESP32 Gateway Starting");
  Serial.println("==========================");

  pinMode(LED_BB_RED, OUTPUT);
  pinMode(LED_BB_GREEN, OUTPUT);
  pinMode(LED_BB_BLUE, OUTPUT);
  pinMode(LED_BB_YELLOW, OUTPUT);
  digitalWrite(LED_BB_RED, LOW);
  digitalWrite(LED_BB_GREEN, LOW);
  digitalWrite(LED_BB_BLUE, LOW);
  digitalWrite(LED_BB_YELLOW, LOW);

  /* 启动 LED 自检 */
  for (int i = 0; i < 4; i++) {
    digitalWrite(bb_led_pins[i], HIGH);
    delay(200);
    digitalWrite(bb_led_pins[i], LOW);
  }

  /* 初始化缓存 */
  data_cache_head = 0;
  data_cache_count = 0;
  wifi_was_down = false;

  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  /* 增大超时：长 JSON（约 300 字节）在 115200bps 下需要约 26ms，50ms 留有足够余量 */
  Serial2.setTimeout(50);
  Serial.println("[UART2] initialized (RX=GPIO16, TX=GPIO17, 115200)");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
    Serial.printf("[WiFi] IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.println("===============================");
    Serial.printf("  手机浏览器访问: http://%s\n", WiFi.localIP().toString().c_str());
    Serial.println("===============================");
  } else {
    Serial.println("\n[WiFi] Connection FAILED! Will retry in loop().");
    wifi_was_down = true;
  }

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("[WS] WebSocket server started on port 81");

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[HTTP] Web server started on port 80");
}

void loop() {
  static unsigned long last_ping_ms = 0;

  webSocket.loop();
  server.handleClient();
  check_wifi();       /* WiFi 断线自动重连 */
  handleSTM32Data();  /* 处理 STM32 数据，带 CRC16 校验 */

  /* 每 2 秒向 STM32 发心跳，让 OLED 的 Cloud 能正确显示 ON
   * （Cloud:ON = STM32↔ESP32 串口通了，不是“手机连上了 WiFi”） */
  if (millis() - last_ping_ms >= 2000UL) {
    last_ping_ms = millis();
    Serial2.print("{\"t\":\"ping\"}\n");
  }

  delay(5);
}