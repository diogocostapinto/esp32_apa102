#include <WiFi.h>
#include <ArtnetWifi.h>
#include <Adafruit_DotStar.h>

// APA102 LED strip configuration
const int numLeds = 30;
const int dataPin = 14;  // Change data pin to GPIO2
const int clockPin = 12; // Change clock pin to GPIO0
Adafruit_DotStar strip = Adafruit_DotStar(numLeds, dataPin, clockPin, DOTSTAR_BGR);

// Art-Net configuration
ArtnetWifi artnet;
const int universe = 0;

const char* ssid = "Quanta-Broadcast";
const char* password = "Quanta@2023!";
//IPAddress artnetIp(10, 1, 9, 242);
//IPAddress routerIP(10, 1, 8, 1);
//IPAddress subnetIP(255, 255, 254, 0);

// GPIO2 for ESP-01 built-in LED
const int builtInLedPin = 2;

void setup() {
  Serial.begin(115200);
  pinMode(builtInLedPin, OUTPUT);

  // Connect to Wi-Fi with fixed IP
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  //WiFi.config(artnetIp, routerIP, subnetIP); // Set gateway and subnet mask
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

  //Serial.println("Art-Net receiver is ready");
  digitalWrite(builtInLedPin, HIGH);  // Turn on built-in LED to indicate successful setup
}

void loop() {
  // Check for Art-Net data
  artnet.read();
}

void onDmxFrame(uint16_t receivedUniverse, uint16_t length, uint8_t sequence, uint8_t* data) {
  // Check if the received universe and length match the expected values
  if (receivedUniverse == universe && length >= numLeds * 3) {
    // Log received Art-Net message
    //Serial.println("Received Art-Net message:");

    // Update LED strip with received data
    for (int i = 0; i < numLeds; i++) {
      int pixelIndex = i * 3;
      strip.setPixelColor(i, strip.Color(data[pixelIndex], data[pixelIndex + 1], data[pixelIndex + 2]));

      /* Log RGB values
      Serial.print("Pixel ");
      Serial.print(i);
      Serial.print(": R=");
      Serial.print(data[pixelIndex]);
      Serial.print(", G=");
      Serial.print(data[pixelIndex + 1]);
      Serial.print(", B=");
      Serial.println(data[pixelIndex + 2]);*/
    }

    // Show updated LED strip
    strip.show();
  }
}
