#include <WiFi.h>
#include <ArtnetWifi.h>
#include <Adafruit_DotStar.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ESPmDNS.h>  // Include the ESPmDNS library

// APA102 LED strip configuration
const int numLeds = 30;
const int dataPin = 14;
const int clockPin = 12;
Adafruit_DotStar strip = Adafruit_DotStar(numLeds, dataPin, clockPin, DOTSTAR_BGR);

// Art-Net configuration
ArtnetWifi artnet;
int universe = 0; // Default universe
WebServer server(80); // Web server for Wi-Fi settings

// Wi-Fi settings
const char* defaultSSID = "wifi";
const char* defaultPassword = "pass";
String ssid = defaultSSID;
String password = defaultPassword;

// EEPROM Addresses for saving Wi-Fi credentials
const int ssidAddr = 0;
const int passwordAddr = 32;

// GPIO for ESP-01 built-in LED
const int builtInLedPin = 2;

unsigned long lastDmxPacketTime = 0; // To track Art-Net packet receipt time
unsigned long dmxTimeout = 5000;     // Timeout for DMX data (5 seconds)

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
}

// Function to set up the Access Point mode
void setupAP() {
  WiFi.softAP("QuantaLed11", "00000000");
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  if (!MDNS.begin("QuantaLed11")) {
    Serial.println("Error setting up mDNS responder!");
  } else {
    Serial.println("mDNS responder started");
  }
}

// Function to connect to Wi-Fi
void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter < 30) {
    delay(500);
    Serial.print(".");
    counter++;
    digitalWrite(builtInLedPin, counter % 2); // Blink LED while connecting
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect, setting up AP...");
    setupAP();
  } else {
    Serial.println("\nConnected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(builtInLedPin, HIGH); // Solid LED when connected

    if (!MDNS.begin("QuantaLed11")) {
      Serial.println("Error setting up mDNS responder!");
    } else {
      Serial.println("mDNS responder started");
    }
  }
}

// Function to handle the web interface
void setupWebServer() {
  server.on("/", [ssid, password, universe]() mutable {
    String ipAddr = WiFi.localIP().toString();
    String html = "<html><body>"
                  "<h1>Quanta Led Box 11</h1>"
                  "<p>IP Address: " + ipAddr + "</p>"
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
    connectToWiFi();
    server.send(200, "text/plain", "Settings updated. Please reconnect to the AP if needed.");
  });

  server.begin();
}

// Art-Net callback function to handle incoming DMX data
void onDmxFrame(uint16_t receivedUniverse, uint16_t length, uint8_t sequence, uint8_t* data) {
  if (receivedUniverse == universe && length >= numLeds * 3) {
    lastDmxPacketTime = millis(); // Update the last packet time

    for (int i = 0; i < numLeds; i++) {
      int pixelIndex = i * 3;
      uint8_t r = data[pixelIndex];
      uint8_t g = data[pixelIndex + 1];
      uint8_t b = data[pixelIndex + 2];
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
  }
}

// Main setup function
void setup() {
  Serial.begin(115200);
  EEPROM.begin(512); // Initialize EEPROM
  loadWiFiCredentials(); // Load stored Wi-Fi credentials
  pinMode(builtInLedPin, OUTPUT);

  connectToWiFi();   // Attempt to connect to Wi-Fi
  setupWebServer();  // Start the web server
  strip.begin();     // Initialize the APA102 LED strip
  strip.clear();
  strip.show();

  // Initialize Art-Net
  artnet.begin();
  artnet.setArtDmxCallback(onDmxFrame);
}

// Main loop function
void loop() {
  artnet.read();       // Read Art-Net data
  server.handleClient(); // Handle web server requests

  // Reset LEDs if no DMX data is received for a while
  if (millis() - lastDmxPacketTime > dmxTimeout) {
    strip.clear();  // Turn off LEDs after timeout
    strip.show();
    lastDmxPacketTime = millis(); // Reset the timeout to avoid continuous clearing
  }
}
