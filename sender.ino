// SENDER
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Pins and Settings
RF24 radio(9, 10);
const byte address[6] = "D4O21";
const uint8_t RF_CHANNEL = 108;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

Adafruit_BMP085 bmp;
bool bmpOK = false;

// 8-Byte Struct matching Receiver
struct Payload {
  int16_t waterRaw;      
  int16_t tempC_x10;
  int32_t pressurePa;    
};

Payload p;
Payload lastSent = { -1, 0, 0 };
unsigned long lastSampleMs = 0;
unsigned long lastSendMs = 0;
unsigned long lastDrawMs = 0;

const unsigned long SAMPLE_MS = 300;
const unsigned long HEARTBEAT_MS = 1200;
const unsigned long DRAW_INTERVAL_MS = 300;

void setup() {
  Serial.begin(9600);
  Wire.begin();
  
  // OLED Init
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED Fail"));
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  bmpOK = bmp.begin();
  
  // Radio Config for maximum stability
  radio.begin();
  radio.setChannel(RF_CHANNEL);
  radio.setPALevel(RF24_PA_MIN);    // Low power to avoid breadboard noise
  radio.setDataRate(RF24_250KBPS);  // Proven speed
  radio.setAutoAck(true);
  radio.setRetries(15, 15);         // Max retries to fix "flopping"
  radio.openWritingPipe(address);
  radio.stopListening();
}

void loop() {
  unsigned long now = millis();
  if (now - lastSampleMs < SAMPLE_MS) return;
  lastSampleMs = now;

  // Read Sensors
  p.waterRaw = (int16_t)analogRead(A0);
  if (bmpOK) {
    p.tempC_x10 = (int16_t)(bmp.readTemperature() * 10.0);
    p.pressurePa = bmp.readPressure();
  }

  // Send Logic: Send if enough time passed (heartbeat) OR if water level changed significantly
  if ((now - lastSendMs >= HEARTBEAT_MS) || (abs(p.waterRaw - lastSent.waterRaw) > 5)) {
    bool rfOK = radio.write(&p, sizeof(p));
    lastSent = p;
    lastSendMs = now;

    // Local UI update
    display.clearDisplay();
    display.setCursor(0,0);
    display.println(F("SENDER (Local)"));
    display.print(F("RF: ")); display.println(rfOK ? F("OK") : F("FAIL"));
    display.print(F("H2O: ")); display.println(p.waterRaw);
    display.print(F("Temp: ")); display.print(p.tempC_x10/10.0); display.println(F("C"));
    display.display();
  }
}
