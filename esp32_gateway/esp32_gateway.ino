/**
 * @file esp32_gateway.ino
 * @brief ESP32 网关 — WiFi WebSocket + 面包板LED控制 + STM32通信
 * 
 * 硬件引脚：
 *   GPIO16 (RX2)  → STM32 PA2 (USART2_TX)    ESP32 接收 STM32 数据
 *   GPIO17 (TX2)  → STM32 PA3 (USART2_RX)    ESP32 发送控制指令给 STM32
 *   GPIO4         → 面包板 LED1 (红) + 220Ω → GND
 *   GPIO5         → 面包板 LED2 (绿) + 220Ω → GND
 *   GPIO18        → 面包板 LED3 (蓝) + 220Ω → GND
 *   GPIO19        → 面包板 LED4 (黄) + 220Ω → GND
 * 
 * 使用说明：
 *   1. 修改下方 WiFi 账号密码
 *   2. 烧录后打开串口监视器(115200)查看 ESP32 获取的 IP 地址
 *   3. 手机连接同一WiFi，浏览器访问该IP
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

/* ==================== WiFi 配置 ==================== */
const char *WIFI_SSID = "111";
const char *WIFI_PASSWORD = "amg1408700";

/* ==================== 硬件引脚定义 ==================== */
/* 面包板 LED 引脚 */
#define LED_BB_RED 4
#define LED_BB_GREEN 5
#define LED_BB_BLUE 18
#define LED_BB_YELLOW 19

/* UART2 与 STM32 通信 (115200, 8N1)
 * ESP32 默认 Serial2 引脚: RX=GPIO16, TX=GPIO17 */
#define RXD2 16
#define TXD2 17

/* ==================== 全局变量 ==================== */
WebServer server(80);
WebSocketsServer webSocket(81);

/* 存储 STM32 最新数据 */
static String stm32_data_json = "{}";

/* 面包板 LED 状态 */
static bool bb_led_state[4] = { false, false, false, false };
static const int bb_led_pins[4] = { LED_BB_RED, LED_BB_GREEN, LED_BB_BLUE, LED_BB_YELLOW };

