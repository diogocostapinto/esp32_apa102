#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Adafruit_DotStar.h>
#include <ArtnetWifi.h>
#include <OSCMessage.h>

const int universe = 0, numLeds = 30, dataPin = 14, clockPin = 12, builtInLedPin = 2;
const int encoderPinA = 4, encoderPinB = 18, encoderButtonPin = 15;
const char* defaultSSID = "____2Ghz";
const char* defaultPassword = "Aa00000000";
String ssid = defaultSSID, password = defaultPassword;
const int ssidAddr = 0, passwordAddr = 32;
unsigned long lastLedToggleTime = 0, lastDmxPacketTime = 0, dmxTimeout = 5000;
const int apModeBlinkInterval = 2000, artnetBlinkOnInterval = 500, artnetBlinkOffInterval = 150;
int currentBlinkInterval = 0, oscValue = 0, lastEncoderStateA = LOW;
bool artnetActive = false, wifiConnected = false;
const unsigned long debounceDelay = 5;
IPAddress broadcastIP;
const int localPort = 8000, remotePort = 9000;

WiFiUDP Udp;
Adafruit_DotStar strip(numLeds, dataPin, clockPin, DOTSTAR_BGR);
ArtnetWifi artnet;
WebServer server(80);

void saveWiFiCredentials(const String& ssid, const String& password) {
  EEPROM.writeString(ssidAddr, ssid);
  EEPROM.writeString(passwordAddr, password);
  EEPROM.commit();
}

void loadWiFiCredentials() {
  ssid = EEPROM.readString(ssidAddr);
  password = EEPROM.readString(passwordAddr);
}

void calculateBroadcastAddress() {
  broadcastIP = (WiFi.localIP() & WiFi.subnetMask()) | ~WiFi.subnetMask();
}

void sendOscMessage(int value) {
  OSCMessage msg("/atlas");
  msg.add((int32_t)value);
  Udp.beginPacket(broadcastIP, remotePort);
  msg.send(Udp);
  Udp.endPacket();
  msg.empty();
}

void handleEncoder() {
  int currentStateA = digitalRead(encoderPinA);
  int currentStateB = digitalRead(encoderPinB);
  bool valueChanged = false;

  if ((currentStateA != lastEncoderStateA) && (millis() - lastDebounceTime > debounceDelay)) {
    oscValue += (currentStateA == HIGH) ? (currentStateB == LOW ? 1 : -1) : 0;
    valueChanged = true;
    lastDebounceTime = millis();
  }
  lastEncoderStateA = currentStateA;

  if (digitalRead(encoderButtonPin) == LOW && (millis() - lastDebounceTime > debounceDelay)) {
    oscValue += 10;
    lastDebounceTime = millis();
    valueChanged = true;
  }

  if (valueChanged) sendOscMessage(oscValue);
}

void blinkLed() {
  unsigned long currentTime = millis();
  if (currentBlinkInterval > 0 && currentTime - lastLedToggleTime >= currentBlinkInterval) {
    ledState = !ledState;
    digitalWrite(builtInLedPin, ledState);
    lastLedToggleTime = currentTime;
    currentBlinkInterval = artnetActive ? (ledState ? artnetBlinkOnInterval : artnetBlinkOffInterval) : currentBlinkInterval;
  } else if (currentBlinkInterval == 0) {
    digitalWrite(builtInLedPin, HIGH);
  } else if (currentBlinkInterval < 0) {
    digitalWrite(builtInLedPin, LOW);
  }
}

void setupDualWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("_Led11", "00000000");
  WiFi.begin(ssid.c_str(), password.c_str());
  currentBlinkInterval = apModeBlinkInterval;

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 30) {
    delay(500);
    retryCount++;
    blinkLed();
  }

  if (WiFi.status() == WL_CONNECTED) {
    currentBlinkInterval = 0;
    wifiConnected = true;
    calculateBroadcastAddress();
    MDNS.begin("led11");
  } else {
    currentBlinkInterval = apModeBlinkInterval;
    wifiConnected = false;
  }
}

void setupWebServer() {
  server.on("/", []() {
    String html = "<html><body><h1>Quanta DMX Led 11</h1><p>AP IP: " + WiFi.softAPIP().toString() +
                  "</p><p>STA IP: " + WiFi.localIP().toString() +
                  "</p><form action='/setwifi' method='POST'>SSID:<input type='text' name='ssid' value='" + ssid +
                  "'><br>Password:<input type='password' name='password' value='" + password +
                  "'><br>DMX Universe:<input type='number' name='universe'><br><input type='submit' value='Update'></form></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/setwifi", []() {
    ssid = server.arg("ssid");
    password = server.arg("password");
    saveWiFiCredentials(ssid, password);
    setupDualWiFi();
    server.send(200, "text/plain", "Settings updated. Please reconnect.");
  });

  server.begin();
}

void setupArtNet() {
  artnet.begin();
  artnet.setArtDmxCallback([](uint16_t receivedUniverse, uint16_t length, uint8_t sequence, uint8_t* data) {
    if (receivedUniverse == universe && length >= numLeds * 3) {
      lastDmxPacketTime = millis();
      for (int i = 0; i < numLeds; i++) {
        int pixelIndex = i * 3;
        strip.setPixelColor(i, strip.Color(data[pixelIndex], data[pixelIndex + 1], data[pixelIndex + 2]));
      }
      strip.show();
      currentBlinkInterval = artnetBlinkOnInterval;
      artnetActive = true;
    }
  });
}

void loop() {
  artnet.read();
  server.handleClient();
  handleEncoder();
  blinkLed();

  if (millis() - lastDmxPacketTime > dmxTimeout && artnetActive) {
    artnetActive = false;
    currentBlinkInterval = wifiConnected ? 0 : apModeBlinkInterval;
  }

  if (WiFi.status() != WL_CONNECTED && wifiConnected) {
    currentBlinkInterval = apModeBlinkInterval;
    wifiConnected = false;
  } else if (WiFi.status() == WL_CONNECTED && !wifiConnected) {
    currentBlinkInterval = 0;
    wifiConnected = true;
  }

  if (!WiFi.isConnected() && WiFi.softAPgetStationNum() == 0) {
    currentBlinkInterval = -1;
  }
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  loadWiFiCredentials();
  pinMode(builtInLedPin, OUTPUT);
  pinMode(encoderPinA, INPUT);
  pinMode(encoderPinB, INPUT);
  pinMode(encoderButtonPin, INPUT_PULLUP);
  setupDualWiFi();
  setupWebServer();
  strip.begin();
  strip.clear();
  strip.show();
  setupArtNet();
}
