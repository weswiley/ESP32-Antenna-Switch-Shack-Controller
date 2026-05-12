#include <WiFi.h>
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

using namespace websockets;

// =====================================================
// Wi-Fi / Antenna Switch ESP Settings
// =====================================================

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// IP address of the ESP running the TechMinds antenna switch code
const char* SWITCH_HOST = "192.168.1.50";
const uint16_t SWITCH_PORT = 80;

// =====================================================
// CYD Hardware Pins
// ESP32-2432S028
// Display handled by TFT_eSPI User_Setup.h
// Touch handled here
// =====================================================

#define TFT_BACKLIGHT_PIN 21

#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25

// Calibrated touch values from your board
#define RAW_X_MIN 588
#define RAW_X_MAX 3512
#define RAW_Y_MIN 848
#define RAW_Y_MAX 3429

// =====================================================
// Objects
// =====================================================

TFT_eSPI tft = TFT_eSPI();

SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

WebsocketsClient wsClient;

// =====================================================
// App State
// =====================================================

String buttonNames[4] = {
  "Antenna 1",
  "Antenna 2",
  "Antenna 3",
  "Antenna 4"
};

bool buttonStates[4] = {
  false,
  false,
  false,
  false
};

bool wifiConnected = false;
bool websocketConnected = false;

unsigned long lastReconnectAttempt = 0;
unsigned long lastTouchTime = 0;
unsigned long lastStatusBlink = 0;

bool statusBlink = false;

// =====================================================
// Button Layout - 320x240 Landscape
// =====================================================

struct ButtonArea {
  int x;
  int y;
  int w;
  int h;
};

ButtonArea buttons[4] = {
  {  15,  55, 140, 70 },
  { 165,  55, 140, 70 },
  {  15, 145, 140, 70 },
  { 165, 145, 140, 70 }
};

// =====================================================
// Utility
// =====================================================

String getWebSocketURL() {
  return "ws://" + String(SWITCH_HOST) + ":" + String(SWITCH_PORT) + "/ws";
}

void clearSerialBootNoise() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("CYD ANTENNA SWITCH CONTROLLER");
  Serial.println("ESP32-2432S028");
  Serial.println("Display: ST7789 via TFT_eSPI");
  Serial.println("Touch: XPT2046 calibrated");
  Serial.println("========================================");
}

String trimLabel(String label, int maxLen) {
  label.trim();

  if (label.length() <= maxLen) {
    return label;
  }

  return label.substring(0, maxLen);
}

// =====================================================
// Display Functions
// =====================================================

void drawHeader() {
  tft.fillRect(0, 0, 320, 42, TFT_NAVY);

  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextSize(2);
  tft.setCursor(10, 8);
  tft.print("Shack Controller");

  tft.setTextSize(1);
  tft.setCursor(230, 8);

  if (wifiConnected && websocketConnected) {
    tft.setTextColor(TFT_GREEN, TFT_NAVY);
    tft.print("ONLINE");
  } else if (wifiConnected && !websocketConnected) {
    tft.setTextColor(TFT_YELLOW, TFT_NAVY);
    tft.print("NO WS");
  } else {
    tft.setTextColor(TFT_RED, TFT_NAVY);
    tft.print("NO WIFI");
  }

  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setCursor(230, 23);
  tft.print(WiFi.localIP());
}

void drawFooter() {
  tft.fillRect(0, 222, 320, 18, TFT_BLACK);

  tft.setTextSize(1);

  if (wifiConnected && websocketConnected) {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(8, 228);
    tft.print("Synced with antenna switch");
  } else if (wifiConnected) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(8, 228);
    tft.print("Trying WebSocket reconnect...");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(8, 228);
    tft.print("Trying Wi-Fi reconnect...");
  }
}

void drawButton(int index) {
  ButtonArea b = buttons[index];

  uint16_t fillColor = buttonStates[index] ? TFT_GREEN : TFT_RED;
  uint16_t borderColor = TFT_WHITE;
  uint16_t textColor = TFT_WHITE;

  tft.fillRoundRect(b.x, b.y, b.w, b.h, 10, fillColor);
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 10, borderColor);

  String label = trimLabel(buttonNames[index], 12);

  tft.setTextColor(textColor, fillColor);
  tft.setTextSize(2);

  int textWidth = label.length() * 12;
  int textX = b.x + ((b.w - textWidth) / 2);
  int textY = b.y + 16;

  if (textX < b.x + 5) {
    textX = b.x + 5;
  }

  tft.setCursor(textX, textY);
  tft.print(label);

  tft.setTextSize(1);

  String statusText = buttonStates[index] ? "SELECTED" : "OFF";
  int statusWidth = statusText.length() * 6;
  int statusX = b.x + ((b.w - statusWidth) / 2);

  tft.setCursor(statusX, b.y + 50);
  tft.print(statusText);
}

void drawMainUI() {
  tft.fillScreen(TFT_BLACK);
  drawHeader();

  for (int i = 0; i < 4; i++) {
    drawButton(i);
  }

  drawFooter();
}

void drawBootScreen(const char* message1, const char* message2 = "") {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  tft.setCursor(20, 70);
  tft.print(message1);

  if (strlen(message2) > 0) {
    tft.setTextSize(1);
    tft.setCursor(20, 110);
    tft.print(message2);
  }
}

