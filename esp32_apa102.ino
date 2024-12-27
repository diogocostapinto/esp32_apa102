#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Adafruit_DotStar.h>
#include <ArtnetWifi.h>

// Configuration
const int numLeds = 30;                  // Number of LEDs
const int dataPin = 14;                  // Data pin for LED strip
const int clockPin = 12;                 // Clock pin for LED strip
const int builtInLedPin = 2;             // Built-in LED for status
const int universe = 0;                  // DMX universe
const int eepromSize = 512;              // EEPROM size for Wi-Fi credentials

// Wi-Fi Default Credentials
const char* defaultSSID = "YourSSID";
const char* defaultPassword = "YourPassword";

// EEPROM Addresses
const int ssidAddr = 0;
const int passwordAddr = 32;

// Instances
Adafruit_DotStar strip(numLeds, dataPin, clockPin, DOTSTAR_BGR);
ArtnetWifi artnet;
WebServer server(80);

// Global Variables
String ssid;
String password;

// Function to save Wi-Fi credentials to EEPROM
void saveWiFiCredentials(const String& ssid, const String& password) {
  EEPROM.writeString(ssidAddr, ssid);
  EEPROM.writeString(passwordAddr, password);
  EEPROM.commit();
}

// Function to load Wi-Fi credentials from EEPROM
void loadWiFiCredentials() {
  ssid = EEPROM.readString(ssidAddr);
  password = EEPROM.readString(passwordAddr);

  if (ssid.isEmpty() || password.isEmpty()) {
    ssid = defaultSSID;
    password = defaultPassword;
  }
}

// Function to clear the LED strip
void clearStrip() {
  strip.clear();
  strip.show();
}

// Function to set up Wi-Fi (Station + Access Point mode)
void setupWiFi() {
  WiFi.mode(WIFI_AP_STA);

  // Start Access Point mode
  WiFi.softAP("ESP32_LED_Control", "12345678");
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Attempt to connect in Station mode
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

// Function to handle Art-Net data
void setupArtNet() {
  artnet.begin();

  // Callback for handling DMX data
  artnet.setArtDmxCallback([](uint16_t receivedUniverse, uint16_t length, uint8_t sequence, uint8_t* data) {
    if (receivedUniverse == universe && length >= numLeds * 3) {
      // Update LED strip
      for (int i = 0; i < numLeds; i++) {
        int offset = i * 3;
        strip.setPixelColor(i, strip.Color(data[offset], data[offset + 1], data[offset + 2]));
      }
      strip.show();
    }
  });
}

// Function to set up the web server for configuration
void setupWebServer() {
  server.on("/", []() {
    String apIpAddr = WiFi.softAPIP().toString();
    String staIpAddr = WiFi.localIP().toString();

    String html = "<html><body>"
                  "<h1>ESP32 LED Control</h1>"
                  "<p>AP IP Address: " + apIpAddr + "</p>"
                  "<p>STA IP Address: " + staIpAddr + "</p>"
                  "<form action='/setwifi' method='POST'>"
                  "SSID: <input type='text' name='ssid'><br>"
                  "Password: <input type='password' name='password'><br>"
                  "<input type='submit' value='Save'>"
                  "</form>"
                  "</body></html>";

    server.send(200, "text/html", html);
  });

  server.on("/setwifi", []() {
    ssid = server.arg("ssid");
    password = server.arg("password");
    saveWiFiCredentials(ssid, password);
    setupWiFi();
    server.send(200, "text/plain", "Wi-Fi settings saved. Please reconnect.");
  });

  server.begin();
  Serial.println("Web server started.");
}

void setup() {
  // Initialize serial, EEPROM, and built-in LED
  Serial.begin(115200);
  EEPROM.begin(eepromSize);
  pinMode(builtInLedPin, OUTPUT);

  // Load Wi-Fi credentials and initialize components
  loadWiFiCredentials();
  setupWiFi();
  setupWebServer();

  // Initialize LED strip and Art-Net
  strip.begin();
  clearStrip();
  setupArtNet();
}

void loop() {
  artnet.read();       // Continuously read Art-Net packets
  server.handleClient(); // Handle web server requests
}
