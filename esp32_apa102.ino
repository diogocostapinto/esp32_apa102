#include <WiFi.h>
#include <ArtnetWifi.h>
#include <Adafruit_DotStar.h>

const char* ssid = "Quanta-Broadcast";
const char* password = "Quanta@2023!";

// APA102 LED strip configuration
const int numLeds = 20;
const int dataPin = 12;  // Change data pin to GPIO2
const int clockPin = 14; // Change clock pin to GPIO0
Adafruit_DotStar strip = Adafruit_DotStar(numLeds, dataPin, clockPin, DOTSTAR_BGR);

// Art-Net configuration
ArtnetWifi artnet;
const int universe = 0;

// Fixed IP address for Art-Net
//IPAddress artnetIp(172, 17, 102, 102);

// GPIO2 for ESP-01 built-in LED
const int builtInLedPin = 2;

//unsigned long lastMillis = 0;
//int frameCounter = 0;

void setup() {
  Serial.begin(115200);
  pinMode(builtInLedPin, OUTPUT);

  // Connect to Wi-Fi with fixed IP
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  WiFi.config(artnetIp, IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0)); // Set gateway and subnet mask
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(builtInLedPin, HIGH);  // Turn on built-in LED to indicate Wi-Fi connection in progress
    delay(500);
    digitalWrite(builtInLedPin, LOW);
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  // Initialize APA102 LED strip
  strip.begin();
  strip.clear();  // Turn off all LEDs
  strip.show();

  // Initialize Art-Net
  artnet.begin();
  artnet.setArtDmxCallback(onDmxFrame);

  Serial.println("Art-Net receiver is ready");
  digitalWrite(builtInLedPin, HIGH);  // Turn on built-in LED to indicate successful setup
}

void loop() {
  // Check for Art-Net data
  artnet.read();

  // Calculate frames per second (FPS)
  //unsigned long currentMillis = millis();
  //if (currentMillis - lastMillis >= 1000) {
  //  lastMillis = currentMillis;
  //  Serial.println("FPS: " + String(frameCounter));
  //  frameCounter = 0;
  //}
}

void onDmxFrame(uint16_t receivedUniverse, uint16_t length, uint8_t sequence, uint8_t* data) {
  // Check if the received universe and length match the expected values
  if (receivedUniverse == universe && length >= numLeds * 3) {
    // Update LED strip with received data
    for (int i = 0; i < numLeds; i++) {
      int pixelIndex = i * 3;
      strip.setPixelColor(i, strip.Color(data[pixelIndex], data[pixelIndex + 1], data[pixelIndex + 2]));
    }

    // Show updated LED strip
    strip.show();
  }

  // Increment frame counter
  //frameCounter++;
}
