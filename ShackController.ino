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
Preferences prefs;

// =====================================================
// App State
// =====================================================

String buttonNames[4] = {
  "Antenna 1",
  "Antenna 2",
  "Antenna 3",
  "Antenna 4"
};

String lastDrawnButtonNames[4] = {
  "",
  "",
  "",
  ""
};

bool buttonStates[4] = {
  false,
  false,
  false,
  false
};

bool lastDrawnButtonStates[4] = {
  true,
  true,
  true,
  true
};

bool wifiConnected = false;
bool websocketConnected = false;

bool lastDrawnWifiConnected = false;
bool lastDrawnWebsocketConnected = false;

bool darkTheme = true;
bool lastDrawnDarkTheme = false;

unsigned long lastReconnectAttempt = 0;
unsigned long lastTouchTime = 0;
unsigned long lastFooterUpdate = 0;

// =====================================================
// Colors
// =====================================================

uint16_t colorBackground;
uint16_t colorHeader;
uint16_t colorHeaderText;
uint16_t colorText;
uint16_t colorSubText;
uint16_t colorBorder;
uint16_t colorButtonOn;
uint16_t colorButtonOff;
uint16_t colorButtonText;
uint16_t colorFooterBg;
uint16_t colorThemeButton;
uint16_t colorThemeButtonText;

// =====================================================
// Button Layout - 320x240 Landscape
// =====================================================

struct ButtonArea {
  int x;
  int y;
  int w;
  int h;
};

ButtonArea antennaButtons[4] = {
  {  15,  52, 140, 68 },
  { 165,  52, 140, 68 },
  {  15, 136, 140, 68 },
  { 165, 136, 140, 68 }
};

ButtonArea themeButton = { 210, 210, 110, 30 };
// =====================================================
// Utility
// =====================================================

String getWebSocketURL() {
  return "ws://" + String(SWITCH_HOST) + ":" + String(SWITCH_PORT) + "/ws";
}

String trimLabel(String label, int maxLen) {
  label.trim();

  if (label.length() <= maxLen) {
    return label;
  }

  return label.substring(0, maxLen);
}

void applyThemeColors() {
  if (darkTheme) {
    colorBackground = TFT_BLACK;
    colorHeader = TFT_NAVY;
    colorHeaderText = TFT_WHITE;
    colorText = TFT_WHITE;
    colorSubText = TFT_DARKGREY;
    colorBorder = TFT_WHITE;
    colorButtonOn = TFT_GREEN;
    colorButtonOff = TFT_RED;
    colorButtonText = TFT_WHITE;
    colorFooterBg = TFT_BLACK;
    colorThemeButton = TFT_DARKGREY;
    colorThemeButtonText = TFT_WHITE;
  } else {
    colorBackground = TFT_WHITE;
    colorHeader = TFT_BLUE;
    colorHeaderText = TFT_WHITE;
    colorText = TFT_BLACK;
    colorSubText = TFT_DARKGREY;
    colorBorder = TFT_BLACK;
    colorButtonOn = TFT_GREEN;
    colorButtonOff = TFT_RED;
    colorButtonText = TFT_WHITE;
    colorFooterBg = TFT_WHITE;
    colorThemeButton = TFT_LIGHTGREY;
    colorThemeButtonText = TFT_BLACK;
  }
}

void forceFullRedrawMarkers() {
  for (int i = 0; i < 4; i++) {
    lastDrawnButtonNames[i] = "";
    lastDrawnButtonStates[i] = !buttonStates[i];
  }

  lastDrawnWifiConnected = !wifiConnected;
  lastDrawnWebsocketConnected = !websocketConnected;
  lastDrawnDarkTheme = !darkTheme;
}

// =====================================================
// Display Functions
// =====================================================

void drawBootScreen(const char* message1, const char* message2 = "") {
  applyThemeColors();

  tft.fillScreen(colorBackground);

  tft.setTextColor(colorText, colorBackground);
  tft.setTextSize(2);

  tft.setCursor(20, 70);
  tft.print(message1);

  if (strlen(message2) > 0) {
    tft.setTextSize(1);
    tft.setCursor(20, 110);
    tft.print(message2);
  }
}

void drawThemeButton() {
  tft.fillRoundRect(
    themeButton.x,
    themeButton.y,
    themeButton.w,
    themeButton.h,
    6,
    colorThemeButton
  );

  tft.drawRoundRect(
    themeButton.x,
    themeButton.y,
    themeButton.w,
    themeButton.h,
    6,
    colorBorder
  );

  tft.setTextColor(colorThemeButtonText, colorThemeButton);
  tft.setTextSize(1);

  String label = darkTheme ? "LIGHT MODE" : "DARK MODE";
  int textWidth = label.length() * 6;
  int textX = themeButton.x + ((themeButton.w - textWidth) / 2);
  int textY = themeButton.y + 7;

  tft.setCursor(textX, textY);
  tft.print(label);
}

