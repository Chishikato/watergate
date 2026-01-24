// ===================== RECEIVER.ino =====================
// OLED: Big status (DRY/SEMI/MOIST/WET) + bar + small %
// OLED does not show raw water, and does not show pressure.
// Serial prints raw for calibration/debug.

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- OLED ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------- Radio ----------
RF24 radio(9, 10);
const byte address[6] = "D4O21";
const uint8_t RF_CHANNEL = 108;

// ---------- Payload (MUST match sender) ----------
struct Payload {
  int16_t waterRaw;     // debug only
  uint8_t waterPct;     // 0..100
  int16_t tempC_x10;    // temp * 10
};

Payload lastRecv = {0, 0, 0};
unsigned long lastRecvMs = 0;
unsigned long lastDrawMs = 0;

const unsigned long DRAW_INTERVAL_MS = 250;
const unsigned long LINK_TIMEOUT_MS  = 2000;

uint32_t rxCount = 0;
uint8_t waterPctSmooth = 0;

static const char* moistureLabel(uint8_t pct) {
  if (pct <= 25) return "DRY";
  if (pct <= 50) return "SEMI";
  if (pct <= 75) return "MOIST";
  return "WET";
}

static void drawBar(int x, int y, int w, int h, uint8_t pct) {
  display.drawRect(x, y, w, h, SSD1306_WHITE);
  int innerW = w - 2;
  int fillW = (innerW * pct) / 100;
  display.fillRect(x + 1, y + 1, fillW, h - 2, SSD1306_WHITE);
}

static void drawUI(bool linkOK, uint8_t pct, float tempC, uint16_t ageMs) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("RECEIVER"));
  display.setCursor(66, 0);
  display.print(F("Link:"));
  display.print(linkOK ? F("OK") : F("LOST"));

  // Big status (or LOST)
  display.setTextSize(2);
  display.setCursor(0, 14);
  if (linkOK) display.print(moistureLabel(pct));
  else display.print(F("LOST"));

  // Bar line: label + bar + small %
  display.setTextSize(1);
  int barX = 40, barY = 38, barW = 70, barH = 10;

  display.setCursor(0, 36);
  display.print(linkOK ? moistureLabel(pct) : "----");

  drawBar(barX, barY, barW, barH, linkOK ? pct : 0);

  display.setCursor(114, 36);
  if (linkOK) {
    display.print(pct);
    display.print('%');
  } else {
    display.print(F("--"));
  }

  // Footer: temp + age
  display.setCursor(0, 54);
  display.print(F("Temp: "));
  if (linkOK) {
    display.print(tempC, 1);
    display.print(F("C"));
  } else {
    display.print(F("N/A"));
  }

  display.setCursor(74, 54);
  display.print(F("Age:"));
  display.print(ageMs);
  display.print(F("ms"));

  display.display();
}

static bool oledBeginAutoAddr() {
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) return true;
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) return true;
  return false;
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  if (!oledBeginAutoAddr()) {
    Serial.println(F("OLED init failed (0x3C/0x3D)."));
  }
  display.clearDisplay();
  display.display();

  // Radio config
  radio.begin();
  radio.setChannel(RF_CHANNEL);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.setAutoAck(true);
  radio.openReadingPipe(1, address);
  radio.startListening();
}

void loop() {
  // Receive
  if (radio.available()) {
    radio.read(&lastRecv, sizeof(Payload));
    lastRecvMs = millis();
    rxCount++;

    // smoothing:
    waterPctSmooth = (uint8_t)((waterPctSmooth * 7 + lastRecv.waterPct * 3) / 10);

    // Serial debug (raw only here)
    Serial.print(F("RX#")); Serial.print(rxCount);
    Serial.print(F(" waterRaw=")); Serial.print(lastRecv.waterRaw);
    Serial.print(F(" waterPct=")); Serial.print(lastRecv.waterPct);
    Serial.print(F(" label=")); Serial.print(moistureLabel(lastRecv.waterPct));
    Serial.print(F(" tempC=")); Serial.println(lastRecv.tempC_x10 / 10.0f);
  }

  // Draw
  unsigned long now = millis();
  if (now - lastDrawMs >= DRAW_INTERVAL_MS) {
    lastDrawMs = now;

    bool linkOK = (now - lastRecvMs) < LINK_TIMEOUT_MS;
    uint16_t ageMs = (lastRecvMs == 0) ? 9999 : (uint16_t)min((unsigned long)9999, now - lastRecvMs);
    float tempC = lastRecv.tempC_x10 / 10.0f;

    drawUI(linkOK, waterPctSmooth, tempC, ageMs);
  }
}
