// ===================== SENDER.ino =====================
// UI: Bar graph + % (no raw water shown on OLED)
// Payload: waterRaw (debug), waterPct (display), tempC_x10

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- Radio ----------
RF24 radio(9, 10);
const byte address[6] = "D4O21";
const uint8_t RF_CHANNEL = 108;

// ---------- OLED ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------- BMP085 ----------
Adafruit_BMP085 bmp;
bool bmpOK = false;

// ---------- Calibration (EDIT THESE) ----------
// If your sensor works "backwards", swap EMPTY and FULL.
const int WATER_RAW_EMPTY = 0;     // raw when "empty" / dry
const int WATER_RAW_FULL  = 1023;  // raw when "full" / wet

// ---------- Payload ----------
struct Payload {
  int16_t waterRaw;      // keep for debug (NOT displayed on OLED)
  uint8_t waterPct;      // 0..100 (display)
  int16_t tempC_x10;     // temperature * 10
};

Payload p;
Payload lastSent = { -1, 0, 0 };

unsigned long lastSampleMs = 0;
unsigned long lastSendMs   = 0;
unsigned long lastDrawMs   = 0;

const unsigned long SAMPLE_MS        = 250;
const unsigned long HEARTBEAT_MS     = 900;
const unsigned long DRAW_INTERVAL_MS = 250;

bool lastRfOK = false;
uint32_t txCount = 0;

// Smoothed percent (reduces jitter)
uint8_t waterPctSmooth = 0;

static uint8_t rawToPercent(int16_t raw) {
  long pct;
  if (WATER_RAW_FULL > WATER_RAW_EMPTY) {
    pct = (long)(raw - WATER_RAW_EMPTY) * 100L / (long)(WATER_RAW_FULL - WATER_RAW_EMPTY);
  } else {
    pct = (long)(WATER_RAW_EMPTY - raw) * 100L / (long)(WATER_RAW_EMPTY - WATER_RAW_FULL);
  }
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return (uint8_t)pct;
}

static void drawBar(int x, int y, int w, int h, uint8_t pct) {
  display.drawRect(x, y, w, h, SSD1306_WHITE);
  int innerW = w - 2;
  int fillW = (innerW * pct) / 100;
  display.fillRect(x + 1, y + 1, fillW, h - 2, SSD1306_WHITE);
}

static void drawUI(bool rfOK, uint8_t pct, bool hasTemp, float tempC) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("SENDER"));
  display.setCursor(72, 0);
  display.print(F("RF:"));
  display.print(rfOK ? F("OK") : F("FAIL"));

  // Big percent
  display.setTextSize(2);
  display.setCursor(0, 14);
  display.print(pct);
  display.print(F("%"));

  // Bar graph
  drawBar(0, 38, 128, 12, pct);

  // Footer (temp + tx count)
  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print(F("Temp: "));
  if (hasTemp) {
    display.print(tempC, 1);
    display.print(F("C"));
  } else {
    display.print(F("N/A"));
  }

  display.setCursor(86, 54);
  display.print(F("#"));
  display.print(txCount);

  display.display();
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED Fail"));
  }
  display.clearDisplay();
  display.display();

  bmpOK = bmp.begin();

  // Radio config (stable)
  radio.begin();
  radio.setChannel(RF_CHANNEL);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.setAutoAck(true);
  radio.setRetries(15, 15);
  radio.openWritingPipe(address);
  radio.stopListening();

  // Initial UI
  drawUI(false, 0, bmpOK, 0.0f);
}

void loop() {
  unsigned long now = millis();

  // ---- Sample sensors ----
  if (now - lastSampleMs >= SAMPLE_MS) {
    lastSampleMs = now;

    p.waterRaw = (int16_t)analogRead(A0);
    p.waterPct = rawToPercent(p.waterRaw);

    // simple smoothing: 70% old, 30% new
    waterPctSmooth = (uint8_t)((waterPctSmooth * 7 + p.waterPct * 3) / 10);

    if (bmpOK) {
      p.tempC_x10 = (int16_t)(bmp.readTemperature() * 10.0f);
    } else {
      p.tempC_x10 = 0;
    }

    // Serial debug (raw stays here, not on OLED)
    Serial.print(F("waterRaw=")); Serial.print(p.waterRaw);
    Serial.print(F(" waterPct=")); Serial.print(p.waterPct);
    Serial.print(F(" tempC="));
    if (bmpOK) Serial.println(p.tempC_x10 / 10.0f);
    else Serial.println(F("N/A"));
  }

  // ---- Send logic ----
  bool shouldSend = (now - lastSendMs >= HEARTBEAT_MS) || (abs(p.waterRaw - lastSent.waterRaw) > 8);
  if (shouldSend) {
    lastRfOK = radio.write(&p, sizeof(p));
    txCount++;
    lastSent = p;
    lastSendMs = now;
  }

  // ---- Draw UI ----
  if (now - lastDrawMs >= DRAW_INTERVAL_MS) {
    lastDrawMs = now;
    float tempC = bmpOK ? (p.tempC_x10 / 10.0f) : 0.0f;
    drawUI(lastRfOK, waterPctSmooth, bmpOK, tempC);
  }
}
