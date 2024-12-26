#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Adafruit_DotStar.h>
#include <ArtnetWifi.h>

// Art-Net and LED strip settings
int universe = 0;
const int numLeds = 30;
const int dataPin = 14;
const int clockPin = 12;
const int builtInLedPin = 2;

Adafruit_DotStar strip = Adafruit_DotStar(numLeds, dataPin, clockPin, DOTSTAR_BGR);
ArtnetWifi artnet;
WebServer server(80);

// Wi-Fi and network settings
const char* defaultSSID = "____2Ghz";
const char* defaultPassword = "Aa00000000";
String ssid = defaultSSID;
String password = defaultPassword;

// EEPROM Addresses
const int ssidAddr = 0;
const int passwordAddr = 32;

// Art-Net timing
const int targetFrameRate = 24;  // Target frame rate in FPS
const unsigned long frameInterval = 1000 / targetFrameRate; // Interval in milliseconds
unsigned long lastFrameTime = 0;

// Variables to track Art-Net state
bool newFrameReceived = false;

void saveWiFiCredentials(const String& ssid, const String& password) {
  EEPROM.writeString(ssidAddr, ssid);
  EEPROM.writeString(passwordAddr, password);
  EEPROM.commit();
}

void loadWiFiCredentials() {
  ssid = EEPROM.readString(ssidAddr);
  password = EEPROM.readString(passwordAddr);
}

// Function to force reset LED strip state
void resetStrip() {
  strip.clear(); // Clear the LED strip (set all to black)
  strip.show();  // Force update to apply the cleared state
  delay(50);     // Small delay to ensure the reset is applied
  for (int i = 0; i < numLeds; i++) {
    strip.setPixelColor(i, 0); // Explicitly set all LEDs to black
  }
  strip.show();  // Final show to ensure all LEDs are off
}

// Function to set up Wi-Fi
void setupDualWiFi() {
  WiFi.mode(WIFI_AP_STA);

  // Start AP mode
  WiFi.softAP("_Led11", "00000000");
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Attempt to connect to Wi-Fi in STA mode
  WiFi.begin(ssid.c_str(), password.c_str());
  int retryCount = 0;

  while (WiFi.status() != WL_CONNECTED && retryCount < 30) {
    delay(100);
    Serial.print(".");
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    if (!MDNS.begin("led11")) {
      Serial.println("Error setting up MDNS responder!");
    } else {
      Serial.println("mDNS responder started");
    }
  } else {
    Serial.println("\nFailed to connect to Wi-Fi.");
  }
}

// Setup the web server for configuration
void setupWebServer() {
  server.on("/", []() {
    String apIpAddr = WiFi.softAPIP().toString();
    String staIpAddr = WiFi.localIP().toString();

    String html = "<html><body>"
                  "<h1>Quanta DMX Led 11</h1>"
                  "<p>AP IP Address: " + apIpAddr + "</p>"
                  "<p>STA IP Address: " + staIpAddr + "</p>"
                  "<form action='/setwifi' method='POST'>"
                  "SSID:<input type='text' name='ssid'><br>"
                  "Password:<input type='password' name='password'><br>"
                  "DMX Universe:<input type='number' name='universe'><br>"
                  "<input type='submit' value='Update'>"
                  "</form>"
                  "</body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/setwifi", []() {
    ssid = server.arg("ssid");
    password = server.arg("password");
    universe = server.arg("universe").toInt();
    saveWiFiCredentials(ssid, password);
    setupDualWiFi();
    server.send(200, "text/plain", "Settings updated. Please reconnect.");
  });

  server.begin();
}

// Handle Art-Net data reception
void setupArtNet() {
  artnet.begin();
  artnet.setArtDmxCallback([](uint16_t receivedUniverse, uint16_t length, uint8_t sequence, uint8_t* data) {
    if (receivedUniverse == universe && length >= numLeds * 3) {
      // Immediately update LED strip
      for (int i = 0; i < numLeds; i++) {
        int pixelIndex = i * 3;
        strip.setPixelColor(i, strip.Color(data[pixelIndex], data[pixelIndex + 1], data[pixelIndex + 2]));
      }
      strip.show();  // Update LEDs as soon as data is received
      newFrameReceived = true;
    }
  });
}

void loop() {
  artnet.read(); // Continuously read Art-Net data
  server.handleClient(); // Handle web server requests

  unsigned long currentTime = millis();
  if (newFrameReceived && currentTime - lastFrameTime >= frameInterval) {
    // Ensure frame rate synchronization
    newFrameReceived = false;  // Reset the flag
    lastFrameTime = currentTime; // Update the frame timer
  }
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  loadWiFiCredentials();
  pinMode(builtInLedPin, OUTPUT);

  setupDualWiFi();
  setupWebServer();
  strip.begin();
  resetStrip(); // Ensure LED strip starts in a clean state
  setupArtNet();
}