/* ==================== HTML 网页 ==================== */
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
.on{background:#22c55e}
.off{background:#ef4444}
.toggle{background:#38bdf8}
.all-on{background:#22c55e}
.all-off{background:#ef4444}
.bt{background:#64748b}.bt-r{background:#ef4444}.bt-g{background:#22c55e}
.bt-b{background:#3b82f6}.bt-y{background:#eab308;color:#000}
.bb{display:flex;flex-direction:column;gap:6px}
.bb .row{display:flex;gap:6px}
.bb .row .btn{flex:1}
.status-bar{text-align:center;font-size:10px;color:#475569;margin-top:12px}
.ip-info{background:#1e293b;border-radius:8px;padding:8px;text-align:center;margin-bottom:10px;
  border:1px solid #334155}
.ip-info .ip{color:#38bdf8;font-size:16px;font-weight:700}
.ip-info .tip{color:#64748b;font-size:10px}
</style>
</head>
<body>

<h2>Gateway 控制面板</h2>

<div class="ip-info">
  <div class="tip">ESP32 IP 地址 (浏览器访问此IP)</div>
  <div class="ip" id="esp_ip">---</div>
</div>

<h3>实时数据</h3>
<div class="g">
  <div class="c"><div class="l">本地采样</div><div class="v" style="color:#38bdf8" id="local">--</div></div>
  <div class="c"><div class="l">远程寄存</div><div class="v" style="color:#22c55e" id="remote">--</div></div>
  <div class="c"><div class="l">Modbus</div><div class="v" style="font-size:14px" id="mb">--</div></div>
  <div class="c"><div class="l">云端</div><div class="v" style="font-size:14px" id="cl">--</div></div>
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
      else if (d.t == 'bb') updateBB(d);
      else if (d.t == 'info') document.getElementById('esp_ip').textContent = d.ip;
    } catch(ex) {}
  };
}

function updateData(d) {
  document.getElementById('local').textContent = d.l;
  document.getElementById('remote').textContent = d.r;
  document.getElementById('mb').innerHTML = d.mb ? '<span style=color:#22c55e>ONLINE</span>' : '<span style=color:#ef4444>OFFLINE</span>';
  document.getElementById('cl').innerHTML = d.cl ? '<span style=color:#22c55e>ONLINE</span>' : '<span style=color:#ef4444>OFFLINE</span>';
  document.getElementById('s').textContent = d.s;
  document.getElementById('ok').textContent = d.ok;
  document.getElementById('f').textContent = d.f;
  document.getElementById('up').textContent = d.up;
}

function updateBB(d) {
  var colors = ['red','green','blue','yellow'];
  var els = document.querySelectorAll('.bb .btn');
}

function bbCmd(idx, act) { ws.send(JSON.stringify({t:'bb',id:idx,act:act})); }
function stmCmd(id, act) { ws.send(JSON.stringify({t:'stm',dev:'led',id:id,act:act})); }
function stmAll(act) { ws.send(JSON.stringify({t:'stm',dev:'led_all',act:act})); }

connectWS();
</script>

</body>
</html>
)rawliteral";


/* ==================== 面包板 LED 控制 ==================== */
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

/* ==================== 向 STM32 发送控制指令 ==================== */
void send_cmd_to_stm32(const char *dev, int id, const char *act)
{
    char json[128];
    snprintf(json, sizeof(json),
        "{\"t\":\"c\",\"dev\":\"%s\",\"id\":%d,\"act\":\"%s\"}\n", dev, id, act);
    Serial2.print(json);
    Serial2.flush();  /* 确保数据发出 */
    Serial.printf("[UART->STM32] %s", json);
}

void send_cmd_all_to_stm32(const char *dev, const char *act)
{
    char json[128];
    snprintf(json, sizeof(json),
        "{\"t\":\"c\",\"dev\":\"%s\",\"act\":\"%s\"}\n", dev, act);
    Serial2.print(json);
    Serial2.flush();  /* 确保数据发出 */
    Serial.printf("[UART->STM32] %s", json);
}

/* ==================== WebSocket 事件处理 ==================== */
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WS] client %u disconnected\n", num);
      break;

    case WStype_CONNECTED:
      Serial.printf("[WS] client %u connected from %s\n", num, webSocket.remoteIP(num).toString().c_str());
      /* 发送 ESP32 IP 和最新数据 */
      {
        String info = "{\"t\":\"info\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
        webSocket.sendTXT(num, info);
      }
      if (stm32_data_json.length() > 4) {
        webSocket.sendTXT(num, stm32_data_json);
      }
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

        /* ---- 面包板 LED 控制 ---- */
        if (strcmp(type, "bb") == 0) {
          int idx = doc["id"];
          const char *act = doc["act"];
          if (strcmp(act, "on") == 0) bb_led_set(idx, true);
          else if (strcmp(act, "off") == 0) bb_led_set(idx, false);
          else if (strcmp(act, "toggle") == 0) bb_led_toggle(idx);
        }
        /* ---- STM32 板载 LED 控制 (转发) ---- */
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

/* ==================== HTTP 服务器 ==================== */
void handleRoot() {
  server.send(200, "text/html; charset=utf-8", HTML_PAGE);
}

void handleNotFound() {
  server.send(404, "text/plain", "404 Not Found");
}

/* ==================== 处理 STM32 发来的数据 ==================== */
void handleSTM32Data() {
  while (Serial2.available()) {
    String line = Serial2.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    /* 检查是否是 STM32 发来的数据 JSON */
    if (line.indexOf("\"t\":\"d\"") >= 0) {
      stm32_data_json = line;
      /* 广播给所有 WebSocket 客户端 */
      webSocket.broadcastTXT(line);
      Serial.printf("[STM32->WS] broadcast: %s\n", line.c_str());
    } else {
      Serial.printf("[UART] unknown: %s\n", line.c_str());
    }
  }
}

/* ==================== 初始化 ==================== */
void setup() {
  Serial.begin(115200);
  Serial.println("\n==========================");
  Serial.println("  ESP32 Gateway Starting");
  Serial.println("==========================");

  /* GPIO 初始化 — 面包板 LED */
  pinMode(LED_BB_RED, OUTPUT);
  pinMode(LED_BB_GREEN, OUTPUT);
  pinMode(LED_BB_BLUE, OUTPUT);
  pinMode(LED_BB_YELLOW, OUTPUT);
  digitalWrite(LED_BB_RED, LOW);
  digitalWrite(LED_BB_GREEN, LOW);
  digitalWrite(LED_BB_BLUE, LOW);
  digitalWrite(LED_BB_YELLOW, LOW);

  /* 面包板 LED 测试闪烁（确认接线正确） */
  for (int i = 0; i < 4; i++) {
    digitalWrite(bb_led_pins[i], HIGH);
    delay(200);
    digitalWrite(bb_led_pins[i], LOW);
  }

  /* UART2 初始化 — 与 STM32 通信 */
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);  /* RX=GPIO16, TX=GPIO17 */
  Serial2.setTimeout(10);  /* 10ms超时，防止阻塞 */
  Serial.println("[UART2] initialized (RX=GPIO16, TX=GPIO17, 115200)");

  /* WiFi 连接 */
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
    Serial.println("\n[WiFi] Connection FAILED! Check SSID/Password.");
  }

  /* WebSocket 初始化 */
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("[WS] WebSocket server started on port 81");

  /* HTTP 服务器初始化 */
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[HTTP] Web server started on port 80");
}

/* ==================== 主循环 ==================== */
void loop() {
  webSocket.loop();
  server.handleClient();
  handleSTM32Data();
  delay(5);
}
