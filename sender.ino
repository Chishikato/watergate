#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

RF24 radio(9, 10);
const byte address[6] = "D4O21";
const uint8_t RF_CHANNEL = 108;

const unsigned long SAMPLE_MS = 300;
const unsigned long HEARTBEAT_MS = 1200;

// OLED (sender)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// BMP (0x77)
Adafruit_BMP085 bmp;
bool bmpOK = false;

// Payload sent over RF (receiver must match)
struct Payload {
  int16_t waterRaw;      // 0..1023
  int16_t tempC_x10;     // temp*10
  int32_t pressurePa;    // Pa
};

Payload lastSent = { -1, 0, 0 };
unsigned long lastSampleMs = 0;
unsigned long lastSendMs = 0;
unsigned long lastDrawMs = 0;
const unsigned long DRAW_INTERVAL_MS = 300;

bool changedEnough(const Payload& a, const Payload& b) {
  return abs(a.waterRaw - b.waterRaw) >= 5 ||
         abs(a.tempC_x10 - b.tempC_x10) >= 2 ||
         labs(a.pressurePa - b.pressurePa) >= 50;
}

bool initOLED(uint8_t addr) {
  if (!display.begin(SSD1306_SWITCHCAPVCC, addr)) return false;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.display();
  return true;
}

void drawLocalUI(const Payload& p, bool rfOK) {
  display.clearDisplay();

  display.setCursor(0, 0);
  display.print("SENDER (Local)");

  display.setCursor(0, 12);
  display.print("RF send: ");
  display.print(rfOK ? "OK" : "FAIL");

  display.setCursor(0, 24);
  display.print("Water: ");
  display.print(p.waterRaw);

  display.setCursor(0, 36);
  display.print("Temp: ");
  display.print(p.tempC_x10 / 10.0);
  display.print("C");

  display.setCursor(0, 48);
  display.print("Pres: ");
  display.print(p.pressurePa / 100.0); // hPa
  display.print("hPa");

  display.display();
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(100000);

  // Try OLED at 0x3C then 0x3D
  bool oledOK = initOLED(0x3C) || initOLED(0x3D);
  Serial.println(oledOK ? "Sender OLED OK" : "Sender OLED NOT FOUND");

  bmpOK = bmp.begin();
  Serial.println(bmpOK ? "BMP OK" : "BMP NOT FOUND");

  radio.begin();
  radio.setChannel(RF_CHANNEL);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.setAutoAck(true);
  radio.setRetries(5, 15);
  radio.openWritingPipe(address);
  radio.stopListening();

  Serial.println("Sender ready");
}

void loop() {
  unsigned long now = millis();
  if (now - lastSampleMs < SAMPLE_MS) return;
  lastSampleMs = now;

  Payload p;
  p.waterRaw = (int16_t)analogRead(A0);

  if (bmpOK) {
    float t = bmp.readTemperature();
    p.tempC_x10 = (int16_t)(t * 10.0f);
    p.pressurePa = bmp.readPressure();
  } else {
    p.tempC_x10 = 0;
    p.pressurePa = 0;
  }

  bool shouldSend = (lastSent.waterRaw < 0) ||
                    changedEnough(p, lastSent) ||
                    (now - lastSendMs >= HEARTBEAT_MS);

  bool rfOK = true;
  if (shouldSend) {
    rfOK = radio.write(&p, sizeof(p));
    lastSent = p;
    lastSendMs = now;

    Serial.print("Sent water=");
    Serial.print(p.waterRaw);
    Serial.print(" temp=");
    Serial.print(p.tempC_x10 / 10.0);
    Serial.print("C pres=");
    Serial.print(p.pressurePa);
    Serial.print("Pa ok=");
    Serial.println(rfOK ? "1" : "0");
  }

  if (now - lastDrawMs >= DRAW_INTERVAL_MS) {
    drawLocalUI(p, rfOK);
    lastDrawMs = now;
  }
}
