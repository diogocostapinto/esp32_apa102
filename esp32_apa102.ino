#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Adafruit_DotStar.h>
#include <ArtnetWifi.h>

const int numLeds = 30;
const int dataPin = 14;
const int clockPin = 12;
const int builtInLedPin = 2;
int universe = 0;
const int eepromSize = 512;

const char* defaultSSID = "____2Ghz";
const char* defaultPassword = "Aa00000000";

const int ssidAddr = 0;
const int passwordAddr = 32;
const int universeAddr = 100;

Adafruit_DotStar strip(numLeds, dataPin, clockPin, DOTSTAR_BGR);
ArtnetWifi artnet;
WebServer server(80);

String ssid;
String password;

void onArtNetDMX(uint16_t receivedUniverse, uint16_t length, uint8_t sequence, uint8_t* data) {
  if (receivedUniverse == universe && length >= numLeds * 3) {
    for (int i = 0; i < numLeds; i++) {
      int offset = i * 3;
      strip.setPixelColor(i, strip.Color(data[offset], data[offset + 1], data[offset + 2]));
    }
    strip.show();
  }
}

void saveSettings(const String& ssid, const String& password, int universe) {
  EEPROM.writeString(ssidAddr, ssid);
  EEPROM.writeString(passwordAddr, password);
  EEPROM.writeInt(universeAddr, universe);
  EEPROM.commit();
}

void loadSettings() {
  ssid = EEPROM.readString(ssidAddr);
  password = EEPROM.readString(passwordAddr);
  universe = EEPROM.readInt(universeAddr);

  if (ssid.isEmpty() || password.isEmpty()) {
    ssid = defaultSSID;
    password = defaultPassword;
  }

  if (universe < 0 || universe > 32767) {
    Serial.println("Universe invalid in EEPROM. Setting to 0.");
    universe = 0;
  }
}

void clearStrip() {
  strip.clear();
  strip.show();
}

void setupWiFi() {
  WiFi.mode(WIFI_AP_STA);

  WiFi.softAP("LED_Control", "00000000");
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Connecting to Wi-Fi");

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 30) {
    delay(500);
    Serial.print(".");
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin("esp32")) {
      Serial.println("mDNS responder started (esp32.local)");
    } else {
      Serial.println("Error starting mDNS responder.");
    }
  } else {
    Serial.println("\nFailed to connect to Wi-Fi.");
  }
}

void setupArtNet() {
  artnet.begin();
  artnet.setArtDmxCallback(onArtNetDMX);
  Serial.print("Listening for Art-Net universe: ");
  Serial.println(universe);
}

void setupWebServer() {
  server.on("/", []() {
    String apIpAddr = WiFi.softAPIP().toString();
    String staIpAddr = WiFi.localIP().toString();

    String html = "<html><body style='font-family: Arial; text-align: center;'>"
                  "<h1>LED Control</h1>"
                  "<p>AP IP Address: <b>" + apIpAddr + "</b></p>"
                  "<p>STA IP Address: <b>" + (staIpAddr == "0.0.0.0" ? "Not Connected" : staIpAddr) + "</b></p>"
                  "<form action='/setwifi' method='POST'>"
                  "SSID: <input type='text' name='ssid' required><br><br>"
                  "Password: <input type='password' name='password' required><br><br>"
                  "Universe: <input type='number' name='universe' min='0' max='32767' value='" + String(universe) + "' required><br><br>"
                  "<input type='submit' value='Save'>"
                  "</form>"
                  "<br><hr><br>"
                  "<form action='/reset' method='POST'>"
                  "<input type='submit' value='Reset Settings (Clear EEPROM)' style='background-color: red; color: white; padding: 10px 20px; border: none; border-radius: 5px;'>"
                  "</form>"
                  "</body></html>";

    server.send(200, "text/html", html);
  });

  server.on("/setwifi", []() {
    String newSSID = server.arg("ssid");
    String newPassword = server.arg("password");
    String universeStr = server.arg("universe");

    if (newSSID.isEmpty() || newPassword.isEmpty() || universeStr.isEmpty()) {
      server.send(400, "text/plain", "All fields must be filled!");
      return;
    }

    int newUniverse = universeStr.toInt();
    if (newUniverse < 0 || newUniverse > 32767) {
      server.send(400, "text/plain", "Invalid universe value!");
      return;
    }

    saveSettings(newSSID, newPassword, newUniverse);

    ssid = newSSID;
    password = newPassword;
    universe = newUniverse;

    Serial.println("Settings saved. Reinitializing Art-Net...");

    setupArtNet();

    server.send(200, "text/plain", "Settings saved. Art-Net universe updated. No reboot needed.");
  });

  server.on("/reset", []() {
    Serial.println("Resetting settings...");

    for (int i = 0; i < eepromSize; i++) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();

    server.send(200, "text/plain", "Settings cleared. Rebooting...");
    delay(1000);
    ESP.restart();
  });

  server.begin();
  Serial.println("Web server started.");
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(eepromSize);
  pinMode(builtInLedPin, OUTPUT);

  loadSettings();
  setupWiFi();
  setupWebServer();

  strip.begin();
  clearStrip();
  setupArtNet();
}

void loop() {
  bool artnetBusy = false;

  while (artnet.read() > 0) {
    artnetBusy = true;
  }

  if (!artnetBusy) {
    server.handleClient();
  }
}