void drawHeader(bool force = false) {
  if (!force &&
      lastDrawnWifiConnected == wifiConnected &&
      lastDrawnWebsocketConnected == websocketConnected &&
      lastDrawnDarkTheme == darkTheme) {
    return;
  }

  tft.fillRect(0, 0, 320, 42, colorHeader);

  tft.setTextColor(colorHeaderText, colorHeader);
  tft.setTextSize(2);
  tft.setCursor(8, 7);
  tft.print("Shack Ctrl");

  tft.setTextSize(1);
  tft.setCursor(8, 29);

  if (wifiConnected && websocketConnected) {
    tft.setTextColor(TFT_GREEN, colorHeader);
    tft.print("ONLINE");
  } else if (wifiConnected && !websocketConnected) {
    tft.setTextColor(TFT_YELLOW, colorHeader);
    tft.print("NO WEBSOCKET");
  } else {
    tft.setTextColor(TFT_RED, colorHeader);
    tft.print("NO WIFI");
  }

  tft.setTextColor(colorHeaderText, colorHeader);
  tft.setCursor(95, 29);

  if (wifiConnected) {
    tft.print(WiFi.localIP());
  } else {
    tft.print("Disconnected");
  }

  lastDrawnWifiConnected = wifiConnected;
  lastDrawnWebsocketConnected = websocketConnected;
  lastDrawnDarkTheme = darkTheme;
}

void drawFooter(bool force = false) {
  if (!force && millis() - lastFooterUpdate < 1000) {
    return;
  }

  lastFooterUpdate = millis();

  tft.fillRect(0, 210, 320, 30, colorFooterBg);

  tft.setTextSize(1);

  if (wifiConnected && websocketConnected) {
    tft.setTextColor(colorSubText, colorFooterBg);
    tft.setCursor(8, 222);
    tft.print("Synced");
  } else if (wifiConnected) {
    tft.setTextColor(TFT_YELLOW, colorFooterBg);
    tft.setCursor(8, 222);
    tft.print("WS reconnect...");
  } else {
    tft.setTextColor(TFT_RED, colorFooterBg);
    tft.setCursor(8, 222);
    tft.print("WiFi reconnect...");
  }

  drawThemeButton();
}

void drawButton(int index, bool force = false) {
  if (index < 0 || index > 3) {
    return;
  }

  if (!force &&
      lastDrawnButtonNames[index] == buttonNames[index] &&
      lastDrawnButtonStates[index] == buttonStates[index]) {
    return;
  }

  ButtonArea b = antennaButtons[index];

  uint16_t fillColor = buttonStates[index] ? colorButtonOn : colorButtonOff;

  tft.fillRoundRect(b.x, b.y, b.w, b.h, 10, fillColor);
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 10, colorBorder);

  String label = trimLabel(buttonNames[index], 12);

  tft.setTextColor(colorButtonText, fillColor);
  tft.setTextSize(2);

  int textWidth = label.length() * 12;
  int textX = b.x + ((b.w - textWidth) / 2);
  int textY = b.y + 15;

  if (textX < b.x + 5) {
    textX = b.x + 5;
  }

  tft.setCursor(textX, textY);
  tft.print(label);

  tft.setTextSize(1);

  String statusText = buttonStates[index] ? "SELECTED" : "OFF";
  int statusWidth = statusText.length() * 6;
  int statusX = b.x + ((b.w - statusWidth) / 2);

  tft.setCursor(statusX, b.y + 49);
  tft.print(statusText);

  lastDrawnButtonNames[index] = buttonNames[index];
  lastDrawnButtonStates[index] = buttonStates[index];
}

void drawStaticBackground() {
  applyThemeColors();

  tft.fillScreen(colorBackground);

  drawHeader(true);

  for (int i = 0; i < 4; i++) {
    drawButton(i, true);
  }

  drawFooter(true);
}

void updateChangedUI() {
  drawHeader(false);

  for (int i = 0; i < 4; i++) {
    drawButton(i, false);
  }

  drawFooter(false);
}

void flashButtonBorder(int index) {
  if (index < 0 || index > 3) {
    return;
  }

  ButtonArea b = antennaButtons[index];

  tft.drawRoundRect(b.x - 2, b.y - 2, b.w + 4, b.h + 4, 12, TFT_YELLOW);
  delay(60);
  tft.drawRoundRect(b.x - 2, b.y - 2, b.w + 4, b.h + 4, 12, colorBackground);
  drawButton(index, true);
}

void flashThemeButton() {
  tft.drawRoundRect(
    themeButton.x - 2,
    themeButton.y - 2,
    themeButton.w + 4,
    themeButton.h + 4,
    8,
    TFT_YELLOW
  );
  delay(80);
}

