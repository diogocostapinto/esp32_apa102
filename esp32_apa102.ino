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

// Variables for controlling LED blinking
unsigned long lastLedToggleTime = 0;
bool ledState = false;
unsigned long lastDmxPacketTime = 0;
unsigned long dmxTimeout = 5000;     // Timeout for DMX data (5 seconds)

// LED blink intervals (in milliseconds)
const int apModeBlinkInterval = 1000;   // Slow blink for AP mode
const int wifiSearchBlinkInterval = 250; // Fast blink when searching for Wi-Fi
const int artnetBlinkInterval = 100;    // Quick blink when receiving Art-Net data
int currentBlinkInterval = 0;           // Current blink interval based on mode

bool artnetActive = false; // To track if Art-Net is active

void saveWiFiCredentials(const String& ssid, const String& password) {
  EEPROM.writeString(ssidAddr, ssid);
  EEPROM.writeString(passwordAddr, password);
  EEPROM.commit();
}

void loadWiFiCredentials() {
  ssid = EEPROM.readString(ssidAddr);
  password = EEPROM.readString(passwordAddr);
}

void blinkLed() {
  unsigned long currentTime = millis();
  if (currentBlinkInterval > 0 && currentTime - lastLedToggleTime >= currentBlinkInterval) {
    ledState = !ledState;
    digitalWrite(builtInLedPin, ledState);
    lastLedToggleTime = currentTime;
  }
}

// Function to set up both Access Point and Station modes
void setupDualWiFi() {
  WiFi.mode(WIFI_AP_STA); // Set both AP and STA modes

  // Start the AP mode
  WiFi.softAP("_Led11", "00000000");
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Attempt to connect to stored Wi-Fi credentials in STA mode
  WiFi.begin(ssid.c_str(), password.c_str());
  currentBlinkInterval = wifiSearchBlinkInterval;  // Set blink interval for Wi-Fi search

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 30) {
    delay(500);
    Serial.print(".");
    retryCount++;
    blinkLed(); // Blink while searching for Wi-Fi
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(builtInLedPin, HIGH);  // Solid LED when connected to Wi-Fi
    currentBlinkInterval = 0;           // Stop blinking (solid ON) in Wi-Fi connected mode

    if (!MDNS.begin("led11")) {  // Start the mDNS responder for led11.local
      Serial.println("Error setting up MDNS responder!");
    } else {
      Serial.println("mDNS responder started");
    }
  } else {
    Serial.println("\nFailed to connect to Wi-Fi.");
    currentBlinkInterval = apModeBlinkInterval;  // Stay in AP mode blink if Wi-Fi fails
  }
}

// Setup a web server for Wi-Fi configuration
void setupWebServer() {
  server.on("/", [ssid, password, universe]() mutable {
    String apIpAddr = WiFi.softAPIP().toString();
    String staIpAddr = WiFi.localIP().toString();

    String html = "<html><body>"
                  "<h1>Quanta DMX Led 11</h1>"
                  "<p>AP IP Address: " + apIpAddr + "</p>"
                  "<p>STA IP Address: " + staIpAddr + "</p>"
                  "<form action='/setwifi' method='POST'>"
                  "SSID:<input type='text' name='ssid' value='" + ssid + "'><br>"
                  "Password:<input type='password' name='password' value='" + password + "'><br>"
                  "DMX Universe:<input type='number' name='universe' value='" + String(universe) + "'><br>"
                  "<input type='submit' value='Update'>"
                  "</form>"
                  "</body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/setwifi", [ssid, password, universe]() mutable {
    ssid = server.arg("ssid");
    password = server.arg("password");
    universe = server.arg("universe").toInt();
    saveWiFiCredentials(ssid, password);
    setupDualWiFi();  // Reconnect to Wi-Fi with new credentials
    server.send(200, "text/plain", "Settings updated. Please reconnect.");
  });

  server.begin();
}

// Function to handle Art-Net data reception and LED control
void setupArtNet() {
  artnet.begin();
  artnet.setArtDmxCallback([](uint16_t receivedUniverse, uint16_t length, uint8_t sequence, uint8_t* data) {
    if (receivedUniverse == universe && length >= numLeds * 3) {
      lastDmxPacketTime = millis(); // Update the last packet time
      for (int i = 0; i < numLeds; i++) {
        int pixelIndex = i * 3;
        strip.setPixelColor(i, strip.Color(data[pixelIndex], data[pixelIndex + 1], data[pixelIndex + 2]));
      }
      strip.show();
      currentBlinkInterval = artnetBlinkInterval; // Set blink interval for Art-Net reception
      artnetActive = true;  // Mark Art-Net as active
    }
  });
}

void loop() {
  artnet.read();
  server.handleClient();

  // Continuously blink the LED according to the current mode
  blinkLed();

  // Check for DMX timeout
  if (millis() - lastDmxPacketTime > dmxTimeout) {
    if (artnetActive) {
      // Freeze the last valid frame if Art-Net becomes inactive
      artnetActive = false;  // Mark as inactive
    }

    if (currentBlinkInterval == artnetBlinkInterval) {
      currentBlinkInterval = wifiSearchBlinkInterval; // Return to Wi-Fi search blink when no Art-Net data
    }
  }
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  loadWiFiCredentials();
  pinMode(builtInLedPin, OUTPUT);

  setupDualWiFi();  // Set up dual-mode Wi-Fi (AP and STA)

  setupWebServer();
  strip.begin();
  strip.clear();
  strip.show();
  setupArtNet();
}