void drawTouchFlash(int index) {
  if (index < 0 || index > 3) {
    return;
  }

  ButtonArea b = buttons[index];
  tft.drawRoundRect(b.x - 2, b.y - 2, b.w + 4, b.h + 4, 12, TFT_YELLOW);
  delay(80);
  drawButton(index);
}

// =====================================================
// Touch Functions
// =====================================================

bool getTouchXY(int &screenX, int &screenY) {
  if (!touch.touched()) {
    return false;
  }

  TS_Point p = touch.getPoint();

  screenX = map(p.x, RAW_X_MIN, RAW_X_MAX, 0, 320);
  screenY = map(p.y, RAW_Y_MIN, RAW_Y_MAX, 0, 240);

  screenX = constrain(screenX, 0, 319);
  screenY = constrain(screenY, 0, 239);

  return true;
}

int getTouchedButton(int x, int y) {
  for (int i = 0; i < 4; i++) {
    ButtonArea b = buttons[i];

    if (x >= b.x &&
        x <= b.x + b.w &&
        y >= b.y &&
        y <= b.y + b.h) {
      return i;
    }
  }

  return -1;
}

// =====================================================
// WebSocket / JSON Functions
// =====================================================

void parseSwitchUpdate(String payload) {
  Serial.print("WebSocket RX: ");
  Serial.println(payload);

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }

  if (doc.containsKey("states")) {
    JsonArray states = doc["states"].as<JsonArray>();

    for (int i = 0; i < 4; i++) {
      if (!states[i].isNull()) {
        buttonStates[i] = states[i].as<bool>();
      }
    }
  }

  if (doc.containsKey("names")) {
    JsonArray names = doc["names"].as<JsonArray>();

    for (int i = 0; i < 4; i++) {
      if (!names[i].isNull()) {
        buttonNames[i] = names[i].as<String>();
      }
    }
  }

  drawMainUI();
}

void sendToggleCommand(int id) {
  if (!websocketConnected) {
    Serial.println("Cannot toggle: WebSocket not connected");
    drawFooter();
    return;
  }

  if (id < 0 || id > 3) {
    return;
  }

  StaticJsonDocument<128> doc;
  doc["action"] = "toggle";
  doc["id"] = id;

  String message;
  serializeJson(doc, message);

  Serial.print("WebSocket TX: ");
  Serial.println(message);

  wsClient.send(message);
}

void connectWebSocket() {
  if (WiFi.status() != WL_CONNECTED) {
    websocketConnected = false;
    return;
  }

  String url = getWebSocketURL();

  Serial.print("Connecting WebSocket: ");
  Serial.println(url);

  websocketConnected = wsClient.connect(url);

  if (websocketConnected) {
    Serial.println("WebSocket connected");
  } else {
    Serial.println("WebSocket connection failed");
  }

  drawMainUI();
}

// =====================================================
// Wi-Fi Functions
// =====================================================

void connectWiFi() {
  drawBootScreen("Connecting WiFi...", WIFI_SSID);

  Serial.print("Connecting WiFi to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(250);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;

    Serial.println("WiFi connected");
    Serial.print("CYD IP: ");
    Serial.println(WiFi.localIP());

    drawBootScreen("WiFi Connected", WiFi.localIP().toString().c_str());
    delay(800);
  } else {
    wifiConnected = false;

    Serial.println("WiFi connection failed");
    drawBootScreen("WiFi Failed", "Will retry automatically");
    delay(1000);
  }
}

// =====================================================
// Setup
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  clearSerialBootNoise();

  pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(TFT_BACKLIGHT_PIN, HIGH);

  tft.init();
  tft.setRotation(1);

  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSPI);
  touch.setRotation(1);

  drawBootScreen("CYD Starting...", "Display and touch OK");
  delay(800);

  wsClient.onMessage([](WebsocketsMessage message) {
    parseSwitchUpdate(message.data());
  });

  wsClient.onEvent([](WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
      Serial.println("WebSocket event: opened");
      websocketConnected = true;
      drawMainUI();
    }

    if (event == WebsocketsEvent::ConnectionClosed) {
      Serial.println("WebSocket event: closed");
      websocketConnected = false;
      drawMainUI();
    }

    if (event == WebsocketsEvent::GotPing) {
      Serial.println("WebSocket event: ping");
    }

    if (event == WebsocketsEvent::GotPong) {
      Serial.println("WebSocket event: pong");
    }
  });

  connectWiFi();

  if (wifiConnected) {
    connectWebSocket();
  } else {
    drawMainUI();
  }
}

// =====================================================
// Main Loop
// =====================================================

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      wifiConnected = true;
      drawMainUI();
    }
  } else {
    if (wifiConnected) {
      wifiConnected = false;
      websocketConnected = false;
      drawMainUI();
    }

    if (millis() - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = millis();
      connectWiFi();
    }

    delay(50);
    return;
  }

  if (websocketConnected) {
    wsClient.poll();
  } else {
    if (millis() - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = millis();
      connectWebSocket();
    }
  }

  int x;
  int y;

  if (getTouchXY(x, y)) {
    if (millis() - lastTouchTime > 500) {
      lastTouchTime = millis();

      int buttonIndex = getTouchedButton(x, y);

      if (buttonIndex >= 0) {
        Serial.print("Touched button ");
        Serial.println(buttonIndex);

        drawTouchFlash(buttonIndex);
        sendToggleCommand(buttonIndex);
      }
    }
  }

  if (millis() - lastStatusBlink > 1000) {
    lastStatusBlink = millis();
    statusBlink = !statusBlink;
  }

  delay(10);
}