void toggleTheme() {
  Serial.println("Theme button detected");
  Serial.println("Toggling theme...");

  flashThemeButton();

  darkTheme = !darkTheme;
  prefs.putBool("darkTheme", darkTheme);

  Serial.print("New theme: ");
  Serial.println(darkTheme ? "DARK" : "LIGHT");

  applyThemeColors();
  forceFullRedrawMarkers();
  drawStaticBackground();
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

bool pointInArea(int x, int y, ButtonArea area) {
  return x >= area.x &&
         x <= area.x + area.w &&
         y >= area.y &&
         y <= area.y + area.h;
}

int getTouchedAntennaButton(int x, int y) {
  for (int i = 0; i < 4; i++) {
    if (pointInArea(x, y, antennaButtons[i])) {
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

  bool changed = false;

  if (doc.containsKey("states")) {
    JsonArray states = doc["states"].as<JsonArray>();

    for (int i = 0; i < 4; i++) {
      if (!states[i].isNull()) {
        bool newState = states[i].as<bool>();

        if (buttonStates[i] != newState) {
          buttonStates[i] = newState;
          changed = true;
        }
      }
    }
  }

  if (doc.containsKey("names")) {
    JsonArray names = doc["names"].as<JsonArray>();

    for (int i = 0; i < 4; i++) {
      if (!names[i].isNull()) {
        String newName = names[i].as<String>();

        if (buttonNames[i] != newName) {
          buttonNames[i] = newName;
          changed = true;
        }
      }
    }
  }

  if (changed) {
    updateChangedUI();
  }
}

void sendToggleCommand(int id) {
  if (!websocketConnected) {
    Serial.println("Cannot toggle: WebSocket not connected");
    drawFooter(true);
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
    updateChangedUI();
    return;
  }

  String url = "ws://" + String(SWITCH_HOST) + ":" + String(SWITCH_PORT) + "/ws";

  Serial.print("Connecting WebSocket: ");
  Serial.println(url);

  bool result = wsClient.connect(url);

  if (result) {
    websocketConnected = true;
    Serial.println("WebSocket connected");
  } else {
    websocketConnected = false;
    Serial.println("WebSocket connection failed");
  }

  updateChangedUI();
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
    delay(700);
  } else {
    wifiConnected = false;

    Serial.println("WiFi connection failed");
    drawBootScreen("WiFi Failed", "Will retry automatically");
    delay(900);
  }
}

// =====================================================
// Setup
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("========================================");
  Serial.println("CYD SHACK CONTROLLER");
  Serial.println("ESP32-2432S028");
  Serial.println("Display: ST7789 via TFT_eSPI");
  Serial.println("Touch: XPT2046 calibrated");
  Serial.println("Theme: saved in Preferences");
  Serial.println("========================================");

  prefs.begin("shackctrl", false);
  darkTheme = prefs.getBool("darkTheme", true);
  applyThemeColors();

  pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(TFT_BACKLIGHT_PIN, HIGH);

  tft.init();
  tft.setRotation(1);

  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSPI);
  touch.setRotation(1);

  drawBootScreen("CYD Starting...", "Display and touch OK");
  delay(700);

  wsClient.onMessage([](WebsocketsMessage message) {
    parseSwitchUpdate(message.data());
  });

  wsClient.onEvent([](WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
      Serial.println("WebSocket event: opened");
      websocketConnected = true;
      updateChangedUI();
    }

    if (event == WebsocketsEvent::ConnectionClosed) {
      Serial.println("WebSocket event: closed");
      websocketConnected = false;
      updateChangedUI();
    }

    if (event == WebsocketsEvent::GotPing) {
      Serial.println("WebSocket event: ping");
    }

    if (event == WebsocketsEvent::GotPong) {
      Serial.println("WebSocket event: pong");
    }
  });

  connectWiFi();

  forceFullRedrawMarkers();
  drawStaticBackground();

  if (wifiConnected) {
    connectWebSocket();
  }
}

// =====================================================
// Main Loop
// =====================================================

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      wifiConnected = true;
      updateChangedUI();
    }
  } else {
    if (wifiConnected) {
      wifiConnected = false;
      websocketConnected = false;
      updateChangedUI();
    }

    if (millis() - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = millis();
      connectWiFi();
      forceFullRedrawMarkers();
      drawStaticBackground();
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
    if (millis() - lastTouchTime > 450) {
      lastTouchTime = millis();

      Serial.print("Touch detected at X=");
      Serial.print(x);
      Serial.print(" Y=");
      Serial.println(y);

      if (pointInArea(x, y, themeButton)) {
        toggleTheme();
        return;
      }

      int buttonIndex = getTouchedAntennaButton(x, y);

      if (buttonIndex >= 0) {
        Serial.print("Touched antenna button ");
        Serial.println(buttonIndex);

        flashButtonBorder(buttonIndex);
        sendToggleCommand(buttonIndex);
      } else {
        Serial.println("Touch did not hit any button");
      }
    }
  }

  updateChangedUI();

  delay(10);
}


